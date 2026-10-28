#pragma once
#include "void/frontend.hpp"
namespace vd {
// Same-directory publication staging protects previous artifacts. Windows links
// in a separate short private directory, then verifies/copies into that staging.
void publish_object(const ValidatedAnalysis&, const std::filesystem::path&, bool optimize);
void build_executable(const ValidatedAnalysis&, const std::filesystem::path&, bool optimize, const char* argv0);
}
