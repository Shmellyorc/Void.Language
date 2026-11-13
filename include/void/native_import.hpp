#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace vd {
struct NativeImportOptions {
    std::filesystem::path header, output, clang;
    std::string provider, source = "/native.void", language = "c";
    std::vector<std::string> preprocessing;
};
// Explicit offline declaration generation. Never called by project compilation.
void import_native_header(const NativeImportOptions&);
std::filesystem::path native_clang_driver(const char* argv0);
}
