#pragma once
#include "../../managed/probe.hpp"
#include <cstring>
#include <string>
namespace advanced_async_test {
template<class Invoke> void exercise(vd::Runtime& runtime,Invoke invoke) {
    using namespace managed_test;
    for(bool forced:{false,true})for(int mode=0;mode<15;++mode) {
        Probe p;p.inspect_roots=true;p.collect_before=forced;p.capture=true;
        std::string diagnostic;int result=0;
        {Observe observing(p);try {result=invoke(mode);}catch(const std::runtime_error& error) {diagnostic=error.what();}}
        const bool cancellation=mode==12 || mode==13;
        const bool failure=(mode>=5 && mode<=7) || mode==10 || mode==11 || mode==14;
        const std::string context="advanced async mode "+std::to_string(mode)+": "+diagnostic;
        require((!diagnostic.empty())==(cancellation || failure),context.c_str());
        if(cancellation)require(diagnostic.find("Async task cancelled")!=std::string::npos,context.c_str());
        if(failure)require(diagnostic.find("index")!=std::string::npos,context.c_str());
        if(mode==14)require(diagnostic.find("index")<diagnostic.find("Async task cancelled"),
                "iterator cleanup failure did not supersede cancellation");
        const int expected[]={106,104,101,111,120,0,0,0,103,101};
        if(mode<5 || mode==8 || mode==9)require(result==expected[mode],(context+" result="+std::to_string(result)).c_str());
        vd::Runtime::Attachment attached(runtime);vd::Runtime::Managed managed(runtime);
        require(p.retained && p.retained->get(),"missing async iterator ledger");
        int closed=0;std::memcpy(&closed,p.retained->get(),sizeof(closed));
        require(closed==((mode==11 || mode==12)?0:1),(context+" closed="+std::to_string(closed)).c_str());
        require(p.roots_valid && p.commits_valid && p.frames.empty() && p.roots_checked,
                "async iteration precise roots/unwind");
        if(mode==13 || mode==14) {
            int reached=0;std::memcpy(&reached,static_cast<const char*>(p.retained->get())+12,sizeof(reached));
            require(reached==10,"cancelled advancement lost reached defer");
        }
        p.retained.reset();runtime.collect();require(runtime.snapshot().objects==0,"async iterator roots leaked");
    }
}
}
