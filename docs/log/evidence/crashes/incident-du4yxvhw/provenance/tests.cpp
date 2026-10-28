#include "../managed/probe.hpp"
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include <future>
#include <iostream>
using namespace vd;
using managed_test::require;
namespace {
void collect_empty(Runtime& r) {
    Runtime::Attachment a(r);Runtime::Managed m(r);
    require(r.collect()==Runtime::Collection::completed && !r.snapshot().objects,"list temporaries survived return");
}
void sources(const std::filesystem::path& root,bool optimized) {
    for(auto [file,answer]:{std::pair{"main",13},{"aliases",29},{"selection",19},{"invalidation",38},{"indexes",12},{"numeric",107},{"large",262151}}) {
        auto program=compile_project(root/"examples/growable_arrays"/(std::string(file)+".void"));
        auto runtime=std::make_shared<Runtime>(1);Jit jit;auto code=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);
        managed_test::Probe probe;probe.inspect_roots=true;
        {managed_test::Observe observe(probe);require(code->invoke(file==std::string("invalidation") || file==std::string("indexes")?0:7)==answer,"source list result");}
        require(probe.roots_valid && probe.roots_checked && probe.frames.empty(),"invalid list roots or cleanup");
        require(runtime->snapshot().completed_epoch>0,"list allocations did not collect");collect_empty(*runtime);
    }
    for(auto [file,input]:{std::pair{"invalidation",1},{"invalidation",2},{"indexes",3},{"indexes",-4}}) {
        auto program=compile_project(root/"examples/growable_arrays"/(std::string(file)+".void"));
        auto runtime=std::make_shared<Runtime>(1);Jit jit;auto code=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);
        bool failed=false;try {code->invoke(input);}catch(const std::runtime_error& e){failed=std::string(e.what()).find(std::string(file)+".void:")!=std::string::npos;}
        require(failed,"missing checked list failure/source location");collect_empty(*runtime);
        require(code->invoke(0)==(file==std::string("invalidation")?38:12),"failed list invocation prevented recovery");collect_empty(*runtime);
    }
}
void extreme_indexes(bool optimized) {
    for(const auto& index:{"-9223372036854775808", "18446744073709551615"}) {
        std::string type=index[0]=='-' ? "i64" : "u64";
        auto analysis=analyze_source({"extreme.void","func entry(x: i32) -> i32:\n    var a = [1]\n    var index: "+type+" = "+index+"\n    return a[index]\n"});
        require(analysis.succeeded(),"valid extreme integer rejected");auto module=analysis.program->lower();
        auto runtime=std::make_shared<Runtime>(1);Jit jit;auto code=jit.compile(module,module.entry_symbol,optimized,{},runtime);
        bool failed=false;try{code->invoke(0);}catch(const std::runtime_error& e){failed=std::string(e.what()).find("extreme.void:")!=std::string::npos;}
        require(failed,"extreme list index escaped checking");collect_empty(*runtime);
    }
}
void scalar_read_effects(bool optimized) {
    auto analysis=analyze_source({"scalar-read-effects.void",R"VOID(
struct holder:
    data: [i32]
    calls: i32
func receiver(value: holder) -> [i32]:
    value.calls++
    return value.data
func change(value: holder, command: i32) -> i32:
    value.calls++
    if command == 1:
        value.data.clear()
        return 0
    value.data.push(99)
    value.data = [7]
    return -1
func readonly_read(const values: [i32], index: i32) -> i32:
    return values[index]
func entry(command: i32) -> i32:
    var value = holder(data = [10, 20, 30], calls = 0)
    var observed = receiver(value)[change(value, command)]
    if observed != 99 or value.calls != 2 or readonly_read(value.data, -1) != 7:
        return 1
    # Selected writes must still reject structural invalidation during the RHS.
    if command == 2:
        value.data[0] = change(value, 1)
    return 27
)VOID"});
    require(analysis.succeeded(),"scalar read effect fixture rejected");auto module=analysis.program->lower();ir::validate(module);
    auto runtime=std::make_shared<Runtime>(1);Jit jit;auto code=jit.compile(module,module.entry_symbol,optimized,{},runtime);
    managed_test::Probe probe;probe.inspect_roots=true;
    {managed_test::Observe observation(probe);require(code->invoke(0)==27,"scalar read did not retain pre-index owner or post-index length");}
    require(probe.roots_valid && probe.roots_checked && probe.frames.empty(),"scalar read root ownership");collect_empty(*runtime);
    for(int command:{1,2}) {
        bool failed=false;try{code->invoke(command);}catch(const std::runtime_error& error){failed=std::string(error.what()).find("scalar-read-effects.void:")!=std::string::npos;}
        require(failed,"scalar read bounds or selected-write invalidation bypassed");collect_empty(*runtime);
        require(code->invoke(0)==27,"scalar read failure prevented recovery");collect_empty(*runtime);
    }
}
void diagnostics_and_ir() {
    const std::string header="func entry(x: i32) -> i32:\n";
    for(const auto& body:{"    var a = []\n    return x\n", "    var a: [i32] = [1]\n    a.push(true)\n    return x\n", "    var a = [1, true]\n    return x\n", "    const a = [1]\n    var b = a\n    b.push(2)\n    return x\n", "    const a = [1]\n    a[0] = 2\n    return x\n", "    var a: [i32, 1] = [1]\n    a.push(2)\n    return x\n", "    var a = [1]\n    return a[true]\n", "    var a: [unknown]\n    return x\n", "    var a = [1]\n    var b = a.push(2)\n    return x\n", "    var a = [1]\n    return a.remove_at(0.5f)\n"})
        require(!analyze_source({"negative.void",header+body}).succeeded(),"invalid list source accepted");
    for(const auto& text:{"func mutate(a: [i32]):\n    a.push(1)\nfunc entry(x: i32) -> i32:\n    const a = [1]\n    mutate(a)\n    return x\n", "func mutate(const a: [i32]):\n    a.remove_at(0)\nfunc entry(x: i32) -> i32:\n    return x\n", "const a: [i32] = [1]\nfunc entry(x: i32) -> i32:\n    a.push(2)\n    return x\n"})
        require(!analyze_source({"negative.void",text}).succeeded(),"readonly list mutation escaped analysis");
    require(!analyze_source({"negative.void","value struct holder:\n    data: [i32]\nconst template = holder()\nfunc entry(x: i32) -> i32:\n    var copy = template\n    copy.data.push(2)\n    return x\n"}).succeeded(),"const template list leaf was thawed");
    auto program=analyze_source({"ir.void",header+"    var a = [1, 2]\n    a.push(x)\n    a[0] = 3\n    return a.remove_at(-1)\n"});
    require(program.succeeded(),"positive list analysis failed");auto module=program.program->lower();ir::validate(module);
    auto reject=[](ir::Module bad){bool failed=false;try {ir::validate(bad);}catch(const std::invalid_argument&){failed=true;}require(failed,"forged list IR accepted");};
    auto bad=module;bad.coordinated=false;reject(bad);
    bad=module;for(auto& f:bad.functions)for(auto& b:f.blocks)for(auto& i:b.instructions)if(i.op==ir::Op::fixed_array)i.type=ir::readonly_view(i.type);reject(bad);
    for(auto op:{ir::Op::list_push,ir::Op::list_commit,ir::Op::list_remove}) {
        bad=module;bool changed=false;
        for(auto& f:bad.functions)for(auto& block:f.blocks)for(auto& i:block.instructions)if(i.op==op && !changed){i.arguments.push_back(999999);changed=true;}
        require(changed,"missing list operation in IR fixture");reject(bad);
    }
    auto analysis=analyze_source({"tooling.void","func identity(a: [i32]) -> [i32]:\n    return a\n"});
    require(analysis.succeeded() && ir::type_name(analysis.program->declarations().front().parameters.front())=="[i32]","tooling lost structural list signature");
}
void source_failures(const std::filesystem::path& root,bool optimized) {
    auto program=compile_project(root/"examples/growable_arrays/main.void");auto runtime=std::make_shared<Runtime>(1);Jit jit;
    auto code=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);require(code->invoke(7)==13,"failure warmup");collect_empty(*runtime);
    unsigned failures=0;bool success=false;
    for(int budget=0;budget<2048;++budget) {
        try {managed_test::AllocationFailure inject(budget);require(code->invoke(7)==13,"failure retry result");success=true;}
        catch(const std::bad_alloc&){++failures;}catch(const std::runtime_error&){++failures;}
        collect_empty(*runtime);require(code->invoke(7)==13,"failure recovery failed");collect_empty(*runtime);if(success)break;
    }
    require(success && failures>10,"source list allocation sweep incomplete");std::cout<<"growable source "<<(optimized?"O2":"normal")<<": "<<failures<<" failing allocation budgets, recovery and cleanup passed\n";
}
void runtime_transactions() {
    Runtime r(1);Runtime::Attachment a(r);Runtime::Managed m(r);
    void* list=nullptr;void* alias=nullptr;constexpr std::size_t zero=0;Runtime::Slot slots[]={{&list,sizeof(list),&zero,1},{&alias,sizeof(alias),&zero,1}};Runtime::Frame frame(r,slots,2);
    const auto encoding=ScalarEncoding::i32;auto token=r.root_word(&list);
    for(unsigned operation=0;operation<4;++operation) {
        unsigned failures=0;bool success=false;
        for(int budget=0;budget<128;++budget) {
            r.list_into(&list,encoding);for(unsigned i=0;i<4;++i)r.list_push(token,encoding,i+10);
            if(operation==1)r.list_remove(token,encoding,3); // in-capacity append
            alias=list;const auto count=r.list_count(token,encoding),version=r.list_version(token,encoding);auto* sentinel=list;
            RuntimeFailure failure;
            {managed_test::AllocationFailure inject(budget);
                if(operation==0 || operation==1)vd_list_push(&list,static_cast<unsigned>(encoding),99,&failure);
                else if(operation==2)(void)vd_list_remove(&list,static_cast<unsigned>(encoding),1,&failure);
                else vd_list_new(&list,static_cast<unsigned>(encoding),0,&failure);
            }
            if(failure.message) {
                ++failures;require(list==sentinel && r.list_count(token,encoding)==count && r.list_version(token,encoding)==version,"failed list operation changed header");
                for(unsigned i=0;i<count;++i)require(r.list_read(token,encoding,i)==i+10,"failed list operation changed elements");
            } else success=true;
            require(r.collect()==Runtime::Collection::completed,"list transaction collection failed");
            require(r.snapshot().objects==(success && operation==3 ? 3u:2u),"abandoned list allocation retained");
            if(success)break;
        }
        // Growth/new headers must allocate; in-capacity append/removal may not.
        const bool allocation_required=operation==0 || operation==3;
        require(success && (!allocation_required || failures>0),"Runtime list allocation sweep incomplete");
        std::cout<<"list transaction "<<operation<<": "<<failures<<" failing allocation budgets passed\n";
    }
    alias=nullptr;r.list_into(&list,encoding);auto version=r.list_version(token,encoding);r.list_push(token,encoding,10);
    bool invalid=false;try {r.list_write(token,encoding,0,99,version);}catch(const ListModified&){invalid=true;}
    require(invalid && r.list_read(token,encoding,0)==10 && r.list_count(token,encoding)==1,"stale selected write was committed");
    RuntimeFailure mismatch;vd_list_push(&list,static_cast<unsigned>(ScalarEncoding::u32),42,&mismatch);
    require(mismatch.message && r.list_count(token,encoding)==1 && r.list_read(token,encoding,0)==10,"wrong list encoding changed storage");
    for(auto [bits,sign]:{std::pair{std::uint64_t{0},0u},{~std::uint64_t{0},1u}}) {
        RuntimeFailure outcome;
        require(vd_list_read_index(&list,static_cast<unsigned>(encoding),bits,sign,&outcome)==10 && !outcome.message,"checked list read normalization");
    }
    for(auto [bits,sign]:{std::pair{std::uint64_t{1},0u},{~std::uint64_t{0},0u},{std::uint64_t{1}<<63,1u},{std::uint64_t{0},2u}}) {
        RuntimeFailure outcome;vd_list_read_index(&list,static_cast<unsigned>(encoding),bits,sign,&outcome);
        require(outcome.message && r.list_read(token,encoding,0)==10,"invalid checked list read accepted or changed data");
    }
    void* unregistered=list;RuntimeFailure root_failure;vd_list_push(&unregistered,static_cast<unsigned>(encoding),42,&root_failure);
    require(root_failure.message && r.list_count(token,encoding)==1,"unregistered list root accepted");
    RuntimeFailure read_root_failure;vd_list_read_index(&unregistered,static_cast<unsigned>(encoding),0,0,&read_root_failure);
    require(read_root_failure.message,"checked scalar read accepted an unregistered root");
    RuntimeFailure failed;auto* sentinel=list;vd_list_new(&list,static_cast<unsigned>(encoding),~std::uint64_t{0},&failed);require(failed.message && list==sentinel,"unrepresentable list published");
    list=alias=nullptr;require(r.collect()==Runtime::Collection::completed && !r.snapshot().objects,"list transaction roots leaked");
}
void cold_construction() {
    unsigned failures=0;bool success=false;
    for(int budget=0;budget<512 && !success;++budget) {
        Runtime r(1);Runtime::Attachment a(r);Runtime::Managed m(r);void* list=nullptr;constexpr std::size_t zero=0;Runtime::Slot slot{&list,sizeof(list),&zero,1};Runtime::Frame frame(r,&slot,1);
        RuntimeFailure failure;{managed_test::AllocationFailure inject(budget);vd_list_new(&list,static_cast<unsigned>(ScalarEncoding::boolean),3,&failure);}
        if(failure.message) {++failures;require(!list,"failed cold construction published a header");}
        else success=true;
        require(r.collect()==Runtime::Collection::completed && r.snapshot().objects==(success?2u:0u),"cold construction retained partial storage");
        if(!success)r.list_into(&list,ScalarEncoding::boolean,3);
        auto token=r.root_word(&list);require(r.list_count(token,ScalarEncoding::boolean)==3 && r.list_read(token,ScalarEncoding::boolean,2)==0,"cold failure prevented recovery/default initialization");
        list=nullptr;require(r.collect()==Runtime::Collection::completed && !r.snapshot().objects,"cold construction cleanup failed");
    }
    require(success && failures>10,"cold allocation sweep incomplete");std::cout<<"cold list construction: "<<failures<<" failing allocation budgets passed\n";
}
void concurrent_collection() {
    Runtime r(1);Runtime::Attachment a(r);Runtime::Managed m(r);void* list=nullptr;constexpr std::size_t zero=0;Runtime::Slot slot{&list,sizeof(list),&zero,1};Runtime::Frame frame(r,&slot,1);
    r.list_into(&list,ScalarEncoding::i32);auto token=r.root_word(&list);for(unsigned i=0;i<4;++i)r.list_push(token,ScalarEncoding::i32,i);
    auto requested=std::async(std::launch::async,[&]{return r.wait_for_request(std::chrono::seconds(5));});
    auto collector=std::async(std::launch::async,[&]{Runtime::Attachment at(r);Runtime::Managed managed(r);for(int i=0;i<16;++i)require(r.collect()==Runtime::Collection::completed,"concurrent list GC failed");});
    require(requested.get(),"concurrent collection did not request a rendezvous");
    for(unsigned i=4;i<4097;++i)r.list_push(token,ScalarEncoding::i32,i);
    {Runtime::Native native(r);collector.get();}
    require(r.snapshot().cooperative_parks>0 && r.list_count(token,ScalarEncoding::i32)==4097,"list GC did not park or retain identity");
    for(unsigned i=0;i<4097;++i)require(r.list_read(token,ScalarEncoding::i32,i)==i,"list backing lost across concurrent collection");
    list=nullptr;require(r.collect()==Runtime::Collection::completed && !r.snapshot().objects,"concurrent list GC leaked");
}
}
int main(int argc,char** argv) {
    try {require(argc==2,"expected repository root");diagnostics_and_ir();runtime_transactions();cold_construction();concurrent_collection();for(bool optimized:{false,true}){sources(argv[1],optimized);extreme_indexes(optimized);scalar_read_effects(optimized);source_failures(argv[1],optimized);}std::cout<<"Growable lists: source, aliases, roots, transactions and failures passed\n";}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
