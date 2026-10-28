#define main existing_log_test_main
#include "performance-base.cpp"
#undef main
#include "performance_metrics.hpp"
#include <cstdlib>

int main(int argc,char** argv){try {
    check(argc==5,"source, workspace, optimization and instrumentation required");
    fs::current_path(argv[2]);
#ifndef _WIN32
    struct rlimit stack{};check(getrlimit(RLIMIT_STACK,&stack)==0 && stack.rlim_cur==1048576 && stack.rlim_max==1048576,"hard 1 MiB stack");
#endif
    bool optimized=std::string(argv[3])=="optimized",instrumented=std::string(argv[4])=="instrumented";
    auto endpoint=std::make_shared<Endpoint>();auto start=std::chrono::steady_clock::now();
#ifdef LOG_AOT_PROBE
    auto runtime=std::make_shared<vd::Runtime>(256,vd::console::Streams::isolated(endpoint));
    auto code=std::make_shared<AotCode>(runtime,0);
#else
    vd::Jit jit;auto code=compile(jit,fs::absolute(argv[1]),optimized,endpoint,true);auto runtime=code->runtime_owner();
#endif
    auto preparation=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
    check(code->invoke(0)==7,"async matrix startup");
    auto hooks=std::make_shared<vd::filesystem::testing::Hooks>();std::map<Op,unsigned> operations;
    std::size_t before_close_heap=0,before_close_handles=0;
    hooks->before=[&](Op operation){++operations[operation];if(instrumented && operation==Op::close){auto snapshot=runtime->memory_snapshot();before_close_heap=snapshot.heap.payload_bytes;before_close_handles=snapshot.handles;}};
    vd::filesystem::testing::Observe observation(hooks);
    auto selected=[](const char* name,int value){auto* filter=std::getenv(name);return !filter || (","+std::string(filter)+",").find(","+std::to_string(value)+",")!=std::string::npos;};
    for(int mode=0;mode<=16;++mode) {
        if(!selected("VOID_PERF_MODES",mode))continue;
        for(int count:{100,250,500,1000}) {
            if(!selected("VOID_PERF_COUNTS",count))continue;
            const bool file=mode<16 && mode%8<4;
            const std::string record=(mode>=8 && mode<16?"2021-01-01T00:00:00.123Z ":"")+std::string("INFO \"\" \"async record\"\n");
            std::string expected;for(int i=0;i<count && mode<16;++i)expected+=record;
            for(int sample=-1;sample<3;++sample) {
                collect(*runtime);fs::remove("async-performance.log");endpoint->bytes.clear();endpoint->calls=0;operations.clear();
                before_close_heap=before_close_handles=0;
                auto before=runtime->memory_snapshot();auto epochs=runtime->snapshot();
                managed_test::Probe probe;probe.frames.reserve(32768);perf::reset();perf::enabled=instrumented;
                auto cpu=std::clock();auto wall=std::chrono::steady_clock::now();int answer;
                {managed_test::Observe source(probe);answer=code->invoke(mode*10000+count);}
                auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-wall).count();
                auto cpu_seconds=double(std::clock()-cpu)/CLOCKS_PER_SEC;perf::enabled=false;
                check(answer==7,"async matrix checksum");
                auto actual=file?read("async-performance.log"):endpoint->bytes;
                check(actual==expected,"async matrix complete-record byte oracle");
                check(operations[Op::write_bytes]==(file?unsigned(count):0u),"native file write count");
                check(endpoint->calls==(mode<16 && !file?unsigned(count):0u),"console transfer count");
                check(operations[Op::flush]==(file?1u:0u),"equivalent file flushing policy");
                auto transient=runtime->memory_snapshot();auto completed=runtime->snapshot();collect(*runtime);auto after=runtime->memory_snapshot();
                check(before.handles==after.handles,"async matrix persistent handle leak");
                std::cout<<"ASYNC {\"mode\":"<<mode<<",\"count\":"<<count<<",\"sample\":"<<sample
                    <<",\"optimized\":"<<optimized<<",\"instrumented\":"<<instrumented<<",\"clock_sampled_then_pinned\":true"
                    <<",\"preparation_ns\":"<<preparation<<",\"wall_ns\":"<<elapsed<<",\"cpu_seconds\":"<<cpu_seconds
                    <<",\"managed_constructions\":"<<probe.constructions<<",\"polls\":"<<probe.polls
                    <<",\"gc_collections\":"<<completed.completed_epoch-epochs.completed_epoch
                    <<",\"heap_before\":"<<before.heap.payload_bytes<<",\"heap_after_root\":"<<transient.heap.payload_bytes
                    <<",\"heap_after_gc\":"<<after.heap.payload_bytes<<",\"handles_before\":"<<before.handles<<",\"handles_after\":"<<after.handles
                    <<",\"before_close_heap\":"<<before_close_heap<<",\"before_close_handles\":"<<before_close_handles
                    <<",\"file_writes\":"<<operations[Op::write_bytes]<<",\"console_calls\":"<<endpoint->calls<<",\"flushes\":"<<operations[Op::flush]
                    <<",\"metadata\":"<<operations[Op::metadata]<<",\"output_bytes\":"<<actual.size()<<",\"metrics\":";
                perf::print(std::cout);std::cout<<"}\n"<<std::flush;
            }
        }
    }
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
