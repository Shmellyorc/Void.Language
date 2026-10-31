#pragma once
#include "void/http_ops.hpp"
#include "void/runtime_abi.hpp"
namespace vd::http {
struct SourceLayout {
    std::array<const NativeArrayElement*, static_cast<unsigned>(SourceRecord::count)> records;
    std::size_t task_size;
    const std::size_t* task_references;
    std::size_t task_reference_count, task_result;
    const char* task_key;
};
}
extern "C" void vd_http_call(void*, const vd::http::SourceLayout*, unsigned,
    void* const*, const std::int64_t*, const void*, bool, vd::RuntimeFailure*) noexcept;
extern "C" void vd_http_cleanup(unsigned, void*, vd::RuntimeFailure*) noexcept;
