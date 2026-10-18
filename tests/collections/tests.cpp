#include "../managed/probe.hpp"
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/tooling.hpp"
#include <cstring>
#include <iostream>
using namespace vd;
using managed_test::require;
namespace {
void empty(Runtime& r) {Runtime::Attachment a(r);Runtime::Managed m(r);require(r.collect()==Runtime::Collection::completed && !r.snapshot().objects,"collection roots/objects leaked");}
void programs(const std::filesystem::path& root,bool optimized) {
    for(auto [name,answer]:{std::pair{"main",12},{"nested",30},{"order",12},{"bounds",53},{"invalidation",2},{"readonly",14},{"numeric",7},{"large",14}}) {
        auto program=compile_project(root/"examples/collections"/(std::string(name)+".void"));auto runtime=std::make_shared<Runtime>(1);Jit jit;
        auto code=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);
        managed_test::Probe probe;probe.inspect_roots=true;
        {managed_test::Observe observe(probe);if(code->invoke(7)!=answer)throw std::runtime_error(std::string("source collection result: ")+name);}
        require(probe.roots_checked && probe.roots_valid && probe.commits_valid && probe.frames.empty(),"collection roots/publication invalid");empty(*runtime);
        if(std::string(name)=="bounds" || std::string(name)=="invalidation" || std::string(name)=="order") {
            auto modes=std::string(name)=="bounds"?std::vector{1,2,3,4,5,6}:std::string(name)=="order"?std::vector{1,2}:std::vector{1,2,6};
            for(auto mode:modes){bool failed=false;try{code->invoke(mode);}catch(const std::runtime_error& error){failed=std::string(error.what()).find(std::string(name)+".void:")!=std::string::npos;}require(failed,"missing checked collection failure");empty(*runtime);require(code->invoke(0)==answer-(std::string(name)=="main"?7:0),"failure poisoned retry");empty(*runtime);}
            if(std::string(name)=="invalidation"){require(code->invoke(3)==0 && code->invoke(4)==1 && code->invoke(5)==2,"break/return/replacement invalidation policy");empty(*runtime);}
        }
    }
}
void diagnostics() {
    const std::string entry="func entry(x: i32) -> i32:\n";
    for(const auto& body:{
        "    const a = [1]\n    a.insert(0, 2)\n    return x\n",
        "    const a = [1]\n    var b = a\n    b.clear()\n    return x\n",
        "    var a: [i32, 2]\n    a.clear()\n    return x\n",
        "    var a: [i32, 2]\n    a.insert(0, 1)\n    return x\n",
        "    var a = [1]\n    a.insert(true, 1)\n    return x\n",
        "    var a = [1]\n    a.insert(0, false)\n    return x\n",
        "    var a = [1]\n    var b = a[true..0]\n    return x\n",
        "    var a = [1]\n    var b = a[0..<0.5f]\n    return x\n",
        "    var a = [1]\n    a.pop()\n    return x\n",
        "    var a = [1]\n    var b = a[..1]\n    return x\n",
        "    var a = [1]\n    var b = a[0..]\n    return x\n",
        "    var a = \"abc\"\n    var b = a[0..1]\n    return x\n"
    })require(!analyze_source({"invalid-collections.void",entry+body}).succeeded(),"invalid collection operation accepted");
    const std::string node="struct node:\n    value: i32\n";
    for(const auto& body:{
        "    const n = node()\n    var a = [n]\n    var b = a[0..0]\n    b[0].value = 2\n    return x\n",
        "    const a = [node()]\n    var b = a[0..0]\n    var c = b\n    c[0].value = 2\n    return x\n",
        "    const a = [[node()]]\n    var b = a[0..0]\n    b[0].push(node())\n    return x\n",
        "    var a = [node()]\n    const n = node()\n    a.insert(0, n)\n    return x\n"
    })require(!analyze_source({"readonly-collections.void",node+entry+body}).succeeded(),"slice/insert erased readonly authority");
}
void failures(const std::filesystem::path& root,bool optimized) {
    for(auto [name,answer]:{std::pair{"main",12},{"nested",30}}) {
        auto program=compile_project(root/"examples/collections"/(std::string(name)+".void"));auto runtime=std::make_shared<Runtime>(1);Jit jit;auto code=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);
        require(code->invoke(7)==answer,"allocation sweep warmup");empty(*runtime);unsigned failures=0;bool success=false;
        for(int budget=0;budget<8192 && !success;++budget){managed_test::Probe probe;probe.inspect_roots=true;probe.frames.reserve(128);
            {managed_test::Observe observe(probe);try{managed_test::AllocationFailure inject(budget);require(code->invoke(7)==answer,"bad allocation-sweep result");success=true;}catch(const std::bad_alloc&){++failures;}catch(const std::runtime_error&){++failures;}}
            require(probe.roots_valid && probe.commits_valid && probe.frames.empty(),"failure root/publication cleanup");empty(*runtime);require(code->invoke(7)==answer,"failure poisoned subsequent invocation");empty(*runtime);
        }require(success && failures>0,"collection failure sweep incomplete");std::cout<<name<<(optimized?" O2: ":" normal: ")<<failures<<" allocation failures, roots/publication/cleanup/retry passed\n";
    }
}
struct Native {
    Runtime r{1};Runtime::Attachment attachment{r};Runtime::Managed managed{r};static constexpr std::size_t zero=0;
    void* list=nullptr;void* item=nullptr;void* result=nullptr;
    Runtime::Slot slots[3]={{&list,sizeof(list),&zero,1},{&item,sizeof(item),&zero,1},{&result,sizeof(result),&zero,1}};Runtime::Frame roots{r,slots,3};
    NativeArrayField field{"collection-node",0,0,11,0};NativeArrayElement element{"collection-elements",sizeof(void*),alignof(void*),1,&field};
    Native(){NominalDescriptor type{"collection-node","","node-v1",{8,{}}};{Runtime::Native n(r);r.admit_types({&type,1});}auto descriptor=r.descriptor("collection-node");auto layout=descriptor->layout;layout.descriptor=descriptor;std::uint64_t value=42;r.construct_into(&item,layout,&value);RuntimeFailure f;vd_value_array_new(&list,&element,0,1,&f);require(!f.message,"native list setup");}
    auto layout(){return r.native_array_layout(element,0);}
    void push(){RuntimeFailure f;vd_value_array_push(&list,&element,&item,&f);require(!f.message,"native append setup");}
    void finish(){list=item=result=nullptr;require(r.collect()==Runtime::Collection::completed && !r.snapshot().objects,"native collection cleanup");}
};
void native_transactions() {
    for(unsigned operation=0;operation<3;++operation){unsigned failures=0;bool success=false;
        for(int budget=0;budget<1024 && !success;++budget){Native f;for(int n=0;n<(operation==0?4:3);++n)f.push();auto layout=f.layout();auto token=f.r.root_word(&f.list);auto before=f.list;auto count=f.r.array_count(token,layout,true),version=f.r.array_version(token,layout);RuntimeFailure error;
            {managed_test::AllocationFailure inject(budget);if(operation<2)vd_value_array_insert(&f.list,&f.element,1,&f.item,version,&error);else vd_value_array_slice(&f.result,&f.list,&f.element,0,1,2,3,1,&error);}
            if(error.message){++failures;require(f.list==before && !f.result && f.r.array_count(token,layout,true)==count && f.r.array_version(token,layout)==version,"failed collection operation changed published state");}else{success=true;require(f.r.array_count(token,layout,true)==count+(operation<2),"native insertion count");require(f.r.array_version(token,layout)==version+(operation<2),"native structural version");}
            for(std::uint64_t i=0;i<count;++i){void* value=nullptr;f.r.array_read(&value,token,layout,true,i);require(value==f.item,"native element integrity lost");}
            require(f.r.collect()==Runtime::Collection::completed,"native collection during transaction");f.finish();
        }require(success && (operation==1 || failures>0),"native allocation-required sweep incomplete");std::cout<<"native operation "<<operation<<": "<<failures<<" allocation failures; rollback passed\n";
    }
    Native f;f.push();auto token=f.r.root_word(&f.list);auto layout=f.layout();auto original=f.list;auto version=f.r.array_version(token,layout);RuntimeFailure error;
    vd_value_array_clear(&f.list,&f.element,&error);require(!error.message && f.list==original && f.r.array_count(token,layout,true)==0 && f.r.array_version(token,layout)==version+1,"clear identity/version");
    vd_value_array_clear(&f.list,&f.element,&error);require(!error.message && f.r.array_version(token,layout)==version+1,"empty clear changed version");f.item=nullptr;
    require(f.r.collect()==Runtime::Collection::completed && f.r.snapshot().objects==2,"clear retained removed references or dropped backing");f.finish();
}
void evaluation_and_reload(const std::filesystem::path& root,bool optimized) {
    auto path=root/"examples/collections/order.void";auto program=compile_project(path);auto runtime=std::make_shared<Runtime>(1);Jit jit;auto code=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);
    managed_test::Probe probe;probe.capture=true;{managed_test::Observe observe(probe);bool failed=false;try{code->invoke(1);}catch(const std::runtime_error&){failed=true;}require(failed,"invalid insert did not fail");}require(bool(probe.retained),"evaluation probe missing");std::int32_t count=0;{Runtime::Attachment a(*runtime);Runtime::Managed m(*runtime);std::memcpy(&count,probe.retained->get(),sizeof(count));}require(count==1,"invalid insertion evaluated value before checking index");probe.retained.reset();empty(*runtime);
    managed_test::Probe modified;modified.capture=true;
    {managed_test::Observe observe(modified);bool failed=false;try{code->invoke(2);}catch(const std::runtime_error&){failed=true;}require(failed,"insertion did not detect structural RHS change");}
    require(bool(modified.retained),"RHS state capture missing");
    {Runtime::Attachment a(*runtime);Runtime::Managed m(*runtime);void* items=nullptr;auto descriptor=runtime->descriptor("/order.void::state");require(descriptor && !descriptor->layout.references.empty(),"state reference layout missing");std::memcpy(&items,static_cast<const char*>(modified.retained->get())+descriptor->layout.references.front(),sizeof(items));
        constexpr std::size_t zero=0;Runtime::Slot root{&items,sizeof(items),&zero,1};Runtime::Frame roots(*runtime,&root,1);auto token=runtime->root_word(&items);
        require(runtime->list_count(token,ScalarEncoding::i32)==3 && runtime->list_read(token,ScalarEncoding::i32,0)==1 && runtime->list_read(token,ScalarEncoding::i32,2)==9,"invalidated insertion lost RHS effects or committed its value");}
    modified.retained.reset();empty(*runtime);
    ReloadSlot slot(code);auto source=read_source(path);auto text=source.text();text.replace(text.rfind("return 12"),9,"return 13");auto changed=analyze_project(path,{{"/order.void",SourceFile(source.path(),text)}});if(!changed.succeeded())throw CompilationError(changed.diagnostics);require(jit.replace(slot,*changed.program,optimized) && slot.invoke(0)==13 && code->invoke(0)==12,"collection reload/old lease");empty(*runtime);
    auto analyzed=analyze_project(root/"examples/collections/nested.void");if(!analyzed.succeeded())throw CompilationError(analyzed.diagnostics);ToolingIndex index(analyzed.program);require(!index.references_to("/nested.void::cut").empty(),"collection generic source navigation lost");
}
}
int main(int argc,char** argv){try{require(argc==2,"expected repository root");diagnostics();native_transactions();for(bool optimized:{false,true}){programs(argv[1],optimized);failures(argv[1],optimized);evaluation_and_reload(argv[1],optimized);}std::cout<<"Collections: source/GC/rollback/readonly/iteration/reload passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
