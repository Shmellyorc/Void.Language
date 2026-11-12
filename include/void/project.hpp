#pragma once
#include "void/diagnostic.hpp"
#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vd::project {
inline constexpr std::size_t manifest_byte_limit = 256 * 1024;
inline constexpr std::size_t manifest_diagnostic_limit = 128;

// Core numbers remain decimal strings: SemVer has no machine-integer width.
class SemanticVersion {
    std::string text_ = "0.0.0";
    std::array<std::string, 3> core_ = {"0", "0", "0"};
    std::vector<std::string> prerelease_, build_;
    friend std::optional<SemanticVersion> parse_version(std::string_view);
public:
    const auto& text() const { return text_; }
    const auto& core() const { return core_; }
    const auto& prerelease() const { return prerelease_; }
    const auto& build() const { return build_; }
    std::strong_ordering compare_precedence(const SemanticVersion&) const noexcept;
    // Full version identity includes build metadata; precedence does not.
    bool operator==(const SemanticVersion& other) const { return text_ == other.text_; }
};
std::optional<SemanticVersion> parse_version(std::string_view);
bool valid_identity(std::string_view) noexcept;
bool valid_name(std::string_view);

struct VersionRequirement {
    struct Bound { SemanticVersion version; bool inclusive; };
    std::optional<SemanticVersion> exact;
    std::optional<Bound> lower, upper;
    bool accepts(const SemanticVersion&) const;
    std::string text() const;
};
std::optional<VersionRequirement> parse_version_requirement(std::string_view);
struct GitPin {
    std::string origin, revision;
    auto operator<=>(const GitPin&) const = default;
};
struct GitDependency { GitPin pin; std::string subdir; };
struct Dependency {
    std::string id, path;
    VersionRequirement version;
    std::optional<GitDependency> git = {};
};
inline constexpr std::size_t dependency_edge_limit = 1024;

enum class Kind { application, library };
enum class Optimization { none, O2 };
enum class DebugInfo { none, line, full };
struct Identity { std::string id, name; SemanticVersion version; };
struct Profile { Optimization optimization; DebugInfo debug_info; };
struct Sources {
    std::vector<std::string> roots = {"."};
    std::vector<std::string> exclude = {"tests", "examples", "vendor", "node_modules", "dist"};
};
struct Field { SourceSpan key, value; };
struct NativeLibrary { std::string descriptor, library, adapter; };
struct NativeRequirement {
    std::string alias, id;
    SemanticVersion version;
    unsigned abi = 0;
};

inline constexpr std::size_t native_catalog_byte_limit = 32 * 1024;
inline constexpr std::size_t native_catalog_file_limit = 24;
inline constexpr std::uint64_t native_catalog_payload_limit = 128 * 1024 * 1024;
inline constexpr std::size_t application_resource_count_limit = 32;
inline constexpr std::size_t application_resource_byte_limit = 16 * 1024 * 1024;
bool valid_application_resource(std::string_view path);
struct NativeArtifact {
    std::string name, sha256;
    std::uint64_t size = 0;
};
struct NativeCatalog {
    std::string id, target, revision, library, adapter;
    SemanticVersion version;
    unsigned abi = 0;
    NativeArtifact descriptor;
    std::vector<NativeArtifact> runtime, link, notices;
};
// Parse owned, bounded artifact claims. The result carries no approval or
// authority to open any file. Invalid catalog bytes throw std::runtime_error.
NativeCatalog parse_native_catalog(std::string_view bytes);

// Schema-validated metadata only. This is not filesystem admission, a selected
// source graph, a Runtime identity, an install receipt or native authority.
class Manifest {
    std::shared_ptr<const SourceFile> source_;
    Identity project_;
    Kind kind_ = Kind::application;
    std::optional<Identity> application_, package_;
    std::vector<std::string> resources_;
    std::optional<std::string> entry_, package_entry_;
    std::map<std::string, Dependency> dependencies_;
    std::map<std::string, NativeLibrary> native_;
    std::optional<NativeRequirement> native_requirement_;
    Sources sources_;
    Profile debug_ = {Optimization::none, DebugInfo::none};
    Profile release_ = {Optimization::O2, DebugInfo::none};
    std::map<std::string, Field> fields_;
    Manifest() = default;
    friend class ManifestParser;
public:
    const SourceFile& source() const { return *source_; }
    const Identity& project() const { return project_; }
    Kind kind() const { return kind_; }
    const auto& application() const { return application_; }
    const auto& resources() const { return resources_; }
    const auto& package() const { return package_; }
    const auto& entry() const { return entry_; }
    const auto& package_entry() const { return package_entry_; }
    const auto& dependencies() const { return dependencies_; }
    const auto& native() const { return native_; }
    const auto& native_requirement() const { return native_requirement_; }
    const Sources& sources() const { return sources_; }
    const Profile& debug() const { return debug_; }
    const Profile& release() const { return release_; }
    const auto& fields() const { return fields_; }
};
struct ParseResult {
    std::shared_ptr<const Manifest> manifest;
    std::vector<Diagnostic> diagnostics;
    bool succeeded() const { return bool(manifest); }
};
// Pure, bounded, owning parse of an already UTF-8-validated SourceFile snapshot.
// Does not open paths, discover entries, query the environment or execute code.
ParseResult parse_manifest(const SourceFile&);
enum class ProfileName { debug, release };
enum class ProfileDebugSupport { unavailable, available };
struct EffectiveProfile {
    ProfileName name=ProfileName::release;
    bool optimize=true;
    DebugInfo debug_info=DebugInfo::none; // Requested setting, never a claim of emitted symbols.
    std::vector<Diagnostic> diagnostics;
    std::string cache_key;
};
// Usage conflicts throw invalid_argument. Only the selected profile can warn.
EffectiveProfile resolve_profile(const Manifest&,std::optional<ProfileName>,bool no_opt,bool cache,
                                 ProfileDebugSupport = ProfileDebugSupport::unavailable);
}
