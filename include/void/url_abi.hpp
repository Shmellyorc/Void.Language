#pragma once
#include "void/runtime_abi.hpp"
#include <array>
namespace vd::url {
struct ReplyLayout {
    std::size_t size;
    std::array<std::size_t, 13> fields;
    const NativeArrayElement* item;
};
}
extern "C" void vd_url_call(void*, const vd::url::ReplyLayout*, unsigned,
    void* const*, const std::int64_t*, vd::RuntimeFailure*) noexcept;
