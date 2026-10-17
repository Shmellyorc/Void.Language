#include "void/runtime_abi.hpp"
// Independently compiled native C++ embedding witness, NOT certified generated
// array code. Caller homes remain registered throughout the private handoff.
extern "C" void fixed_six_witness(void** a,void** b,void** c,void** d,void** e,void** f,
    void** result,const vd::BufferLayoutRef* layout,const vd::BufferWorkQuota* quota,vd::RuntimeFailure* failure) noexcept {
    if(!failure || failure->message) return;
    // Reads validate each caller root before loading its handle. No poll occurs
    // between a validated read and registration of the callee's bounded homes.
    void** args[]={a,b,c,d,e,f};
    for(auto arg:args) {vd_fixed_read(arg,layout,0,failure);if(failure->message) return;}
    void* homes[]={*a,*b,*c,*d,*e,*f};
    constexpr std::size_t zero=0;
    vd::Runtime::Slot slots[6];
    for(unsigned n=0;n<6;++n) slots[n]={&homes[n],sizeof(void*),&zero,1};
    try {
        auto& runtime=vd::Runtime::current();vd::Runtime::Frame frame(runtime,slots,6);
        vd_gc_collect(failure);if(failure->message) return;
        vd_fixed_write(&homes[0],layout,0,99,failure);if(failure->message) return;
        vd_fixed_copy(result,&homes[0],layout,quota,failure);
    } catch(...) {std::terminate();} // Broken Frame invariants remain fatal.
}
