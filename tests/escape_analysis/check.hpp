#pragma once
#include "../allocation_elimination/check.hpp"
namespace escape_analysis {
template<class Invoke> void retired_children(const char* name,Invoke invoke) {
    if(std::string(name)!="root_retirement")return;
    managed_test::Probe probe;probe.inspect_roots=true;probe.frames.reserve(64);
    bool reclaimed=true;unsigned checked=0;
    probe.before_construct=[&](std::size_t count,const void*,std::size_t) {
        auto& runtime=vd::Runtime::current();
        reclaimed &= runtime.collect()==vd::Runtime::Collection::completed;
        if(count>1){++checked;reclaimed &= runtime.snapshot().objects==0;}
    };
    {managed_test::Observe observe(probe);
     managed_test::require(invoke(3)==3,"retirement value oracle");}
    managed_test::require(checked==2 && reclaimed,"dead payload children retained across independent loop instances");
    managed_test::require(probe.frames.empty() && probe.roots_valid && probe.commits_valid,"retirement precise roots");
}
template<class Invoke> void remaining_failure(const char* name,Invoke invoke,bool baseline) {
    const std::string test=name;
    if(test!="null_failure" && test!="direct_null_failure" && test!="numeric_failure")return;
    managed_test::Probe probe;probe.inspect_roots=true;probe.collect_before=true;probe.frames.reserve(1024);
    bool failed=false;
    { managed_test::Observe observe(probe);
      try {invoke(-1);} catch(const std::runtime_error& e) {
          failed=std::string(e.what()).find(test!="numeric_failure"?"Null managed reference":"zero")!=std::string::npos;
      }
    }
    managed_test::require(failed && probe.constructions==(baseline?1u:0u),"remaining null/arithmetic failure");
    managed_test::require(probe.frames.empty() && probe.roots_valid && probe.commits_valid,"remaining failure roots/publication");
}
}
