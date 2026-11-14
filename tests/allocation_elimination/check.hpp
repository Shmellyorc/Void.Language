#pragma once
#include "../managed/probe.hpp"
#include <iostream>
#include <string>
namespace allocation_elimination {
inline thread_local unsigned polls=0;
inline void force_collection() {
    if (++polls%31==0 && vd::Runtime::current().collect()!=vd::Runtime::Collection::completed)
        std::terminate();
}
template<class Invoke> void check(const char* name, Invoke invoke, vd::Runtime& runtime,
                                  int expected, std::size_t constructions) {
    using namespace managed_test;
    require(invoke(1000)==expected,"allocation elimination warm oracle");
    std::size_t live_after_warm;
    {
        vd::Runtime::Attachment attachment(runtime); vd::Runtime::Managed managed(runtime);
        require(runtime.collect()==vd::Runtime::Collection::completed,"elision warm reclamation");
        live_after_warm=runtime.snapshot().objects; // Eager global cells remain rooted.
    }
    Probe p; p.inspect_roots=true; p.collect_before=true; p.frames.reserve(1024);
    polls=0; concurrent_poll=force_collection;
    struct Reset { ~Reset(){ concurrent_poll=nullptr; } } reset;
    { Observe observe(p); require(invoke(1000)==expected,"allocation elimination forced-GC oracle"); }
    concurrent_poll=nullptr;
    require(p.frames.empty() && p.roots_valid && p.commits_valid,"allocation elimination roots/publication");
    if(p.constructions!=constructions)throw std::runtime_error(std::string(name)+": constructions="+std::to_string(p.constructions)+" expected="+std::to_string(constructions));
    auto collect=[&] {
        vd::Runtime::Attachment attachment(runtime); vd::Runtime::Managed managed(runtime);
        require(runtime.collect()==vd::Runtime::Collection::completed && runtime.snapshot().objects==live_after_warm,
                "allocation elimination final reclamation");
    };
    collect();
    Probe fault; fault.inspect_roots=true; fault.frames.reserve(1024);
    fault.fail_construction=1; fault.fail_allocation=0; bool failed=false;
    { Observe observe(fault); try { require(invoke(1000)==expected,"allocation elimination fault oracle"); }
      catch (const std::runtime_error& e) {
        require(std::string(e.what()).find("allocation failed")!=std::string::npos,
                "allocation elimination wrong failure"); failed=true;
      }
    }
    require(fault.frames.empty() && fault.roots_valid && fault.commits_valid,
            "allocation elimination failure root/publication");
    require(failed==(constructions!=0),"remaining versus eliminated allocation OOM");
    collect(); require(invoke(1000)==expected,"allocation elimination recovery"); collect();
    if(std::string(name)=="survives_allocation") {
        // Pre-attach so the injected native failure reaches source allocation.
        vd::Runtime::Attachment attachment(runtime); vd::Runtime::Managed managed(runtime);
        bool array_failed=false;
        { AllocationFailure inject(0,true);
          try { invoke(1000); } catch(const std::runtime_error& e) {
            array_failed=std::string(e.what()).find("Array operation failed")!=std::string::npos;
          }
        }
        require(array_failed,"elision lost remaining array allocation failure");
        require(invoke(1000)==expected,"remaining array failure recovery");
    }
    std::cout<<name<<" constructions="<<p.constructions<<" roots_checked="<<p.roots_checked
             <<" forced_polls="<<polls<<" remaining_oom="<<failed<<'\n';
}
}
