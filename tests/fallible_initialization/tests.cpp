#include "../storage_proof.hpp"
#include "proof.hpp"
#include "void/frontend.hpp"
#include <iostream>
namespace {
using namespace managed_test;
bool rejected(const vd::ir::Module& module) {
    try{vd::ir::validate(module);return false;}catch(const std::invalid_argument&){return true;}
}
void malformed(const vd::ir::Module& module) {
    using O=vd::ir::Op;
    auto corrupt=module;bool changed=false;
    for(auto& f:corrupt.functions)for(auto& b:f.blocks)if(b.end.kind==vd::ir::Terminator::Kind::branch && b.instructions.at(b.end.value).op==O::stage_check) {
        std::swap(b.end.yes,b.end.no);changed=true;
    }
    require(changed && rejected(corrupt),"IR accepted swapped construction result branches");
    corrupt=module;changed=false;
    for(auto& f:corrupt.functions)for(auto& b:f.blocks)for(auto& i:b.instructions)if(i.op==O::stage_abort) {
        i={O::gc_poll};changed=true;
    }
    require(changed && rejected(corrupt),"IR accepted undisposed rejected construction");
    corrupt=module;changed=false;
    for(auto& f:corrupt.functions)if(f.initializer)for(auto& b:f.blocks)for(auto& i:b.instructions)if(i.op==O::initializer_return) {
        const auto& value=b.instructions.at(i.a);
        if(value.op==O::sum){i=value;changed=true;}
    }
    require(changed && rejected(corrupt),"IR accepted unchecked initializer return/readiness");
    corrupt=module;changed=false;
    for(auto& f:corrupt.functions)if(f.initializer)for(auto& b:f.blocks)for(auto& i:b.instructions)if(i.op==O::gc_write) {
        i={O::gc_poll};changed=true;
    }
    require(changed && rejected(corrupt),"IR accepted successful init without required writes");
}
}
int main(int argc,char** argv) {
    try {
        if(argc==5 && std::string(argv[1])=="--emit-library") {
            auto program=vd::compile_project(argv[2]);
            storage_fixture::emit(program.module,argv[3],std::string(argv[4])=="optimized",false);return 0;
        }
        require(argc==2,"fallible test source root");
        auto program=vd::compile_project(std::filesystem::path(argv[1])/"examples/fallible_initialization/roots.void");
        malformed(program.module);
        for(bool optimized:{false,true}) {
            vd::Jit jit;auto runtime=std::make_shared<vd::Runtime>(1);
            auto code=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);
            fallible_test::exercise(*runtime,[&](int input){return code->invoke(input);});
            Probe p;p.inspect_roots=true;p.collect_before=true;p.fail_construction=3;p.fail_allocation=0;
            bool failed=false;
            {Observe observing(p);try{code->invoke(7);}catch(const std::runtime_error& error){failed=std::string(error.what()).find("allocation failed")!=std::string::npos;}}
            require(failed && p.roots_valid && p.commits_valid && p.frames.empty(),"fallible constructor commit failure published output/leaked roots");
            {vd::Runtime::Attachment attached(*runtime);vd::Runtime::Managed managed(*runtime);runtime->collect();require(runtime->snapshot().objects==0,"commit failure retained private fields");}
            require(code->invoke(-7)==7,"typed construction failed to recover after commit failure");
        }
        std::cout<<"fallible initialization: typed errors, private stages, precise roots and malformed IR passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
