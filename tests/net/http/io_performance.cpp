#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/runtime.hpp"
#include <chrono>
#include <ctime>
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::runtime_error("IO comparison source required");
        auto analysis=vd::analyze_project(argv[1]);
        if(!analysis.succeeded())throw vd::CompilationError(analysis.diagnostics);
        auto module=analysis.program->lower();module.async_root=true;
        auto runtime=std::make_shared<vd::Runtime>();runtime->async_worker_limit(1);
        vd::Jit jit;auto code=jit.compile(module,module.entry_symbol,true,{},runtime);
        for(int mode=0;mode<3;++mode)for(unsigned repeat=0;repeat<3;++repeat){
            const auto start=std::chrono::steady_clock::now();const auto cpu=std::clock();
            if(code->invoke(mode)!=0)throw std::runtime_error("IO/HTTP comparison failed");
            const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
            std::cout<<"{\"mode\":"<<mode<<",\"repeat\":"<<repeat<<",\"bytes\":16777216,\"writes\":256,\"workers\":1,\"seconds\":"<<seconds
                <<",\"process_cpu_seconds\":"<<static_cast<double>(std::clock()-cpu)/CLOCKS_PER_SEC<<"}\n";
            vd::Runtime::Attachment attached(*runtime);vd::Runtime::Managed managed(*runtime);
            runtime->collect();if(runtime->snapshot().objects)throw std::runtime_error("IO comparison retained managed values");
        }
        if(runtime->close())throw std::runtime_error("IO comparison Runtime shutdown");
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
