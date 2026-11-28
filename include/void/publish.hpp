#pragma once
#include "void/project.hpp"
#include <filesystem>
#include <span>

namespace vd::publish {
inline constexpr std::string_view policy_revision = "native-publish-1";
struct Request {
    std::filesystem::path project = ".";
    std::optional<std::string> entry, target, profile, optimization, resources, symbols, lto, native_catalog;
    std::optional<std::filesystem::path> output;
};
// Pure CLI parsing. No source operands, subprocesses or writes.
Request parse_arguments(std::span<const std::string_view>);

// This stage records a contract, never an admitted graph or emission authority.
// Later stages must capture/revalidate source, lock, toolchain, native and resource
// snapshots before constructing an executable publication plan.
class PublishPlan final {
    std::string target_, profile_, manifest_sha256_, optimization_, resources_, symbols_, lto_;
    std::filesystem::path output_;
    std::string project_id_;
    std::optional<std::string> entry_;
    std::vector<std::string> required_capabilities_;
    PublishPlan(const Request&, const project::Manifest&, std::string_view host);
    friend PublishPlan plan_contract(const Request&, const project::Manifest&, std::string_view);
public:
    PublishPlan(const PublishPlan&) = default;
    PublishPlan(PublishPlan&&) = default;
    PublishPlan& operator=(const PublishPlan&) = delete;
    PublishPlan& operator=(PublishPlan&&) = delete;
    bool thin_lto() const { return lto_ == "thin"; }
    const auto& optimization() const { return optimization_; }
    const auto& resources() const { return resources_; }
    const auto& symbols() const { return symbols_; }
    const auto& target() const { return target_; }
    const auto& profile() const { return profile_; }
    const auto& manifest_sha256() const { return manifest_sha256_; }
    const auto& output() const { return output_; }
    const auto& project_id() const { return project_id_; }
    const auto& entry() const { return entry_; }
    const auto& required_capabilities() const { return required_capabilities_; }
    constexpr auto revision() const { return policy_revision; }
    constexpr bool admitted() const { return false; }
};
PublishPlan plan_contract(const Request&, const project::Manifest&, std::string_view host);
// Bounded read-only contract validation before analysis or output creation.
PublishPlan preflight(const Request&, std::string_view host);
}
