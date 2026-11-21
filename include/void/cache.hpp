#pragma once
#include <filesystem>
#include <string>
#include <string_view>
namespace vd {
// Optional persistent native-object policy (IR O2 and no-IR-O2 have distinct
// identities). Kept under its original API name for source compatibility.
// Arbitrary native bindings disable reuse; dynamic plans need complete image or
// payload identities, verified afresh per generation and included in the key.
struct DebugCache { std::filesystem::path project_root; std::string build_identity; };
// Embedded by the build from the actual compiler/link inputs, not Git or mtimes.
std::string_view compiler_build_identity();
}
