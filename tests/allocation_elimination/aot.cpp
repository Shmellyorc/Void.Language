#include "cases.hpp"
#include "check.hpp"
// Compiler-private scalar exports from /elision.void::entry. Every object was
// independently emitted; this executable links Runtime without LLVM or ORC.
#define DECLARE(name,expected,count) \
extern "C" std::int32_t __vd_2f656c6973696f6e2e766f69643a3a656e747279_##name(std::int32_t,vd::RuntimeFailure*); \
extern "C" const vd::RuntimeTypeRecord __vd_types_##name[]; \
extern "C" const std::size_t __vd_type_count_##name; \
extern "C" const vd::RuntimeWitnessRecord __vd_witnesses_##name[]; \
extern "C" const std::size_t __vd_witness_count_##name;
VD_ELISION_CASES(DECLARE)
#undef DECLARE
int main() {
    try {
#define CHECK(name,expected,count) { \
    vd::Runtime runtime(1); vd::RuntimeFailure admission; \
    vd_runtime_admit_metadata(&runtime,__vd_types_##name,__vd_type_count_##name, \
        __vd_witnesses_##name,__vd_witness_count_##name,&admission); \
    managed_test::require(!admission.message,"elision AOT metadata admission"); \
    auto invoke=[&](int n) { \
        vd::Runtime::Attachment attachment(runtime); vd::Runtime::Managed managed(runtime); \
        vd::RuntimeFailure failure; auto value=__vd_2f656c6973696f6e2e766f69643a3a656e747279_##name(n,&failure); \
        if(failure.message)throw std::runtime_error(failure.message); return value; \
    }; \
    allocation_elimination::check(#name,invoke,runtime,expected,count); \
}
        VD_ELISION_CASES(CHECK)
#undef CHECK
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
