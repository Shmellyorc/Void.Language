#include "void/project.hpp"
#include "void/frontend.hpp"
#include "void/path.hpp"
#include "project_files.hpp"
#include <algorithm>
#include <set>
#include <tuple>

// Private parser dependency. Configuration must not escape this translation unit.
#define TOML_HEADER_ONLY 1
#define TOML_EXCEPTIONS 0
#define TOML_ENABLE_FORMATTERS 0
#define TOML_ENABLE_UNRELEASED_FEATURES 0
#define TOML_MAX_NESTED_VALUES 8
#include "toml.hpp"

namespace vd::project {
namespace {
bool relative_path(std::string_view s, bool root) {
    if (root && s == ".") return true;
    if (s.empty() || s.front() == '/' || s.back() == '/') return false;
    for (std::size_t begin = 0; begin < s.size();) {
        auto end = s.find('/', begin);
        if (end == std::string_view::npos) end = s.size();
        auto part = s.substr(begin, end - begin);
        if (part == "." || part == ".." || !portable_path_component(part)) return false;
        begin = end + 1;
    }
    return true;
}
std::string folded(std::string s) {
    for (auto& c : s) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return s;
}
bool ancestor(std::string_view a, std::string_view b) {
    return a == "." || a == b || (b.starts_with(a) && b.size() > a.size() && b[a.size()] == '/');
}
}

std::string fold_project_name(std::string_view name) { return folded(std::string(name)); }

bool dependency_admin_directory(std::string_view name) {
    const auto folded_name = fold_project_name(name);
    return folded_name == ".git" || folded_name == ".void" || folded_name == "build";
}

bool valid_dependency_path(std::string_view path) {
    if (path.empty() || path.size() > 4096 || path.back() == '/') return false;
    std::size_t components = 0;
    while (!path.empty()) {
        const auto slash = path.find('/');
        const auto part = path.substr(0, slash);
        if (++components > 64 || (part != ".." &&
            (!portable_path_component(part) || dependency_admin_directory(part) ||
             fold_project_name(part) == lock_name))) return false;
        if (slash == path.npos) break;
        path.remove_prefix(slash + 1);
    }
    return true;
}

bool VersionRequirement::accepts(const SemanticVersion& version) const {
    if (exact) return version == *exact;
    if (!version.prerelease().empty()) return false;
    if (lower) {
        const auto order = version.compare_precedence(lower->version);
        if (order < 0 || (order == 0 && !lower->inclusive)) return false;
    }
    if (upper) {
        const auto order = version.compare_precedence(upper->version);
        if (order > 0 || (order == 0 && !upper->inclusive)) return false;
    }
    return true;
}

std::string VersionRequirement::text() const {
    if (exact) return "=" + exact->text();
    std::string result;
    if (lower) result = (lower->inclusive ? ">=" : ">") + lower->version.text();
    if (upper) {
        if (lower) result += ',';
        result += (upper->inclusive ? "<=" : "<") + upper->version.text();
    }
    return result;
}

std::optional<VersionRequirement> parse_version_requirement(std::string_view input) {
    if (input.size() > 1024) return {};
    auto trim = [](std::string_view text) {
        const auto start = text.find_first_not_of(" \t");
        if (start == text.npos) return std::string_view{};
        return text.substr(start, text.find_last_not_of(" \t") - start + 1);
    };
    input = trim(input);
    VersionRequirement requirement;
    if (input.starts_with('=')) {
        requirement.exact = parse_version(trim(input.substr(1)));
        if (!requirement.exact) return {};
        return requirement;
    }
    for (unsigned count = 0;; ++count) {
        const auto comma = input.find(',');
        auto term = trim(input.substr(0, comma));
        if (count == 16 || term.empty() || (term.front() != '<' && term.front() != '>')) return {};
        const bool lower = term.front() == '>';
        term.remove_prefix(1);
        const bool inclusive = term.starts_with('=');
        if (inclusive) term.remove_prefix(1);
        auto version = parse_version(trim(term));
        if (!version || !version->prerelease().empty() || !version->build().empty()) return {};
        auto& bound = lower ? requirement.lower : requirement.upper;
        const auto order = bound ? version->compare_precedence(bound->version) : std::strong_ordering::equal;
        if (!bound || (lower ? order > 0 : order < 0) || (order == 0 && !inclusive))
            bound = VersionRequirement::Bound{std::move(*version), inclusive};
        if (comma == input.npos) break;
        input.remove_prefix(comma + 1);
    }
    if (requirement.lower && requirement.upper) {
        const auto& lower = *requirement.lower;
        const auto& upper = *requirement.upper;
        const auto order = lower.version.compare_precedence(upper.version);
        if (order > 0 || (order == 0 && (!lower.inclusive || !upper.inclusive))) return {};
        const auto& a = lower.version.core();
        const auto& b = upper.version.core();
        if (!lower.inclusive && !upper.inclusive && a[0] == b[0] && a[1] == b[1]) {
            // An exclusive interval between consecutive patches has no stable version.
            auto next = a[2];
            const auto digit = next.find_last_not_of('9');
            if (digit == next.npos) next = "1" + std::string(next.size(), '0');
            else {
                ++next[digit];
                std::fill(next.begin() + static_cast<std::ptrdiff_t>(digit + 1), next.end(), '0');
            }
            if (next == b[2]) return {};
        }
    }
    // Stable versions cannot be below the first stable release.
    if (requirement.upper && !requirement.upper->inclusive && requirement.upper->version.text() == "0.0.0") return {};
    return requirement;
}

LockShape dependency_lock_shape(std::string_view bytes) {
    const auto parsed = toml::parse(bytes);
    if (!parsed) return LockShape::malformed;
    const auto& lock = parsed.table();
    const auto format = lock["format"].value_exact<std::int64_t>();
    const auto resolver = lock["resolver"].value_exact<std::int64_t>();
    if (!format || !resolver) return LockShape::malformed;
    if (*format != 1 || *resolver != 1) return LockShape::unsupported;
    auto id = [](const auto& node) {
        const auto value = node.template value_exact<std::string_view>();
        return value && valid_identity(*value);
    };
    auto digest = [](const auto& node) {
        const auto value = node.template value_exact<std::string_view>();
        return value && value->size() == 64 && value->find_first_not_of("0123456789abcdef") == value->npos;
    };
    std::size_t edge_count = 0;
    auto edges = [&](const toml::table* entries) {
        if (!entries || entries->size() > dependency_edge_limit - edge_count) return false;
        edge_count += entries->size();
        for (const auto& [alias, target] : *entries)
            if (alias.str().size() > 128 || !valid_editor_identifier(std::string(alias.str())) || !id(target)) return false;
        return true;
    };
    const auto* root = lock["root"].as_table();
    if (!root || root->size() != 4 || !id((*root)["id"]) || !digest((*root)["inputs"]) ||
        !edges((*root)["dependencies"].as_table())) return LockShape::malformed;
    const auto kind = (*root)["kind"].value_exact<std::string_view>();
    if (!kind || (*kind != "application" && *kind != "library")) return LockShape::malformed;
    if (!lock.contains("package")) return lock.size() == 3 ? LockShape::valid : LockShape::malformed;
    const auto* packages = lock["package"].as_array();
    if (lock.size() != 4 || !packages || packages->empty() || packages->size() > dependency_package_limit)
        return LockShape::malformed;
    std::set<std::string_view> ids, paths;
    for (const auto& item : *packages) {
        const auto* package = item.as_table();
        if (!package || package->size() != 6 || !id((*package)["id"]) ||
            !digest((*package)["manifest"]) || !digest((*package)["tree"]) ||
            !edges((*package)["dependencies"].as_table())) return LockShape::malformed;
        const auto version = (*package)["version"].value_exact<std::string_view>();
        const auto path = (*package)["path"].value_exact<std::string_view>();
        if (!version || !parse_version(*version) || !path || !relative_path(*path, false) || !valid_dependency_path(*path))
            return LockShape::malformed;
        if (!ids.insert(*(*package)["id"].value_exact<std::string_view>()).second || !paths.insert(*path).second)
            return LockShape::conflicting;
    }
    return LockShape::valid;
}

class ManifestParser {
    using Code = DiagnosticCode;
    std::shared_ptr<Manifest> manifest_ = std::shared_ptr<Manifest>(new Manifest);
    std::vector<Diagnostic> errors_;
    std::size_t offset(toml::source_position pos) const {
        const auto& source = *manifest_->source_;
        if (!pos.line || pos.line > source.line_starts().size()) return source.text().size();
        auto i = source.line_starts()[pos.line - 1];
        if (!i && source.text().starts_with("\xef\xbb\xbf")) i = 3;
        for (std::size_t column = 1; column < pos.column && i < source.text().size() &&
            source.text()[i] != '\r' && source.text()[i] != '\n'; ++column)
            i += utf8_scalar_bytes(source.text(), i);
        return i;
    }
    SourceSpan span(const toml::source_region& region) const {
        auto begin = offset(region.begin), end = offset(region.end);
        return {manifest_->source_, {}, begin, std::max(begin, end)};
    }
    SourceSpan eof() const {
        auto n = manifest_->source_->text().size();
        return {manifest_->source_, {}, n, n};
    }
    void fail(Code code, SourceSpan where, std::string description, std::string hint,
        std::optional<SourceSpan> previous = {}) {
        Diagnostic error{code};
        error.primary = std::move(where);
        error.description = std::move(description);
        error.hint = std::move(hint);
        if (previous) error.related.push_back({*previous, "Previously declared"});
        errors_.push_back(std::move(error));
    }
    void fields(const toml::table& table, const std::string& prefix, std::initializer_list<std::string_view> allowed) {
        for (const auto& [key, value] : table) {
            auto name = prefix.empty() ? std::string(key.str()) : prefix + "." + std::string(key.str());
            if (std::find(allowed.begin(), allowed.end(), key.str()) == allowed.end()) {
                fail(Code::project_field, span(key.source()), "Unknown project manifest field.", "Remove the unsupported field; use the schema 1 field list.");
            } else manifest_->fields_.emplace(name, Field{span(key.source()), span(value.source())});
        }
    }
    const toml::node* field(const toml::table& table, std::string_view key, bool required) {
        const auto* value = table.get(key);
        if (!value && required) fail(Code::project_field, table.empty() ? eof() : span(table.source()),
            "Missing required manifest field '" + std::string(key) + "'.", "Add the required field to its containing table.");
        return value;
    }
    const toml::table* table(const toml::table& parent, std::string_view key, bool required = false) {
        const auto* value = field(parent, key, required);
        if (!value) return nullptr;
        if (!value->is_table()) {
            fail(Code::project_field, span(value->source()), "Manifest field requires a table.", "Use a TOML table, not a scalar or array of tables.");
            return nullptr;
        }
        return value->as_table();
    }
    std::optional<std::string> text(const toml::table& table, std::string_view key, bool required) {
        const auto* value = field(table, key, required);
        if (!value) return {};
        if (!value->is_string()) {
            fail(Code::project_field, span(value->source()), "Manifest field requires a string.", "Quote the value; no type coercion is performed.");
            return {};
        }
        return value->ref<std::string>();
    }
    void identity(const toml::table& table, Identity& target, bool package) {
        if (auto id = text(table, "id", true)) {
            target.id = *id;
            if (!valid_identity(*id)) fail(Code::project_identity, span(table.get("id")->source()),
                "Invalid portable manifest identity.", "Use 1-255 lowercase ASCII bytes with letters/digits and interior dot, underscore or hyphen; no empty dot pieces or reserved device stems.");
        }
        if (auto name = text(table, "name", (&target == &manifest_->project_ || package))) {
            target.name = *name;
            if (!valid_name(*name) || (package && !valid_editor_identifier(*name)))
                fail(Code::project_identity, span(table.get("name")->source()), "Invalid manifest name.",
                    package ? "Use a non-keyword lowercase compiler identifier for package.name." : "Use 1-128 Unicode scalars, including a non-space scalar, with no C0/C1 control characters.");
        }
        if (auto version = text(table, "version", &target == &manifest_->project_)) {
            auto parsed = parse_version(*version);
            if (!parsed) fail(Code::project_version, span(table.get("version")->source()), "Invalid semantic version.",
                "Use major.minor.patch with optional -prerelease and +build identifiers; numeric core/prerelease identifiers cannot have leading zeroes. Limit: 255 ASCII bytes.");
            else target.version = std::move(*parsed);
        }
    }
    void dependencies(const toml::table& entries) {
        if (entries.size() > dependency_edge_limit) {
            fail(Code::limit, span(entries.source()), "Project dependency count exceeds 1024.", "Reduce the number of declared dependencies.");
            return;
        }
        for (const auto& [key, node] : entries) {
            const std::string alias(key.str());
            if (alias.size() > 128 || !valid_editor_identifier(alias)) {
                fail(Code::project_identity, span(key.source()), "Invalid dependency alias.", "Use a lowercase non-keyword compiler identifier of at most 128 bytes.");
                continue;
            }
            const auto* settings = node.as_table();
            if (!settings) {
                fail(Code::project_reserved, span(node.source()), "Dependency declarations require a local library table.", "Declare id, path and version under [dependencies." + alias + "].");
                continue;
            }
            fields(*settings, "dependencies." + alias, {"id", "path", "version"});
            auto id = text(*settings, "id", true);
            auto path = text(*settings, "path", true);
            auto version = text(*settings, "version", true);
            auto requirement = version ? parse_version_requirement(*version) : std::nullopt;
            if (id && !valid_identity(*id))
                fail(Code::project_identity, span(settings->get("id")->source()), "Invalid dependency package ID.", "Use a portable package identity.");
            if (path && !valid_dependency_path(*path))
                fail(Code::project_path, span(settings->get("path")->source()), "Invalid local dependency path.", "Use portable relative components outside reserved administrative directories.");
            if (version && !requirement)
                fail(Code::project_version, span(settings->get("version")->source()), "Invalid dependency version constraint.", "Use = followed by an exact SemVer or comma-separated stable bounds.");
            if (id && path && requirement)
                manifest_->dependencies_.emplace(alias, Dependency{std::move(*id), std::move(*path), std::move(*requirement)});
        }
    }
    void paths(const toml::table& source, std::string_view key, std::vector<std::string>& target, bool roots) {
        const auto* value = source.get(key);
        if (!value) return;
        auto* array = value->as_array();
        if (!array) {
            fail(Code::project_field, span(value->source()), "Source paths require an array of strings.", "Use a TOML array, including [] for empty exclusions.");
            return;
        }
        if (array->size() > (roots ? 32u : 256u) || (roots && array->empty())) {
            fail(Code::limit, span(value->source()), "Invalid source path count.", roots ? "Use 1-32 source roots." : "Use at most 256 exclusions.");
            return;
        }
        target.clear();
        std::map<std::string, SourceSpan> previous;
        for (const auto& item : *array) {
            if (!item.is_string()) {
                fail(Code::project_field, span(item.source()), "Source path requires a string.", "Quote each relative source path.");
                continue;
            }
            const auto& path = item.ref<std::string>();
            if (!relative_path(path, roots)) {
                fail(Code::project_path, span(item.source()), "Invalid manifest-relative source path.", "Use exact portable /-separated relative components without traversal, aliases, absolute paths or glob syntax.");
                continue;
            }
            bool reserved = false;
            if (roots && path != ".") {
                for (std::size_t begin = 0; begin < path.size();) {
                    auto end = path.find('/', begin);
                    if (end == std::string::npos) end = path.size();
                    auto part = std::string_view(path).substr(begin, end - begin);
                    reserved |= part == ".git" || part == ".void" || part == "build" || part == "libs";
                    begin = end + 1;
                }
            }
            if (reserved) {
                fail(Code::project_path, span(item.source()), "Reserved automatic source root.",
                    "Choose application/library source roots outside .git, .void, build and libs; actual in-root imports remain a later admission step.");
                continue;
            }
            auto fold = folded(path);
            bool conflict = false;
            for (const auto& [old, location] : previous) {
                if (old == fold || (roots && (ancestor(old, fold) || ancestor(fold, old)))) {
                    fail(old == fold ? Code::project_duplicate : Code::project_path, span(item.source()),
                        old == fold ? "Duplicate or case-conflicting source path." : "Source roots overlap.",
                        "Keep unique source paths; roots must not contain one another.", location);
                    conflict = true; break;
                }
            }
            if (!conflict) { previous.emplace(std::move(fold), span(item.source())); target.push_back(path); }
        }
    }
    void profile(const toml::table& build, std::string_view key, Profile& target) {
        auto* settings = table(build, key);
        if (!settings) return;
        fields(*settings, "build." + std::string(key), {"optimization", "debug_info"});
        if (auto optimization = text(*settings, "optimization", false)) {
            if (*optimization == "none") target.optimization = Optimization::none;
            else if (*optimization == "O2") target.optimization = Optimization::O2;
            else fail(Code::project_field, span(settings->get("optimization")->source()), "Unsupported optimization setting.", "Use exactly 'none' or 'O2'.");
        }
        if (auto debug = text(*settings, "debug_info", false)) {
            if (*debug == "none") target.debug_info = DebugInfo::none;
            else if (*debug == "line") target.debug_info = DebugInfo::line;
            else if (*debug == "full") target.debug_info = DebugInfo::full;
            else fail(Code::project_field, span(settings->get("debug_info")->source()), "Unsupported debug information setting.", "Use exactly 'none', 'line' or 'full'.");
        }
    }
    ParseResult finish() {
        std::stable_sort(errors_.begin(), errors_.end(), [](const auto& a, const auto& b) {
            return std::tie(a.primary.begin, a.primary.end, a.code, a.description, a.hint) <
                std::tie(b.primary.begin, b.primary.end, b.code, b.description, b.hint);
        });
        if (errors_.size() > manifest_diagnostic_limit) {
            errors_.erase(errors_.begin() + manifest_diagnostic_limit, errors_.end());
            fail(Code::limit, eof(), "Manifest diagnostic limit reached.", "Correct the reported fields and retry; no validated metadata was produced.");
        }
        return {errors_.empty() ? manifest_ : nullptr, std::move(errors_)};
    }
public:
    explicit ManifestParser(const SourceFile& source) { manifest_->source_ = std::make_shared<const SourceFile>(source); }
    ParseResult parse() {
        if (manifest_->source_->text().size() > manifest_byte_limit) {
            fail(Code::limit, eof(), "Project manifest exceeds 256 KiB.", "Reduce the manifest before parsing.");
            return finish();
        }
        auto result = toml::parse(manifest_->source_->text());
        if (!result) {
            const auto& error = result.error();
            auto prior = error.previous_definition();
            auto message = std::string(error.description());
            auto code = prior.begin.line ? Code::project_duplicate :
                (message.find("exceeded maximum nested value depth of ") != std::string::npos ||
                 message.find("exceeded VOID manifest key depth limit") != std::string::npos) ? Code::limit : Code::project_syntax;
            fail(code, span(error.source()), "Invalid project TOML: " + message,
                code == Code::project_duplicate ? "Define each key/table once; the earlier definition is retained as a related location." :
                code == Code::limit ? "Reduce nested arrays/inline tables and dotted key depth; each limit is 8." : "Use TOML 1.0 syntax; no configuration code is executed.",
                prior.begin.line ? std::optional{span(prior)} : std::nullopt);
            return finish();
        }
        const auto& root = result.table();
        fields(root, "", {"schema", "project", "application", "package", "sources", "build", "dependencies", "native"});
        const auto* schema = root.get("schema");
        if (!schema || !schema->is_integer() || schema->ref<std::int64_t>() != 1) {
            fail(Code::project_schema, schema ? span(schema->source()) : eof(), "Project schema must be integer 1.",
                "Add schema = 1, or use a matching compiler for another schema; schema is not the project's semantic version.");
            return finish();
        }
        auto* project = table(root, "project", true);
        if (!project) return finish();
        fields(*project, "project", {"id", "name", "version", "kind", "entry"});
        identity(*project, manifest_->project_, false);
        auto kind = text(*project, "kind", true);
        bool valid_kind = kind && (*kind == "application" || *kind == "library");
        if (kind && !valid_kind) fail(Code::project_field, span(project->get("kind")->source()), "Invalid project kind.", "Use exactly 'application' or 'library'.");
        if (valid_kind) {
            manifest_->kind_ = *kind == "application" ? Kind::application : Kind::library;
            auto section = *kind == "application" ? "application" : "package";
            auto forbidden = *kind == "application" ? "package" : "application";
            if (auto* other = root.get(forbidden)) fail(Code::project_field, span(other->source()), "Section is forbidden for this project kind.", "Use one application or library target; keep application and package identity separate.");
            if (auto* selected = table(root, section, true)) {
                if (*kind == "library") fields(*selected, section, {"id", "name", "version", "entry"});
                else fields(*selected, section, {"id", "name", "version"});
                Identity info{{}, manifest_->project_.name, manifest_->project_.version};
                identity(*selected, info, *kind == "library");
                if (*kind == "application") manifest_->application_ = std::move(info);
                else {
                    manifest_->package_ = std::move(info);
                    if (auto entry = text(*selected, "entry", false)) {
                        if (!relative_path(*entry, false) || !valid_dependency_path(*entry) || !entry->ends_with(".void"))
                            fail(Code::project_path, span(selected->get("entry")->source()), "Invalid package entry path.", "Use an exact portable package-relative .void file path.");
                        else manifest_->package_entry_ = std::move(*entry);
                    }
                }
            }
        }
        if (auto entry = text(*project, "entry", false)) {
            if (valid_kind && manifest_->kind_ == Kind::library) fail(Code::project_field, span(project->get("entry")->source()), "Library manifests cannot select an entry.", "Remove project.entry; ordinary libraries need no main function.");
            if (!relative_path(*entry, false) || !entry->ends_with(".void")) fail(Code::project_path, span(project->get("entry")->source()), "Invalid project entry path.", "Use an exact portable relative .void file path; entry is optional and never a function name.");
            manifest_->entry_ = *entry;
        }
        if (auto* sources = table(root, "sources")) {
            fields(*sources, "sources", {"roots", "exclude"});
            paths(*sources, "roots", manifest_->sources_.roots, true);
            paths(*sources, "exclude", manifest_->sources_.exclude, false);
        }
        if (auto* build = table(root, "build")) {
            fields(*build, "build", {"debug", "release"});
            profile(*build, "debug", manifest_->debug_);
            profile(*build, "release", manifest_->release_);
        }
        if (auto* declarations = table(root, "dependencies")) dependencies(*declarations);
        if (auto* native = table(root, "native")) for (const auto& [key, value] : *native) {
            const auto platform = key.str();
            if ((platform != "linux" && platform != "windows") || manifest_->kind_ != Kind::application) {
                fail(Code::project_reserved, span(value.source()), "Native declarations require an application and a linux or windows table.",
                    "Declare a local C provider under [native.linux] or [native.windows].");
                continue;
            }
            if (auto* settings = table(*native, platform)) {
                fields(*settings, "native." + std::string(platform), {"descriptor", "library", "adapter"});
                NativeLibrary declaration;
                for (auto [name, output] : {std::pair{"descriptor", &declaration.descriptor},
                                           {"library", &declaration.library}, {"adapter", &declaration.adapter}}) {
                    if (auto filename = text(*settings, name, true)) {
                        if (!relative_path(*filename, false) || filename->find('/') != filename->npos)
                            fail(Code::project_path, span(settings->get(name)->source()), "Expected a filename in the project native directory.",
                                "Use one portable filename relative to native/.");
                        *output = std::move(*filename);
                    }
                }
                manifest_->native_.emplace(platform, std::move(declaration));
            }
        }
        return finish();
    }
};
ParseResult parse_manifest(const SourceFile& source) { return ManifestParser(source).parse(); }
EffectiveProfile resolve_profile(const Manifest& manifest,std::optional<ProfileName> requested,bool no_opt,bool cache) {
    auto name=requested.value_or(cache?ProfileName::debug:ProfileName::release);
    const auto& profile=name==ProfileName::debug?manifest.debug():manifest.release();
    bool optimize=profile.optimization==Optimization::O2 && !no_opt;
    if(cache && (name!=ProfileName::debug || optimize))throw std::invalid_argument("--cache requires the debug profile with effective optimization='none'; it cannot override an explicit release profile or O2 setting.");
    EffectiveProfile result{name,optimize,profile.debug_info,{},"project-profile-v1:"};
    const std::string label=name==ProfileName::debug?"debug":"release";
    result.cache_key+=label+":"+(optimize?"O2":"none")+":"+std::to_string(static_cast<int>(profile.debug_info));
    if(profile.debug_info!=DebugInfo::none) {
        Diagnostic warning(DiagnosticCode::project_debug_info);warning.severity=Severity::warning;
        warning.description="Project profile '"+label+"' requests debug_info='"+(profile.debug_info==DebugInfo::line?"line":"full")+"'; profile-controlled debug-information emission is not implemented.";
        auto field=manifest.fields().find("build."+label+".debug_info");
        warning.primary=field!=manifest.fields().end()?field->second.value:SourceSpan{std::make_shared<const SourceFile>(manifest.source()),"/void.project.toml",manifest.source().text().size(),manifest.source().text().size()};
        warning.primary.module_identity="/void.project.toml";
        warning.hint="The requested mode was not applied; existing emitter behavior is unchanged. Optimization/cache settings still apply. debug_info='none' suppresses this warning.";
        result.diagnostics.push_back(std::move(warning));
    }
    return result;
}

}
