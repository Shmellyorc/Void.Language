#pragma once
#include "../allocation_elimination/check.hpp"
namespace escape_analysis {
template<class Invoke> void remaining_failure(const char* name,Invoke invoke,bool baseline) {
    const std::string test=name;
    if(test!="null_failure" && test!="numeric_failure")return;
    managed_test::Probe probe;probe.inspect_roots=true;probe.collect_before=true;probe.frames.reserve(1024);
    bool failed=false;
    { managed_test::Observe observe(probe);
      try {invoke(-1);} catch(const std::runtime_error& e) {
          failed=std::string(e.what()).find(test=="null_failure"?"Null managed reference":"zero")!=std::string::npos;
      }
    }
    managed_test::require(failed && probe.constructions==(baseline?1u:0u),"remaining null/arithmetic failure");
    managed_test::require(probe.frames.empty() && probe.roots_valid && probe.commits_valid,"remaining failure roots/publication");
}
}
