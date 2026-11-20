#include "../../managed/catalog.hpp"
#include "proof.hpp"
#include <iostream>
extern "C" std::int32_t __vd_2f726f6f74732e766f69643a3a656e747279(std::int32_t,vd::RuntimeFailure*);
int main() {
    try {
        vd::Runtime runtime(1);managed_test::admit(runtime);
        advanced_async_test::exercise(runtime,[&](int mode) {
            vd::Runtime::Attachment attached(runtime);vd::Runtime::Managed managed(runtime);
            vd::RuntimeFailure failure;
            auto value=__vd_2f726f6f74732e766f69643a3a656e747279(mode,&failure);
            if(failure.message)throw std::runtime_error(failure.message);return value;
        });
        std::cout<<"native async iteration: precise roots and one-shot checked-failure cleanup passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
