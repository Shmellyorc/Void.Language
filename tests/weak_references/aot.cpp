#include "void/runtime.hpp"
#include "void/runtime_abi.hpp"
#include <iostream>

extern "C" const vd::RuntimeTypeRecord __vd_types[];
extern "C" const std::size_t __vd_type_count;
extern "C" std::int32_t __vd_2f6d61696e2e766f69643a3a656e747279(std::int32_t,vd::RuntimeFailure*);

int main() {
    vd::Runtime runtime(1);
    vd::RuntimeFailure failure;
    vd_runtime_admit(&runtime,__vd_types,__vd_type_count,&failure);
    if(failure.message) {
        std::cerr<<failure.message<<'\n';
        return 1;
    }
    vd::Runtime::Attachment attached(runtime);
    vd::Runtime::Managed managed(runtime);
    const int expected[]={42,1,6};
    for(int mode=0;mode<3;++mode) {
        const auto result=__vd_2f6d61696e2e766f69643a3a656e747279(mode,&failure);
        if(failure.message || result!=expected[mode]) {
            std::cerr<<(failure.message?failure.message:"Unexpected AOT result")<<" (mode "<<mode<<")\n";
            return 1;
        }
    }
}
