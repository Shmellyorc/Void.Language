#include "void/backend.hpp"
#include "void/frontend.hpp"
#include <iostream>
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("Supply the project-local native example entry .void file");
    // Explicitly trust only the statically linked fixture and its checked catalog.
    auto analysis=vd::analyze_project(argv[1],vd::native_fixture_project());
    if(!analysis.succeeded())throw vd::CompilationError(analysis.diagnostics);
    vd::Jit jit;auto code=jit.compile(*analysis.program);std::cout<<code->invoke(7)<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
