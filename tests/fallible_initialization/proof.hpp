#pragma once
#include "../managed/probe.hpp"
namespace fallible_test {
template<class Invoke> void exercise(vd::Runtime& runtime,Invoke invoke) {
    using namespace managed_test;
    for(bool forced:{false,true})for(int input:{7,-7,7,-7}) {
        Probe p;p.inspect_roots=true;p.collect_before=forced;
        {Observe observing(p);require(invoke(input)==(input<0?7:17),"fallible construction result/error payload");}
        require(p.constructions==(input<0?2u:4u),"failed parent published or initializer rerun");
        require(p.commits_valid && p.roots_valid && p.frames.empty() && p.roots_checked>0,"fallible construction precise roots/frame cleanup");
        vd::Runtime::Attachment attached(runtime);vd::Runtime::Managed managed(runtime);
        runtime.collect();require(runtime.snapshot().objects==0,"failed stage or completed result roots leaked");
    }
    for(int input:{42,43,42,43}) {
        Probe p;p.inspect_roots=true;p.collect_before=true;bool observed=false;
        p.before_construct=[&](auto count,const void*,auto) {
            if(count==2) {
                require(runtime.collect()==vd::Runtime::Collection::completed,"value failure collection refused");
                observed=runtime.snapshot().objects==(input==42?0u:1u);
            }
        };
        {Observe observing(p);require(invoke(input)==(input==42?9:8),"fallible value payload/result");}
        require(observed && p.constructions==2 && p.roots_valid && p.commits_valid && p.frames.empty(),"abandoned value storage retained fields or exposed invalid roots");
        vd::Runtime::Attachment attached(runtime);vd::Runtime::Managed managed(runtime);runtime.collect();
        require(runtime.snapshot().objects==0,"fallible value roots leaked after return");
    }
}
}
