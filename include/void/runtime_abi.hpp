#pragma once
#include "void/runtime.hpp"
#include "void/execution.hpp"
// Private, same-build ABI. The LLVM lowerer checks sizes/alignment against its
// target layout. Neither this frame ABI nor aggregate layouts are a public FFI.
extern "C" {
void vd_gc_frame_enter(void*, const vd::Runtime::Slot*, std::size_t) noexcept;
void vd_gc_frame_leave(void*) noexcept;
void vd_gc_poll() noexcept;
void vd_gc_collect(vd::RuntimeFailure*) noexcept;
void vd_gc_construct(void**, std::size_t, const std::size_t*, std::size_t, const void*, vd::RuntimeFailure*) noexcept;
std::int32_t vd_runtime_entry(vd::NativeCode::CheckedEntry, std::int32_t, vd::RuntimeFailure*) noexcept;
void vd_gc_allocate(void**, std::size_t, const std::size_t*, std::size_t, vd::RuntimeFailure*) noexcept;
bool vd_gc_alive(void*) noexcept;
void vd_gc_native_enter(void*) noexcept;
void vd_gc_native_leave(void*) noexcept;
}
