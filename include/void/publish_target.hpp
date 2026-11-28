#pragma once
#include <array>
#include <string_view>

namespace vd::publish {
enum class System { linux_gnu, windows_msvc, macos };
enum class Architecture { x64, arm64 };
struct Target {
    std::string_view id, triple;
    System system;
    Architecture architecture;
};
inline constexpr std::array targets{
    Target{"linux-x64", "x86_64-unknown-linux-gnu", System::linux_gnu, Architecture::x64},
    Target{"linux-arm64", "aarch64-unknown-linux-gnu", System::linux_gnu, Architecture::arm64},
    Target{"windows-x64", "x86_64-pc-windows-msvc", System::windows_msvc, Architecture::x64},
    Target{"windows-arm64", "aarch64-pc-windows-msvc", System::windows_msvc, Architecture::arm64},
    Target{"macos-x64", "x86_64-apple-macosx14.0", System::macos, Architecture::x64},
    Target{"macos-arm64", "arm64-apple-macosx14.0", System::macos, Architecture::arm64},
};
inline constexpr const Target* find_target(std::string_view id) {
    for (const auto& target : targets)
        if (target.id == id) return &target;
    return nullptr;
}
inline constexpr bool custom_target(std::string_view name) {
    return name.starts_with("custom-") && name.size() > 7 && name.size() <= 48 &&
        name.back() != '-' && name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-") == name.npos;
}
inline constexpr bool target_name(std::string_view name) {
    return find_target(name) || custom_target(name);
}
}
