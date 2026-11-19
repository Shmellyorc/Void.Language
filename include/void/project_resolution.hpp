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
// Exact ordered digest inputs, owned by a verified analysis. A missing value
// denotes a nested-project boundary; it is distinct from an empty file.
struct EditorPackageInputs {
    std::map<std::string,std::optional<std::string>> files;
    std::string digest;
};
struct LockedPackage {
    std::string locator, entry;
    DependencyEdges dependencies;
    PackageSources sources; // Package-relative paths and exact verified bytes.
    std::shared_ptr<const EditorPackageInputs> editor_inputs = {};
};
class LockedDependencies {
    std::string identity_;
    std::string native_catalog_identity_, root_identity_;
    DependencyEdges direct_;
    std::map<std::string, LockedPackage, std::less<>> packages_;
    std::shared_ptr<const CPrimitiveAdmission> native_;
    std::vector<NativeArtifact> native_link_inputs_;
    std::shared_ptr<const std::map<std::string,std::string>> editor_native_inputs_;
    LockedDependencies() = default;
    friend std::shared_ptr<const LockedDependencies> verify_locked_dependencies(
        const std::filesystem::path&, const Manifest&, bool, const LockedDependencies*);
public:
    const auto& identity() const { return identity_; }
    const auto& native_catalog_identity() const { return native_catalog_identity_; }
    const auto& root_identity() const { return root_identity_; }
    const auto& direct() const { return direct_; }
    const auto& packages() const { return packages_; }
    const auto& native() const { return native_; }
    const auto& native_link_inputs() const { return native_link_inputs_; }
    const auto& editor_native_inputs() const { return editor_native_inputs_; }
};
// Verify against the effective root manifest. Only the compiler's first capture
// needs to retain the bytes. A recapture can verify the entire plan without them.
std::shared_ptr<const LockedDependencies> verify_locked_dependencies(
    const std::filesystem::path&, const Manifest&, bool retain_sources = true);
// Editor-only reuse hint. All filesystem and byte inputs are still recaptured.
std::shared_ptr<const LockedDependencies> verify_locked_dependencies(
    const std::filesystem::path&, const Manifest&, bool retain_sources,
    const LockedDependencies* editor_reuse);

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
    std::optional<std::string> edit_token;
};
// The selected manifest and exact local lock are verified before returning any
// package claims. The compiler's acquisition and publication paths are absent.
DependencyExplanation explain_dependencies(const std::filesystem::path&, bool for_edit = false);

// Only this operation may consult the explicitly authorized Git repositories.
std::size_t fetch_dependencies(const std::filesystem::path&, const std::vector<GitPin>&);
bool valid_git_pin(const GitPin&);

struct AddOptions {
    std::string alias;
    std::optional<std::filesystem::path> path;
    std::optional<GitDependency> git;
    bool copy = false;
    std::optional<std::string> edit_token;
};
struct MaintenanceReport {
    bool changed = false;
    std::vector<std::string> advice;
};
MaintenanceReport add_dependency(const std::filesystem::path&, const AddOptions&,
                                 const std::function<std::filesystem::path()>& clang = {}, bool sanitize = false);
MaintenanceReport remove_dependency(const std::filesystem::path&, const std::string& alias,
                                    const std::optional<std::string>& edit_token = {});
std::optional<std::string> suggest_local_package_alias(const std::filesystem::path& root,
                                                       const std::filesystem::path& source);
MaintenanceReport install_user_package(const AddOptions&, const std::function<std::filesystem::path()>& clang,
                                       bool sanitize);
struct RegisteredPackage {
    std::string alias, version, id, identity;
    std::optional<GitDependency> git;
};
std::optional<MaintenanceReport> add_user_package(const std::filesystem::path&, const std::string& alias,
    const std::optional<std::string>& edit_token = {}, const std::optional<std::string>& registration_identity = {});
std::vector<RegisteredPackage> list_user_packages();
void uninstall_user_package(const std::string& alias);
void require_outside_user_package_store(const std::filesystem::path&);
struct BundledRaylib {
    std::filesystem::path source;
    std::string_view inventory, snapshot_sha256, receipt_sha256, preparer_sha256;
};
MaintenanceReport add_verified_raylib(const std::filesystem::path& project,
    const BundledRaylib&, const std::filesystem::path& compiler,
    const std::filesystem::path& clang, bool sanitize,
    const std::optional<std::string>& edit_token = {});
MaintenanceReport tidy_dependencies(const std::filesystem::path&, bool check);
}
