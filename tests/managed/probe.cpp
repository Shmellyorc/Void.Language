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
namespace {
void inspect(managed_test::Probe& p) {
    if (!p.inspect_roots) return;
    for (const auto& frame:p.frames) for (std::size_t i=0;i<frame.count;++i) {
        const auto& slot=frame.slots[i];
        for (std::size_t j=0;j<slot.count;++j) {
            auto offset=slot.offsets[j];
            if (offset>slot.size || slot.size-offset<sizeof(void*)) { p.roots_valid=false; continue; }
            void* value=nullptr; std::memcpy(&value,static_cast<const char*>(slot.storage)+offset,sizeof(value));
            ++p.roots_checked;
            p.roots_valid &= !value || vd::Runtime::current().contains(value);
        }
    }
}
}
extern "C" void vd_gc_real_frame_enter(void*,const vd::Runtime::Slot*,std::size_t) noexcept;
extern "C" void vd_gc_real_frame_leave(void*) noexcept;
extern "C" void vd_gc_frame_enter(void* address,const vd::Runtime::Slot* slots,std::size_t count) noexcept {
    vd_gc_real_frame_enter(address,slots,count);
    if (auto* p=managed_test::active;p && p->inspect_roots) { p->frames.push_back({address,slots,count}); inspect(*p); }
}
extern "C" void vd_gc_frame_leave(void* address) noexcept {
    if (auto* p=managed_test::active;p && p->inspect_roots) {
        inspect(*p);
        if (p->frames.empty() || p->frames.back().address!=address) std::terminate();
        p->frames.pop_back();
    }
    vd_gc_real_frame_leave(address);
}
extern "C" void vd_gc_real_poll() noexcept;
extern "C" void vd_gc_real_construct_typed(void**,std::size_t,const std::size_t*,std::size_t,const void*,const char*,vd::RuntimeFailure*) noexcept;
extern "C" void vd_gc_real_construct(void**,std::size_t,const std::size_t*,std::size_t,const void*,vd::RuntimeFailure*) noexcept;
extern "C" void vd_gc_poll() noexcept {
    if (auto* p=managed_test::active) {
        inspect(*p);
        ++p->polls;
        if ((p->gate_poll && p->polls==p->gate_poll) || (p->gate_after_constructions && p->constructions==p->gate_after_constructions && !p->gated)) { p->gated=true; p->reached.release(); p->release.acquire(); }
    }
    vd_gc_real_poll();
}
template<class Commit> void observe_construct(void** slot,std::size_t size,const std::size_t*,std::size_t,const void* payload,vd::RuntimeFailure* failure,Commit commit) noexcept {
    auto* p=managed_test::active;
    auto* previous=*slot;
    if (p) {
        inspect(*p);
        ++p->constructions;
        if (p->before_construct) p->before_construct(p->constructions,payload,size);
        if (p->collect_before && vd::Runtime::current().collect()!=vd::Runtime::Collection::completed) std::terminate();
        if (p->constructions==p->fail_construction) fail_after=p->fail_allocation;
    }
    commit();
    fail_after=-1;
    if (p) {
        if (failure->message) p->commits_valid &= *slot==previous;
        else {
            // Observe the actual published source object, including all pointer
            // leaves, after the production helper has released its commit lock.
            p->commits_valid &= *slot && std::memcmp(*slot,payload,size)==0;
            if (p->capture && p->constructions==p->capture_at && !p->retained) p->retained=std::make_unique<vd::Runtime::Handle>(vd::Runtime::current(),*slot);
        }
    }
}

extern "C" void vd_gc_construct(void** slot,std::size_t size,const std::size_t* offsets,std::size_t count,const void* payload,vd::RuntimeFailure* failure) noexcept {
    observe_construct(slot,size,offsets,count,payload,failure,[&]{vd_gc_real_construct(slot,size,offsets,count,payload,failure);});
}
extern "C" void vd_gc_construct_typed(void** slot,std::size_t size,const std::size_t* offsets,std::size_t count,const void* payload,const char* key,vd::RuntimeFailure* failure) noexcept {
    observe_construct(slot,size,offsets,count,payload,failure,[&]{vd_gc_real_construct_typed(slot,size,offsets,count,payload,key,failure);});
}
