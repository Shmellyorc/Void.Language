#pragma once
#include "void/project.hpp"
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace vd { class CPrimitiveAdmission; }
namespace vd::project {
using DependencyEdges = std::map<std::string, std::string, std::less<>>;
using PackageSources = std::map<std::string, std::string, std::less<>>;
struct LockedPackage {
    std::string locator, entry;
    DependencyEdges dependencies;
    PackageSources sources; // Package-relative paths and exact verified bytes.
};
class LockedDependencies {
    std::string identity_;
    std::string native_catalog_identity_, root_identity_;
    DependencyEdges direct_;
    std::map<std::string, LockedPackage, std::less<>> packages_;
    std::shared_ptr<const CPrimitiveAdmission> native_;
    std::vector<NativeArtifact> native_link_inputs_;
    LockedDependencies() = default;
    friend std::shared_ptr<const LockedDependencies> verify_locked_dependencies(
        const std::filesystem::path&, const Manifest&, bool);
public:
    const auto& identity() const { return identity_; }
    const auto& native_catalog_identity() const { return native_catalog_identity_; }
    const auto& root_identity() const { return root_identity_; }
    const auto& direct() const { return direct_; }
    const auto& packages() const { return packages_; }
    const auto& native() const { return native_; }
    const auto& native_link_inputs() const { return native_link_inputs_; }
};
// Verify against the effective root manifest. Only the compiler's first capture
// needs to retain the bytes. A recapture can verify the entire plan without them.
std::shared_ptr<const LockedDependencies> verify_locked_dependencies(
    const std::filesystem::path&, const Manifest&, bool retain_sources = true);

enum class ResolveMode { create, refresh, locked };
enum class ResolveResult { no_dependencies, verified, created, refreshed };
// Resolve local source metadata and verify or publish the selected root's lock.
ResolveResult resolve_dependencies(const std::filesystem::path&, ResolveMode);

struct ExplainedPackage {
    std::string id, version, entry, path, manifest_sha256, tree_sha256;
    std::optional<GitDependency> git;
};
struct ExplainedDependency {
    bool direct;
    std::string parent, alias, target, version_requirement, path;
    std::optional<GitDependency> git;
};
struct DependencyExplanation {
    std::string project_id;
    std::optional<std::string> lock_sha256;
    std::vector<ExplainedPackage> packages;
    std::vector<ExplainedDependency> relationships;
};
// The selected manifest and exact local lock are verified before returning any
// package claims. The compiler's acquisition and publication paths are absent.
DependencyExplanation explain_dependencies(const std::filesystem::path&);

// Only this operation may consult the explicitly authorized Git repositories.
std::size_t fetch_dependencies(const std::filesystem::path&, const std::vector<GitPin>&);
bool valid_git_pin(const GitPin&);

struct AddOptions {
    std::string alias;
    std::optional<std::filesystem::path> path;
    std::optional<GitDependency> git;
};
struct MaintenanceReport {
    bool changed = false;
    std::vector<std::string> advice;
};
MaintenanceReport add_dependency(const std::filesystem::path&, const AddOptions&,
                                 const std::function<std::filesystem::path()>& clang = {}, bool sanitize = false);
MaintenanceReport remove_dependency(const std::filesystem::path&, const std::string& alias);
MaintenanceReport install_user_package(const AddOptions&, const std::function<std::filesystem::path()>& clang,
                                       bool sanitize);
std::optional<MaintenanceReport> add_user_package(const std::filesystem::path&, const std::string& alias);
std::vector<std::string> list_user_packages();
void uninstall_user_package(const std::string& alias);
void require_outside_user_package_store(const std::filesystem::path&);
MaintenanceReport add_verified_raylib(const std::filesystem::path& project,
    const std::filesystem::path& source, const std::filesystem::path& compiler,
    const std::filesystem::path& clang, bool sanitize);
MaintenanceReport tidy_dependencies(const std::filesystem::path&, bool check);
}
