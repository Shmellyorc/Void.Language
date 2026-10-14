#include "proof.hpp"
namespace string_test {thread_local Calls* active=nullptr;}
extern "C" void vd_real_string_literal(void**,const char*,std::size_t,vd::RuntimeFailure*) noexcept;
extern "C" void vd_real_string_concat(void**,void*,void*,vd::RuntimeFailure*) noexcept;
namespace {
template<class Call> void invoke(void** slot,vd::RuntimeFailure* failure,Call call) noexcept {
    string_test::inspect();auto* p=string_test::active;auto* previous=*slot;
    if(p) ++p->count;
    if(p && p->count==p->fail_at) {managed_test::AllocationFailure fail(p->fail_after);call();}else call();
    if(p && failure->message) p->protected_output&=*slot==previous;
    if(p && p->capture && !p->captured && *slot && !failure->message) p->captured=std::make_unique<vd::Runtime::Handle>(vd::Runtime::current(),*slot);
}
}
extern "C" void vd_string_literal(void** slot,const char* text,std::size_t size,vd::RuntimeFailure* failure) noexcept {
    invoke(slot,failure,[&]{vd_real_string_literal(slot,text,size,failure);});
}
extern "C" void vd_string_concat(void** slot,void* left,void* right,vd::RuntimeFailure* failure) noexcept {
    invoke(slot,failure,[&]{vd_real_string_concat(slot,left,right,failure);});
}
