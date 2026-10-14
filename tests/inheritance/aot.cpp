#include "proof.hpp"
#include "../managed/catalog.hpp"
#include <iostream>
#include <future>
extern "C" std::int32_t __vd_2f7374726573732e766f69643a3a656e747279(std::int32_t,vd::RuntimeFailure*);
extern "C" const vd::RuntimeTypeRecord __vd_types__reloaded[];
extern "C" const std::size_t __vd_type_count__reloaded;
extern "C" std::int32_t __vd_2f7374726573732e766f69643a3a696e7370656374__reloaded(void*,vd::RuntimeFailure*);
int main() {
    using namespace managed_test;
    try {
        vd::Runtime runtime(1);admit(runtime);
        for(bool forced:{false,true}) {
            vd::Runtime::Attachment attached(runtime);vd::Runtime::Managed managed(runtime);vd::RuntimeFailure failure;
            Probe p;p.inspect_roots=true;p.collect_before=forced;bool witnessed=false;
            p.before_construct=[&](auto n,const void* payload,auto size){inheritance_test::inspect(p,witnessed,n,payload,size);};
            {Observe observe(p);require(__vd_2f7374726573732e766f69643a3a656e747279(10,&failure)==1110 && !failure.message,"native inherited virtual result");}
            require(witnessed && p.constructions==2002 && p.roots_valid && p.commits_valid && p.frames.empty(),"native sole-base roots/prefix/publication cleanup");
            require(runtime.snapshot().completed_epoch>=2000,"native budget-one collections absent");
            runtime.collect();require(runtime.snapshot().objects==0,"native inherited cycle leak");
        }
        Probe retained;retained.capture=true;retained.capture_at=2;
        {
            vd::Runtime::Attachment attached(runtime);vd::Runtime::Managed managed(runtime);vd::RuntimeFailure failure;
            {Observe observe(retained);require(__vd_2f7374726573732e766f69643a3a656e747279(10,&failure)==1110 && !failure.message,"old generation object setup");}
            runtime.collect();require(retained.retained && runtime.snapshot().objects==2,"old object/derived child not retained independently");
        }
        vd::RuntimeFailure admitted;vd_runtime_admit(&runtime,__vd_types__reloaded,__vd_type_count__reloaded,&admitted);require(!admitted.message,"new generation descriptor admission");
        void* old_object=nullptr;
        {vd::Runtime::Attachment attached(runtime);vd::Runtime::Managed managed(runtime);old_object=retained.retained->get();}
        Probe handoff;handoff.inspect_roots=true;handoff.gate_poll=1;
        auto worker=std::async(std::launch::async,[&] {
            vd::Runtime::Attachment attached(runtime);vd::Runtime::Managed managed(runtime);vd::RuntimeFailure failure;
            Observe observe(handoff);auto result=__vd_2f7374726573732e766f69643a3a696e7370656374__reloaded(old_object,&failure);
            require(!failure.message,"cross-generation dispatch failure");return result;
        });
        struct Release {Probe& p;bool done=false;void now(){if(!done){done=true;p.release.release();}}~Release(){now();}} release{handoff};
        require(handoff.reached.try_acquire_for(std::chrono::seconds(5)),"new-generation argument root overlap absent");
        retained.retained.reset(); // Only the new source function's Base roots remain.
        auto collector=std::async(std::launch::async,[&]{vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);return runtime.collect(std::chrono::seconds(5));});
        bool requested=runtime.wait_for_request(std::chrono::seconds(5));release.now();
        require(requested && collector.get()==vd::Runtime::Collection::completed && worker.get()==111 && handoff.roots_valid && handoff.frames.empty(),"new-generation override/root handoff on old object failed");
        {vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);runtime.collect();require(runtime.snapshot().objects==0,"cross-generation object retained code/data");}
        vd::Runtime::Attachment attached(runtime);vd::Runtime::Managed managed(runtime);vd::RuntimeFailure failure;
        Probe p;p.inspect_roots=true;p.fail_construction=3;p.fail_allocation=0;
        {Observe observe(p);require(__vd_2f7374726573732e766f69643a3a656e747279(10,&failure)==0,"native failed construction published result");}
        require(failure.message && p.roots_valid && p.commits_valid && p.frames.empty(),"native failure cleanup");
        runtime.collect();require(runtime.snapshot().objects==0,"native failure retained graph");
        failure={};require(__vd_2f7374726573732e766f69643a3a656e747279(10,&failure)==1110 && !failure.message,"native failure recovery");
        std::cout<<"native inheritance: full prefix, sole Base carriers, virtual dispatch, precise cycles and failure recovery passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
