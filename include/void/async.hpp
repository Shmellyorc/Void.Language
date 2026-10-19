#pragma once
#include "void/execution.hpp"
#include <cstddef>
// Private same-build compiler/runtime ABI. No public task groups or cancellation.
extern "C" {
using vd_async_start_entry = void* (*)(std::int32_t,vd::RuntimeFailure*);
using vd_async_resume_entry = void (*)(void*,vd::RuntimeFailure*);
std::int32_t vd_async_root(vd_async_start_entry,std::int32_t,std::size_t,const void*,vd::RuntimeFailure*) noexcept;
void vd_async_new(void**,void**,std::size_t,const std::size_t*,std::size_t,std::size_t,const std::size_t*,std::size_t,const char*,const void*,vd_async_resume_entry,vd::RuntimeFailure*) noexcept;
void vd_async_start(void*,vd::RuntimeFailure*) noexcept;
bool vd_async_ready(void*,vd::RuntimeFailure*) noexcept;
void vd_async_suspend(void*,void*,vd::RuntimeFailure*) noexcept;
void vd_async_complete(void*) noexcept;
}
