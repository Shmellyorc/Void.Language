#pragma once
#include "void/runtime_abi.hpp"
#include <array>
#include <cstddef>
namespace vd::os {
// status, native code, scalar payload, flag, owned text, owned native domain.
struct ReplyLayout { std::size_t size; std::array<std::size_t, 6> fields; };
}
extern "C" void vd_os_call(void*, const vd::os::ReplyLayout*, unsigned, void*, vd::RuntimeFailure*) noexcept;

namespace vd::os {
struct EmbeddedResource { const char* name; std::size_t name_size; const char* bytes; std::size_t size; };
}
extern "C" std::int64_t vd_os_embedded_resource(void**, void*, const vd::os::EmbeddedResource*,
                                                std::size_t, vd::RuntimeFailure*) noexcept;
