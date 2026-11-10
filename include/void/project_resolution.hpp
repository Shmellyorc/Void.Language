#pragma once
#include <filesystem>
#include <map>
#include <memory>
#include <string>

namespace vd::project {
class Manifest;
using DependencyEdges = std::map<std::string, std::string, std::less<>>;
using PackageSources = std::map<std::string, std::string, std::less<>>;
struct LockedPackage {
    std::string locator, entry;
    DependencyEdges dependencies;
    PackageSources sources; // Package-relative paths and exact verified bytes.
};
class LockedDependencies {
    std::string identity_;
    DependencyEdges direct_;
    std::map<std::string, LockedPackage, std::less<>> packages_;
    LockedDependencies() = default;
    friend std::shared_ptr<const LockedDependencies> verify_locked_dependencies(
        const std::filesystem::path&, const Manifest&, bool);
public:
    const auto& identity() const { return identity_; }
    const auto& direct() const { return direct_; }
    const auto& packages() const { return packages_; }
};
// Verify against the effective root manifest. Only the compiler's first capture
// needs to retain the bytes. A recapture can verify the entire plan without them.
std::shared_ptr<const LockedDependencies> verify_locked_dependencies(
    const std::filesystem::path&, const Manifest&, bool retain_sources = true);

enum class ResolveMode { create, refresh, locked };
enum class ResolveResult { no_dependencies, verified, created, refreshed };
// Resolve local source metadata and verify or publish the selected root's lock.
ResolveResult resolve_dependencies(const std::filesystem::path&, ResolveMode);
}
