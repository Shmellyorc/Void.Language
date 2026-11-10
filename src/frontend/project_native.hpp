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
SelectedNative select_managed_native(const NativeRequirement&);
std::map<std::string, std::string> read_managed_payload(const CPrimitiveAdmission&);
std::map<std::string, std::string> read_managed_link_inputs(const CPrimitiveAdmission&,
                                                          const std::vector<NativeArtifact>&);
}
