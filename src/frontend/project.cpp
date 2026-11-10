#include "void/project.hpp"
#include "void/frontend.hpp"
#include "void/path.hpp"
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
                fields(*selected, section, {"id", "name", "version"});
                Identity info{{}, manifest_->project_.name, manifest_->project_.version};
                identity(*selected, info, *kind == "library");
                if (*kind == "application") manifest_->application_ = std::move(info);
                else manifest_->package_ = std::move(info);
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
        if (auto* reserved = table(root, "dependencies"); reserved && !reserved->empty())
            fail(Code::project_reserved, span(reserved->source()), "Project dependency resolution is reserved.", "Remove the declarations.");
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
