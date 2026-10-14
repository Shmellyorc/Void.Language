#include "../managed/catalog.hpp"
#include "proof.hpp"
#include <iostream>
#include <future>
extern "C" std::int32_t __vd_2f7374726573732e766f69643a3a656e747279(std::int32_t,vd::RuntimeFailure*);
extern "C" const vd::RuntimeTypeRecord __vd_types__reloaded[];
extern "C" const std::size_t __vd_type_count__reloaded;
extern "C" const vd::RuntimeWitnessRecord __vd_witnesses__reloaded[];
extern "C" const std::size_t __vd_witness_count__reloaded;
extern "C" std::int32_t __vd_2f7374726573732e766f69643a3a696e7370656374__reloaded(void*,vd::RuntimeFailure*);
extern "C" std::int32_t __vd_2f7374726573732e766f69643a3a68616e646f6666(std::int32_t,vd::RuntimeFailure*);
extern "C" std::int32_t __vd_2f7374726573732e766f69643a3a6f72646572696e67(std::int32_t,vd::RuntimeFailure*);
int main() {
    using namespace managed_test;
    try {
        vd::Runtime runtime(1);admit(runtime);
        for(bool forced:{false,true}) {
            vd::Runtime::Attachment attached(runtime);vd::Runtime::Managed managed(runtime);vd::RuntimeFailure failure;
            Probe p;p.inspect_roots=true;p.collect_before=forced;
            {Observe observe(p);require(__vd_2f7374726573732e766f69643a3a656e747279(10,&failure)==1510 && !failure.message,"native interface result");}
            require(p.constructions==1502 && p.roots_valid && p.commits_valid && p.frames.empty(),"native interface root/publication cleanup");
            runtime.collect();require(runtime.snapshot().objects==0,"native interface graph leak");
        }
        {vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);vd::RuntimeFailure failure;Probe p;p.inspect_roots=true;p.collect_before=true;bool witnessed=false;void* target=nullptr;
            p.before_construct=[&](auto count,const void*,auto){interface_test::handoff(p,witnessed,target,count);};
            {Observe observe(p);require(__vd_2f7374726573732e766f69643a3a68616e646f6666(10,&failure)==110 && !failure.message,"native nested interface handoff result");}
            require(witnessed && p.constructions==101 && p.roots_valid && p.frames.empty(),"native nested interface sole roots/cleanup");runtime.collect();require(runtime.snapshot().objects==0,"native nested interface leak");
        }
        {vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);vd::RuntimeFailure failure;Probe p;p.inspect_roots=true;p.collect_before=true;bool witnessed=false;void* target=nullptr;
            p.before_construct=[&](auto count,const void* payload,auto){interface_test::selected(p,witnessed,target,count,payload);};
            {Observe observe(p);require(__vd_2f7374726573732e766f69643a3a6f72646572696e67(10,&failure)==1200 && !failure.message,"native selected interface receiver result");}
            require(witnessed && p.constructions==4 && p.roots_valid && p.frames.empty(),"native selected receiver root snapshot/cleanup");runtime.collect();require(runtime.snapshot().objects==0,"native selected receiver leak");
        }
        Probe retained;retained.capture=true;retained.capture_at=2;
        {vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);vd::RuntimeFailure failure;Observe observe(retained);require(__vd_2f7374726573732e766f69643a3a656e747279(10,&failure)==1510 && !failure.message,"old native interface holder setup");}
        require(bool(retained.retained),"old native interface holder absent");
        vd::RuntimeFailure admitted;vd_runtime_admit_metadata(&runtime,__vd_types__reloaded,__vd_type_count__reloaded,__vd_witnesses__reloaded,__vd_witness_count__reloaded,&admitted);require(!admitted.message,"new generation interface metadata admission");
        void* old=nullptr;{vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);old=retained.retained->get();}
        Probe handoff;handoff.inspect_roots=true;handoff.gate_poll=1;
        auto worker=std::async(std::launch::async,[&]{vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);vd::RuntimeFailure failure;Observe observe(handoff);auto result=__vd_2f7374726573732e766f69643a3a696e7370656374__reloaded(old,&failure);require(!failure.message,"old native witness/new generation dispatch failed");return result;});
        struct Release {Probe& p;bool done=false;void now(){if(!done){done=true;p.release.release();}}~Release(){now();}} release{handoff};
        require(handoff.reached.try_acquire_for(std::chrono::seconds(5)),"native interface receiver root overlap absent");retained.retained.reset();
        auto collector=std::async(std::launch::async,[&]{vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);return runtime.collect(std::chrono::seconds(5));});
        bool requested=runtime.wait_for_request(std::chrono::seconds(5));release.now();
        require(requested && collector.get()==vd::Runtime::Collection::completed && worker.get()==11 && handoff.roots_valid && handoff.frames.empty(),"native cross-generation interface/GC handoff failed");
        {vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);runtime.collect();require(runtime.snapshot().objects==0,"native cross-generation interface leak");}
        std::cout<<"native interface tracing and dispatch passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
