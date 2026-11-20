#pragma once
#include "../managed/probe.hpp"
#include <cstring>
namespace iteration_test {
template<class Invoke> void exercise(vd::Runtime& runtime,Invoke invoke) {
    using namespace managed_test;
    for(bool forced:{false,true})for(int mode=0;mode<12;++mode) {
        Probe p;p.inspect_roots=true;p.collect_before=forced;p.capture=true;
        bool failed=false;int result=0;
        {Observe observing(p);try {result=invoke(mode);}catch(const std::runtime_error& error) {
            failed=std::string(error.what()).find("index")!=std::string::npos;
        }}
        require(failed==((mode>=5 && mode<=7) || mode>=10),"custom iteration checked failure boundary");
        const int expected[]={106,104,101,111,120,0,0,0,5,101};
        if(mode<5 || mode==8 || mode==9)require(result==expected[mode],"custom iteration early result/cleanup ordering");
        vd::Runtime::Attachment attached(runtime);vd::Runtime::Managed managed(runtime);
        require(p.retained && p.retained->get(),"custom iterator ledger observation");
        int closed=0;std::memcpy(&closed,p.retained->get(),sizeof(closed));
        require(closed==((mode==8 || mode==11)?0:1),"iterator deinit omitted or invoked more than once");
        require(p.roots_valid && p.commits_valid && p.frames.empty() && p.roots_checked,
                "custom iteration precise roots/unwind");
        p.retained.reset();
        runtime.collect();require(runtime.snapshot().objects==0,"custom iterator state/value roots leaked");
    }
}
}
