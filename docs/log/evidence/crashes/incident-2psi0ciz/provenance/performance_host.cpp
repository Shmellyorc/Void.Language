// The generated include is the existing native test host with only its Runtime
// budget changed to the production 256. Its main is unused here.
#define main existing_log_test_main
#include "performance-base.cpp"
#undef main
#include "performance_metrics.hpp"
#include <cstdlib>

int main(int argc,char** argv) {try {
    check(argc==5,"source, workspace, optimized flag and mode required");
    fs::current_path(argv[2]);
#ifndef _WIN32
    struct rlimit stack{};
    check(getrlimit(RLIMIT_STACK,&stack)==0 && stack.rlim_cur==1048576 && stack.rlim_max==1048576,"hard 1 MiB stack");
#endif
    const bool optimized=std::string(argv[3])=="optimized";
    const bool instrumented=std::string(argv[4])=="instrumented";
    auto endpoint=std::make_shared<Endpoint>();
    auto prepared=std::chrono::steady_clock::now();
#ifdef LOG_AOT_PROBE
    auto runtime=std::make_shared<vd::Runtime>(256,vd::console::Streams::isolated(endpoint));
    auto code=std::make_shared<AotCode>(runtime,0);
#else
    vd::Jit jit;
    auto code=compile(jit,fs::absolute(argv[1]),optimized,endpoint,true);
    auto runtime=code->runtime_owner();
#endif
    auto preparation_ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-prepared).count();
    auto hooks=std::make_shared<vd::filesystem::testing::Hooks>();std::map<Op,unsigned> operations;
    hooks->before=[&](Op operation){++operations[operation];};
    vd::filesystem::testing::Observe observe(hooks);
    auto selected=[](const char* name,int value) {
        const auto* filter=std::getenv(name);
        return !filter || (","+std::string(filter)+",").find(","+std::to_string(value)+",")!=std::string::npos;
    };
    for(int shape=0;shape<10;++shape) {
        if(!selected("VOID_PERF_SHAPES",shape))continue;
        check(code->invoke(1000+shape)==7,"framing input setup");
        for(int stage=0;stage<14;++stage) {
            if(!selected("VOID_PERF_STAGES",stage))continue;
            // The interpolation comparator is deliberately restricted to input
            // that requires no escaping; it is a lower bound, not a logger.
            if(stage==13 && (shape/3==1 || shape==9))continue;
            int expected=-1;
            for(int sample=-1;sample<3;++sample) {
                collect(*runtime);endpoint->bytes.clear();endpoint->calls=0;operations.clear();
                auto before=runtime->memory_snapshot();auto epochs=runtime->snapshot();
                managed_test::Probe probe;probe.frames.reserve(32768);
                perf::reset();perf::enabled=instrumented;
                auto cpu=std::clock();auto wall=std::chrono::steady_clock::now();
                int checksum;
                {managed_test::Observe source(probe);checksum=code->invoke(stage);}
                auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-wall).count();
                auto cpu_seconds=double(std::clock()-cpu)/CLOCKS_PER_SEC;
                perf::enabled=false;
                check(checksum>=0 && (expected==-1 || checksum==expected),"stable component checksum");expected=checksum;
                auto transient=runtime->memory_snapshot();auto completed=runtime->snapshot();
                collect(*runtime);auto after=runtime->memory_snapshot();
                std::cout<<"PERF {\"shape\":"<<shape<<",\"stage\":"<<stage<<",\"sample\":"<<sample
                    <<",\"optimized\":"<<optimized<<",\"instrumented\":"<<instrumented
                    <<",\"preparation_ns\":"<<preparation_ns<<",\"wall_ns\":"<<elapsed<<",\"cpu_seconds\":"<<cpu_seconds
                    <<",\"checksum\":"<<checksum<<",\"managed_constructions\":"<<probe.constructions<<",\"polls\":"<<probe.polls
                    <<",\"gc_collections\":"<<completed.completed_epoch-epochs.completed_epoch
                    <<",\"heap_before\":"<<before.heap.payload_bytes<<",\"heap_after_root\":"<<transient.heap.payload_bytes
                    <<",\"heap_after_gc\":"<<after.heap.payload_bytes<<",\"handles_before\":"<<before.handles
                    <<",\"handles_after\":"<<after.handles<<",\"console_calls\":"<<endpoint->calls
                    <<",\"output_bytes\":"<<endpoint->bytes.size()<<",\"native_writes\":"<<operations[Op::write_bytes]
                    <<",\"metrics\":";perf::print(std::cout);std::cout<<"}\n"<<std::flush;
            }
        }
    }
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
