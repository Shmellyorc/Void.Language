#pragma once
#include "void/runtime_abi.hpp"
#include <array>
namespace vd::host {
struct ReplyLayout {std::size_t size;std::array<std::size_t,7> fields;const NativeArrayElement* strings;};
template<std::size_t Fields>
struct AsyncReplyLayout {
    std::size_t size;
    std::array<std::size_t,Fields> fields;
    const NativeArrayElement* strings;
    std::size_t task_size;
    const std::size_t* task_references;
    std::size_t task_reference_count, task_result;
    const char* task_key;
};
using CaptureLayout = AsyncReplyLayout<11>;
using NetworkLayout = AsyncReplyLayout<12>;
}
extern "C" void vd_host_call(void*,const vd::host::ReplyLayout*,unsigned,void* const*,const std::int64_t*,vd::RuntimeFailure*) noexcept;
extern "C" void vd_host_cleanup(unsigned,void*,vd::RuntimeFailure*) noexcept;
extern "C" void vd_host_capture(void*,const vd::host::CaptureLayout*,unsigned,void* const*,const std::int64_t*,const void*,vd::RuntimeFailure*) noexcept;
extern "C" void vd_net_call(void*,const vd::host::NetworkLayout*,unsigned,void* const*,const std::int64_t*,const void*,vd::RuntimeFailure*) noexcept;
extern "C" void vd_net_cleanup(unsigned,void*,vd::RuntimeFailure*) noexcept;
