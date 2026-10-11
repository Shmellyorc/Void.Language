#pragma once
#include <filesystem>
#include <string>
#include <string_view>
namespace vd {
// Never interpret UTF-8 source/URI paths through the Windows ANSI code page.
inline std::filesystem::path utf8_path(std::string_view text) {
    return std::filesystem::path(std::u8string(text.begin(),text.end()));
}
inline std::string path_utf8(const std::filesystem::path& path) {
    auto text=path.generic_u8string(); return {text.begin(),text.end()};
}
}
