// Test-only embedding of the unchanged source application in one existing Runtime.
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/runtime.hpp"
#include "void/application.hpp"
#include "../managed/probe.hpp"
#include <iostream>
#include <stdexcept>
#include <string_view>

int main(int argc,char** argv) {
    try {
        if(argc<5)throw std::runtime_error("source, normal|o2, gc|faults, literal application arguments required");
        auto analysis=vd::analyze_project(argv[1]);
        if(!analysis.succeeded())throw vd::CompilationError(analysis.diagnostics);
        const bool optimized=std::string_view(argv[2])=="o2";
        const bool faults=std::string_view(argv[3])=="faults";
        auto runtime=std::make_shared<vd::Runtime>(256);
        runtime->async_worker_limit(1);
        std::vector<std::string> values;
        for(int index=4;index<argc;++index)values.emplace_back(argv[index]);
        runtime->application_arguments(vd::checked_application_arguments(std::move(values)));
        vd::Jit jit;
        auto module=analysis.program->lower();
        auto code=jit.compile(module,module.entry_symbol,optimized,{},runtime);
        if(faults && code->invoke(0)!=0)throw std::runtime_error("unfaulted source warmup failed");
        unsigned failures=0,successes=0;
        for(int budget: faults?std::vector<int>{0,1,4,16,64,256,1024,4096,16384,65536}:std::vector<int>{-1}) {
            managed_test::Probe probe;probe.inspect_roots=true;probe.collect_before=!faults;probe.frames.reserve(16384);
            std::size_t gc_allocation_failures=0;
            if(faults)probe.before_construct=[&](std::size_t,const void*,std::size_t) {
                auto collection=runtime->collect();
                // The injected allocator also covers GC's own temporary root
                // storage. Its documented allocation_failure is recoverable;
                // the generic probe's unconditional terminate is unsuitable
                // for this combined sweep. Keep real GC and root inspection.
                if(collection==vd::Runtime::Collection::allocation_failure)++gc_allocation_failures;
                else if(collection!=vd::Runtime::Collection::completed)std::terminate();
            };
            std::string error;int status=-1;
            {
                managed_test::Observe observe(probe);
                try {
                    if(budget>=0){managed_test::AllocationFailure fault(budget,true);status=code->invoke(0);}
                    else status=code->invoke(0);
                }catch(const std::exception& cause){error=cause.what();std::cerr<<"budget "<<budget<<": "<<error<<'\n';}
            }
            if(!probe.roots_valid || !probe.commits_valid || !probe.frames.empty())
                throw std::runtime_error("VOIDUP precise root/frame/commit imbalance");
            if(status==0)++successes;else ++failures;
            std::cout<<"{\"budget\":"<<budget<<",\"status\":"<<status<<",\"exception\":"<<(!error.empty()?"true":"false")
                     <<",\"constructions\":"<<probe.constructions<<",\"checked_roots\":"<<probe.roots_checked
                     <<",\"gc_allocation_failures\":"<<gc_allocation_failures<<"}\n";
            if(!faults && (status!=0 || !error.empty()))throw std::runtime_error("source GC execution failed: "+error);
            if(faults && code->invoke(0)!=0)throw std::runtime_error("unfaulted source recovery failed after allocation fault");
        }
        const auto snapshot=runtime->snapshot();
        if(!snapshot.completed_epoch)throw std::runtime_error("source did not complete a real collection");
        {
            vd::Runtime::Attachment attached(*runtime);vd::Runtime::Managed managed(*runtime);
            runtime->collect();
            {vd::Runtime::Native native(*runtime);if(runtime->close())throw std::runtime_error("Runtime close failed");}
            if(runtime->memory_snapshot().handles)throw std::runtime_error("native Handles retained after shutdown");
        }
        if(faults && !failures)throw std::runtime_error("allocation faults did not reach source execution");
        std::cout<<"{\"source_voidup\":true,\"one_runtime\":true,\"workers\":1,\"completed_epoch\":"<<snapshot.completed_epoch
                 <<",\"failures\":"<<failures<<",\"successes\":"<<successes<<",\"handles_after_close\":0}\n";
        return 0;
    }catch(const std::exception& cause){std::cerr<<cause.what()<<'\n';return 1;}
}
