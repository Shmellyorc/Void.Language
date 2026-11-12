#pragma once
#include "void/frontend.hpp"
namespace vd {
bool native_source_debug_supported();
// Same-directory publication staging protects previous artifacts. Windows links
// in a separate short private directory, then verifies/copies into that staging.
void publish_object(const ValidatedAnalysis&, const std::filesystem::path&, bool optimize, std::string_view profile = {},
                    std::optional<project::DebugInfo> source_debug = {});
void build_executable(const ValidatedAnalysis&, const std::filesystem::path&, bool optimize, const char* argv0, std::string_view profile = {},
                      std::optional<project::DebugInfo> source_debug = {});
void build_application_package(const ProjectAnalysis&, const std::filesystem::path&, bool optimize,
                               const char* argv0, std::string_view profile, bool replace);
void verify_application_package(const std::filesystem::path&);
}
