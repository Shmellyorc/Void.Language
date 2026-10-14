#include "../managed/catalog.hpp"
#include "probe.hpp"
#include <cstring>
#include <iostream>
// Compiler-private scalar export for parsed /main.void::entry. This harness is
// not a public managed-reference ABI and has no LLVM/ORC dependency.
extern "C" std::int32_t __vd_2f6d61696e2e766f69643a3a656e747279(std::int32_t,vd::RuntimeFailure*);
int main() {
    using namespace managed_test;
    try {
        vd::Runtime runtime(1); managed_test::admit(runtime);
        for (bool capture:{false,true}) {
            Probe p; p.collect_before=capture; p.capture=capture;
            vd::Runtime::Attachment attachment(runtime); vd::Runtime::Managed managed(runtime);
            vd::RuntimeFailure failure;
            { Observe observe(p); require(__vd_2f6d61696e2e766f69643a3a656e747279(10,&failure)==23 && !failure.message,"source AOT result"); }
            require(p.constructions==1002 && p.commits_valid,"source AOT complete publication");
            require(runtime.snapshot().completed_epoch>=1000,"source AOT collections missing");
            require(runtime.collect()==vd::Runtime::Collection::completed,"source AOT final collection");
            require(runtime.snapshot().objects==(capture ? 2u : 0u),"source AOT cyclic reachability");
            p.retained.reset();
            require(runtime.collect()==vd::Runtime::Collection::completed && runtime.snapshot().objects==0,"source AOT cycle reclamation");
        }
        {
            Probe p; p.capture=true; p.fail_construction=2; p.fail_allocation=0;
            vd::Runtime::Attachment attachment(runtime); vd::Runtime::Managed managed(runtime);
            vd::RuntimeFailure failure;
            { Observe observe(p); require(__vd_2f6d61696e2e766f69643a3a656e747279(10,&failure)==0,"failed source AOT published a successful result"); }
            require(failure.message && std::strstr(failure.message,"allocation failed") && p.constructions==2 && p.commits_valid && p.retained,"source AOT allocation failure/publication protection");
            require(runtime.collect()==vd::Runtime::Collection::completed && runtime.snapshot().objects==1,"failed source AOT frame cleanup");
            std::int32_t value=0; std::memcpy(&value,p.retained->get(),sizeof(value));
            require(value==10,"failed source AOT changed an existing object");
            p.retained.reset();
            require(runtime.collect()==vd::Runtime::Collection::completed && runtime.snapshot().objects==0,"failed source AOT staged-object leak");
            failure={};
            require(__vd_2f6d61696e2e766f69643a3a656e747279(10,&failure)==23 && !failure.message,"source AOT failure recovery");
        }
        {
            vd::RuntimeFailure failure; std::int32_t result=-1;
            { AllocationFailure inject(0); result=vd_runtime_entry_typed(__vd_2f6d61696e2e766f69643a3a656e747279,10,&failure,__vd_types,__vd_type_count); }
            require(result==0 && failure.message && std::strstr(failure.message,"Runtime startup allocation failed"),"source AOT bootstrap allocation protection");
            failure={};
            require(vd_runtime_entry_typed(__vd_2f6d61696e2e766f69643a3a656e747279,10,&failure,__vd_types,__vd_type_count)==23 && !failure.message,"source AOT bootstrap attachment/shutdown recovery");
        }
        std::cout << "parsed source AOT: roots, cyclic graphs, complete publication, automatic/forced GC and startup/failure recovery passed\n";
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
