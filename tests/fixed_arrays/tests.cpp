#include "../managed/probe.hpp"
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include <iostream>
using namespace vd;
using managed_test::require;
namespace {
void collect_empty(Runtime& runtime) {
    Runtime::Attachment attachment(runtime);Runtime::Managed managed(runtime);
    require(runtime.collect()==Runtime::Collection::completed,"array collection failed");
    require(!runtime.snapshot().objects,"array temporaries survived return");
}
void sources(const std::filesystem::path& root,bool optimized) {
    for(auto [file,expected]:{std::pair{"main",17}, {"copies",30},{"order",156},{"numeric",107},{"value_fields",36},{"empty",7},{"large",53},{"index_types",56},{"bounds_order",-33},{"loops",29}}) {
        auto project=compile_project(root/"examples/fixed_arrays"/(std::string(file)+".void"));
        auto runtime=std::make_shared<Runtime>(1);Jit jit;
        auto code=jit.compile(project.module,project.module.entry_symbol,optimized,{},runtime);
        managed_test::Probe probe;probe.inspect_roots=true;
        {managed_test::Observe observe(probe);require(code->invoke(std::string(file)=="index_types" || std::string(file)=="bounds_order"?0:7)==expected,"fixed array source result");}
        require(probe.roots_valid && probe.roots_checked && probe.frames.empty(),"invalid or unbalanced array roots");
        if(std::string(file)!="empty") require(runtime->snapshot().completed_epoch>0,"arrays did not cross collection");
        collect_empty(*runtime);
    }
}
void failures(const std::filesystem::path& root,bool optimized) {
    for(auto [file,input]:{std::pair{"indexes",3},{"indexes",-4},{"empty",-1},{"index_types",-1},{"index_types",1},{"bounds_order",3}}) {
        auto project=compile_project(root/"examples/fixed_arrays"/(std::string(file)+".void"));
        auto runtime=std::make_shared<Runtime>(1);Jit jit;
        auto code=jit.compile(project.module,project.module.entry_symbol,optimized,{},runtime);
        bool failed=false;
        try {code->invoke(input);}catch(const std::runtime_error& e) {failed=std::string(e.what()).find("Fixed array index outside extent")!=std::string::npos && std::string(e.what()).find(std::string(file)+".void:")!=std::string::npos;}
        require(failed,"array bounds/source failure missing");collect_empty(*runtime);
        require(code->invoke(file==std::string("empty")?7:0)==(file==std::string("empty")?7:file==std::string("index_types")?56:file==std::string("bounds_order")?-33:10),"array failure recovery");
        collect_empty(*runtime);
    }
}
void allocation_failures(const std::filesystem::path& root,bool optimized) {
    auto project=compile_project(root/"examples/fixed_arrays/copies.void");
    auto runtime=std::make_shared<Runtime>();Jit jit;auto code=jit.compile(project.module,project.module.entry_symbol,optimized,{},runtime);
    require(code->invoke(7)==30,"array allocation warmup");collect_empty(*runtime);
    unsigned failures=0;bool success=false;
    for(int budget=0;budget<1024;++budget) {
        try {managed_test::AllocationFailure inject(budget);require(code->invoke(7)==30,"allocation retry result");success=true;}
        catch(const std::bad_alloc&) {++failures;}
        catch(const std::runtime_error&) {++failures;}
        collect_empty(*runtime);
        require(code->invoke(7)==30,"allocation failure prevented retry");collect_empty(*runtime);
        if(success)break;
    }
    require(success && failures>10,"array allocation sweep incomplete");
    std::cout<<"source array "<<(optimized?"O2":"normal")<<": "<<failures<<" failing allocation budgets, recovery and cleanup passed\n";
    // Exercise the production ABI's output transaction directly with old
    // content still rooted, using the same allocation injector and Runtime.
    Runtime::Attachment attachment(*runtime);Runtime::Managed managed(*runtime);
    auto layout=runtime->fixed_scalar_layout(ScalarEncoding::i32,3);
    void* source=nullptr;void* result=nullptr;const std::size_t zero=0;
    const Runtime::Slot slots[]={{&source,sizeof(source),&zero,1},{&result,sizeof(result),&zero,1}};
    Runtime::Frame frame(*runtime,slots,2);RuntimeFailure failure;
    vd_array_default(&source,static_cast<unsigned>(ScalarEncoding::i32),3,7,&failure);
    vd_array_default(&result,static_cast<unsigned>(ScalarEncoding::i32),3,9,&failure);
    require(!failure.message,"array ABI setup failed");auto* sentinel=result;
    success=false;failures=0;
    for(int budget=0;budget<256;++budget) {
        failure={};
        {managed_test::AllocationFailure inject(budget);vd_array_copy(&result,&source,static_cast<unsigned>(ScalarEncoding::i32),3,&failure);}
        if(failure.message) {++failures;require(result==sentinel,"failed array copy published a result");}
        else {success=true;require(result!=sentinel && result!=source,"array copy did not produce independent storage");}
        require(runtime->collect()==Runtime::Collection::completed && runtime->snapshot().objects==2,"failed array copy retained private buffers");
        if(success)break;
    }
    require(success && failures>2,"array ABI failure sweep incomplete");
    source=result=nullptr;require(runtime->collect()==Runtime::Collection::completed && !runtime->snapshot().objects,"ABI failure sweep leaked");
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"expected repository root");
        for(bool optimized:{false,true}) {sources(argv[1],optimized);failures(argv[1],optimized);allocation_failures(argv[1],optimized);}
        std::cout<<"Numeric fixed arrays: source execution, roots, publication and failure recovery passed\n";
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
