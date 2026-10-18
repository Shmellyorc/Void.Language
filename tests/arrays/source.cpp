#include "../managed/probe.hpp"
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include <future>
#include <cstring>
#include <iostream>
using namespace vd;
using managed_test::require;
namespace {
void collect_empty(Runtime& r) {
    Runtime::Attachment a(r);Runtime::Managed m(r);
    require(r.collect()==Runtime::Collection::completed && !r.snapshot().objects,"array temporaries/cycles survived cleanup");
}
void source_programs(const std::filesystem::path& root,bool optimized) {
    for(auto [name,answer]:{std::pair{"strings",10},{"values",10},{"nested",24},{"defaults",14},{"interfaces",19},{"selection",32},{"tree",10},{"readonly",9},{"iteration",37},{"ranges",25},{"cycles",14},{"empty",7},{"large",14},{"failures",17}}) {
        auto program=compile_project(root/"examples/arrays"/(std::string(name)+".void"));auto runtime=std::make_shared<Runtime>(1);Jit jit;
        auto code=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);
        managed_test::Probe probe;probe.inspect_roots=true;
        {managed_test::Observe observe(probe);if(code->invoke(7)!=answer)throw std::runtime_error(std::string("array source result: ")+name);}
        require(probe.roots_valid && probe.roots_checked && probe.frames.empty(),"invalid/balanced source array roots");
        require(runtime->snapshot().completed_epoch>0,"array pressure did not collect");collect_empty(*runtime);
    }
    for(auto [name,modes]:{std::pair{"invalidation",std::vector{1,2,5,6}},{"nested_failure",std::vector{1,2}}}) {
        auto program=compile_project(root/"examples/arrays"/(std::string(name)+".void"));auto runtime=std::make_shared<Runtime>(1);Jit jit;
        auto code=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);
        for(auto mode:modes) {
            bool failed=false;try{code->invoke(mode);}catch(const std::runtime_error& e){failed=std::string(e.what()).find(std::string(name)+".void:")!=std::string::npos;}
            require(failed,"missing structural/bounds failure location");collect_empty(*runtime);
            require(code->invoke(0)==(std::string(name)=="invalidation"?3:1),"array invocation failed to recover");collect_empty(*runtime);
        }
        if(std::string(name)=="invalidation") {require(code->invoke(3)==1 && code->invoke(4)==40,"break/return performed a spurious iterator access");collect_empty(*runtime);}
    }
}
void negative_sources() {
    const std::string head="struct node:\n    value: i32\nfunc entry(x: i32) -> i32:\n";
    for(const auto& body:{
        "    var a = [1]\n    a[0].count = 3\n    return x\n",
        "    var a: [[i32]] = [[1]]\n    a[0].count = 3\n    return x\n",
        "    var a: [node, 2]\n    return x\n",
        "    var a = [null]\n    return x\n",
        "    const item = node()\n    var a: [node] = []\n    a.push(item)\n    return x\n",
        "    const item = node()\n    var a = [item]\n    var alias = a\n    alias[0].value = 3\n    return x\n",
        "    const item = node()\n    var a = [item]\n    var b: [node] = a\n    b[0].value = 3\n    return x\n",
        "    const n = node()\n    var a = [n]\n    var b = [node()]\n    a = b\n    a.push(n)\n    return b[0].value\n",
        "    const a: [[node]] = [[node()]]\n    a[0][0].value = 3\n    return x\n",
        "    var a: [node, 1] = [node()]\n    const view = a\n    var copy = view\n    copy[0].value = 3\n    return x\n",
        "    for n in 0.5f..1.5f:\n        pass\n    return x\n",
        "    var end: i64 = 2\n    var start: i32 = 0\n    for n in start..end:\n        pass\n    return x\n",
        "    for n, index in 0..3:\n        pass\n    return x\n",
        "    var a = [1]\n    for n in a:\n        var n = 2\n    return x\n",
        "    var a = [1]\n    for n in a:\n        pass\n    return n\n"
    })if(analyze_source({"negative.void",head+body}).succeeded())throw std::runtime_error(std::string("invalid array source accepted: ")+body);
    const std::string thaw="struct node:\n    value: i32\nfunc expose(a: [node]) -> node:\n    return a[0]\nfunc entry(x: i32) -> i32:\n    const n = node()\n    var a = [n]\n    var b = expose(a)\n    b.value = 2\n    return x\n";
    require(!analyze_source({"thaw.void",thaw}).succeeded(),"array argument erased readonly element profile");
    const std::string recursive="value struct tree:\n    children: [tree]\nfunc entry(x: i32) -> i32:\n    var root = tree()\n    root.children.push(tree())\n    const frozen = root\n    var copied = frozen.children[0]\n    copied.children.push(tree())\n    return x\n";
    require(!analyze_source({"recursive-readonly.void",recursive}).succeeded(),"recursive element profile was thawed");
    const std::string inline_method="value struct point:\n    value: i32\nfunc (p point) change():\n    p.value++\nfunc entry(x: i32) -> i32:\n    var a: [point] = [point()]\n    a[0].change()\n    return x\n";
    require(!analyze_source({"borrow.void",inline_method}).succeeded(),"inline array element method silently mutated a copy");
}
void ir_checks(const std::filesystem::path& root) {
    auto module=compile_project(root/"examples/arrays/selection.void").module;ir::validate(module);
    auto reject=[](ir::Module bad){bool failed=false;try{ir::validate(bad);}catch(const std::invalid_argument&){failed=true;}require(failed,"malformed array IR accepted");};
    for(auto op:{ir::Op::array_get,ir::Op::array_set,ir::Op::list_commit}) {
        auto bad=module;bool found=false;for(auto& f:bad.functions)for(auto& b:f.blocks)for(auto& i:b.instructions)if(i.op==op && !found){i.arguments.push_back(999999);found=true;}
        require(found,"missing malformed IR fixture");reject(bad);
    }
    auto bad=module;for(auto& f:bad.functions)for(auto& b:f.blocks)for(auto& i:b.instructions)if(i.op==ir::Op::fixed_array && i.type.kind==ir::Type::growable_array)i.type=ir::readonly_view(i.type);reject(bad);
    auto arrays=compile_project(root/"examples/arrays/defaults.void").module;
    bool changed=false;for(auto& f:arrays.functions)for(auto& b:f.blocks)for(auto& i:b.instructions)if(i.op==ir::Op::array_initialize && !changed){i.arguments.clear();changed=true;}
    require(changed,"missing runtime construction CFG");reject(arrays);
}
void source_failures(const std::filesystem::path& root,bool optimized) {
    for(auto [name,answer]:{std::pair{"failures",17},{"defaults",14}}) {
    auto program=compile_project(root/"examples/arrays"/(std::string(name)+".void"));auto runtime=std::make_shared<Runtime>(1);Jit jit;
    auto code=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);require(code->invoke(7)==answer,"failure warmup");collect_empty(*runtime);
    unsigned failures=0;bool success=false;
    for(int budget=0;budget<4096 && !success;++budget) {
        try{managed_test::AllocationFailure inject(budget);require(code->invoke(7)==answer,"wrong successful failure-injected result");success=true;}
        catch(const std::bad_alloc&){++failures;}catch(const std::runtime_error&){++failures;}
        collect_empty(*runtime);require(code->invoke(7)==answer,"failed array invocation poisoned retry");collect_empty(*runtime);
    }
    require(success && failures>20,"array failure sweep incomplete");std::cout<<name<<" source "<<(optimized?"O2":"normal")<<": "<<failures<<" failing allocation budgets; cleanup/retry passed\n";
    }
}
struct NativeFixture {
    static constexpr std::size_t zero=0;
    Runtime r{1};Runtime::Attachment attachment{r};Runtime::Managed managed{r};
    void* list=nullptr;void* item=nullptr;void* removed=nullptr;
    Runtime::Slot slots[3]={{&list,sizeof(list),&zero,1},{&item,sizeof(item),&zero,1},{&removed,sizeof(removed),&zero,1}};
    Runtime::Frame roots{r,slots,3};
    NativeArrayField field{"array-test-node",0,0,11,0};NativeArrayElement element{"array-test-elements",sizeof(void*),alignof(void*),1,&field};
    NativeFixture(){NominalDescriptor type{"array-test-node","","node-v1",{sizeof(std::uint64_t),{}}};{Runtime::Native native(r);r.admit_types({&type,1});}auto layout=r.descriptor("array-test-node")->layout;layout.descriptor=r.descriptor("array-test-node");std::uint64_t number=123;r.construct_into(&item,layout,&number);}
    void create(){RuntimeFailure f;vd_value_array_new(&list,&element,0,1,&f);require(!f.message,"native list construction failed");}
    void push(){RuntimeFailure f;vd_value_array_push(&list,&element,&item,&f);require(!f.message,"native reference append failed");}
    void clear(){list=item=removed=nullptr;require(r.collect()==Runtime::Collection::completed && !r.snapshot().objects,"native array leaked");}
};
void native_transactions() {
    unsigned cold=0;bool made=false;
    for(int budget=0;budget<512 && !made;++budget) {
        NativeFixture f;RuntimeFailure error;
        {managed_test::AllocationFailure inject(budget);vd_value_array_new(&f.list,&f.element,0,1,&error);}
        if(error.message){++cold;require(!f.list,"failed native construction published");require(f.r.collect()==Runtime::Collection::completed && f.r.snapshot().objects==1,"failed construction retained objects");f.create();}else made=true;
        f.push();f.clear();
    }require(made && cold>10,"cold reference construction sweep incomplete");
    for(unsigned operation=0;operation<3;++operation) {
        unsigned failures=0;bool success=false;
        for(int budget=0;budget<512 && !success;++budget) {
            NativeFixture f;f.create();for(int n=0;n<4;++n)f.push();
            if(operation==1){RuntimeFailure error;vd_value_array_remove(&f.removed,&f.list,&f.element,3,&error);require(!error.message,"in-capacity setup failed");}
            auto layout=f.r.native_array_layout(f.element,0);auto token=f.r.root_word(&f.list);auto count=f.r.array_count(token,layout,true),version=f.r.array_version(token,layout);auto* header=f.list;f.removed=nullptr;
            RuntimeFailure error;{managed_test::AllocationFailure inject(budget);if(operation==2)vd_value_array_remove(&f.removed,&f.list,&f.element,1,&error);else vd_value_array_push(&f.list,&f.element,&f.item,&error);}
            if(error.message) {
                ++failures;require(f.list==header && !f.removed && f.r.array_count(token,layout,true)==count && f.r.array_version(token,layout)==version,"failed native operation changed published state");
                for(std::uint64_t i=0;i<count;++i){f.r.array_read(&f.removed,token,layout,true,i);require(f.removed==f.item,"failed native operation changed references");}
            }else success=true;
            require(f.r.collect()==Runtime::Collection::completed && f.r.snapshot().objects==3,"abandoned growth buffer was retained");
            f.clear();
        }require(success && failures>0,"native transaction sweep incomplete");std::cout<<"reference transaction "<<operation<<": "<<failures<<" failing budgets passed\n";
    }
    NativeFixture f;f.create();f.push();RuntimeFailure error;vd_value_array_remove(&f.removed,&f.list,&f.element,0,&error);require(!error.message && f.removed==f.item,"removed reference not published");f.item=nullptr;
    require(f.r.collect()==Runtime::Collection::completed && f.r.snapshot().objects==3,"removed live result was lost");f.removed=nullptr;
    require(f.r.collect()==Runtime::Collection::completed && f.r.snapshot().objects==2,"vacated backing retained deleted reference");
    auto wrong=f.element;wrong.bytes*=2;RuntimeFailure invalid;auto* sentinel=f.list;vd_value_array_new(&f.list,&wrong,0,1,&invalid);require(invalid.message && f.list==sentinel,"incompatible element metadata published");f.clear();
    std::cout<<"cold reference construction: "<<cold<<" failing budgets passed\n";
}
void retirement_and_reload(const std::filesystem::path& root,bool optimized) {
    auto program=compile_project(root/"examples/arrays/retirement.void");auto runtime=std::make_shared<Runtime>(1);Jit jit;
    auto code=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);
    managed_test::Probe probe;probe.capture=true;probe.capture_at=2;
    {managed_test::Observe observe(probe);require(code->invoke(7)==7,"array retirement setup");}
    require(probe.retained && jit.live_generations()==1,"array carrier capture absent");
    require(program.module.interface_bindings.size()==1,"retirement interface binding absent");
    const auto binding=program.module.interface_bindings.front().key;const auto target=ir::Type::key(program.module.interface_bindings.front().interface);
    const auto key="void.array.element.v1/"+std::to_string(ir::Type::interface_ref)+":"+target;
    code.reset();require(jit.live_generations()==0,"array metadata retained allocating code");
    const void* witness=nullptr;
    {Runtime::Attachment a(*runtime);Runtime::Managed m(*runtime);require(runtime->collect()==Runtime::Collection::completed,"retired array GC failed");
        void* list=nullptr;std::memcpy(&list,probe.retained->get(),sizeof(list));struct Pair{void* object=nullptr;const void* witness=nullptr;} pair;
        constexpr std::size_t zero=0;Runtime::Slot slots[]={{&list,sizeof(list),&zero,1},{&pair,sizeof(pair),&zero,1}};Runtime::Frame roots(*runtime,slots,2);
        NativeArrayField field{target.c_str(),0,0,13,0};NativeArrayElement element{key.c_str(),sizeof(pair),alignof(Pair),1,&field};
        auto layout=runtime->native_array_layout(element,0);runtime->array_read(&pair,runtime->root_word(&list),layout,true,0);witness=pair.witness;
        const char* keys[]={binding.c_str()};require(runtime->interface_binding(pair.object,witness,keys)==0 && runtime->snapshot().objects==4,"retired array lost type/witness/reference data");
    }
    {
        auto next=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);ReloadSlot slot(next);
        auto source=read_source(root/"examples/arrays/retirement.void");auto changed=source.text();const auto position=changed.rfind(".read()");require(position!=std::string::npos,"reload edit absent");changed.insert(position+7," + 1");
        // Analyze an owned override of the same VFS path so only the body changes.
        SourceOverrides overrides{{"/retirement.void",SourceFile(source.path(),changed)}};
        auto revised=analyze_project(root/"examples/arrays/retirement.void",overrides);if(!revised.succeeded())throw CompilationError(revised.diagnostics);
        require(jit.replace(slot,*revised.program,optimized) && slot.invoke(7)==8,"array body hot reload failed");
        {Runtime::Attachment a(*runtime);Runtime::Managed m(*runtime);require(runtime->witness(binding)==witness,"compatible reload replaced array witness identity");}
    }
    probe.retained.reset();collect_empty(*runtime);require(jit.live_generations()==0,"array reload generation leaked");
}
void concurrent_collection() {
    NativeFixture f;f.create();for(int n=0;n<4;++n)f.push();
    auto requested=std::async(std::launch::async,[&]{return f.r.wait_for_request(std::chrono::seconds(5));});
    auto worker=std::async(std::launch::async,[&]{Runtime::Attachment a(f.r);Runtime::Managed m(f.r);for(int n=0;n<16;++n)require(f.r.collect()==Runtime::Collection::completed,"concurrent reference GC failed");});
    require(requested.get(),"GC request was not visible");for(int n=4;n<257;++n)f.push();{Runtime::Native native(f.r);worker.get();}
    auto layout=f.r.native_array_layout(f.element,0);auto token=f.r.root_word(&f.list);require(f.r.array_count(token,layout,true)==257,"concurrent growth lost count");
    for(std::uint64_t n=0;n<257;++n){f.r.array_read(&f.removed,token,layout,true,n);require(f.removed==f.item,"concurrent collection lost element identity");}f.clear();
}
}
int main(int argc,char** argv) {
    try{require(argc==2,"expected repository root");negative_sources();ir_checks(argv[1]);native_transactions();concurrent_collection();for(bool optimize:{false,true}){source_programs(argv[1],optimize);source_failures(argv[1],optimize);retirement_and_reload(argv[1],optimize);}std::cout<<"Phase 23 arrays/ranges: source, GC, readonly, publication and failure checks passed\n";}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
