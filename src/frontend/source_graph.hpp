#pragma once

#include "model.hpp"
#include "void/project_resolution.hpp"

namespace vd::frontend {

std::string folded(std::string);
bool identifier(const std::string&);
std::string normalize(const Unit& from, const Token& path);

class SourceReadError : public CompilationError {
    static Diagnostic record(const std::string& path, const std::string& message);
public:
    SourceReadError(const std::string& path, const std::string& message);
};

class SourceGraph {
    std::filesystem::path root;
    Diagnostics& diagnostics;
    unsigned depth = 0;
    const SourceOverrides& overrides;
    const NativeProject* native=nullptr;
    CPrimitiveAdmission::Owner primitives;
    std::string physical_primitive_module;
    std::optional<std::filesystem::path> standard_root;
    struct Directory {std::map<std::string,std::set<std::string>> names;std::error_code error;};
    std::map<std::filesystem::path,Directory> directories;
    std::map<std::string,std::map<std::string,std::set<std::string>>> virtual_children;
    std::set<std::string> virtual_directories;
    const Directory& directory_entries(const std::filesystem::path& path);
public:
    std::map<std::string, std::unique_ptr<Unit>> units;
    // Parsed discovery trees are not execution members until load follows them.
    std::map<std::string, std::unique_ptr<Unit>> parsed;
    std::function<void(const std::filesystem::path&,const std::string&,bool)> admit;
    std::function<SourceFile(const std::filesystem::path&,const std::string&)> reader;
    const project::LockedDependencies* dependencies = nullptr;
    BuildConditions conditions=native_build_conditions();
    BuildConditions conditions_for(const std::string& id) const;
    bool imports_only = false; // Metadata inspection grants no native or lowering authority.
    explicit SourceGraph(std::filesystem::path project_root, Diagnostics& d, const SourceOverrides& o,const NativeProject* bindings=nullptr,
                         CPrimitiveAdmission::Owner c_primitives = {});
    void generated_primitives(CPrimitiveAdmission::Owner admission);
    std::function<bool()> input_check() const;
    const project::LockedPackage* package(std::string_view identity) const;
    const project::DependencyEdges* direct_packages(std::string_view identity) const;
    SourceFile package_snapshot(const std::string& id) const;
    std::string package_identity(const std::filesystem::path& physical) const;
    std::pair<std::string,std::filesystem::path> resolve(const Unit& from, const Token& path, bool directory=false);
    Unit& parse(const Unit& from, const Token& path);
    Unit& parse_resolved(const Unit& from,const Token& path,const std::string& id,const std::filesystem::path& physical);
    Unit& load(const Unit& from, const Token& path);
};
}
