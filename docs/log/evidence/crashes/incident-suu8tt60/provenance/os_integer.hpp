#pragma once
#include <cstdint>
namespace vd::os::detail {
// DWORD is unsigned. Keep its full bit range before entering the signed VOID ABI.
constexpr std::int64_t process_id_from_dword(std::uint32_t value) noexcept {
    return static_cast<std::int64_t>(value);
}
}
