#pragma once
#include "void/source.hpp"
#include <map>
#include <string>
#include <string_view>
#include <vector>
namespace vd {
// Immutable per-analysis input. Source code cannot mutate these conditions.
struct BuildConditions {
    std::string target_os, target_arch;
    std::map<std::string,bool> features;
    std::string key() const;
};
// Uses LLVM target identities, including Darwin -> macos and ARM64 -> aarch64.
BuildConditions target_build_conditions(std::string_view triple);
const BuildConditions& native_build_conditions();
bool valid_build_condition_name(std::string_view);
// Restricted boolean grammar; validates every operand, including skipped ones.
bool evaluate_build_condition(const SourceFile&,std::size_t begin,std::size_t end,const BuildConditions&);
struct ConditionalRegion {
    std::size_t begin, end;
    bool directive = false;
};
}
