#include "void/project_resolution.hpp"
#include "project_files.hpp"
#include "project_git.hpp"
#include "void/path.hpp"
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/SHA256.h>
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <set>

namespace vd::project {
namespace {
namespace fs = std::filesystem;
using Code = DiagnosticCode;
constexpr std::string_view manifest_name = "void.project.toml";
constexpr std::size_t depth_limit = 64, file_limit = 8192;
constexpr std::size_t visit_limit = 131072, package_bytes = 64 * 1024 * 1024;
constexpr std::size_t total_bytes = 256 * 1024 * 1024;

class Digest {
    llvm::SHA256 hash;
public:
    void bytes(std::string_view value) { hash.update(llvm::StringRef(value.data(), value.size())); }
    void field(std::string_view value) {
        const auto length = static_cast<std::uint64_t>(value.size());
        char encoded[8];
        for (unsigned i = 0; i < 8; ++i) encoded[i] = static_cast<char>((length >> (56 - i * 8)) & 255);
        bytes({encoded, sizeof encoded});
        bytes(value);
    }
    std::string finish() {
        constexpr std::string_view hex = "0123456789abcdef";
        std::string text;
        for (auto byte : hash.final()) {
            text += hex[byte >> 4];
            text += hex[byte & 15];
        }
        return text;
    }
};

std::string sha256(std::string_view bytes) {
    Digest digest;
    digest.bytes(bytes);
    return digest.finish();
}

// Package IDs, versions and portable path components cannot contain TOML quotes,
// backslashes, control bytes or newlines. Aliases are bare compiler identifiers.
void string_field(std::string& text, std::string_view key, std::string_view value) {
    text.append(key).append(" = \"").append(value).append("\"\n");
}
using Edges = DependencyEdges;

struct Trace {
    const Manifest& owner;
    const std::string& alias;
    const Dependency& dependency;
    SourceSpan span(std::string_view field) const {
        if (field == "path" && dependency.git) field = "git";
        return owner.fields().at("dependencies." + alias + "." + std::string(field)).value;
    }
};

struct Package {
    std::string path, version, manifest_digest, tree_digest;
    std::optional<GitDependency> git;
    Edges edges;
    PackageSources sources;
    std::size_t height = 0;
    std::string first_chain;
    SourceSpan first_span;
};

struct Snapshot {
    std::string manifest, lock;
};

class Resolver {
    const fs::path& root;
    const Manifest* effective_root;
    bool retain_sources;
    const std::vector<GitPin>* fetch;
    using Directory = std::map<std::string, std::set<std::string>>;
    std::map<fs::path, Directory> directories;
    std::map<std::string, Package> packages;
    std::map<std::string, std::shared_ptr<const Manifest>> manifests;
    std::set<std::string> active;
    std::vector<Trace> trace;
    std::size_t visited = 0, hashed_bytes = 0, edge_count = 0;
    std::size_t snapshot_bytes = 0;
    std::set<GitPin> authorized, pending;
    std::map<GitPin, GitSnapshot> snapshots;
    std::map<fs::path, const GitPin*> snapshot_roots;

    std::string chain() const {
        std::string result;
        for (const auto& step : trace) {
            if (!result.empty()) result += " -> ";
            result += step.alias + " (" + step.dependency.id + ")";
        }
        return result;
    }
    [[noreturn]] void fail(Code code, std::string message, std::string_view field = "path",
                           const Package* earlier = nullptr) const {
        Diagnostic error(code);
        error.primary = trace.empty()
            ? SourceSpan{std::make_shared<const SourceFile>(path_utf8(root / manifest_name), ""), {}, 0, 0}
            : trace.back().span(field);
        error.description = std::move(message);
        if (!trace.empty()) error.hint = "Dependency chain: " + chain();
        if (earlier) error.related.push_back({earlier->first_span, "First selection via " + earlier->first_chain});
        throw CompilationError({std::move(error)});
    }
    const GitPin* owner(const fs::path& path) const {
        if (snapshot_roots.empty()) return nullptr;
        const auto relative = path.lexically_relative(root);
        auto it = relative.begin();
        if (it == relative.end() || *it != ".void" || ++it == relative.end() || *it != git_store_name)
            return nullptr;
        auto tree = root / ".void" / git_store_name;
        for (unsigned i = 0; i < 3; ++i) {
            if (++it == relative.end()) return nullptr;
            tree /= *it;
        }
        const auto found = snapshot_roots.find(tree);
        return found == snapshot_roots.end() ? nullptr : found->second;
    }
    static std::string relative_to(const fs::path& path, const fs::path& base) {
        return path == base ? "" : path_utf8(path.lexically_relative(base));
    }
    std::string describe(const std::string& locator) const {
        const auto path = root / utf8_path(locator);
        if (const auto* pin = owner(path)) {
            const auto subdir = relative_to(path, snapshots.at(*pin).tree);
            return pin->origin + " at " + pin->revision + (subdir.empty() ? "" : " subdir " + subdir);
        }
        return locator.empty() ? "." : locator;
    }
    std::string input(const fs::path& path, std::size_t limit) const {
        if (const auto* pin = owner(path)) {
            const auto& snapshot = snapshots.at(*pin);
            const auto& bytes = snapshot.files.at(relative_to(path, snapshot.tree));
            if (bytes.size() > limit) fail(Code::limit, "Git dependency input byte limit exceeded.");
            return bytes;
        }
        return read_project_input(path, limit);
    }
    bool is_directory(const fs::path& path) const {
        if (const auto* pin = owner(path)) {
            const auto& snapshot = snapshots.at(*pin);
            return snapshot.directories.contains(relative_to(path, snapshot.tree));
        }
        return fs::is_directory(fs::symlink_status(path)) && !project_path_alias(path);
    }
    void require_path(const fs::path& path, bool directory) const {
        if (const auto* pin = owner(path)) {
            const auto& snapshot = snapshots.at(*pin);
            const auto relative = relative_to(path, snapshot.tree);
            if (!(directory ? snapshot.directories.contains(relative) : snapshot.files.contains(relative)))
                fail(Code::project_boundary, "Dependency path is outside the verified Git snapshot: " + relative);
            return;
        }
        const auto type = fs::symlink_status(path).type();
        const auto expected = directory ? fs::file_type::directory : fs::file_type::regular;
        if (type != expected || project_path_alias(path) || fs::canonical(path) != path)
            fail(Code::project_boundary, "Dependency path must be exact and free of filesystem aliases: " + path_utf8(path));
    }
    const Directory& directory(const fs::path& path) {
        if (auto found = directories.find(path); found != directories.end()) return found->second;
        require_path(path, true);
        Directory names;
        auto add = [&](std::string leaf) {
            auto& exact_names = names[fold_project_name(leaf)];
            if (exact_names.insert(std::move(leaf)).second && ++visited > visit_limit)
                fail(Code::limit, "Dependency directory-entry work limit exceeded.");
        };
        if (const auto* pin = owner(path)) {
            const auto& snapshot = snapshots.at(*pin);
            const auto relative = relative_to(path, snapshot.tree);
            const auto prefix = relative.empty() ? "" : relative + "/";
            auto child = [&](std::string_view name) {
                if (name == prefix) return;
                auto suffix = name.substr(prefix.size());
                add(std::string(suffix.substr(0, suffix.find('/'))));
            };
            for (auto entry = snapshot.files.lower_bound(prefix); entry != snapshot.files.end() && entry->first.starts_with(prefix); ++entry)
                child(entry->first);
            for (auto entry = snapshot.directories.lower_bound(prefix); entry != snapshot.directories.end() && entry->starts_with(prefix); ++entry)
                child(*entry);
        } else for (const auto& child : fs::directory_iterator(path)) {
            add(path_utf8(child.path().filename()));
        }
        return directories.emplace(path, std::move(names)).first->second;
    }
    bool exact(const fs::path& path, std::string_view name) {
        const auto& entries = directory(path);
        const auto found = entries.find(fold_project_name(name));
        if (found == entries.end()) return false;
        if (found->second.size() != 1 || *found->second.begin() != name)
            fail(Code::project_path, "Dependency path has wrong case or a case collision: " + path_utf8(path / name));
        return true;
    }
    bool boundary(const fs::path& path) {
        if (!exact(path, manifest_name)) return false;
        const auto manifest = path / manifest_name;
        require_path(manifest, false);
        if (!owner(path) && fs::hard_link_count(manifest) != 1)
            fail(Code::project_boundary, "Dependency manifest is a hard-link alias: " + path_utf8(manifest));
        return true;
    }
    const Manifest& read_manifest(const std::string& locator) {
        if (locator.empty() && effective_root) return *effective_root;
        if (auto found = manifests.find(locator); found != manifests.end()) return *found->second;
        const auto path = locator.empty() ? root : root / utf8_path(locator);
        if (!boundary(path)) fail(Code::project_boundary, "Missing exact void.project.toml at " + path_utf8(path));
        const auto manifest = path / manifest_name;
        auto result = parse_manifest(SourceFile(path_utf8(manifest), input(manifest, manifest_byte_limit)));
        if (!result.succeeded()) {
            if (!trace.empty())
                for (auto& diagnostic : result.diagnostics)
                    diagnostic.related.push_back({trace.back().span("path"), "Required via " + chain()});
            throw CompilationError(std::move(result.diagnostics));
        }
        return *manifests.emplace(locator, std::move(result.manifest)).first->second;
    }
    std::string locate(fs::path path, const fs::path& enclosure, const std::string& requested) {
        const auto relative = utf8_path(requested);
        for (auto component = relative.begin(); component != relative.end(); ++component) {
            if (*component == "..") {
                if (path == enclosure) fail(Code::project_boundary, "Dependency path escapes the selected project enclosure.");
                path = path.parent_path();
                continue;
            }
            const auto leaf = path_utf8(*component);
            if (!exact(path, leaf)) fail(Code::project_path, "Missing dependency directory: " + path_utf8(path / *component));
            path /= *component;
            require_path(path, true);
            if (std::next(component) != relative.end() && boundary(path))
                fail(Code::project_boundary, "Dependency path crosses nested project boundary: " + path_utf8(path));
        }
        if (path == root) return {};
        const auto locator = relative_to(path, root);
        if (!valid_git_subdir(relative_to(path, enclosure))) fail(Code::limit, "Canonical dependency path exceeds its work limits.");
        return locator;
    }

    std::string locate(const std::string& declaring, const Dependency& requirement) {
        if (!requirement.git) {
            auto path = declaring.empty() ? root : root / utf8_path(declaring);
            const auto* pin = owner(path);
            return locate(path, pin ? snapshots.at(*pin).tree : root, requirement.path);
        }
        const auto& selection = *requirement.git;
        auto found = snapshots.find(selection.pin);
        if (found == snapshots.end()) {
            try {
                auto snapshot = read_git_snapshot(git_snapshot_path(root, selection.pin), selection.pin);
                if (!snapshot) {
                    if (!fetch) fail(Code::project_path, "Missing Git dependency snapshot for " + selection.pin.origin +
                        " at " + selection.pin.revision + ". Run voidc deps fetch --project <directory>.");
                    if (!authorized.contains(selection.pin))
                        fail(Code::project_reserved, "Transitive Git pin requires explicit acquisition authorization: " +
                            selection.pin.origin + " " + selection.pin.revision + ". Pass --allow-git <origin> <rev>.");
                    snapshot = acquire_git_snapshot(root, selection.pin);
                    pending.insert(selection.pin);
                }
                auto retained = snapshot->bytes + snapshot->proof.size();
                for (const auto& [path, bytes] : snapshot->files) retained += path.size();
                for (const auto& path : snapshot->directories) retained += path.size();
                if (retained > total_bytes - snapshot_bytes)
                    fail(Code::limit, "Git dependency snapshot byte limit exceeded.");
                snapshot_bytes += retained;
                found = snapshots.emplace(selection.pin, std::move(*snapshot)).first;
                snapshot_roots.emplace(found->second.tree, &found->first);
            } catch (const std::runtime_error& failure) {
                fail(Code::project_path, "Git dependency " + selection.pin.origin + " at " + selection.pin.revision +
                    ": " + failure.what());
            }
        }
        return locate(found->second.tree, found->second.tree, selection.subdir);
    }

    // false denotes a regular file, true denotes an exact nested project boundary.
    using Tree = std::map<std::string, bool>;
    void inventory(const fs::path& base, const std::string& prefix, std::size_t depth, Tree& tree) {
        const auto path = prefix.empty() ? base : base / utf8_path(prefix);
        if (depth > depth_limit) fail(Code::limit, "Dependency tree depth limit exceeded.");
        for (const auto& [folded, names] : directory(path)) {
            if (prefix.empty() && (dependency_admin_directory(folded) || folded == lock_name)) continue;
            if (names.size() != 1)
                fail(Code::project_path, "Case collision in dependency tree at " + path_utf8(path));
            const auto& leaf = *names.begin();
            if (!portable_path_component(leaf))
                fail(Code::project_path, "Nonportable filename in dependency tree: " + path_utf8(path / utf8_path(leaf)));
            auto relative = prefix.empty() ? leaf : prefix + "/" + leaf;
            if (relative.size() > 4096) fail(Code::limit, "Dependency tree path byte limit exceeded.");
            const auto child = path / utf8_path(leaf);
            const auto directory = is_directory(child);
            if (directory) {
                if (!boundary(child)) {
                    inventory(base, relative, depth + 1, tree);
                    continue;
                }
            } else require_path(child, false);
            if (tree.size() == file_limit) fail(Code::limit, "Dependency tree file and boundary limit exceeded.");
            tree.emplace(std::move(relative), directory);
        }
    }
    std::string tree_digest(const std::string& locator, const Manifest& manifest, PackageSources& sources) {
        const auto path = root / utf8_path(locator);
        Tree tree;
        inventory(path, "", 0, tree);
        const auto entry = tree.find(*manifest.package_entry());
        if (entry == tree.end() || entry->second)
            fail(Code::project_entry, "Package entry is unavailable or crosses a nested project boundary: " + *manifest.package_entry());
        Digest digest;
        digest.field("void-package-tree-v1");
        std::size_t bytes = 0;
        for (const auto& [name, is_boundary] : tree) {
            digest.field(is_boundary ? "boundary" : "file");
            digest.field(name);
            if (is_boundary) continue;
            auto contents = input(path / utf8_path(name), std::min(package_bytes - bytes, total_bytes - hashed_bytes));
            if (name == manifest_name && contents != manifest.source().text())
                fail(Code::project_changed, "Dependency manifest changed during resolution; retry.");
            bytes += contents.size();
            hashed_bytes += contents.size();
            digest.field(contents);
            if (retain_sources && name.ends_with(".void")) sources.emplace(name, std::move(contents));
        }
        if (!tree.contains(std::string(manifest_name)))
            fail(Code::project_changed, "Dependency manifest disappeared during resolution; retry.");
        return digest.finish();
    }

    std::size_t resolve_edges(const Manifest& manifest, const std::string& declaring, Edges& edges, std::size_t depth) {
        std::size_t height = 0;
        for (const auto& [alias, requirement] : manifest.dependencies()) {
            trace.push_back({manifest, alias, requirement});
            if (++edge_count > dependency_edge_limit) fail(Code::limit, "Dependency graph edge limit exceeded.");
            if (depth == depth_limit) fail(Code::limit, "Dependency graph depth limit exceeded.");
            const auto locator = locate(declaring, requirement);
            if (!locator.empty() && !manifests.contains(locator) &&
                manifests.size() >= dependency_package_limit + (effective_root ? 0 : 1))
                fail(Code::limit, "Dependency graph package limit exceeded.");
            const auto& selected = read_manifest(locator);
            if (!selected.package())
                fail(Code::project_identity, "Dependency must select a library project.");
            const auto& package = *selected.package();
            if (package.id != requirement.id)
                fail(Code::project_identity, "Wrong dependency package ID. Expected '" + requirement.id + "', found '" + package.id + "'.", "id");
            const auto prior = packages.find(package.id);
            const auto* earlier = prior == packages.end() ? nullptr : &prior->second;
            if (earlier && earlier->path != locator)
                fail(Code::project_duplicate, "Dependency origin conflict for '" + package.id + "': '" + describe(earlier->path) +
                    "' and '" + describe(locator) + "'.", "path", earlier);
            if (!requirement.version.accepts(package.version))
                fail(Code::project_version, "Dependency version conflict for '" + package.id + "': " + package.version.text() +
                    " does not satisfy " + requirement.version.text() + ".", "version", earlier);
            if (active.contains(locator))
                fail(Code::project_boundary, "Dependency cycle closes at '" + describe(locator) + "'.");
            if (!selected.package_entry())
                fail(Code::project_entry, "Consumed library requires package.entry.");
            auto [position, fresh] = packages.try_emplace(package.id);
            auto& node = position->second;
            if (fresh) {
                node.path = locator;
                const auto physical = root / utf8_path(locator);
                if (const auto* pin = owner(physical))
                    node.git = GitDependency{*pin, relative_to(physical, snapshots.at(*pin).tree)};
                node.version = package.version.text();
                node.manifest_digest = sha256(selected.source().text());
                node.tree_digest = tree_digest(locator, selected, node.sources);
                node.first_chain = chain();
                node.first_span = trace.back().span("path");
                active.insert(locator);
                node.height = resolve_edges(selected, locator, node.edges, depth + 1);
                active.erase(locator);
            }
            if (node.height >= depth_limit - depth) fail(Code::limit, "Dependency graph depth limit exceeded.");
            height = std::max(height, node.height + 1);
            edges.emplace(alias, package.id);
            trace.pop_back();
        }
        return height;
    }

public:
    explicit Resolver(const fs::path& selected_root, const Manifest* manifest = nullptr, bool keep = false,
                      const std::vector<GitPin>* authorized_pins = nullptr)
        : root(selected_root), effective_root(manifest), retain_sources(keep), fetch(authorized_pins) {}

    std::size_t publish(const CreationRoot& admitted) const {
        for (const auto& pin : pending) publish_git_snapshot(admitted, pin, snapshots.at(pin));
        return snapshots.size();
    }

    auto take_packages() {
        std::map<std::string, LockedPackage, std::less<>> result;
        for (auto& [id, package] : packages) {
            auto entry = *manifests.at(package.path)->package_entry();
            result.emplace(id, LockedPackage{std::move(package.path), std::move(entry),
                std::move(package.edges), std::move(package.sources)});
        }
        return result;
    }

    Snapshot capture() {
        const auto& manifest = read_manifest("");
        if (fetch) {
            authorized.insert(fetch->begin(), fetch->end());
            for (const auto& [alias, dependency] : manifest.dependencies())
                if (dependency.git) authorized.insert(dependency.git->pin);
        }
        Edges edges;
        active.insert("");
        resolve_edges(manifest, "", edges, 0);
        const std::string_view kind = manifest.kind() == Kind::application ? "application" : "library";
        Digest inputs;
        const bool git_graph = !snapshots.empty();
        inputs.field(git_graph ? "void-dependency-inputs-v2" : "void-dependency-inputs-v1");
        inputs.field(manifest.project().id);
        inputs.field(kind);
        for (const auto& [alias, dependency] : manifest.dependencies()) {
            inputs.field(alias);
            inputs.field(dependency.id);
            const auto& package = packages.at(dependency.id);
            if (git_graph) inputs.field(package.git ? "git" : "path");
            if (package.git) {
                inputs.field(package.git->pin.origin);
                inputs.field(package.git->pin.revision);
                inputs.field(package.git->subdir);
            } else inputs.field(package.path);
            inputs.field(dependency.version.text());
        }
        std::string lock = git_graph ? "format = 2\nresolver = 2\n\n[root]\n" : "format = 1\nresolver = 1\n\n[root]\n";
        string_field(lock, "id", manifest.project().id);
        string_field(lock, "kind", kind);
        string_field(lock, "inputs", inputs.finish());
        lock += "\n[root.dependencies]\n";
        for (const auto& [alias, id] : edges) string_field(lock, alias, id);
        for (const auto& [id, package] : packages) {
            lock += "\n[[package]]\n";
            string_field(lock, "id", id);
            if (package.git) {
                string_field(lock, "git", package.git->pin.origin);
                string_field(lock, "rev", package.git->pin.revision);
                string_field(lock, "subdir", package.git->subdir);
            } else string_field(lock, "path", package.path);
            string_field(lock, "version", package.version);
            string_field(lock, "manifest", package.manifest_digest);
            string_field(lock, "tree", package.tree_digest);
            lock += "\n[package.dependencies]\n";
            for (const auto& [alias, target] : package.edges) string_field(lock, alias, target);
        }
        if (lock.size() > lock_byte_limit) fail(Code::limit, "Dependency lock byte limit exceeded.");
        return {manifest.source().text(), std::move(lock)};
    }
    bool has_dependencies() { return !read_manifest("").dependencies().empty(); }
};

void require_lock_shape(std::string_view bytes) {
    switch (dependency_lock_shape(bytes)) {
    case LockShape::unsupported: throw std::runtime_error("Unsupported void.lock format or resolver revision.");
    case LockShape::malformed: throw std::runtime_error("Malformed void.lock. Review and move it aside before resolving.");
    case LockShape::conflicting: throw std::runtime_error("Conflicting package IDs or paths in void.lock. Review and move it aside before resolving.");
    case LockShape::valid: break;
    }
}
[[noreturn]] void missing_lock() {
    throw std::runtime_error("Missing void.lock. Run voidc resolve --project <directory>.");
}
[[noreturn]] void stale_lock() {
    throw std::runtime_error("Stale or tampered void.lock. Review the inputs and run voidc resolve --project <directory> --refresh.");
}
}

std::shared_ptr<const LockedDependencies> verify_locked_dependencies(
    const fs::path& requested, const Manifest& manifest, bool retain_sources) {
    if (manifest.dependencies().empty()) return {};
    const auto root = inspect_creation_root(requested);
    const auto previous = read_dependency_lock(root.path);
    if (!previous) missing_lock();
    require_lock_shape(*previous);
    Resolver resolver(root.path, &manifest, retain_sources);
    if (resolver.capture().lock != *previous) stale_lock();
    if (inspect_creation_root(root.path).identity != root.identity || read_dependency_lock(root.path) != previous)
        throw std::runtime_error("Project root or dependency lock changed during verification; retry.");
    auto plan = std::shared_ptr<LockedDependencies>(new LockedDependencies);
    plan->identity_ = sha256(*previous);
    for (const auto& [alias, dependency] : manifest.dependencies()) plan->direct_.emplace(alias, dependency.id);
    plan->packages_ = resolver.take_packages();
    return plan;
}

ResolveResult resolve_dependencies(const fs::path& requested, ResolveMode mode) {
    const auto root = inspect_creation_root(requested);
    Resolver resolver(root.path);
    const bool dependencies = resolver.has_dependencies();
    const auto previous = read_dependency_lock(root.path);
    if (!previous && !dependencies) return ResolveResult::no_dependencies;
    if (!previous && mode == ResolveMode::locked) missing_lock();
    if (previous) require_lock_shape(*previous);
    const auto captured = resolver.capture();
    auto recheck = [&] {
        if (inspect_creation_root(root.path).identity != root.identity)
            throw std::runtime_error("Project root changed during dependency resolution; retry.");
        const auto fresh = Resolver(root.path).capture();
        if (fresh.manifest != captured.manifest || fresh.lock != captured.lock)
            throw std::runtime_error("Dependency inputs changed during resolution; retry.");
    };
    if (previous && *previous == captured.lock) {
        recheck();
        if (read_dependency_lock(root.path) != previous)
            throw std::runtime_error("Dependency lock changed during verification; retry.");
        return ResolveResult::verified;
    }
    if (previous && mode != ResolveMode::refresh) stale_lock();
    if (!publish_dependency_lock(root, previous, captured.lock, recheck)) return ResolveResult::verified;
    return previous ? ResolveResult::refreshed : ResolveResult::created;
}

std::size_t fetch_dependencies(const fs::path& requested, const std::vector<GitPin>& allowed) {
    if (allowed.size() > dependency_edge_limit ||
        std::any_of(allowed.begin(), allowed.end(), [](const auto& pin) { return !valid_git_pin(pin); }))
        throw std::invalid_argument("Invalid Git acquisition authorization.");
    const auto root = inspect_creation_root(requested);
    Resolver resolver(root.path, nullptr, false, &allowed);
    const auto captured = resolver.capture();
    if (inspect_creation_root(root.path).identity != root.identity ||
        read_project_input(root.path / manifest_name, manifest_byte_limit) != captured.manifest)
        throw std::runtime_error("Project root or manifest changed during Git acquisition; retry.");
    return resolver.publish(root);
}
}
