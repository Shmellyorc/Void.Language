#pragma once
#include "void/frontend.hpp"
namespace vd {
namespace publish { struct Pack; }
namespace publish { class PublishPlan; }
bool native_source_debug_supported();
// Same-directory publication staging protects previous artifacts. Windows links
// in a separate short private directory, then verifies/copies into that staging.
void publish_object(const ValidatedAnalysis&, const std::filesystem::path&, bool optimize, std::string_view profile = {},
                    std::optional<project::DebugInfo> source_debug = {});
void build_executable(const ValidatedAnalysis&, const std::filesystem::path&, bool optimize, const char* argv0, std::string_view profile = {},
                      std::optional<project::DebugInfo> source_debug = {});
void build_application_package(const ProjectAnalysis&, const std::filesystem::path&, bool optimize,
                               const char* argv0, std::string_view profile, bool replace);
std::filesystem::path publish_native_application(const ProjectAnalysis&, const publish::PublishPlan&, const char* argv0, const publish::Pack* = nullptr);
void verify_native_publication(const std::filesystem::path&, std::string_view target);
void verify_application_package(const std::filesystem::path&);
}
