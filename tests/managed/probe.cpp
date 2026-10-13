#include "probe.hpp"
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
namespace { thread_local int fail_after=-1; }
void* operator new(std::size_t size) {
    if (fail_after==0) throw std::bad_alloc();
    if (fail_after>0) --fail_after;
    if (auto* p=std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
// LLVM also uses nothrow temporary buffers. Keep the entire ordinary allocation
// family paired with this fixture's malloc/free replacement under ASan.
void* operator new(std::size_t size,const std::nothrow_t&) noexcept {
    try { return ::operator new(size); } catch (...) { return nullptr; }
}
void operator delete(void* p,const std::nothrow_t&) noexcept { ::operator delete(p); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete[](void* p,std::size_t) noexcept { ::operator delete(p); }
void* operator new[](std::size_t size,const std::nothrow_t&) noexcept {
    return ::operator new(size,std::nothrow);
}
void operator delete[](void* p,const std::nothrow_t&) noexcept { ::operator delete(p); }
namespace managed_test {
thread_local Probe* active=nullptr;
AllocationFailure::AllocationFailure(int budget) { fail_after=budget; }
AllocationFailure::~AllocationFailure() { fail_after=-1; }
void require(bool yes,const char* message) { if (!yes) throw std::runtime_error(message); }
}
extern "C" void vd_gc_real_poll() noexcept;
extern "C" void vd_gc_real_construct(void**,std::size_t,const std::size_t*,std::size_t,const void*,vd::RuntimeFailure*) noexcept;
extern "C" void vd_gc_poll() noexcept {
    if (auto* p=managed_test::active) {
        ++p->polls;
        if (p->gate_poll && p->polls==p->gate_poll) { p->reached.release(); p->release.acquire(); }
    }
    vd_gc_real_poll();
}
extern "C" void vd_gc_construct(void** slot,std::size_t size,const std::size_t* offsets,std::size_t count,const void* payload,vd::RuntimeFailure* failure) noexcept {
    auto* p=managed_test::active;
    auto* previous=*slot;
    if (p) {
        ++p->constructions;
        if (p->collect_before && vd::Runtime::current().collect()!=vd::Runtime::Collection::completed) std::terminate();
        if (p->constructions==p->fail_construction) fail_after=p->fail_allocation;
    }
    vd_gc_real_construct(slot,size,offsets,count,payload,failure);
    fail_after=-1;
    if (p) {
        if (failure->message) p->commits_valid &= *slot==previous;
        else {
            // Observe the actual published source object, including all pointer
            // leaves, after the production helper has released its commit lock.
            p->commits_valid &= *slot && std::memcmp(*slot,payload,size)==0;
            if (p->capture && !p->retained) p->retained=std::make_unique<vd::Runtime::Handle>(vd::Runtime::current(),*slot);
        }
    }
}
