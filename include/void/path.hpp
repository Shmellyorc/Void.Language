#pragma once
#include <filesystem>
#include <string>
#include <string_view>
namespace vd {
// Shared lexical VFS component policy. Physical path admission remains separate.
inline bool portable_path_component(std::string_view component) {
    if (component.empty() || component.back() == ' ' || component.back() == '.') return false;
    for (unsigned char c : component)
        if (c < 32 || c >= 127 || std::string_view("<>:\"|?*\\").find(static_cast<char>(c)) != std::string_view::npos) return false;
    auto base = std::string(component.substr(0, component.find('.')));
    for (auto& c : base) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return !(base == "con" || base == "prn" || base == "aux" || base == "nul" ||
        (base.size() == 4 && (base.starts_with("com") || base.starts_with("lpt")) && base[3] >= '1' && base[3] <= '9'));
}
// Never interpret UTF-8 source/URI paths through the Windows ANSI code page.
inline std::filesystem::path utf8_path(std::string_view text) {
    return std::filesystem::path(std::u8string(text.begin(),text.end()));
}
inline std::string path_utf8(const std::filesystem::path& path) {
    auto text=path.generic_u8string(); return {text.begin(),text.end()};
}
}
