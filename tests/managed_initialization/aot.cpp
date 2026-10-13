#include "../managed/probe.hpp"
#include <cstring>
#include <iostream>
extern "C" std::int32_t __vd_2f7374726573732e766f69643a3a656e747279(std::int32_t,vd::RuntimeFailure*);
int main() {
    using namespace managed_test;
    try {
        vd::Runtime runtime(1);
        for(bool forced:{false,true}) {
            Probe p; p.inspect_roots=true; p.capture=true; p.collect_before=forced; p.capture_at=forced ? 303 : 1;
            bool body_completed=forced;
            p.before_construct=[&](auto count,const void*,auto) {if(count==303 && !forced) {std::int32_t value=0;std::memcpy(&value,p.retained->get(),sizeof(value));body_completed=value==213;}};
            vd::Runtime::Attachment attached(runtime); vd::Runtime::Managed managed(runtime); vd::RuntimeFailure failure;
            {Observe observe(p); require(__vd_2f7374726573732e766f69643a3a656e747279(10,&failure)==246 && !failure.message,"native managed init result");}
            require(body_completed && p.constructions==303 && p.commits_valid && p.roots_valid && p.frames.empty() && p.roots_checked>1000,"native staged roots/once-only commit");
            require(runtime.snapshot().completed_epoch>=302,"native automatic GC absent");
            runtime.collect();require(runtime.snapshot().objects==(forced ? 2u : 1u),"native complete graph reachability");
            p.retained.reset();runtime.collect();require(runtime.snapshot().objects==0,"native staged root leak");
        }
        {
            Probe p; p.inspect_roots=true;p.capture=true;p.fail_construction=303;p.fail_allocation=0;
            vd::Runtime::Attachment attached(runtime);vd::Runtime::Managed managed(runtime);vd::RuntimeFailure failure;
            {Observe observe(p);require(__vd_2f7374726573732e766f69643a3a656e747279(10,&failure)==0,"native failed init produced successful output");}
            require(failure.message && std::strstr(failure.message,"allocation failed") && p.commits_valid && p.roots_valid && p.frames.empty(),"native commit failure cleanup");
            std::int32_t value=0;std::memcpy(&value,p.retained->get(),sizeof(value));require(value==213,"native body effects did not precede commit failure");
            runtime.collect();require(runtime.snapshot().objects==1,"native failed parent escaped");p.retained.reset();runtime.collect();
            failure={};require(__vd_2f7374726573732e766f69643a3a656e747279(10,&failure)==246 && !failure.message,"native constructor recovery");
        }
        vd::RuntimeFailure failure;std::int32_t result=-1;
        {AllocationFailure fault(0);result=vd_runtime_entry(__vd_2f7374726573732e766f69643a3a656e747279,10,&failure);}
        require(result==0 && failure.message,"native constructor bootstrap failure");failure={};
        require(vd_runtime_entry(__vd_2f7374726573732e766f69643a3a656e747279,10,&failure)==246 && !failure.message,"native constructor bootstrap recovery");
        std::cout<<"native managed init: staged roots, commit, GC, failures and bootstrap passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
