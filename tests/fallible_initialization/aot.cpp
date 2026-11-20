#include "../managed/catalog.hpp"
#include "proof.hpp"
#include <iostream>
extern "C" std::int32_t __vd_2f726f6f74732e766f69643a3a656e747279(std::int32_t,vd::RuntimeFailure*);
int main() {
    try {
        vd::Runtime runtime(1);managed_test::admit(runtime);
        fallible_test::exercise(runtime,[&](int input) {
            vd::Runtime::Attachment attached(runtime);vd::Runtime::Managed managed(runtime);
            vd::RuntimeFailure failure;
            auto value=__vd_2f726f6f74732e766f69643a3a656e747279(input,&failure);
            managed_test::require(!failure.message,"native fallible construction runtime failure");return value;
        });
        {
            managed_test::Probe p;p.inspect_roots=true;p.collect_before=true;p.fail_construction=3;p.fail_allocation=0;
            vd::Runtime::Attachment attached(runtime);vd::Runtime::Managed managed(runtime);vd::RuntimeFailure failure;
            {managed_test::Observe observing(p);managed_test::require(__vd_2f726f6f74732e766f69643a3a656e747279(7,&failure)==0 && failure.message,"native failed commit produced output");}
            managed_test::require(p.roots_valid && p.commits_valid && p.frames.empty(),"native fallible commit leaked roots/storage");
            runtime.collect();managed_test::require(runtime.snapshot().objects==0,"native failed commit retained fields");
            failure={};managed_test::require(__vd_2f726f6f74732e766f69643a3a656e747279(-7,&failure)==7 && !failure.message,"native constructor recovery after failed commit");
        }
        std::cout<<"native fallible initialization: success/error publication and precise roots passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
