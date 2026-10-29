#include "void/backend.hpp"
#include "void/frontend.hpp"
#include <chrono>
#include <ctime>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <sys/resource.h>
using Clock = std::chrono::steady_clock;
static double elapsed(Clock::time_point start) { return std::chrono::duration<double>(Clock::now()-start).count(); }
int main(int argc,char** argv) {try {
    if(argc!=6 && argc!=7)throw std::runtime_error("source, O2 flag, async flag, input and expected result required");
    rlimit stack{};if(getrlimit(RLIMIT_STACK,&stack)||stack.rlim_cur!=1048576||stack.rlim_max!=1048576)throw std::runtime_error("hard 1 MiB required");
    auto start=Clock::now();auto analysis=vd::analyze_project(argv[1]);
    if(!analysis.succeeded())throw vd::CompilationError(analysis.diagnostics);
    auto analysis_seconds=elapsed(start);start=Clock::now();
    auto module=analysis.program->lower();module.async_root=std::stoi(argv[3]);
    auto lowering_seconds=elapsed(start);start=Clock::now();auto jit=std::make_unique<vd::Jit>();
    auto jit_setup_seconds=elapsed(start);start=Clock::now();
    auto code=jit->compile(module,module.entry_symbol,std::stoi(argv[2]));auto compilation_seconds=elapsed(start);
    int input=std::stoi(argv[4]),expected=std::stoi(argv[5]);
    if(argc==7 && code->invoke(std::stoi(argv[6]))!=7)throw std::runtime_error("setup checksum mismatch");
    std::cout<<"METRIC {\"analysis_seconds\":"<<analysis_seconds<<",\"typed_lowering_seconds\":"<<lowering_seconds<<",\"jit_setup_seconds\":"<<jit_setup_seconds<<",\"llvm_compile_seconds\":"<<compilation_seconds<<",\"samples\":[";
    for(int sample=0;sample<3;++sample){start=Clock::now();auto cpu=std::clock();auto result=code->invoke(input);auto wall=elapsed(start);auto cpu_seconds=double(std::clock()-cpu)/CLOCKS_PER_SEC;if(result!=expected)throw std::runtime_error("independent checksum mismatch: "+std::to_string(result));if(sample)std::cout<<',';std::cout<<"{\"wall_seconds\":"<<wall<<",\"cpu_seconds\":"<<cpu_seconds<<",\"checksum\":"<<result<<'}';}
    start=Clock::now();code.reset();jit.reset();auto teardown=elapsed(start);
    rusage usage{};getrusage(RUSAGE_SELF,&usage);
    std::cout<<"],\"code_jit_teardown_seconds\":"<<teardown<<",\"peak_rss_kib\":"<<usage.ru_maxrss<<"}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
