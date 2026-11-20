#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "proof.hpp"
#include <iostream>
int main(int argc,char** argv) {
    try {
        if(argc==5 && std::string(argv[1])=="--emit-library") {
            auto program=vd::compile_project(argv[2]);program.module.async_root=true;
            vd::emit_runtime_object(program.module,argv[3],std::string(argv[4])=="optimized");return 0;
        }
        managed_test::require(argc==2,"repository root required");
        auto analysis=vd::analyze_project(std::filesystem::path(argv[1])/"tests/async/advanced/roots.void");
        if(!analysis.succeeded())throw vd::CompilationError(analysis.diagnostics);
        for(bool optimized:{false,true}) {
            vd::Jit jit;auto code=jit.compile_async(*analysis.program,optimized);auto runtime=code->runtime_owner();
            advanced_async_test::exercise(*runtime,[&](int mode){return code->invoke(mode);});
        }
        std::cout<<"advanced async: 60 JIT exit/GC observations passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
