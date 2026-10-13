#include "void/runtime_abi.hpp"
#include <new>
#include <stdexcept>
namespace vd {
Runtime& Runtime::current() {
    if (!current_runtime_) throw std::logic_error("generated code has no runtime attachment");
    return *current_runtime_;
}
}
extern "C" {
void vd_gc_frame_enter(void* frame, const vd::Runtime::Slot* slots, std::size_t count) noexcept {
    try { new (frame) vd::Runtime::Frame(vd::Runtime::current(), slots, count); }
    catch (...) { std::terminate(); }
}
void vd_gc_frame_leave(void* frame) noexcept { static_cast<vd::Runtime::Frame*>(frame)->~Frame(); }
void vd_gc_poll() noexcept {
    try { vd::Runtime::current().poll(); } catch (...) { std::terminate(); }
}
void vd_gc_collect(vd::RuntimeFailure* failure) noexcept {
    try {
        auto status = vd::Runtime::current().collect();
        if (status != vd::Runtime::Collection::completed) failure->message = "GC collection could not complete";
    } catch (...) { std::terminate(); }
}
std::int32_t vd_runtime_entry(vd::NativeCode::CheckedEntry entry, std::int32_t value, vd::RuntimeFailure* failure) noexcept {
    try {
        vd::Runtime runtime(256);
        vd::Runtime::Attachment attachment(runtime);
        vd::Runtime::Managed managed(runtime);
        return entry(value,failure);
    } catch (const std::bad_alloc&) { failure->message="Runtime startup allocation failed"; }
      catch (...) { std::terminate(); }
    return 0;
}
void vd_gc_construct(void** slot, std::size_t size, const std::size_t* offsets, std::size_t count, const void* payload, vd::RuntimeFailure* failure) noexcept {
    try {
        vd::TypeLayout layout{size,{}};
        if (count) layout.references.assign(offsets,offsets+count);
        vd::Runtime::current().construct_into(slot,layout,payload);
    } catch (const std::bad_alloc&) { failure->message="Managed construction allocation failed"; }
      catch (...) { std::terminate(); }
}
void vd_gc_allocate(void** slot, std::size_t size, const std::size_t* offsets, std::size_t count, vd::RuntimeFailure* failure) noexcept {
    try {
        vd::TypeLayout layout{size, {}};
        if (count) layout.references.assign(offsets, offsets + count);
        vd::Runtime::current().allocate_into(slot, layout);
    } catch (const std::bad_alloc&) { failure->message = "Managed allocation failed"; }
    catch (...) { std::terminate(); }
}
bool vd_gc_alive(void* object) noexcept {
    try { return vd::Runtime::current().contains(object); } catch (...) { std::terminate(); }
}
void vd_gc_native_enter(void* guard) noexcept {
    try { new (guard) vd::Runtime::Native(vd::Runtime::current()); } catch (...) { std::terminate(); }
}
void vd_gc_native_leave(void* guard) noexcept { static_cast<vd::Runtime::Native*>(guard)->~Native(); }
}
