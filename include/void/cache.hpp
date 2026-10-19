#pragma once
#include <filesystem>
#include <string>
#include <string_view>
namespace vd {
// Optional development-host policy. Arbitrary native bindings disable reuse.
struct DebugCache { std::filesystem::path project_root; std::string build_identity; };
// Embedded by the build from the actual compiler/link inputs, not Git or mtimes.
std::string_view compiler_build_identity();
}
