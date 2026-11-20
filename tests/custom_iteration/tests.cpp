#include "../storage_proof.hpp"
#include "proof.hpp"
#include "void/frontend.hpp"
#include <iostream>
int main(int argc,char** argv) {
    try {
        if(argc==5 && std::string(argv[1])=="--emit-library") {
            auto program=vd::compile_project(argv[2]);
            storage_fixture::emit(program.module,argv[3],std::string(argv[4])=="optimized",false);return 0;
        }
        managed_test::require(argc==2,"custom iteration source root");
        auto program=vd::compile_project(std::filesystem::path(argv[1])/"tests/custom_iteration/roots.void");
        for(bool optimized:{false,true}) {
            vd::Jit jit;auto runtime=std::make_shared<vd::Runtime>(1);
            auto code=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);
            iteration_test::exercise(*runtime,[&](int mode){return code->invoke(mode);});
        }
        auto analysis=vd::analyze_project(std::filesystem::path(argv[1])/"tests/custom_iteration/async.void");
        if(!analysis.succeeded())throw vd::CompilationError(analysis.diagnostics);
        for(bool optimized:{false,true}) {
            vd::Jit jit;auto code=jit.compile_async(*analysis.program,optimized);auto runtime=code->runtime_owner();
            iteration_test::exercise(*runtime,[&](int mode){return code->invoke(mode);});
        }
        std::cout<<"custom iteration: precise iterator/value roots and one-shot checked-failure cleanup passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
