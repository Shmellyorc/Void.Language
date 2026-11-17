#pragma once

#include "void/c_primitive.hpp"
#include "void/project.hpp"
#include <filesystem>
#include <map>

namespace vd::project {
struct SelectedNative {
    NativeCatalog artifacts;
    CPrimitiveAdmission::Owner admission;
};
const NativeRequirement& verified_raylib_requirement();
inline constexpr std::string_view verified_raylib_catalog = ".void/native";
SelectedNative select_managed_native(const Manifest&, const std::filesystem::path& project_root);
std::map<std::string, std::string> read_managed_payload(const CPrimitiveAdmission&);
std::map<std::string, std::string> read_managed_link_inputs(const CPrimitiveAdmission&,
                                                          const std::vector<NativeArtifact>&);
}
