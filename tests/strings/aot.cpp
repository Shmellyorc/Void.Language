#include "proof.hpp"
#include "../managed/catalog.hpp"
#include <iostream>
extern "C" std::int32_t __vd_2f7374726573732e766f69643a3a656e747279(std::int32_t,vd::RuntimeFailure*);
extern "C" std::int32_t __vd_2f7374726573732e766f69643a3a696e7370656374(void*,vd::RuntimeFailure*);
extern "C" std::int32_t __vd_2f7374726573732e766f69643a3a696e7370656374__reloaded(void*,vd::RuntimeFailure*);
int main() {
    using managed_test::require;
    try {
        string_test::runtime_checks();vd::Runtime runtime(1);managed_test::admit(runtime);
        vd::Runtime::Attachment attach(runtime);vd::Runtime::Managed managed(runtime);
        managed_test::Probe roots;roots.inspect_roots=true;roots.capture=true;roots.capture_at=1;
        string_test::Calls calls;vd::RuntimeFailure failure;
        {managed_test::Observe observe(roots);string_test::Observe strings(calls);require(__vd_2f7374726573732e766f69643a3a656e747279(10,&failure)==310 && !failure.message,"AOT string result");}
        require(roots.roots_valid && roots.frames.empty() && calls.nested_roots>0 && roots.retained,"AOT string roots/holder absent");
        runtime.collect();auto* old=roots.retained->get();
        require(__vd_2f7374726573732e766f69643a3a696e7370656374(old,&failure)==7 && !failure.message,"old AOT string holder");
        require(__vd_2f7374726573732e766f69643a3a696e7370656374__reloaded(old,&failure)==8 && !failure.message,"new AOT code/old string holder");
        roots.retained.reset();runtime.collect();require(runtime.snapshot().objects==0,"AOT string holder leak");
        for(unsigned call:{1u,3u,8u}) {
            managed_test::Probe cleanup;cleanup.inspect_roots=true;string_test::Calls faults;faults.fail_at=call;
            vd::RuntimeFailure checked;
            {managed_test::Observe observe(cleanup);string_test::Observe strings(faults);__vd_2f7374726573732e766f69643a3a656e747279(10,&checked);}
            require(checked.message && faults.protected_output && cleanup.roots_valid && cleanup.frames.empty(),"AOT string checked failure cleanup");
            runtime.collect();require(runtime.snapshot().objects==0,"AOT string failure leak");
        }
        std::cout<<"Runtime-only AOT strings, old-holder/new-code, roots and failures passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
