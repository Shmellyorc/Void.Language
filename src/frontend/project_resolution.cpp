#include "void/project_resolution.hpp"
#include "project_files.hpp"
#include "project_git.hpp"
#include "project_native.hpp"
#include "void/path.hpp"
#include "void/frontend.hpp"
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

void append_native_lock(std::string& lock, const NativeCatalog& plan) {
    auto identity = [](const NativeArtifact& file) { return std::to_string(file.size) + ':' + file.sha256; };
    lock += "\n[native]\n";
    string_field(lock, "id", plan.id);
    string_field(lock, "version", plan.version.text());
    string_field(lock, "target", plan.target);
    string_field(lock, "revision", plan.revision);
    lock += "abi = " + std::to_string(plan.abi) + '\n';
    string_field(lock, "descriptor", plan.descriptor.name);
    string_field(lock, "descriptor-identity", identity(plan.descriptor));
    string_field(lock, "library", plan.library);
    string_field(lock, "adapter", plan.adapter);
    for (const auto& [role, files] : {std::pair{"runtime", &plan.runtime}, {"link", &plan.link}, {"notices", &plan.notices}}) {
        lock += "\n[native." + std::string(role) + "]\n";
        for (const auto& file : *files) string_field(lock, '"' + file.name + '"', identity(file));
    }
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
    Edges direct;
};

[[noreturn]] void native_selection_failed(const Manifest& manifest, const NativeRequirement& requirement,
                                           std::string_view reason) {
    Diagnostic error(Code::project_path);
    error.primary = manifest.fields().at("native-requirements." + requirement.alias + ".id").value;
    error.description = reason;
    error.hint = "Select the approved local catalog matching this requirement and host target.";
    throw CompilationError({std::move(error)});
}

constexpr std::string_view view_receipt=".void-package";
ProjectFiles package_view(const GitSnapshot& snapshot,const std::string& subdir) {
    ProjectFiles files;
    const auto prefix=subdir.empty()?std::string{}:subdir+"/";
    for(const auto& directory:snapshot.directories)if(directory.starts_with(prefix) && directory!=subdir)
        files.emplace(directory.substr(prefix.size())+"/","");
    for(const auto& [path,bytes]:snapshot.files)if(path.starts_with(prefix))files.emplace(path.substr(prefix.size()),bytes);
    if(files.contains(std::string(view_receipt)))throw std::runtime_error("Package source collides with reserved materialization receipt.");
    files.emplace(view_receipt,snapshot.proof);
    return files;
}
bool view_present(const fs::path& root,const std::string& alias) {
    if(!fs::exists(root/"libs") && !project_path_alias(root/"libs"))return false;
    inspect_creation_root(root/"libs");
    bool present=false;
    for(const auto& child:fs::directory_iterator(root/"libs"))if(fold_project_name(path_utf8(child.path().filename()))==fold_project_name(alias)) {
        if(path_utf8(child.path().filename())!=alias || present)throw std::runtime_error("Installed package path case collision: libs/"+alias);
        present=true;
    }
    return present;
}

class Resolver {
    const fs::path& root;
    const Manifest* effective_root;
    bool retain_sources;
    bool inspect_views;
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
    std::optional<SelectedNative> native;
    std::map<std::string,std::string> managed_locators;

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
                      const std::vector<GitPin>* authorized_pins = nullptr,bool views=true)
        : root(selected_root), effective_root(manifest), retain_sources(keep), inspect_views(views), fetch(authorized_pins) {}

    std::size_t publish(const CreationRoot& admitted) const {
        for (const auto& pin : pending) publish_git_snapshot(admitted, pin, snapshots.at(pin));
        return snapshots.size();
    }

    auto take_packages() {
        std::map<std::string, LockedPackage, std::less<>> result;
        for (auto& [id, package] : packages) {
            auto entry = *manifests.at(package.path)->package_entry();
            auto locator=managed_locators.contains(id)?managed_locators.at(id):std::move(package.path);
            result.emplace(id, LockedPackage{std::move(locator), std::move(entry),
                std::move(package.edges), std::move(package.sources)});
        }
        return result;
    }

    Snapshot capture() {
        const auto& manifest = read_manifest("");
        if (!fetch && manifest.native_requirement()) {
            const auto& requirement = *manifest.native_requirement();
            try {
                native = select_managed_native(requirement);
            } catch (const std::invalid_argument& error) {
                native_selection_failed(manifest, requirement, error.what());
            } catch (const std::runtime_error& error) {
                native_selection_failed(manifest, requirement, error.what());
            }
        }
        if (fetch) {
            authorized.insert(fetch->begin(), fetch->end());
            for (const auto& [alias, dependency] : manifest.dependencies())
                if (dependency.git) authorized.insert(dependency.git->pin);
        }
        Edges edges;
        active.insert("");
        resolve_edges(manifest, "", edges, 0);
        // Only declared direct Git views are inspected. Cached snapshots remain
        // authoritative, including for packages without a materialized view.
        for(const auto& [alias,dependency]:manifest.dependencies())if(!fetch && inspect_views && dependency.git && view_present(root,alias)) {
            const auto& snapshot=snapshots.at(dependency.git->pin);
            if(read_package_directory(root/"libs"/utf8_path(alias))!=package_view(snapshot,dependency.git->subdir))
                fail(Code::project_changed,"Modified or unowned installed Git package: libs/"+alias+". Restore the exact locked source; tidy never overwrites edits.");
            const auto base=root/"libs"/utf8_path(alias);
            const auto cached=snapshot.tree/utf8_path(dependency.git->subdir);
            for(const auto& [id,package]:packages) {
                auto relative=(root/utf8_path(package.path)).lexically_relative(cached);
                if(relative.empty() || *relative.begin()=="..")continue;
                managed_locators.try_emplace(id,path_utf8((base/relative).lexically_normal().lexically_relative(root)));
            }
        }

        const std::string_view kind = manifest.kind() == Kind::application ? "application" : "library";
        Digest inputs;
        const bool git_graph = !snapshots.empty();
        inputs.field(native ? "void-dependency-inputs-v3" :
            git_graph ? "void-dependency-inputs-v2" : "void-dependency-inputs-v1");
        inputs.field(manifest.project().id);
        inputs.field(kind);
        for (const auto& [alias, dependency] : manifest.dependencies()) {
            inputs.field(alias);
            inputs.field(dependency.id);
            const auto& package = packages.at(dependency.id);
            if (git_graph || native) inputs.field(package.git ? "git" : "path");
            if (package.git) {
                inputs.field(package.git->pin.origin);
                inputs.field(package.git->pin.revision);
                inputs.field(package.git->subdir);
            } else inputs.field(package.path);
            inputs.field(dependency.version.text());
        }
        if (native) {
            const auto& required = *manifest.native_requirement();
            inputs.field(required.alias);
            inputs.field(required.id);
            inputs.field(required.version.text());
            inputs.field(std::to_string(required.abi));
        }
        std::string lock = native ? "format = 3\nresolver = 3\n\n[root]\n" :
            git_graph ? "format = 2\nresolver = 2\n\n[root]\n" : "format = 1\nresolver = 1\n\n[root]\n";
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
        if (native) append_native_lock(lock, native->artifacts);
        if (lock.size() > lock_byte_limit) fail(Code::limit, "Dependency lock byte limit exceeded.");
        return {manifest.source().text(), std::move(lock), std::move(edges)};
    }
    bool has_dependencies() {
        const auto& manifest = read_manifest("");
        return !manifest.dependencies().empty() || manifest.native_requirement().has_value();
    }
    std::optional<SelectedNative> take_native() { return std::move(native); }

    DependencyExplanation explanation(const Snapshot& captured, const std::optional<std::string>& lock) {
        const auto& manifest = read_manifest("");
        DependencyExplanation report;
        report.project_id = manifest.project().id;
        if (lock) report.lock_sha256 = sha256(*lock);
        auto add_edges = [&](bool direct, const std::string& parent, const Manifest& owner, const Edges& edges) {
            for (const auto& [alias, target] : edges) {
                const auto& declaration = owner.dependencies().at(alias);
                report.relationships.push_back({direct, parent, alias, target, declaration.version.text(),
                    declaration.path, declaration.git});
            }
        };
        add_edges(true, report.project_id, manifest, captured.direct);
        for (const auto& [id, package] : packages) {
            const auto& owner = *manifests.at(package.path);
            report.packages.push_back({id, package.version, *owner.package_entry(),
                package.git ? std::string{} : package.path, package.manifest_digest, package.tree_digest, package.git});
            add_edges(false, id, owner, package.edges);
        }
        return report;
    }
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

void recheck_capture(const CreationRoot& root, const Snapshot& captured) {
    if (inspect_creation_root(root.path).identity != root.identity)
        throw std::runtime_error("Project root changed during dependency resolution; retry.");
    const auto fresh = Resolver(root.path).capture();
    if (fresh.manifest != captured.manifest || fresh.lock != captured.lock)
        throw std::runtime_error("Dependency inputs changed during resolution; retry.");
}

Snapshot capture_locked_project(const CreationRoot& root, Resolver& resolver, const std::optional<std::string>& previous) {
    if (!previous && resolver.has_dependencies()) missing_lock();
    if (previous) require_lock_shape(*previous);
    auto captured = resolver.capture();
    if (previous && *previous != captured.lock) stale_lock();
    recheck_capture(root, captured);
    if (inspect_creation_root(root.path).identity != root.identity)
        throw std::runtime_error("Project root changed during dependency resolution; retry.");
    if (read_dependency_lock(root.path) != previous)
        throw std::runtime_error("Dependency lock changed during verification; retry.");
    return captured;
}
}

std::shared_ptr<const LockedDependencies> verify_locked_dependencies(
    const fs::path& requested, const Manifest& manifest, bool retain_sources) {
    require_no_package_edit(requested);
    if (manifest.dependencies().empty() && !manifest.native_requirement()) return {};
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
    if (auto native = resolver.take_native()) {
        plan->native_ = std::move(native->admission);
        plan->native_link_inputs_ = std::move(native->artifacts.link);
    }
    return plan;
}

ResolveResult resolve_dependencies(const fs::path& requested, ResolveMode mode) {
    const auto root = inspect_creation_root(requested);
    require_no_package_edit(root.path);
    Resolver resolver(root.path);
    const bool dependencies = resolver.has_dependencies();
    const auto previous = read_dependency_lock(root.path);
    if (!previous && !dependencies) return ResolveResult::no_dependencies;
    if (mode == ResolveMode::locked) {
        (void)capture_locked_project(root, resolver, previous);
        return ResolveResult::verified;
    }
    if (previous) require_lock_shape(*previous);
    const auto captured = resolver.capture();
    auto recheck = [&] { recheck_capture(root, captured); };
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

DependencyExplanation explain_dependencies(const fs::path& requested) {
    const auto root = inspect_creation_root(requested);
    require_no_package_edit(root.path);
    Resolver resolver(root.path);
    (void)resolver.has_dependencies(); // Admit the exact manifest before examining its lock.
    const auto previous = read_dependency_lock(root.path);
    const auto captured = capture_locked_project(root, resolver, previous);
    return resolver.explanation(captured, previous);
}

std::size_t fetch_dependencies(const fs::path& requested, const std::vector<GitPin>& allowed) {
    if (allowed.size() > dependency_edge_limit ||
        std::any_of(allowed.begin(), allowed.end(), [](const auto& pin) { return !valid_git_pin(pin); }))
        throw std::invalid_argument("Invalid Git acquisition authorization.");
    const auto root = inspect_creation_root(requested);
    require_no_package_edit(root.path);
    Resolver resolver(root.path, nullptr, false, &allowed);
    const auto captured = resolver.capture();
    if (inspect_creation_root(root.path).identity != root.identity ||
        read_project_input(root.path / manifest_name, manifest_byte_limit) != captured.manifest)
        throw std::runtime_error("Project root or manifest changed during Git acquisition; retry.");
    return resolver.publish(root);
}
namespace {
std::shared_ptr<const Manifest> maintenance_manifest(const fs::path& path,const std::optional<std::string>& bytes={}) {
    auto parsed=parse_manifest(SourceFile(path_utf8(path),bytes?*bytes:read_project_input(path,manifest_byte_limit)));
    if(!parsed.succeeded())throw CompilationError(std::move(parsed.diagnostics));
    return std::move(parsed.manifest);
}
fs::path add_source_path(const fs::path& root,const fs::path& requested) {
    auto path=requested.is_absolute()?requested:root/requested;
    auto current=path.root_path();
    for(const auto& part:path.relative_path()) {
        if(part==".")continue;
        if(part=="..")current=current.parent_path();else current/=part;
        inspect_creation_root(current); // Check before normalizing any parent step.
    }
    return inspect_creation_root(current).path;
}
ProjectFiles local_source_files(const fs::path& source) {
    // Source installation excludes only existing D3 administration, not user
    // sources. Nested manifests remain physical boundaries in the resolver.
    ProjectFiles files;
    std::size_t bytes=0,entries=0;
    std::function<void(const fs::path&,const std::string&,unsigned)> walk;
    walk=[&](const fs::path& directory,const std::string& prefix,unsigned depth) {
        if(depth>64)throw std::runtime_error("Local package depth limit exceeded.");
        inspect_creation_root(directory);
        std::set<std::string> names;
        for(const auto& child:fs::directory_iterator(directory)) {
            if(++entries>8192)throw std::runtime_error("Local package entry limit exceeded.");
            auto name=path_utf8(child.path().filename());
            if(!portable_path_component(name) || !names.insert(fold_project_name(name)).second || project_path_alias(child.path()))
                throw std::runtime_error("Local package has an invalid path, alias or case collision.");
            if(dependency_admin_directory(name) || (prefix.empty() && name==lock_name))continue;
            auto relative=prefix+name;
            if(child.is_directory()) {files.emplace(relative+"/","");walk(child.path(),relative+"/",depth+1);}
            else {
                auto content=read_project_input(child.path(),package_bytes);
                bytes+=content.size();if(bytes>package_bytes)throw std::runtime_error("Local package byte limit exceeded.");
                files.emplace(relative,std::move(content));
            }
        }
    };
    walk(source,"",0);return files;
}
void report_native_sources(const ProjectFiles& files,MaintenanceReport& report,const std::string& alias) {
    // Preparation belongs to the existing package. This is advice, never a
    // native requirement or approval inferred from an untrusted README.
    std::set<std::string> providers;
    for(const auto& [name,bytes]:files)if(name.ends_with(".void") && bytes.find("native")!=std::string::npos) {
        auto provider=inspect_native_provider(SourceFile(name,bytes));
        if(!provider.empty())providers.insert(std::move(provider));
    }
    for(const auto& provider:providers)report.advice.push_back(alias+": source declares native provider '"+provider+"'; read its README and existing preparation instructions. Select the matching approved D4 catalog explicitly before execution. No native binaries were prepared or approved.");
}
std::map<std::string,ProjectFiles> missing_git_views(const CreationRoot& root,const Manifest& manifest,MaintenanceReport& report,
    const std::optional<std::string>& previous,std::map<std::string,ProjectFiles>& replacements) {
    std::map<std::string,ProjectFiles> additions;
    auto old=previous?locked_direct_git_sources(*previous):std::map<std::string,GitDependency>{};
    for(const auto& [alias,dependency]:manifest.dependencies())if(dependency.git) {
        auto snapshot=read_git_snapshot(git_snapshot_path(root.path,dependency.git->pin),dependency.git->pin);
        if(!snapshot)throw std::runtime_error("Missing verified Git snapshot; run voidc deps fetch with the declared immutable pin.");
        auto files=package_view(*snapshot,dependency.git->subdir);
        report_native_sources(files,report,alias);
        if(!view_present(root.path,alias))additions.emplace(alias,std::move(files));
        else {
            auto actual=read_package_directory(root.path/"libs"/utf8_path(alias));
            if(actual==files)continue;
            auto prior=old.find(alias);
            if(prior==old.end() || (prior->second.pin==dependency.git->pin && prior->second.subdir==dependency.git->subdir))
                throw std::runtime_error("Modified or unowned installed Git package: libs/"+alias);
            auto original=read_git_snapshot(git_snapshot_path(root.path,prior->second.pin),prior->second.pin);
            if(!original || actual!=package_view(*original,prior->second.subdir))
                throw std::runtime_error("Modified previous installed Git package: libs/"+alias);
            replacements.emplace(alias,std::move(actual));additions.emplace(alias,std::move(files));
        }
    }
    return additions;
}
void import_advice(const fs::path& root,const Manifest& manifest,MaintenanceReport& report) {
    auto used=inspect_project_imports(root,manifest.source().text());
    for(const auto& [alias,dependency]:manifest.dependencies())if(!used.contains(alias))
        report.advice.push_back("Unused direct dependency '"+alias+"' retained; import inspection cannot establish that deletion is safe.");
    if(manifest.native_requirement())report.advice.push_back("Native requirement remains separate from source installation; existing D4 catalog selection and --allow-native are required for execution.");
}
}
MaintenanceReport add_dependency(const fs::path& requested,const AddOptions& options) {
    if(!valid_editor_identifier(options.alias) || options.alias.size()>128 || bool(options.path)==bool(options.git))
        throw std::invalid_argument("add requires a valid alias and exactly one explicit --path or --git with --rev.");
    if(options.git && (!valid_git_pin(options.git->pin) || !valid_git_subdir(options.git->subdir)))
        throw std::invalid_argument("add requires a canonical Git origin, full lowercase commit SHA and portable subdir.");
    const auto root=inspect_creation_root(requested);
    recover_package_edit(root);
    auto manifest=maintenance_manifest(root.path/manifest_name);
    const auto previous=read_dependency_lock(root.path);
    if(previous)require_lock_shape(*previous);
    // Existing dependencies must already be consistent; adding an unrelated
    // source never silently blesses a stale or forged lock.
    if(!manifest->dependencies().empty() || manifest->native_requirement()) {
        Resolver existing(root.path,manifest.get());
        auto captured=existing.capture();
        if(!previous || *previous!=captured.lock)stale_lock();
    }
    MaintenanceReport report;
    ProjectFiles files;
    std::shared_ptr<const Manifest> selected;
    std::string path;
    bool install_local=false;
    if(options.git) {
        auto snapshot=read_git_snapshot(git_snapshot_path(root.path,options.git->pin),options.git->pin);
        if(!snapshot)snapshot=acquire_git_snapshot(root.path,options.git->pin);
        files=package_view(*snapshot,options.git->subdir);
        auto entry=files.find(std::string(manifest_name));
        if(entry==files.end())throw std::runtime_error("Git package has no exact void.project.toml at the selected subdir.");
        selected=maintenance_manifest(snapshot->tree/utf8_path(options.git->subdir)/manifest_name,entry->second);
        publish_git_snapshot(root,options.git->pin,*snapshot);
    } else {
        const auto source=add_source_path(root.path,*options.path);
        selected=maintenance_manifest(source/manifest_name);
        auto relative=source.lexically_relative(root.path);
        if(relative.empty() || relative==".")throw std::runtime_error("A project cannot depend on itself.");
        if(*relative.begin()!="..")path=path_utf8(relative);
        else {path="libs/"+options.alias;install_local=true;}
        files=local_source_files(source);
    }
    if(!selected->package() || !selected->package_entry())throw std::runtime_error("Source must be a VOID library with an explicit package.entry.");
    const auto& identity=*selected->package();
    report_native_sources(files,report,options.alias);
    auto prior=manifest->dependencies().find(options.alias);
    if(prior!=manifest->dependencies().end()) {
        const auto& declaration=prior->second;
        bool same=declaration.id==identity.id && declaration.version.accepts(identity.version) &&
            (options.git?declaration.git && declaration.git->pin==options.git->pin && declaration.git->subdir==options.git->subdir:!declaration.git && declaration.path==path);
        if(!same)throw std::runtime_error("Conflicting dependency alias '"+options.alias+"'; existing project retained.");
        // Adopting an existing manual Git declaration also installs its optional
        // source view. Editable local contents remain the declared authority.
        if(options.git && !view_present(root.path,options.alias)) {
            publish_package_edit(root,manifest->source().text(),previous,{{options.alias,files}},[&] {
                Resolver fresh(root.path,manifest.get());
                if(!previous || fresh.capture().lock!=*previous)stale_lock();
                return std::pair{manifest->source().text(),previous};
            });
            report.changed=true;
        }
        return report;
    }
    auto text=manifest->source().text();
    const auto newline=text.find("\r\n")!=std::string::npos?"\r\n":"\n";
    std::string addition="\n[dependencies."+options.alias+"]\n";
    string_field(addition,"id",identity.id);
    if(options.git) {
        string_field(addition,"git",options.git->pin.origin);string_field(addition,"rev",options.git->pin.revision);
        if(!options.git->subdir.empty())string_field(addition,"subdir",options.git->subdir);
    } else string_field(addition,"path",path);
    string_field(addition,"version","="+identity.version.text());
    if(!text.empty() && text.back()!='\n')text+=newline;
    for(char c:addition) {if(c=='\n')text+=newline;else text+=c;}
    auto future=maintenance_manifest(root.path/manifest_name,text);
    std::map<std::string,ProjectFiles> additions;
    if(options.git || install_local) {
        if(view_present(root.path,options.alias))throw CreationCollision("Developer-owned package destination already exists: libs/"+options.alias);
        additions.emplace(options.alias,std::move(files));
    }
    const std::vector<GitPin> direct_only;
    // D3 performs any explicitly declared source acquisition; no native
    // preparation, provider selection or execution occurs during acquisition.
    if(options.git) {
        Resolver acquisition(root.path,future.get(),false,&direct_only);
        acquisition.capture();acquisition.publish(root);
    }
    publish_package_edit(root,manifest->source().text(),previous,additions,[&] {
        Resolver resolver(root.path,future.get());
        auto captured=resolver.capture();
        // add permits sources before their call sites are edited. Tidy performs
        // the relevant import inspection separately.
        Resolver fresh(root.path,future.get());
        if(fresh.capture().lock!=captured.lock)throw std::runtime_error("Package source changed during addition.");
        return std::pair{text,std::optional(captured.lock)};
    });
    report.changed=true;
    if(!options.git && path=="libs/"+options.alias)report.advice.push_back("Editable local source installed at "+path+"; subsequent source edits belong there.");
    return report;
}
MaintenanceReport tidy_dependencies(const fs::path& requested,bool check) {
    const auto root=inspect_creation_root(requested);
    if(check)require_no_package_edit(root.path);else recover_package_edit(root);
    auto manifest=maintenance_manifest(root.path/manifest_name);
    const auto previous=read_dependency_lock(root.path);
    if(previous)require_lock_shape(*previous);
    Resolver resolver(root.path,manifest.get(),false,nullptr,false);
    const bool dependencies=resolver.has_dependencies();
    auto captured=resolver.capture();
    MaintenanceReport report;
    import_advice(root.path,*manifest,report);
    std::map<std::string,ProjectFiles> replacements;
    auto additions=missing_git_views(root,*manifest,report,previous,replacements);
    const auto desired=dependencies||previous?std::optional(captured.lock):std::nullopt;
    if(previous && *previous!=captured.lock && !tidy_lock_change_allowed(*previous,captured.lock))stale_lock();
    if(check) {
        if(previous!=desired || !replacements.empty())throw std::runtime_error("Dependency lock or installed source requires reconciliation; run voidc tidy.");
        for(const auto& [alias,files]:additions)report.advice.push_back("Optional installed source view missing: libs/"+alias+"; tidy can materialize the verified D3 snapshot.");
        return report;
    }
    if(previous==desired && additions.empty())return report;
    publish_package_edit(root,manifest->source().text(),previous,additions,[&] {
        Resolver fresh(root.path,manifest.get());
        if(fresh.capture().lock!=captured.lock)throw std::runtime_error("Dependency sources changed during tidy; retry.");
        import_advice(root.path,*manifest,report);
        return std::pair{manifest->source().text(),desired};
    },nullptr,replacements);
    report.changed=true;return report;
}

}
