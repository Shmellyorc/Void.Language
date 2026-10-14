#pragma once
#include "void/runtime.hpp"
#include "void/execution.hpp"
// Private, same-build ABI. The LLVM lowerer checks sizes/alignment against its
// target layout. Neither this frame ABI nor aggregate layouts are a public FFI.
namespace vd {
struct RuntimeWitnessRecord {const char* key;const char* interface_identity;const char* view_identity;const char* contract;};
struct RuntimeTypeRecord {
    const char* identity;
    const char* base;
    const char* shape;
    std::size_t size;
    const std::size_t* references;
    std::size_t count;
};
}
extern "C" {
void vd_gc_frame_enter(void*, const vd::Runtime::Slot*, std::size_t) noexcept;
void vd_gc_frame_leave(void*) noexcept;
void vd_gc_poll() noexcept;
void vd_gc_collect(vd::RuntimeFailure*) noexcept;
void vd_gc_construct(void**, std::size_t, const std::size_t*, std::size_t, const void*, vd::RuntimeFailure*) noexcept;
void vd_gc_construct_typed(void**, std::size_t, const std::size_t*, std::size_t, const void*, const char*, vd::RuntimeFailure*) noexcept;
void vd_runtime_admit(vd::Runtime*,const vd::RuntimeTypeRecord*,std::size_t,vd::RuntimeFailure*) noexcept;
void vd_runtime_admit_metadata(vd::Runtime*,const vd::RuntimeTypeRecord*,std::size_t,const vd::RuntimeWitnessRecord*,std::size_t,vd::RuntimeFailure*) noexcept;
const void* vd_gc_witness(const char*) noexcept;
std::int64_t vd_gc_interface_binding(void*,const void*,const char* const*,std::size_t) noexcept;
std::int32_t vd_runtime_entry_metadata(vd::NativeCode::CheckedEntry,std::int32_t,vd::RuntimeFailure*,const vd::RuntimeTypeRecord*,std::size_t,const vd::RuntimeWitnessRecord*,std::size_t) noexcept;
std::int64_t vd_gc_dynamic_type(void*,const char* const*,std::size_t) noexcept;
std::int32_t vd_runtime_entry(vd::NativeCode::CheckedEntry, std::int32_t, vd::RuntimeFailure*) noexcept;
std::int32_t vd_runtime_entry_typed(vd::NativeCode::CheckedEntry,std::int32_t,vd::RuntimeFailure*,const vd::RuntimeTypeRecord*,std::size_t) noexcept;
void vd_gc_allocate(void**, std::size_t, const std::size_t*, std::size_t, vd::RuntimeFailure*) noexcept;
bool vd_gc_alive(void*) noexcept;
void vd_gc_native_enter(void*) noexcept;
void vd_gc_native_leave(void*) noexcept;
}
