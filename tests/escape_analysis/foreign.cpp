#include "void/runtime_abi.hpp"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <exception>
#include <stdexcept>
#ifndef VD_FOREIGN_SUFFIX
#define VD_FOREIGN_SUFFIX incoming_reference
#endif
#define VD_JOIN_INNER(a,b) a##b
#define VD_JOIN(a,b) VD_JOIN_INNER(a,b)
#define VD_NAME(a) VD_JOIN(a,VD_FOREIGN_SUFFIX)
extern "C" std::int32_t VD_NAME(__vd_2f656c6973696f6e2e766f69643a3a77726170_)(void*,vd::RuntimeFailure*);
extern "C" const vd::RuntimeTypeRecord VD_NAME(__vd_types_)[];
extern "C" const std::size_t VD_NAME(__vd_type_count_);
extern "C" const vd::RuntimeWitnessRecord VD_NAME(__vd_witnesses_)[];
extern "C" const std::size_t VD_NAME(__vd_witness_count_);
int main(){
    std::set_terminate([]{
        try {if(auto error=std::current_exception())std::rethrow_exception(error);}
        catch(const std::logic_error& e){
            if(std::strcmp(e.what(),"prepared reference belongs to another heap")==0){
                std::fputs("preserved current-heap child validation\n",stderr);std::fflush(stderr);std::_Exit(86);
            }
        }catch(...){}
        std::_Exit(87);
    });
    vd::Runtime foreign,current;
    std::int32_t payload=42;
    vd::Runtime::Handle child(foreign);void* foreign_pointer=nullptr;
    {
        vd::Runtime::Attachment attached(foreign);vd::Runtime::Managed managed(foreign);
        void* home=nullptr;const std::size_t offset=0;vd::Runtime::Slot slot{&home,sizeof(home),&offset,1};
        vd::Runtime::Frame roots(foreign,&slot,1);foreign.construct_into(&home,{sizeof(payload),{}},&payload);child.reset(home);foreign_pointer=home;
#ifdef VD_FOREIGN_NESTED
        // A valid foreign owner whose immutable child is also foreign. Reading
        // that child must not establish affinity with the current Runtime.
        void* owner=nullptr;vd::Runtime::Slot owner_slot{&owner,sizeof(owner),&offset,1};
        vd::Runtime::Frame owner_roots(foreign,&owner_slot,1);
        foreign.construct_into(&owner,{sizeof(home),{offset}},&home);
        child.reset(owner);foreign_pointer=owner;
#endif
    }
    vd::RuntimeFailure admission;
    vd_runtime_admit_metadata(&current,VD_NAME(__vd_types_),VD_NAME(__vd_type_count_),VD_NAME(__vd_witnesses_),VD_NAME(__vd_witness_count_),&admission);
    if(admission.message)return 2;
    vd::Runtime::Attachment attached(current);vd::Runtime::Managed managed(current);vd::RuntimeFailure failure;
    const auto result=VD_NAME(__vd_2f656c6973696f6e2e766f69643a3a77726170_)(foreign_pointer,&failure);
    std::fprintf(stderr,"ownership validation disappeared: result=%d\n",result);
    return 1;
}
