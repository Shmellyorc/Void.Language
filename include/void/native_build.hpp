#pragma once
#include "void/frontend.hpp"
namespace vd {
// Same-directory staging protects previous artifacts and keeps linker paths physical.
void publish_object(const ValidatedAnalysis&, const std::filesystem::path&, bool optimize);
void build_executable(const ValidatedAnalysis&, const std::filesystem::path&, bool optimize, const char* argv0);
}
