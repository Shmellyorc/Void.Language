#pragma once

#include "void/c_primitive.hpp"
#include "void/project.hpp"
#include "project_files.hpp"
#include <filesystem>
#include <map>

namespace vd::project {
struct SelectedNative {
    NativeCatalog artifacts;
    CPrimitiveAdmission::Owner admission;
};
const NativeRequirement& verified_raylib_requirement();
inline constexpr std::string_view verified_raylib_catalog = ".void/native";
inline constexpr std::string_view native_build_recipe = "native/void.build.toml";
SemanticVersion parse_native_build_recipe(std::string_view);
ProjectFiles prepare_native_package(const ProjectFiles&, const NativeRequirement&,
                                    const std::filesystem::path& clang, bool sanitize);
SelectedNative select_managed_native(const Manifest&, const std::filesystem::path& project_root);
std::map<std::string, std::string> read_managed_payload(const CPrimitiveAdmission&);
std::map<std::string, std::string> read_managed_link_inputs(const CPrimitiveAdmission&,
                                                          const std::vector<NativeArtifact>&);
}
