#pragma once

#include "void/runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace vd::filesystem {
// Geometry is emitted from the same LLVM DataLayout as the validated source
// reply and entry. It is private same-build data, never public source FFI.
struct ReplyLayout {
    std::size_t size;
    std::array<std::size_t, 10> fields;
    const NativeArrayElement* entry;
    std::size_t task_size;
    const std::size_t* task_references;
    std::size_t task_reference_count;
    std::size_t task_result;
    const char* task_key;
};
}

extern "C" void vd_filesystem_call(void*, const vd::filesystem::ReplyLayout*,
    unsigned, void* const*, const std::int64_t*, const void*, bool,
    vd::RuntimeFailure*) noexcept;

extern "C" std::int64_t vd_file_control(unsigned, void*, vd::RuntimeFailure*) noexcept;
