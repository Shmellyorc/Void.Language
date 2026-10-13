#include "../reference_values/proof.hpp"
#include <iostream>
extern "C" std::int32_t __vd_2f7374726573732e766f69643a3a656e747279(std::int32_t,vd::RuntimeFailure*);
int main() {
    using namespace managed_test;
    try {
        vd::Runtime runtime(1);
        for(bool forced:{false,true}) {
            Probe p;p.inspect_roots=true;p.collect_before=forced;bool witnessed=false;
            p.before_construct=[&](auto n,const void*,auto) {reference_value_test::inspect_carriers(p,witnessed,n);};
            vd::Runtime::Attachment attached(runtime);vd::Runtime::Managed managed(runtime);vd::RuntimeFailure failure;
            {Observe observe(p);require(__vd_2f7374726573732e766f69643a3a656e747279(10,&failure)==2024 && !failure.message,"native reference value result");}
            require(witnessed && p.constructions==1002 && p.roots_valid && p.commits_valid && p.frames.empty() && p.roots_checked>1000,"native aggregate roots/cleanup");
            require(runtime.snapshot().completed_epoch>=1000,"native automatic collection absent");
            runtime.collect();require(runtime.snapshot().objects==0,"native aggregate/cycle leak");
        }
        vd::Runtime::Attachment attached(runtime);vd::Runtime::Managed managed(runtime);
        Probe p;p.inspect_roots=true;p.fail_construction=3;p.fail_allocation=0;vd::RuntimeFailure failure;
        {Observe observe(p);require(__vd_2f7374726573732e766f69643a3a656e747279(10,&failure)==0,"native failure produced output");}
        require(failure.message && p.commits_valid && p.roots_valid && p.frames.empty(),"native aggregate failure cleanup");
        runtime.collect();require(runtime.snapshot().objects==0,"native aggregate failure retained graph");
        failure={};require(__vd_2f7374726573732e766f69643a3a656e747279(10,&failure)==2024 && !failure.message,"native aggregate failure recovery");
        std::cout<<"native composition: sole aggregate carriers, cycles, GC, failure and recovery passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
