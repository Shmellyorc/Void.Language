#pragma once
#include "void/runtime_abi.hpp"
#include <array>
namespace vd::host {
struct ReplyLayout {std::size_t size;std::array<std::size_t,7> fields;const NativeArrayElement* strings;};
}
extern "C" void vd_host_call(void*,const vd::host::ReplyLayout*,unsigned,void* const*,const std::int64_t*,vd::RuntimeFailure*) noexcept;
extern "C" void vd_host_cleanup(unsigned,void*,vd::RuntimeFailure*) noexcept;
