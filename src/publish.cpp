#include "void/publish.hpp"
#include "void/path.hpp"
#include "frontend/project_digest.hpp"
#include "frontend/project_files.hpp"
#include <set>
#include <stdexcept>

namespace vd::publish {
namespace {
bool target_name(std::string_view value) {
    return value == "linux-x64" || value == "windows-x64" || value == "macos-arm64";
}
bool profile_name(std::string_view value) {
    return value == "release" || value == "performance" || value == "protected";
}
}
Request parse_arguments(std::span<const std::string_view> args) {
    Request request;
    std::set<std::string_view> seen;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto key = args[i];
        if (key != "--project" && key != "--entry" && key != "--target" && key != "--profile" && key != "-o")
            throw std::invalid_argument("Unsupported publish argument: " + std::string(key) + ". Use --project DIR; publish accepts no source operand or native consent flag.");
        if (!seen.insert(key).second) throw std::invalid_argument("Duplicate publish argument: " + std::string(key));
        if (++i == args.size() || args[i].empty() || args[i].starts_with('-') || args[i].find('\0') != args[i].npos)
            throw std::invalid_argument("Expected one value after " + std::string(key));
        const auto value = args[i];
        if (key == "--project") request.project = utf8_path(value);
        else if (key == "--entry") {
            auto p = utf8_path(value);
            if (p.has_root_path() || value.find('\\') != value.npos || value.find(':') != value.npos || !value.ends_with(".void"))
                throw std::invalid_argument("Publish --entry requires an exact project-relative .void path.");
            for (const auto& part : p) if (part == "." || part == ".." || part.empty())
                throw std::invalid_argument("Publish --entry cannot contain dot or parent components.");
            if (path_utf8(p.lexically_normal()) != value) throw std::invalid_argument("Publish --entry must be normalized.");
            request.entry = value;
        } else if (key == "--target") {
            if (!target_name(value)) throw std::invalid_argument("Unsupported publish target: " + std::string(value));
            request.target = value;
        } else if (key == "--profile") {
            if (!profile_name(value)) throw std::invalid_argument("Unsupported publish profile: " + std::string(value));
            request.profile = value;
        } else request.output = utf8_path(value);
    }
    return request;
}
PublishPlan::PublishPlan(const Request& request, const project::Manifest& manifest, std::string_view host) {
    if (manifest.kind() != project::Kind::application) throw std::invalid_argument("Publish requires an application project; libraries are ineligible.");
    const auto& config = manifest.publishing();
    target_ = request.target.value_or(config.target.value_or(std::string(host)));
    profile_ = request.profile.value_or(config.default_profile.value_or("release"));
    project_id_ = manifest.project().id;
    entry_ = request.entry ? request.entry : manifest.entry();
    if (!target_name(target_)) throw std::invalid_argument("Unsupported publish target: " + target_);
    if (!profile_name(profile_)) throw std::invalid_argument("Unsupported publish profile: " + profile_);
    output_ = request.output.value_or(request.project / "dist" / manifest.application()->id / target_ / profile_);
    project::Digest digest; digest.bytes(manifest.source().text()); manifest_sha256_ = digest.finish();
    required_capabilities_ = {"admitted-project-snapshot", "native-target:" + target_, "private-diagnostics", "final-bundle-audit", "fresh-directory-transaction"};
    if (!manifest.dependencies().empty()) required_capabilities_.push_back("verified-dependency-lock");
    if (!manifest.resources().empty()) required_capabilities_.push_back("resource-snapshot");
    if (!manifest.native().empty() || manifest.native_requirement()) required_capabilities_.push_back("publish-native-authorization");
    if (profile_ == "performance") required_capabilities_.push_back("qualified-performance-policy");
    if (profile_ == "protected") required_capabilities_.push_back("qualified-protection-policy");
}
PublishPlan plan_contract(const Request& request, const project::Manifest& manifest, std::string_view host) {
    return PublishPlan(request, manifest, host);
}
void require_capabilities(const PublishPlan& plan) {
    throw std::runtime_error("Publish capability unavailable in Stage A for " + plan.target() + "/" + plan.profile() +
        "; no target is qualified and no output was created. Native approvals, dependency admission and final-binary gates require separate review.");
}
void preflight(const Request& request, std::string_view host) {
    // Reuse the compiler's bounded manifest reader and root alias admission.
    const auto root = project::inspect_creation_root(request.project);
    const auto path = root.path / "void.project.toml";
    auto parsed = project::parse_manifest(SourceFile(path_utf8(path), project::read_project_input(path, project::manifest_byte_limit)));
    if (!parsed.succeeded()) throw CompilationError(std::move(parsed.diagnostics));
    auto plan = plan_contract(request, *parsed.manifest, host);
    std::error_code ec;
    auto status = std::filesystem::symlink_status(plan.output(), ec);
    if (ec && ec != std::errc::no_such_file_or_directory) throw std::runtime_error("Cannot inspect publish output destination.");
    if (std::filesystem::exists(status)) throw std::invalid_argument("Publish output must be a fresh directory; replacement is unavailable.");
    require_capabilities(plan);
}
}
