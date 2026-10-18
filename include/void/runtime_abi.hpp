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
void vd_string_literal(void**, const char*, std::size_t, vd::RuntimeFailure*) noexcept;
void vd_string_concat(void**, void*, void*, vd::RuntimeFailure*) noexcept;
bool vd_string_equal(void*, void*, vd::RuntimeFailure*) noexcept;
// Private same-build scalar proof bridge. The embedding caller retains these
// admitted layout/explicit work-quota values for the duration of a call.
// No transaction object or C++ exception crosses a generated frame. Literal
// words must be stable non-managed storage, as in Runtime::fixed_literal_into.
// Check a static scalar signature at the embedding boundary. Data helpers
// already validate registered roots, buffer layout and publication themselves.
void vd_fixed_validate_layout(const vd::BufferLayoutRef*,std::uint64_t,unsigned,vd::RuntimeFailure*) noexcept;
void vd_fixed_default(void**,const vd::BufferLayoutRef*,const vd::BufferWorkQuota*,vd::RuntimeFailure*) noexcept;
void vd_fixed_literal(void**,const vd::BufferLayoutRef*,const std::uint64_t*,std::size_t,const vd::BufferWorkQuota*,vd::RuntimeFailure*) noexcept;
void vd_fixed_copy(void**,void**,const vd::BufferLayoutRef*,const vd::BufferWorkQuota*,vd::RuntimeFailure*) noexcept;
std::uint64_t vd_fixed_read(void**,const vd::BufferLayoutRef*,std::uint64_t,vd::RuntimeFailure*) noexcept;
void vd_fixed_write(void**,const vd::BufferLayoutRef*,std::uint64_t,std::uint64_t,vd::RuntimeFailure*) noexcept;
// Source numeric fixed arrays use Runtime buffers and ordinary scalar/pointer ABI.
void vd_array_default(void**,unsigned,std::uint64_t,std::uint64_t,vd::RuntimeFailure*) noexcept;
void vd_array_copy(void**,void**,unsigned,std::uint64_t,vd::RuntimeFailure*) noexcept;
std::uint64_t vd_array_read(void**,std::uint64_t,unsigned,std::uint64_t,vd::RuntimeFailure*) noexcept;
void vd_array_write(void**,std::uint64_t,std::uint64_t,unsigned,std::uint64_t,vd::RuntimeFailure*) noexcept;
void vd_list_new(void**,unsigned,std::uint64_t,vd::RuntimeFailure*) noexcept;
std::uint64_t vd_list_count(void**,unsigned,vd::RuntimeFailure*) noexcept;
std::uint64_t vd_list_version(void**,unsigned,vd::RuntimeFailure*) noexcept;
std::uint64_t vd_list_read(void**,unsigned,std::uint64_t,vd::RuntimeFailure*) noexcept;
void vd_list_write(void**,unsigned,std::uint64_t,std::uint64_t,std::uint64_t,unsigned,vd::RuntimeFailure*) noexcept;
void vd_list_push(void**,unsigned,std::uint64_t,vd::RuntimeFailure*) noexcept;
std::uint64_t vd_list_remove(void**,unsigned,std::uint64_t,vd::RuntimeFailure*) noexcept;
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
extern "C" {
void vd_value_array_new(void**,const vd::NativeArrayElement*,std::uint64_t,unsigned,vd::RuntimeFailure*) noexcept;
std::uint64_t vd_value_array_count(void**,const vd::NativeArrayElement*,vd::RuntimeFailure*) noexcept;
std::uint64_t vd_value_array_version(void**,const vd::NativeArrayElement*,vd::RuntimeFailure*) noexcept;
void vd_value_array_read(void*,void**,const vd::NativeArrayElement*,std::uint64_t,unsigned,std::uint64_t,vd::RuntimeFailure*) noexcept;
void vd_value_array_write(void**,const vd::NativeArrayElement*,std::uint64_t,unsigned,std::uint64_t,const void*,std::uint64_t,unsigned,unsigned,vd::RuntimeFailure*) noexcept;
void vd_value_array_push(void**,const vd::NativeArrayElement*,const void*,vd::RuntimeFailure*) noexcept;
void vd_value_array_remove(void*,void**,const vd::NativeArrayElement*,std::uint64_t,vd::RuntimeFailure*) noexcept;
void vd_value_array_copy(void**,void**,const vd::NativeArrayElement*,std::uint64_t,vd::RuntimeFailure*) noexcept;
}
