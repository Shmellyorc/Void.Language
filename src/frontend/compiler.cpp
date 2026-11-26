#include "semantics.hpp"
#include "type_resolution.hpp"
#include "constant_evaluation.hpp"
#include "lowering.hpp"
#include "diagnostics.hpp"
#include "model.hpp"
#include "parser.hpp"
#include "stack_guard.hpp"
#include "void/compiler_observation.hpp"
#include "void/compile_evaluation.hpp"
#include "void/frontend.hpp"
#include "void/conditional.hpp"
#include "void/c_primitive.hpp"
#include "void/path.hpp"
#include "void/project_creation.hpp"
#include "void/project_resolution.hpp"
#include "project_files.hpp"
#include "project_native.hpp"
#include "void/numeric.hpp"
#include "void/math_ops.hpp"
#include "void/temporal_ops.hpp"
#include "void/console_ops.hpp"
#include "void/filesystem_ops.hpp"
#include "void/url_ops.hpp"
#include "void/http_ops.hpp"
#include "void/os.hpp"
#include "void/host_ops.hpp"
#include "void/temporal.hpp"
#include "void/reference_capabilities.hpp"
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <fstream>
#include <functional>
#include <bit>
#include <iterator>
#include <list>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <llvm/Support/SHA256.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/ADT/StringRef.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace vd {
namespace frontend {

std::string folded(std::string text) {
    for (auto& c : text) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return text;
}
bool identifier(const std::string& text) {
    return !text.empty() && letter(text[0]) && !reserved.contains(text) &&
        std::all_of(text.begin(), text.end(), [](char c) { return letter(c) || digit(c); });
}
void portable_component(const SourceFile& source, const Token& at, const std::string& component) {
    if (!portable_path_component(component)) error(source, at, Code::import_path, "Non-portable module path component '" + component + "'.",
        "Use printable ASCII filenames without Windows-reserved characters/names or trailing dots/spaces; Unicode module filenames are deferred.");
}
std::string normalize(const Unit& from, const Token& path) {
    const auto& text = path.text;
    if (text.empty() || text.find('\\') != std::string::npos)
        error(from.source, path, Code::import_path, "Invalid import path.", "Use a nonempty VFS path with / separators on every platform.");
    bool absolute = text[0] == '/';
    if (text.starts_with("//")) error(from.source, path, Code::import_path, "Invalid root-relative import path.", "A single leading / denotes the virtual project root, not an OS or network path.");
    if (!absolute && !text.starts_with("./") && !text.starts_with("../") && text.find(':') != std::string::npos)
        error(from.source, path, Code::import_path, "OS filesystem paths are not valid VOID imports.", "Use a project-root /path.void or importer-relative ./path.void instead of a drive path.");
    std::vector<std::string> parts;
    if (!absolute) {
        auto parent = utf8_path(from.identity).parent_path();
        for (const auto& component : parent.relative_path()) parts.push_back(path_utf8(component));
    }
    std::size_t begin = absolute ? 1 : 0;
    while (begin <= text.size()) {
        auto end = text.find('/', begin); if (end == std::string::npos) end = text.size();
        auto component = text.substr(begin, end - begin);
        if (component == "..") {
            if (parts.empty()) error(from.source, path, Code::import_path, "Import escapes the project root.", "External imports require an explicit permission/identity contract and are not enabled in this phase.");
            parts.pop_back();
        } else if (!component.empty() && component != ".") {
            portable_component(from.source, path, component); parts.push_back(std::move(component));
        }
        if (end == text.size()) break;
        begin = end + 1;
    }
    if (parts.empty() || text.back() == '/' || !parts.back().ends_with(".void"))
        error(from.source, path, Code::import_path, "Import must name a .void source file.", "Supply an exact-case filename, not a directory.");
    std::string id;
    for (const auto& component : parts) id += "/" + component;
    return id;
}
class SourceReadError : public CompilationError {
    static Diagnostic record(const std::string& path, const std::string& message) {
        Diagnostic d{Code::import_read}; d.description = message;
        d.primary = {std::make_shared<const SourceFile>(path, ""), {}, 0, 0};
        d.hint = "Check the source path, permissions and storage availability.";
        return d;
    }
public:
    SourceReadError(const std::string& path, const std::string& message) : CompilationError({record(path, message)}) {}
};
class SourceGraph {
    std::filesystem::path root;
    Diagnostics& diagnostics;
    unsigned depth = 0;
    const SourceOverrides& overrides;
    const NativeProject* native=nullptr;
    CPrimitiveAdmission::Owner primitives;
    std::string physical_primitive_module;
    std::optional<std::filesystem::path> standard_root;
    struct Directory {std::map<std::string,std::set<std::string>> names;std::error_code error;};
    std::map<std::filesystem::path,Directory> directories;
    std::map<std::string,std::map<std::string,std::set<std::string>>> virtual_children;
    std::set<std::string> virtual_directories;
    const Directory& directory_entries(const std::filesystem::path& path) {
        auto [entry,fresh]=directories.try_emplace(path);
        if(fresh)for(std::filesystem::directory_iterator it(path,entry->second.error),end;!entry->second.error && it!=end;it.increment(entry->second.error)) {
            auto name=path_utf8(it->path().filename());entry->second.names[folded(name)].insert(name);
        }
        return entry->second;
    }
public:
    std::map<std::string, std::unique_ptr<Unit>> units;
    // Parsed discovery trees are not execution members until load follows them.
    std::map<std::string, std::unique_ptr<Unit>> parsed;
    std::function<void(const std::filesystem::path&,const std::string&,bool)> admit;
    std::function<SourceFile(const std::filesystem::path&,const std::string&)> reader;
    const project::LockedDependencies* dependencies = nullptr;
    BuildConditions conditions=native_build_conditions();
    BuildConditions conditions_for(const std::string& id) const {
        auto result=conditions;
        if(standard_module(id))result.features.clear();
        else if(dependencies && package_source(id))result.features=package(id)->conditions;
        return result;
    }
    bool imports_only = false; // Metadata inspection grants no native or lowering authority.
    explicit SourceGraph(std::filesystem::path project_root, Diagnostics& d, const SourceOverrides& o,const NativeProject* bindings=nullptr,
                         CPrimitiveAdmission::Owner c_primitives = {})
        : root(std::move(project_root)), diagnostics(d), overrides(o), native(bindings), primitives(std::move(c_primitives)) {
        for(const auto& [key,value]:overrides) {
            std::string parent;
            for(std::size_t begin=1;begin<key.size();) {
                auto end=key.find('/',begin);auto child=key.substr(begin,end==std::string::npos?end:end-begin);
                virtual_children[parent][folded(child)].insert(child);
                if(value && end!=std::string::npos)virtual_directories.insert(key.substr(0,end));
                if(end==std::string::npos)break;parent=key.substr(0,end);begin=end+1;
            }
        }
    }
    void generated_primitives(CPrimitiveAdmission::Owner admission) {
        if(!admission->physical_source())
            virtual_children[""][folded(admission->module().substr(1))].insert(admission->module().substr(1));
        primitives = std::move(admission);
    }
    std::function<bool()> input_check() const {
        for(const auto& [id,unit]:units)if(unit->failed_import)return {};
        std::map<std::string,SourceFile> sources;
        for(const auto* group:{&units,&parsed})for(const auto& [id,unit]:*group)sources.insert_or_assign(id,unit->source);
        return [root=root,overlays=overrides,sources=std::move(sources),directories=directories] {
            if(std::filesystem::canonical(root)!=root)return false;
            Diagnostics diagnostics;SourceGraph fresh(root,diagnostics,overlays);
            Unit anchor(SourceFile(path_utf8(root),""),"/");
            for(const auto& [path,observed]:directories) {
                const auto& current=fresh.directory_entries(path);
                if(current.names!=observed.names || current.error!=observed.error)return false;
            }
            for(const auto& [id,source]:sources) {
                Token token{TokenKind::path,id,0};
                if(standard_module(id)){token.kind=TokenKind::name;token.text=id.substr(9,id.size()-9-5);}
                const auto [identity,path]=fresh.resolve(anchor,token);
                const auto overlay=overlays.find(id);
                const auto current=overlay!=overlays.end() && overlay->second?*overlay->second:read_source(path);
                if(identity!=id || (overlay==overlays.end() && path_utf8(path)!=source.path()) || current.text()!=source.text())return false;
                // resolve checks physical aliases against prior module owners.
                // Retain their identities without parsing/binding a second AST.
                fresh.units.emplace(id,std::make_unique<Unit>(current,id));
            }
            return true;
        };
    }
    const project::LockedPackage* package(std::string_view identity) const {
        if (!dependencies || !identity.starts_with(package_source_prefix)) return nullptr;
        const auto found = dependencies->packages().find(source_domain(identity).substr(package_source_prefix.size()));
        return found == dependencies->packages().end() ? nullptr : &found->second;
    }
    const project::DependencyEdges* direct_packages(std::string_view identity) const {
        if (!dependencies || standard_module(identity)) return nullptr;
        if (!package_source(identity)) return &dependencies->direct();
        const auto* selected = package(identity);
        return selected ? &selected->dependencies : nullptr;
    }
    SourceFile package_snapshot(const std::string& id) const {
        const auto* selected = package(id);
        if (selected) {
            const auto relative = package_relative_source(id);
            if (auto found = selected->sources.find(relative); found != selected->sources.end())
                return SourceFile(path_utf8(root / utf8_path(selected->locator) / utf8_path(relative)), found->second);
        }
        throw std::invalid_argument("Source is outside the verified package snapshot.");
    }
    std::string package_identity(const std::filesystem::path& physical) const {
        if (!dependencies) return {};
        for (const auto& [id, selected] : dependencies->packages()) {
            const auto relative = path_utf8(physical.lexically_relative(root / utf8_path(selected.locator)));
            if (selected.sources.contains(relative)) return std::string(package_source_prefix) + id + "/" + relative;
        }
        return {};
    }
    std::pair<std::string,std::filesystem::path> resolve(const Unit& from, const Token& path, bool directory=false) {
        if (path.kind != TokenKind::path && path.text.starts_with("pkg/")) {
            const auto alias = path.text.substr(4);
            const auto* direct = direct_packages(from.identity);
            if (directory || !identifier(alias) || !direct || !direct->contains(alias))
                error(from.source, path, Code::import_path, "Undeclared direct package import '" + path.text + "'.",
                    "Use pkg/<alias> for a direct dependency in the importing manifest.");
            const auto& id = direct->at(alias);
            const auto& selected = dependencies->packages().at(id);
            return {std::string(package_source_prefix) + id + "/" + selected.entry,
                root / utf8_path(selected.locator) / utf8_path(selected.entry)};
        }
        const bool local=path.kind!=TokenKind::path && path.text.starts_with("lib/");
        if((path.kind==TokenKind::path || local) && standard_module(from.identity))error(from.source,path,Code::import_path,"Standard modules cannot import project VFS sources.","Use an unquoted standard-library module path outside lib/.");
        bool standard=path.kind!=TokenKind::path && !local;
        const auto* owner = package(from.identity);
        Unit anchor(from.source,owner ? "/" + std::string(package_relative_source(from.identity)) : from.identity);
        Token requested=path;
        if(path.kind!=TokenKind::path) requested.text="/"+path.text+".void";
        if(local) {
            auto name=path.text.substr(4);
            // Only lib/<library> has the explicit repeated-name entry convention.
            // Directory probes and deeper modules never select implicit indexes.
            requested.text="/libs/"+name;
            if(!directory && name.find('/')==std::string::npos)requested.text+="/"+name;
            requested.text+=".void";
        }
        auto relative=normalize(anchor,requested);
        if(relative.starts_with("/@stdlib/"))error(from.source,path,Code::import_path,"Reserved standard-library source identity.");
        if(package_source(relative))error(from.source,path,Code::import_path,"Reserved package source identity.","Import packages with pkg/<alias>.");
        // A source package may name the exact compiler-generated module only
        // after its application has admitted that provider. Relative imports
        // and every other absolute path retain the locked package boundary.
        const bool admitted_native = owner && !directory && path.kind == TokenKind::path &&
            primitives && !primitives->physical_source() && path.text == primitives->module();
        if (admitted_native && owner->sources.contains(std::string_view(relative).substr(1)))
            error(from.source,path,Code::import_path,"Host-admitted C binding module collides with a package source.");
        if (owner && !standard && !admitted_native) {
            if (directory) relative = path_utf8(utf8_path(relative).parent_path());
            auto suffix = std::string_view(relative);
            if (suffix.starts_with('/')) suffix.remove_prefix(1);
            if (directory) {
                const auto prefix = suffix.empty() ? std::string{} : std::string(suffix) + "/";
                const auto child = owner->sources.lower_bound(prefix);
                if (child == owner->sources.end() || !child->first.starts_with(prefix))
                    error(from.source,path,Code::import_read,"Directory is outside the verified package sources.");
            } else if (!owner->sources.contains(suffix))
                error(from.source,path,Code::import_read,"Cannot find exact-case source '" + relative + "' in the verified package.",
                    "Use a locked source within the owning package's boundary.");
            return {std::string(source_domain(from.identity)) + relative,
                root / utf8_path(owner->locator) / utf8_path(suffix)};
        }
        auto id=standard ? "/@stdlib"+relative : relative;
        if(directory) {relative=path_utf8(utf8_path(relative).parent_path());id=standard ? "/@stdlib"+relative : relative;}
        auto overlay = overrides.find(id);
        const bool generated = !directory && primitives && !primitives->physical_source() && id == primitives->module();
        // All parse/load/recapture readers resolve first, including cached units.
        // Overrides are frozen for the synchronous analysis: a tombstone can
        // never reach their optional dereferences or fall back to disk bytes.
        if (overlay != overrides.end() && !overlay->second)
            error(from.source, path, Code::import_read, "Source is explicitly unavailable: '" + id + "'.", "Restore the document or close its override to return to disk.");
        auto physical=root;
        if(standard) {
            if(!standard_root) {
                try {physical=standard_library_root();}catch(const std::invalid_argument& failure){error(from.source,path,Code::import_path,failure.what());}
                std::error_code root_error;physical=std::filesystem::canonical(physical,root_error);
                if(root_error)error(from.source,path,Code::import_read,"Standard-library source root is unavailable.","Set VOID_STDLIB_ROOT to the matching toolchain source directory.");
                standard_root=physical;
            }
            physical=*standard_root;
        }
        std::string prefix;
        std::error_code ec;
        for (const auto& part : utf8_path(relative).relative_path()) {
            auto spelling = path_utf8(part);
            std::set<std::string> matches;
            const auto& entries=directory_entries(physical);ec=entries.error;
            if(auto found=entries.names.find(folded(spelling));found!=entries.names.end())matches=found->second;
            // Merge virtual children into the same exact-case namespace. Missing
            // virtual parents are permitted; existing unreadable parents are not.
            if (ec == std::errc::no_such_file_or_directory) ec.clear();
            if(!standard)if(auto parent=virtual_children.find(prefix);parent!=virtual_children.end())
                if(auto child=parent->second.find(folded(spelling));child!=parent->second.end())matches.insert(child->second.begin(),child->second.end());
            if (ec) error(from.source, path, Code::import_read, "Cannot inspect import directory for '" + id + "'.", "Check that each parent exists, is a readable directory and uses the exact filename case.");
            if (matches.size() > 1) error(from.source, path, Code::import_path, "Filesystem case collision for '" + spelling + "'.", "Rename case-only siblings so this project has the same identity on every platform.");
            if (matches.empty()) error(from.source, path, Code::import_read, "Cannot find imported file '" + id + "'.", "Check this path relative to the importing file or virtual project root.");
            if (*matches.begin() != spelling) error(from.source, path, Code::import_path, "Import path case mismatch: requested '" + spelling + "', found '" + *matches.begin() + "'.", "Use the exact on-disk spelling on every platform.");
            physical /= part;
            prefix += "/" + spelling;
            if (prefix != relative && !standard && overrides.contains(prefix))
                error(from.source, path, Code::import_path, "An overridden source cannot also be a virtual directory: '" + prefix + "'.");
            auto status = std::filesystem::symlink_status(physical, ec);
            if (ec == std::errc::no_such_file_or_directory && (generated || overlay != overrides.end() || (directory && !standard && virtual_directories.contains(id)))) ec.clear();
            if (ec) error(from.source, path, Code::import_read, "Cannot inspect imported path '" + id + "'.");
            if (prefix == relative && primitives && !primitives->physical_source() && id == primitives->module() && std::filesystem::exists(status))
                error(from.source,path,Code::import_path,"Host-admitted C binding module collides with a project source.");
            if (std::filesystem::is_symlink(status)) error(from.source, path, Code::import_path, "Symbolic links are not supported in module paths.", "Import a regular project file; symlink identity and external permissions are deferred.");
            if (std::filesystem::exists(status)) {
                auto resolved_part = std::filesystem::canonical(physical, ec);
                if (ec || resolved_part != physical) error(from.source, path, Code::import_path, "Module path contains a filesystem alias.");
            }
            if (!standard && admit) admit(physical,prefix,prefix==relative && !directory);
        }
        if (directory) {
            if(!std::filesystem::is_directory(physical,ec) && ec!=std::errc::no_such_file_or_directory)error(from.source,path,Code::import_read,"Import directory is unavailable.");
        } else if (!generated && overlay == overrides.end()) {
            if (!std::filesystem::is_regular_file(physical, ec) || ec)
                error(from.source, path, Code::import_read, "Imported path is not a regular .void source file.");
        }
        // Existing host files retain alias restrictions even when overlaid. New
        // virtual files have no host identity; their owned bytes are authoritative.
        if (!directory && std::filesystem::exists(physical, ec)) {
            if (!std::filesystem::is_regular_file(physical, ec) || ec)
                error(from.source, path, Code::import_path, "Module path names a non-file host object.");
            for (const auto* store : {&units,&parsed}) for (const auto& [other_id, other] : *store) {
                if(other_id==id)continue;
                auto other_path = package_source(other_id) ? utf8_path(other->source.path()) :
                    standard_module(other_id) ? *standard_root / utf8_path(other_id.substr(9)) : root / utf8_path(other_id).relative_path();
                if (std::filesystem::equivalent(physical, other_path, ec) && !ec)
                    error(from.source, path, Code::import_path, "Hard-link aliases have conflicting module identities: '" + id + "' and '" + other_id + "'.", "Use one VFS filename for this physical source.");
                ec.clear();
            }
        }
        return {id,physical};
    }
    Unit& parse(const Unit& from, const Token& path) {
        auto [id,physical]=resolve(from,path);
        return parse_resolved(from,path,id,physical);
    }
    Unit& parse_resolved(const Unit& from,const Token& path,const std::string& id,const std::filesystem::path& physical) {
        if(auto found=units.find(id);found!=units.end())return *found->second;
        if(auto found=parsed.find(id);found!=parsed.end())return *found->second;
        auto overlay=overrides.find(id);
        std::unique_ptr<Unit> unit;
        try { unit = std::make_unique<Unit>(package_source(id) ? package_snapshot(id) :
            reader ? reader(physical,id) : overlay == overrides.end() ? read_source(physical) : *overlay->second, id); }
        catch (const SourceReadError&) { error(from.source, path, Code::import_read, "Cannot read imported file '" + id + "'.", "Check source file permissions and storage availability."); }
        catch (const CompilationError& failure) {
            auto records = failure.diagnostics();
            for (auto& record : records) record.primary.module_identity = id;
            throw CompilationError(std::move(records));
        }
        auto* result = unit.get(); parsed.emplace(id, std::move(unit));
        diagnostics.bind(result->source, id);
        auto before = diagnostics.size();
        { observation::Scope timing(observation::Phase::lex_parse);
          parse_unit(*result,diagnostics,id,conditions_for(id)); }
        result->parse_failed |= diagnostics.size() != before;
        if(id==console::source_module)for(auto& structure:result->structures)if(structure.token.text=="terminal_session"){structure.definition->native_resource=true;structure.definition->native_provider="void.console.v1";}
        if(imports_only)return *result;
        const auto declaration_path=package_source(id)?"/"+std::string(package_relative_source(id)):id;
        if(!result->c_provider.empty() && (!primitives || !primitives->physical_source() ||
           declaration_path!=primitives->module() || result->c_provider!=primitives->provider()))
            error(result->source,path,Code::unsupported_type,"Physical native declarations require the application's matching admitted descriptor.");
        if (primitives && declaration_path == primitives->module()) {
            if(primitives->physical_source()) {
                if(result->c_provider!=primitives->provider())
                    error(result->source,path,Code::unsupported_type,"Physical native declaration provider differs from the admitted descriptor.");
                if(!physical_primitive_module.empty() && physical_primitive_module!=id)
                    error(result->source,path,Code::import_path,"More than one physical C declaration module matches the descriptor.");
                physical_primitive_module=id;
                if(!result->imports.empty() || !result->constants.empty() || !result->globals.empty() ||
                   !result->generic_functions.empty() || !result->generic_structures.empty() ||
                   result->functions.size()!=primitives->bindings().size() ||
                   result->structures.size()!=primitives->records().size()+primitives->resources().size())
                    error(result->source,path,Code::unsupported_type,"Physical C declaration inventory differs from the admitted descriptor.");
                std::set<std::string> names;
                for(const auto& structure:result->structures) {
                    if(structure.base_type || !structure.generics.empty())
                        error(result->source,structure.token,Code::unsupported_type,"Physical native declarations cannot extend types or introduce generics.");
                    for(const auto& field:structure.fields)if(field.expression &&
                        (field.name!="_seal" || field.expression->kind!=Expr::Kind::literal || field.expression->token.text!="0"))
                        error(result->source,field.token,Code::unsupported_type,"Physical native declarations cannot contain executable field initializers.");
                }
                for(const auto& f:result->functions) {
                    if(!names.insert(f.token.text).second || f.receiver || f.asynchronous || !f.generics.empty() ||
                       f.body.size()!=1 || f.body.front().kind!=Statement::Kind::no_op ||
                       std::none_of(primitives->bindings().begin(),primitives->bindings().end(),[&](const auto& b){return b.name==f.token.text;}) ||
                       std::any_of(f.parameters.begin(),f.parameters.end(),[](const auto& p){return bool(p.default_expression);}))
                        error(result->source,f.token,Code::unsupported_type,"Physical C bindings require exact descriptor declarations with a pass body.");
                }
                result->c_primitives=primitives->at_source_module(id);
            } else result->c_primitives = primitives;
            for (auto& structure : result->structures)
                for (const auto& resource : primitives->resources())
                    if (structure.token.text == resource.name) {
                        structure.definition->native_resource = true;
                        structure.definition->native_provider = primitives->provider();
                    }
            const auto& bindings = primitives->bindings();
            for (auto& f : result->functions)
                for (unsigned index = 0; index < bindings.size(); ++index)
                    if (f.token.text == bindings[index].name) f.c_primitive = index;
        }
        if(native && id==native->module){for(auto& structure:result->structures){structure.definition->native_resource=true;structure.definition->native_provider=native->library;}for(auto& f:result->functions)for(const auto& b:native->bindings)if(f.token.text==b.name)f.native_symbol=b.symbol;}
        return *result;
    }
    Unit& load(const Unit& from, const Token& path) {
        auto [id,physical]=resolve(from,path);
        if(auto found=units.find(id);found!=units.end())return *found->second;
        Depth guard(depth,from.source,path);
        auto& result=parse_resolved(from,path,id,physical);
        // Publish before traversing imports: function-only declaration cycles are safe.
        units.insert(parsed.extract(result.identity));
        for (auto& import : result.imports) {
            try {
                auto& target = load(result, import.path);
                import.target = target.identity;
            } catch (const CompilationError& failure) {
                diagnostics.add(failure); result.failed_import = true;
                auto qualifier = import_alias(import);
                result.failed_qualifiers.insert(qualifier);
            }
        }
        return result;
    }
};
#include "capabilities.inc"
#include "construction.inc"
#include "inheritance.inc"
ir::Function member_header(const Unit& u,const Function& f) {
    ir::Function header;header.name=f.symbol;header.module_identity=u.identity;header.method_name=f.receiver?(f.concrete_method_name.empty()?f.token.text:f.concrete_method_name):std::string{};
    header.static_member=bool(f.static_owner) || (f.global_initializer && !f.lexical_owner.empty());
    header.asynchronous=f.asynchronous;header.task=f.task;header.bound_receiver=f.bound_receiver;header.closure=f.anonymous;header.accessor=f.accessor;header.receiver=static_cast<bool>(f.receiver);header.initializer=f.initializer;header.visibility=f.visibility;header.result=f.result;header.virtual_slot=f.virtual_slot;header.receiver_mutates=f.receiver_mutates;
    header.lexical_owner=f.receiver?Type::key(f.receiver->type):f.lexical_owner;header.lexical_parent=f.lexical_parent;header.operator_owner=f.operator_owner;header.operator_glyph=f.operator_glyph;
    header.parameters.clear();if(f.receiver)header.parameters.push_back(f.receiver->type);
    for(const auto& p:f.parameters)header.parameters.push_back(p.type);
    return header;
}
ir::Module member_catalog(const std::map<std::string,std::unique_ptr<Unit>>& units,const std::string& entry) {
    ir::Module m;m.root_identity=entry;
    for(const auto& [id,u]:units) {
        auto& imports=m.source_imports[id];for(const auto& i:u->imports) if(!i.target.empty()) imports.insert(i.target);
        for(const auto& s:u->structures) m.nominal_types.push_back(Type::nominal(s.definition));
        for(const auto& f:u->functions) if(!f.parse_failed) {
            auto header=member_header(*u,f);
            if(f.interface_requirement) m.interface_requirements.push_back(std::move(header));else m.functions.push_back(std::move(header));
        }
    }
    return m;
}
void validate_graph(std::map<std::string, std::unique_ptr<Unit>>& units, const std::string& entry, Diagnostics& diagnostics, Unit* library_scope=nullptr) {
    observation::Scope timing(observation::Phase::semantic_analysis);
    const auto parse_errors=diagnostics.size();
    for(const auto& [id,u]:units)if(!u->c_provider.empty() && !u->c_primitives)
        error(u->source,{TokenKind::name,"native",0},Code::unsupported_type,
              "Physical native declarations require the application's matching admitted descriptor.");
    for(auto& [id,u]:units)if(u->application_body) {
        try {
            if(id!=entry)error(u->source,{TokenKind::name,"",0},Code::entry,"Imported modules cannot contain executable top-level statements.");
            for(const auto& f:u->functions)if(f.token.text=="main" && !f.receiver && !f.static_owner && f.parameters.empty() && f.generics.empty() && !f.asynchronous && (f.result==Type::none || f.result==Type::i32))error(u->source,f.token,Code::entry,"Executable top-level statements compete with main().");
        } catch(const CompilationError& failure){diagnostics.add(failure);}
    }
    auto arena = std::make_shared<ir::TypeArena>();
    for (auto& [id,unit] : units) for (auto& structure : unit->structures) {
        structure.definition->self = structure.definition;
        structure.definition->arena = arena;
        arena->definitions.push_back(structure.definition);
    }
    // Every external resolved Type aliases this table; field edges are nonowning.

    // Discover every signature before checking any body, including cyclic imports.
    for (auto& [id, unit] : units) {
    std::vector<Function*> declarations;for(auto& f:unit->functions)declarations.push_back(&f);for(auto& f:unit->generic_functions)declarations.push_back(&f);
    for(auto* declaration:declarations) {auto& f=*declaration;
        if (f.receiver || f.static_owner || !f.operator_glyph.empty()) continue;
        f.compile_owner=unit.get();
        f.symbol = id + "::" + f.token.text;
        auto [previous, inserted] = unit->declarations.emplace(f.token.text, &f);
        if (!inserted) {
            unit->invalid_declarations.insert(f.token.text);
            try { error(unit->source, f.token, Code::duplicate, "Duplicate function '" + f.token.text + "'.", {}, {{span(unit->source, previous->second->token), "Previously declared"}}); }
            catch (const CompilationError& failure) { diagnostics.add(failure); }
        }
    }
    }
    auto& root = library_scope ? *library_scope : *units.at(entry);
    // Every graph entry path (manifest, library, independent editor document
    // and legacy entry) needs the same context for qualified type completion.
    root.editor=diagnostics.editor;
    for(auto& [id,u]:units)for(auto& s:u->generic_structures) {
        s.definition->identity=id+"::"+s.token.text;s.definition->module_identity=id;s.definition->visibility=s.visibility;
        if(u->declarations.contains(s.token.text) || !u->structure_names.emplace(s.token.text,&s).second)
            error(u->source,s.token,Code::duplicate,"Duplicate generic declaration '"+s.token.text+"'.");
    }
    for (auto& [id,unit] : units) for (auto& structure : unit->structures) {
        auto name = structure.token.text; structure.definition->identity = id + "::" + name;
        structure.definition->source_offset=structure.token.offset;structure.definition->module_identity=id;structure.definition->visibility=structure.visibility;
        if (unit->declarations.contains(name) || !unit->structure_names.emplace(name,&structure).second) {
            unit->invalid_declarations.insert(name);
            try { error(unit->source,structure.token,Code::duplicate,"Duplicate declaration '" + name + "'."); }
            catch (const CompilationError& failure) { diagnostics.add(failure); }
        }
    }
    for (auto& [id, unit] : units) for (auto& c : unit->constants) {
        auto& token = c.declaration.token;
        auto previous = unit->constant_names.find(token.text);
        if (unit->structure_names.contains(token.text) || unit->declarations.contains(token.text) || previous != unit->constant_names.end()) {
            auto location = unit->structure_names.contains(token.text) ? unit->structure_names.at(token.text)->token : unit->declarations.contains(token.text) ? unit->declarations.at(token.text)->token : previous->second->declaration.token;
            try { error(unit->source, token, Code::duplicate, "Duplicate declaration '" + token.text + "'.", {}, {{span(unit->source, location), "Previously declared"}}); }
            catch (const CompilationError& failure) { diagnostics.add(failure); }
            c.state = 3; unit->invalid_declarations.insert(token.text);
        } else unit->constant_names.emplace(token.text, &c);
    }
    for (auto& [id, unit] : units) for (auto& value : unit->globals) {
        value.identity = id + "::" + value.token.text;
        if (unit->constant_names.contains(value.token.text) || unit->structure_names.contains(value.token.text) || unit->declarations.contains(value.token.text) || !unit->global_names.emplace(value.token.text, &value).second)
            error(unit->source, value.token, Code::duplicate, "Duplicate declaration '" + value.token.text + "'.");
    }
    for (auto& [id, unit] : units) {
        if (unit.get() != &root && same_library(id,root.identity)) for (const auto& [name, value] : root.global_names) if (value->visibility != "private") {
            if (unit->global_names.contains(name) || unit->constant_names.contains(name) || unit->structure_names.contains(name) || unit->declarations.contains(name))
                error(unit->source, unit->global_names.contains(name) ? unit->global_names.at(name)->token : Token{TokenKind::name, name, 0}, Code::duplicate, "Declaration shadows project-root global '" + name + "'.");
        }
        for (const auto& [name, value] : unit->global_names) if ((same_library(id, root.identity) && root.constant_names.contains(name) && !root.constant_names.at(name)->file_private) || (same_library(id, root.identity) && root.structure_names.contains(name) && root.structure_names.at(name)->visibility != "private"))
            error(unit->source, value->token, Code::duplicate, "Global shadows a project-root declaration.");
        if (unit.get() != &root && same_library(id,root.identity)) for (const auto& [name, c] : root.constant_names) if (!c->file_private) {
            try {
            if (unit->constant_names.contains(name)) error(unit->source, unit->constant_names.at(name)->declaration.token, Code::duplicate, "Declaration shadows project-root constant '" + name + "'.", {}, {{span(root.source, c->declaration.token), "Previously declared"}});
            if (unit->declarations.contains(name)) error(unit->source, unit->declarations.at(name)->token, Code::duplicate, "Declaration shadows project-root constant '" + name + "'.", {}, {{span(root.source, c->declaration.token), "Previously declared"}});
            } catch (const CompilationError& failure) { diagnostics.add(failure); unit->invalid_declarations.insert(name); }
        }
        if (unit.get() != &root && same_library(id,root.identity)) for (const auto& [name,type] : root.structure_names) if (type->visibility != "private") {
            auto duplicate = [&](const Token& token) {
                try { error(unit->source,token,Code::duplicate,"Declaration shadows project-root value type '" + name + "'.",{},{{span(root.source,type->token),"Previously declared"}}); }
                catch (const CompilationError& failure) { diagnostics.add(failure); unit->invalid_declarations.insert(name); }
            };
            if (unit->structure_names.contains(name)) duplicate(unit->structure_names.at(name)->token);
            if (unit->constant_names.contains(name)) duplicate(unit->constant_names.at(name)->declaration.token);
            if (unit->declarations.contains(name)) duplicate(unit->declarations.at(name)->token);
        }
        if (unit.get() != &root && same_library(id,root.identity)) for (const auto& [name,c] : root.constant_names) if (!c->file_private && unit->structure_names.contains(name)) {
            try { error(unit->source,unit->structure_names.at(name)->token,Code::duplicate,"Declaration shadows project-root constant '" + name + "'.",{},{{span(root.source,c->declaration.token),"Previously declared"}}); }
            catch (const CompilationError& failure) { diagnostics.add(failure); unit->invalid_declarations.insert(name); }
        }
        for (const auto& import : unit->imports) {
            if (import.target.empty()) continue;
            if (units.at(import.target)->parse_failed) unit->failed_import = true;
            auto alias = import_alias(import);
            try {
            if (!import.explicit_alias && !identifier(alias)) continue;
            if (unit->global_names.contains(alias) || (same_library(id,root.identity) && root.global_names.contains(alias) && root.global_names.at(alias)->visibility!="private") || unit->structure_names.contains(alias) || (same_library(id,root.identity) && root.structure_names.contains(alias) && root.structure_names.at(alias)->visibility != "private") || (import.explicit_alias && unit->declarations.contains(alias)) || unit->constant_names.contains(alias) || (same_library(id,root.identity) && root.constant_names.contains(alias) && !root.constant_names.at(alias)->file_private))
                error(unit->source, import.alias, Code::alias_conflict, "Import alias '" + alias + "' conflicts with " + (unit->declarations.contains(alias) ? "a function declaration." : "a constant declaration."), "Choose a distinct explicit alias with as.");
            auto* target = units.at(import.target).get();
            auto& targets = unit->qualifiers[alias];
            bool repeated = std::find(targets.begin(), targets.end(), target) != targets.end();
            bool different = std::any_of(targets.begin(), targets.end(), [&](Unit* previous) { return previous != target; });
            if (different && (import.explicit_alias || unit->explicit_qualifiers.contains(alias))) {
                std::vector<RelatedLocation> related;
                for (const auto& previous : unit->imports) {
                    if (&previous == &import) break;
                    auto previous_alias = import_alias(previous);
                    if (previous_alias == alias && !previous.target.empty()) { related.push_back({span(unit->source, previous.alias), "Previously imported"}); break; }
                }
                error(unit->source, import.alias, Code::alias_conflict, "Conflicting import alias '" + alias + "'.", "Give these modules different aliases using as.", std::move(related));
            }
            if (import.explicit_alias) unit->explicit_qualifiers.insert(alias);
            if (!repeated) targets.push_back(target);
            } catch (const CompilationError& failure) { diagnostics.add(failure); unit->failed_qualifiers.insert(alias); }
        }
    }
    // Publish stable generic method identities before layout expressions can
    // request their specializations. Full owner/type/constraint validation stays
    // in the existing generic declaration pass; no dependent body is checked here.
    for(auto& [id,u]:units)for(auto& f:u->generic_functions)if(f.receiver && f.receiver->type.definition) {
        auto spelling=f.receiver->type.definition->name;
        for(const auto& application:u->applications)if(application.definition==f.receiver->type.definition){spelling=application.name;break;}
        if(auto owner=u->structure_names.find(spelling);owner!=u->structure_names.end())
            f.symbol=owner->second->definition->identity+"::method::"+f.token.text;
    }
    // Extents may request existing pure value constants before the ordinary
    // body/default pass. Discover runtime-only init presence first, so staged
    // resolution cannot accidentally treat such construction as a pure template.
    for(auto& [id,u]:units) for(auto& f:u->functions)
        if(f.receiver && f.token.text=="init" && f.receiver->type.kind==Type::unresolved) {
            auto found=u->structure_names.find(f.receiver->type.definition->name);
            if(found!=u->structure_names.end()) found->second->initializer=&f;
        }
    root.member_catalog=std::make_shared<ir::Module>(member_catalog(units,entry));
    std::function<void(Unit&,Function&)> prepare_function;
    std::function<void(Type,const Token&)> prepare_inline;
    bool headers_ready=false;
    std::function<void(Unit&,Structure&)> layout;
    std::function<void(Unit&,Structure&)> defaults;
    std::function<void(Unit&,Constant&)> evaluate;
    SemanticContext semantics{units, root, evaluate, defaults, diagnostics};
    CompileEvaluation compile_evaluation;root.compile_evaluation=&compile_evaluation;
    struct ClearCompileEvaluation {Unit& root;~ClearCompileEvaluation(){root.compile_evaluation=nullptr;}} clear_compile_evaluation{root};
    std::set<const ir::ValueType*> evaluated_numeric_types;
    compile_evaluation.numeric_definition=[&](Type type) {
        if(evaluated_numeric_types.contains(type.definition.get()))return;
        for(auto& [id,u]:units)for(auto& structure:u->structures)if(structure.definition==type.definition) {
            u->resolve_numeric(structure);evaluated_numeric_types.insert(type.definition.get());return;
        }
    };
    compile_evaluation.prepare=[&](Function& f,const SourceFile& source,const Token& site,bool body) {
        if(!f.compile_time || !f.compile_owner)error(source,site,Code::constant_call,"Only compile func source declarations can be evaluated.");
        if(f.compile_failed || f.parse_failed)throw Poison{};
        if(!f.prepared)prepare_function(*f.compile_owner,f);
        if(f.result==error_type)throw Poison{};
        semantics.prepare_compile_body(*f.compile_owner, f, source, site, body);
    };
    for(auto& [id,u]:units)u->resolve_numeric=[&,owner=u.get()](Structure& structure) {
        if(!structure.definition->enum_backing || structure.numeric_state==2)return;
        if(structure.numeric_state==3)throw Poison{};
        if(structure.numeric_state==1)error(owner->source,structure.token,Code::constant_cycle,"Circular numeric enum discriminant dependency.");
        Depth guard(diagnostics.semantic_depth(),owner->source,structure.token,64);
        structure.numeric_state=1;
        try {
            std::map<std::uint64_t,Token> discriminants;
            auto backing=Type(*structure.definition->enum_backing);
            auto& members=structure.definition->numeric_members;
            for(unsigned n=0;n<members.size();++n) {
                auto& expression=structure.numeric_expressions.at(n);std::uint64_t bits=1;
                if(expression) {
                    // Preserve flags' earlier-member vocabulary inside the ordinary
                    // typed expression; no separate arithmetic evaluator remains.
                    if(structure.definition->enum_flags) {
                        std::vector<Expr*> pending{expression.get()};
                        while(!pending.empty()) {
                            auto& e=*pending.back();pending.pop_back();
                            if(e.kind==Expr::Kind::name && e.qualifier.empty())for(unsigned previous=0;previous<n;++previous)
                                if(members[previous].name==e.token.text){e.kind=Expr::Kind::literal;e.type=backing;e.value=members[previous].bits;e.target=structure.definition->identity+"::variant::"+members[previous].name;break;}
                            for(auto& part:e.parts)pending.push_back(part.get());
                        }
                    }
                    auto type=semantics.pure_expression(*owner,*expression,backing);
                    if(type==error_type)throw Poison{};
                    if(type!=backing)error(owner->source,expression->token,Code::type_mismatch,"Enum discriminant requires exactly its integer backing type.");
                    bits=constant_value(owner->source,*expression,&compile_evaluation).bits;
                } else {
                    std::uint64_t used=0;for(unsigned previous=0;previous<n;++previous)used|=members[previous].bits;
                    while(used){if(bits>(numeric::limit(Type::u64,true)>>1))error(owner->source,structure.case_tokens[n],Code::integer_range,"No automatic flag bit remains.");bits<<=1;used>>=1;}
                    if(numeric::width(backing)<64 && bits>>numeric::width(backing))error(owner->source,structure.case_tokens[n],Code::integer_range,"Automatic flag value exceeds the backing width.");
                }
                auto [previous,inserted]=discriminants.emplace(bits,structure.case_tokens[n]);
                if(!inserted)error(owner->source,structure.case_tokens[n],Code::duplicate,"Duplicate numeric enum discriminant.",{},{{span(owner->source,previous->second),"Previously declared value."}});
                members[n].bits=bits;
            }
            structure.numeric_state=2;
        }catch(...){structure.numeric_state=3;throw;}
    };
    std::vector<std::string> dependency;
    evaluate = [&](Unit& owner, Constant& c) {
        if (c.state == 2) return;
        if (c.state == 3) throw Poison{};
        if (dependency.size() >= 128) error(owner.source, c.declaration.token, Code::nesting, "Constant dependency depth limit exceeded.", "Reduce the dependency chain between constants.");
        auto key = owner.identity + "::" + c.declaration.name;
        if (c.state == 1) {
            std::string chain;
            for (const auto& item : dependency) chain += item + " -> ";
            error(owner.source, c.declaration.token, Code::constant_cycle, "Circular constant dependency: " + chain + key + ".");
        }
        c.state = 1; dependency.push_back(key);
        try {
            semantics.analyze_constant(owner,c);
            c.aggregate_value = constant_value(owner.source, *c.declaration.expression,root.compile_evaluation); c.value = c.aggregate_value.bits;
            c.state = 2; dependency.pop_back();
        } catch (...) { c.state = 3; dependency.pop_back(); throw; }
    };
    defaults = [&](Unit& owner, Structure& structure) {
        if (structure.definition->interface_type) {structure.default_state=2;return;}
        if(structure.layout_state==1) error(owner.source,structure.token,Code::constant_cycle,"Circular layout/constant dependency.");
        if(structure.layout_state==0) layout(owner,structure);
        if (structure.layout_state != 2) throw Poison{};
        if (structure.default_state == 2) return;
        if (structure.default_state == 3) throw Poison{};
        if (structure.default_state == 1) error(owner.source,structure.token,Code::constant_cycle,"Circular field default dependency for '" + structure.token.text + "'.");
        Depth guard(diagnostics.semantic_depth(),owner.source,structure.token,128);
        structure.default_state = 1;
        try {
            if (auto base=structure.definition->base.lock()) for (auto& [id,u]:units) for (auto& parent:u->structures) if (parent.definition==base) {
                defaults(*u,parent); structure.defaults.fields=parent.defaults.fields;
            }
            semantics.field_defaults(owner,structure);
            structure.default_state = 2;
        } catch (...) { structure.default_state = 3; throw; }
    };
    auto array_extent=[&](Unit& owner,ArraySyntax& syntax) {
        std::optional<std::uint64_t> extent;
            if(syntax.extent) {
                auto type=semantics.pure_expression(owner,*syntax.extent);
                if(type==error_type) throw Poison{};
                if(!numeric::integer(type)) error(owner.source,syntax.extent->token,Code::unsupported_type,"Fixed-array extent requires a pure nonnegative integer module-constant expression.");
                const auto value=constant_value(owner.source,*syntax.extent,root.compile_evaluation).bits;
                if(numeric::signed_integer(type) && (value & (std::uint64_t{1}<<(numeric::width(type)-1))))
                    error(owner.source,syntax.extent->token,Code::integer_range,"Fixed-array extent cannot be negative.");
                if(value>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
                    error(owner.source,syntax.extent->token,Code::integer_range,"Array extent is not representable as an i64 count.");
                extent=value;
            }
        return extent;
    };
    std::map<std::string,std::shared_ptr<ir::ValueType>> canonical_arrays;
    for(auto& [id,u]:units)u->make_dictionary=[&](Type key,Type value) {
        auto raw=ir::dictionary_type(key,value);auto& canonical=canonical_arrays[Type::key(raw)];if(!canonical)canonical=std::const_pointer_cast<ir::ValueType>(raw.definition);
        return ir::with_readonly_paths(Type(canonical,Type::dictionary),ir::readonly_paths(raw));
    };
    for(auto& [id,u]:units) u->resolve_array=[&,owner=u.get()](Type raw,const Token&)->Type {
        auto found=std::find_if(owner->arrays.begin(),owner->arrays.end(),[&](const auto& a){return a.definition==raw.definition;});
        if(found==owner->arrays.end()) return raw; // Already canonical.
        auto& syntax=*found;
        if(syntax.state==2) {auto result=syntax.resolved;result.readonly=raw.readonly;return result;}
        if(syntax.state==3) throw Poison{};
        if(syntax.state==1) error(owner->source,syntax.token,Code::constant_cycle,"Circular array extent/type dependency.");
        Depth guard(diagnostics.semantic_depth(),owner->source,syntax.token,128);syntax.state=1;
        try {
            auto element=resolve_type(syntax.element,*owner,root,units,syntax.token);
            if(syntax.key) {
                auto key=resolve_type(*syntax.key,*owner,root,units,syntax.token);
                if(key.kind==Type::aggregate)for(auto& [id,u]:units)for(auto& child:u->structures)if(child.definition==key.definition)layout(*u,child);
                check_dictionary_key(owner->source,syntax.token,key,units);
                syntax.resolved=owner->make_dictionary(key,element);syntax.state=2;auto result=syntax.resolved;result.readonly=raw.readonly;return result;
            }
            auto extent=array_extent(*owner,syntax);
            if(extent && (element.kind==Type::aggregate || element.kind==Type::fixed_array))prepare_inline(element,syntax.token);

            // Fixed ownership remains finite; growable edges have pointer-sized storage.
            ir::CompactValueShape shape;
            try {shape=extent ? ir::compact_value_shape(element) : ir::CompactValueShape{0,sizeof(void*),1};}
            catch(const std::length_error& failure) {error(owner->source,syntax.token,Code::limit,failure.what());}
            catch(const std::invalid_argument& failure) {error(owner->source,syntax.token,Code::unsupported_type,failure.what());}
            if(shape.depth>=128) error(owner->source,syntax.token,Code::nesting,"Array value shape depth limit exceeded.");
            if(shape.template_nodes>=65536) error(owner->source,syntax.token,Code::limit,"Array default-template analysis work exhausted.");
            if(extent && shape.scalar_bytes && *extent>std::numeric_limits<std::uint64_t>::max()/shape.scalar_bytes)
                error(owner->source,syntax.token,Code::limit,"Array logical scalar extent is not representable in 64 bits.");
            const auto key=ir::array_identity(element,extent);
            auto& canonical=canonical_arrays[key];
            if(!canonical) {
                canonical=std::make_shared<ir::ValueType>();canonical->identity=key;canonical->name=key;
                canonical->element=ir::FieldType(element);canonical->extent=extent;
                canonical->self=canonical;canonical->arena=arena;arena->definitions.push_back(canonical);
            }
            syntax.resolved=Type(canonical,extent ? Type::fixed_array : Type::growable_array);syntax.state=2;
            auto result=syntax.resolved;result.readonly=raw.readonly;return result;
        } catch(...) {syntax.state=3;throw;}
    };
    auto environment=std::make_shared<ir::ValueType>();environment->name="$environment";environment->identity="$void::"+environment->name;environment->managed=true;environment->visibility="private";
    environment->self=environment;environment->arena=arena;arena->definitions.push_back(environment);
    std::map<std::string,std::shared_ptr<ir::ValueType>> canonical_tasks;
    for(auto& [id,u]:units)u->make_task=[&](Type result) {
        auto eventual=result;result=ir::with_readonly_paths(result,{});
        auto key="$task:"+std::to_string(result.kind)+":"+Type::key(result)+(result.nullable?"?":"")+":"+ir::capability_key(result);
        auto& d=canonical_tasks[key];if(!d){d=std::make_shared<ir::ValueType>();d->identity=d->name=key;d->managed=true;d->visibility="private";d->task_result=ir::FieldType(result);
            d->fields={{"$status",Type::i32,key},{"$result",result==Type::none?Type::boolean:result,key}};d->self=d;d->arena=arena;arena->definitions.push_back(d);root.synthetic_types.emplace_back(d,Type::managed_ref);}
        return ir::with_field_type(Type(d,Type::managed_ref),1,eventual);
    };
    std::map<std::string,std::shared_ptr<ir::ValueType>> canonical_callables;
    for(auto& [id,u]:units) {
        u->make_callable=[&](const std::vector<Type>& parameters,Type result)->Type {
            std::string key="$func:";for(auto type:parameters){const auto part=ir::array_identity(type,{})+":"+ir::capability_key(type);key+=std::to_string(part.size())+":"+part;}key+="->"+ir::array_identity(result,{})+":"+ir::capability_key(result);
            auto& canonical=canonical_callables[key];
            if(!canonical){if(canonical_callables.size()==1)root.synthetic_types.emplace_back(environment,Type::managed_ref);canonical=std::make_shared<ir::ValueType>();canonical->identity=canonical->name=key;canonical->callable=true;canonical->call_result=ir::FieldType(result);
                canonical->display_name="func(";for(unsigned n=0;n<parameters.size();++n){if(n)canonical->display_name+=", ";canonical->display_name+=ir::type_name(parameters[n]);canonical->call_parameters.emplace_back(parameters[n]);}canonical->display_name+=")";if(result!=Type::none)canonical->display_name+=" -> "+ir::type_name(result);
                Type nullable_environment(environment,Type::managed_ref);nullable_environment.nullable=true;canonical->fields={{"$function",Type::u64,key},{"$environment",nullable_environment,key}};
                canonical->self=canonical;canonical->arena=arena;arena->definitions.push_back(canonical);root.synthetic_types.emplace_back(canonical);
            }
            return Type(canonical);
        };
        u->resolve_callable=[&,owner=u.get()](Type raw,const Token& token)->Type {
            auto syntax=std::find_if(owner->function_types.begin(),owner->function_types.end(),[&](const auto& value){return value.definition==raw.definition;});
            if(syntax==owner->function_types.end())return raw;
            if(syntax->resolved!=Type::none)return syntax->resolved;
            Depth guard(diagnostics.semantic_depth(),owner->source,token,128);std::vector<Type> parameters;
            for(auto type:syntax->parameters)parameters.push_back(resolve_type(type,*owner,root,units,token));
            return syntax->resolved=owner->make_callable(parameters,resolve_type(syntax->result,*owner,root,units,token));
        };
    }
    std::map<std::string,Type> cleanup_records;
    for(auto& [id,u]:units)u->make_cleanup=[&,owner=u.get()]()->Type{
        if(auto found=cleanup_records.find(owner->identity);found!=cleanup_records.end())return found->second;
        auto definition=std::make_shared<ir::ValueType>();definition->identity=owner->identity+"::$cleanup";definition->name="$cleanup";definition->module_identity=owner->identity;definition->visibility="private";definition->managed=true;definition->self=definition;definition->arena=arena;arena->definitions.push_back(definition);Type node(definition,Type::managed_ref);auto next=node;next.nullable=true;
        definition->fields.push_back({"$previous",next,definition->identity});definition->fields.push_back({"$handler",owner->make_callable({},Type::none),definition->identity});definition->fields.push_back({"$active",Type::boolean,definition->identity});owner->synthetic_types.push_back(node);cleanup_records.emplace(owner->identity,node);return node;
    };
    std::map<std::string,Type> capture_records;
    for(auto& [id,u]:units)u->make_capture_record=[&,owner=u.get()](const std::string& identity,Type value,bool cell)->Type {
        if(auto found=capture_records.find(identity);found!=capture_records.end())return found->second;
        auto definition=std::make_shared<ir::ValueType>();definition->identity=identity;definition->module_identity=owner->identity;definition->name=identity.substr(owner->identity.size()+2);definition->managed=true;definition->visibility="private";
        if(cell)definition->fields.push_back({"$value",value,identity});else definition->base=environment;
        definition->self=definition;definition->arena=arena;arena->definitions.push_back(definition);Type type(definition,Type::managed_ref);owner->synthetic_types.push_back(type);capture_records.emplace(identity,type);return type;
    };
    std::map<std::string,std::shared_ptr<ir::ValueType>> canonical_results;
    auto make_result=[&](Type success,Type failure)->Type {
        const auto key="$result:"+ir::array_identity(success,{})+":"+ir::array_identity(failure,{});auto& canonical=canonical_results[key];
        if(!canonical){canonical=std::make_shared<ir::ValueType>();canonical->identity=canonical->name=key;canonical->result_type=true;canonical->display_name="result<"+ir::type_name(success)+", "+ir::type_name(failure)+">";canonical->cases={{"Ok",1,1},{"Err",2,1}};canonical->fields={{"$tag",Type::i32,key},{"$ok",ir::with_readonly_paths(success,{}),key},{"$err",ir::with_readonly_paths(failure,{}),key}};canonical->self=canonical;canonical->arena=arena;arena->definitions.push_back(canonical);}
        return ir::with_field_type(ir::with_field_type(Type(canonical),1,success),2,failure);
    };
    for(auto& [id,u]:units)u->make_result=make_result;
    std::map<std::string,Type> return_types;
    auto make_return_type=[&](const std::vector<ReturnComponent>& components) {
        std::vector<ir::ValueField> fields;
        for(unsigned n=0;n<components.size();++n)
            fields.push_back({"$"+std::to_string(n),ir::with_readonly_paths(components[n].type,{})});
        auto key=ir::multiple_return_identity(fields);
        auto found=return_types.find(key);
        if(found==return_types.end()) {
            auto definition=std::make_shared<ir::ValueType>();
            definition->identity=key;definition->name="$returns";definition->visibility="private";
            definition->self=definition;definition->arena=arena;
            for(auto& field:fields)field.owner=key;
            definition->fields=std::move(fields);
            arena->definitions.push_back(definition);
            found=return_types.emplace(key,Type(definition)).first;
        }
        auto type=found->second;
        for(unsigned n=0;n<components.size();++n)type=ir::with_field_type(type,n,components[n].type);
        return type;
    };
    std::map<std::string,Type> canonical_options;
    auto make_option=[&](Type payload) {
        auto authority=payload;payload=ir::with_readonly_paths(payload,{});payload.readonly=false;
        const auto key=ir::option_identity(payload);
        if(auto found=canonical_options.find(key);found!=canonical_options.end())return ir::with_field_type(found->second,1,authority);
        auto d=std::make_shared<ir::ValueType>();
        d->identity=d->name=key;d->display_name="option<"+ir::type_name(payload)+">";d->option_type=true;
        d->cases={{"None",1,0},{"Some",1,1}};
        d->fields={{"$tag",Type::i32,key},{"$some",payload,key}};
        d->self=d;d->arena=arena;arena->definitions.push_back(d);
        auto type=Type(d);canonical_options.emplace(key,type);root.synthetic_types.push_back(type);
        return ir::with_field_type(type,1,authority);
    };
    for(auto& [id,u]:units)u->make_option=make_option;
    std::map<std::string,Type> canonical_nullables;
    auto make_nullable=[&](Type payload) {
        if(ir::nullable_type(payload))return payload;
        if(ir::reference_value(payload) || (payload.definition && payload.definition->callable)){payload.nullable=true;return payload;}
        auto authority=payload;payload=ir::with_readonly_paths(payload,{});payload.readonly=false;
        const auto key=ir::nullable_identity(payload);
        auto found=canonical_nullables.find(key);
        if(found==canonical_nullables.end()) {
            auto d=std::make_shared<ir::ValueType>();d->identity=d->name=key;d->visibility="private";d->nullable_value=true;
            d->cases={{"$null",1,0},{"Ok",1,1}};d->fields={{"$tag",Type::i32,key},{"$value",payload,key}};
            d->self=d;d->arena=arena;arena->definitions.push_back(d);auto type=Type(d);
            found=canonical_nullables.emplace(key,type).first;root.synthetic_types.push_back(type);
        }
        return ir::with_field_type(found->second,1,authority);
    };
    for(auto& [id,u]:units)u->make_nullable=make_nullable;

    for(auto& [id,u]:units)u->resolve_result=[&,owner=u.get()](Type raw,const Token& token)->Type {
        auto found=std::find_if(owner->results.begin(),owner->results.end(),[&](const auto& syntax){return syntax.definition==raw.definition;});
        if(found==owner->results.end())return raw;
        if(found->resolved!=Type::none)return found->resolved;
        Depth guard(diagnostics.semantic_depth(),owner->source,token,128);
        auto success=resolve_type(found->success,*owner,root,units,token),failure=resolve_type(found->failure,*owner,root,units,token);
        return found->resolved=make_result(success,failure);
    };
    // Follow physical inline edges only. Growable/managed indirection remains
    // a valid recursion boundary, including nullable elements behind a list.
    prepare_inline=[&](Type root_type,const Token& token) {
        std::vector<Type> pending{root_type};std::set<const ir::ValueType*> seen;
        while(!pending.empty()) {
            auto type=pending.back();pending.pop_back();
            if(type.kind==Type::fixed_array){pending.push_back(ir::array_element(type));continue;}
            if(type.kind!=Type::aggregate || !type.definition || type.definition->callable || !seen.insert(type.definition.get()).second)continue;
            bool owned=false;
            for(auto& [id,owner]:units)for(auto& child:owner->structures)if(child.definition==type.definition){layout(*owner,child);owned=true;}
            if(!owned)for(const auto& field:type.definition->fields)pending.push_back(field.type);
            if(seen.size()>65536)error(root.source,token,Code::limit,"Inline value dependency work exhausted.");
        }
    };
    layout = [&](Unit& owner, Structure& structure) {
        if (structure.definition->interface_type) {structure.layout_state=2;return;}
        if (structure.layout_state == 2) return;
        if (structure.layout_state == 1) error(owner.source,structure.token,Code::unsupported_type,"Recursive value layout or inheritance for '" + structure.token.text + "'.","Use an acyclic nesting of value types.");
        if (structure.layout_state == 3) throw Poison{};
        Depth guard(diagnostics.semantic_depth(),owner.source,structure.token,128);
        structure.layout_state = 1;
        try {
            if(!structure.statics_registered) {
                structure.statics_registered=true;std::set<std::string> names;
                for(const auto& field:structure.fields)names.insert(field.name);
                for(auto& field:structure.static_fields) {
                    if(!names.insert(field.name).second)error(owner.source,field.token,Code::duplicate,"Static field conflicts with another member: "+field.name+".");
                    Global value;value.token=field.token;value.visibility=field.visibility;value.readonly=field.readonly;
                    value.static_owner=Type::nominal(structure.definition);value.identity=Type::key(value.static_owner)+"::static::"+field.name;
                    value.syntax=field.constant_syntax+(field.readonly?":const":":var");
                    Function initializer;initializer.global_initializer=true;initializer.visibility="private";initializer.token=field.token;
                    initializer.token.text="$static$"+structure.definition->name+"$"+field.name;initializer.symbol=owner.identity+"::"+initializer.token.text;
                    initializer.lexical_owner=Type::key(value.static_owner);
                    if(structure.generic_origin)for(unsigned n=0;n<structure.type_arguments.size();++n)initializer.substitutions.emplace(structure.generic_origin->generics[n].token.text,structure.type_arguments[n]);
                    initializer.body_syntax=value.syntax;field.name=field.token.text="$value$"+field.name;initializer.body.push_back(std::move(field));
                    owner.functions.push_back(std::move(initializer));value.initializer=&owner.functions.back();
                    owner.globals.push_back(std::move(value));auto& stored=owner.globals.back();owner.static_fields.emplace(stored.identity,&stored);
                }
            }
            std::set<std::string> names;
            if (structure.base_type) {
                auto t=resolve_type(*structure.base_type,owner,root,units,structure.base_token);
                if (!structure.definition->managed || t.kind!=Type::managed_ref || t.nullable)
                    error(owner.source,structure.base_token,Code::unsupported_type,"Only a managed struct may extend a non-nullable managed struct.");
                Structure* parent=nullptr; Unit* parent_owner=nullptr;
                for (auto& [id,u]:units) for (auto& candidate:u->structures) if (candidate.definition==t.definition) { parent=&candidate; parent_owner=u.get(); }
                if (!parent) throw Poison{};
                if ((structure.visibility=="public" && parent->visibility!="public") || (structure.visibility=="internal" && parent->visibility=="private"))
                    error(owner.source,structure.base_token,Code::visibility,"A derived type cannot expose a less-visible base type.",{},{{span(parent_owner->source,parent->token),"Base type declared here."}});
                layout(*parent_owner,*parent);
                if(parent->definition->native_resource)error(owner.source,structure.base_token,Code::unsupported_type,"Native resource identities cannot be extended or forged.");
                structure.base_type=t; structure.definition->base=parent->definition;
                structure.definition->fields=parent->definition->fields;
                structure.effective_fields=parent->effective_fields;
                structure.expanded_fields=parent->expanded_fields;
                structure.layout_depth=parent->layout_depth+1;
                if(structure.layout_depth>128) error(owner.source,structure.base_token,Code::nesting,"Physical layout depth limit exceeded.","Reduce the combined inheritance and inline-value depth; the current limit is 128.");
                for (const auto& field:structure.definition->fields) names.insert(field.name);
            }
            for (auto& field : structure.fields) {
                if (!names.insert(field.name).second) error(owner.source,field.token,Code::duplicate,"Duplicate field '" + field.name + "'.");
                auto t = resolve_type(*field.annotation,owner,root,units,field.token);
                if(field.event){
                    if(!t.definition || !t.definition->callable || t.nullable || Type(*t.definition->call_result)!=Type::none)error(owner.source,field.token,Code::type_mismatch,"An event requires a non-nullable, no-result function signature.");
                    auto definition=std::make_shared<ir::ValueType>();definition->name=definition->identity=ir::array_identity(t,{});definition->element=ir::FieldType(t);definition->self=definition;definition->arena=arena;arena->definitions.push_back(definition);t=Type(definition,Type::growable_array);
                }
                if (field.using_field && ((t.kind!=Type::aggregate && t.kind!=Type::managed_ref) || t.nullable || (t.definition && (t.definition->callable || !t.definition->cases.empty()))))
                    error(owner.source,field.token,Code::unsupported_type,"A using field requires a non-nullable nominal struct type.","Use an ordinary field for primitives or nullable references.");
                if(t.kind==Type::aggregate && t.definition && (t.definition->nullable_value || t.definition->option_type || t.definition->result_type))prepare_inline(t,field.token);
                std::size_t expanded = 1;
                if(ir::nullable_value(t)) {
                    const auto shape=ir::compact_value_shape(t);
                    expanded=shape.template_nodes;structure.layout_depth=std::max(structure.layout_depth,shape.depth+1);
                    if(structure.layout_depth>128)error(owner.source,field.token,Code::nesting,"Physical layout depth limit exceeded.");
                }
                if(t.kind==Type::fixed_array) {
                    const auto shape=ir::compact_value_shape(t);
                    expanded=shape.template_nodes;structure.layout_depth=std::max(structure.layout_depth,shape.depth+1);
                    if(structure.layout_depth>128) error(owner.source,field.token,Code::nesting,"Physical layout depth limit exceeded.");
                }
                if (t.kind == Type::aggregate) for (auto& [id,u] : units) for (auto& child : u->structures) if (child.definition == t.definition) {
                    layout(*u,child); expanded += child.expanded_fields; structure.layout_depth=std::max(structure.layout_depth,child.layout_depth+1);
                    if(structure.layout_depth>128) error(owner.source,field.token,Code::nesting,"Physical layout depth limit exceeded.","Reduce the combined inheritance and inline-value depth; the current limit is 128.");
                }
                if (expanded > 65536 - structure.expanded_fields) error(owner.source,field.token,Code::nesting,"Expanded value layout limit exceeded.","Reduce nested value duplication; the compiler currently permits 65536 expanded fields per value.");
                structure.expanded_fields += expanded;
                if(field.readonly)t=ir::readonly_view(t);
                field.annotation = t; structure.definition->fields.push_back({field.name,t,structure.definition->identity,field.using_field,field.event,field.visibility,field.readonly,field.token.offset});
                structure.effective_fields.emplace_back(&owner,&field);
            }
            structure.layout_state = 2;
        } catch (...) { structure.layout_state = 3; throw; }
    };
#include "generics.inc"
    prepare_function = [&](Unit& owner,Function& f) {
        auto* unit=&owner;f.compile_owner=&owner;
        if(f.prepared)return;f.prepared=true;
        Diagnostics::Context header_context(diagnostics,f.instantiation_context,std::max(f.specialization_depth,diagnostics.current_specialization_depth()));
        auto resolve = [&](Type& t, const Token& token) {
            try { t = resolve_type(t,*unit,root,units,token); }
            catch (const CompilationError& failure) { diagnostics.add(failure); t = error_type; f.parse_failed = true; }
            catch (const Poison&) { t = error_type; f.parse_failed = true; }
        };
        if(f.returns.empty())resolve(f.result,f.token);
        else {
            if(!f.native_symbol.empty() || f.c_primitive)
                error(unit->source,f.token,Code::unsupported_type,"Native bindings cannot declare multiple VOID return values.");
            for(auto& component:f.returns)resolve(component.type,component.site);
            if(std::none_of(f.returns.begin(),f.returns.end(),[](const auto& component){return component.type==error_type;}))
                f.result=make_return_type(f.returns);
            else f.result=error_type;
        }
        for (auto& parameter : f.parameters) {
            if(parameter.type==Type::none && parameter.default_expression) {
                try {
                    const bool readonly=parameter.type.readonly;
                    parameter.type=semantics.infer_default(owner,*parameter.default_expression,f);
                    if(parameter.type==Type::none || parameter.type==Type::null_value || parameter.type==error_type || parameter.type.kind==Type::unresolved)
                        error(owner.source,parameter.token,Code::type_mismatch,"Default does not determine a concrete parameter type.");
                    parameter.type.readonly=readonly;
                } catch(const CompilationError& failure) {diagnostics.add(failure);parameter.type=error_type;f.parse_failed=true;}
                  catch(const Poison&) {parameter.type=error_type;f.parse_failed=true;}
            }
            resolve(parameter.type,parameter.token);
        }
        if(!f.operator_glyph.empty() && !f.parse_failed)try {
            for(auto& parameter:f.parameters){parameter.type=ir::readonly_view(parameter.type);parameter.type.readonly=true;}
            f.lexical_owner=Type::key(f.operator_owner);
            std::vector<Type> operands;for(const auto& parameter:f.parameters)operands.push_back(parameter.type);
            f.symbol=ir::operator_identity(f.operator_owner,f.operator_glyph,operands);
            f.concrete_name=f.symbol.substr(owner.identity.size()+2);f.display_name="operator "+f.operator_glyph;
            ir::validate_operator_declaration(member_header(owner,f));
            auto previous=std::find_if(owner.functions.begin(),owner.functions.end(),[&](const auto& other){return &other!=&f && other.prepared && other.symbol==f.symbol;});
            if(previous!=owner.functions.end())error(owner.source,f.token,Code::duplicate,"Duplicate operator operand tuple.",{},{{span(owner.source,previous->token),"Previously declared"}});
        }catch(const CompilationError& failure){diagnostics.add(failure);f.parse_failed=true;}
         catch(const std::invalid_argument& reason){try{error(owner.source,f.token,Code::type_mismatch,reason.what());}catch(const CompilationError& failure){diagnostics.add(failure);f.parse_failed=true;}}
        if(f.static_owner) {
            resolve(f.static_owner->type,f.static_owner->token);
            auto t=f.static_owner->type;
            if(t==error_type){f.parse_failed=true;return;}
            if((t.kind!=Type::aggregate && t.kind!=Type::managed_ref) || t.nullable || !t.definition->cases.empty() || t.definition->callable)error(owner.source,f.token,Code::receiver,"Static methods require an owning-module struct type.");
            if(t.definition->module_identity!=owner.identity)error(owner.source,f.token,Code::receiver,"Static methods must be declared in their type's owning module.");
            if(f.virtual_method || f.overriding || f.token.text=="init" || f.token.text=="deinit")error(owner.source,f.token,Code::receiver,"Static methods cannot be lifecycle or virtual methods.");
            f.symbol=Type::key(t)+"::static::"+(f.concrete_method_name.empty()?f.token.text:f.concrete_method_name);
            f.lexical_owner=Type::key(t);
            if(owner.static_fields.contains(f.symbol) || !owner.static_methods.emplace(f.symbol,&f).second)error(owner.source,f.token,Code::duplicate,"Duplicate static member: "+f.token.text+".");
            for(const auto& field:t.definition->fields)if(field.name==f.token.text)error(owner.source,f.token,Code::duplicate,"Static method conflicts with an instance field.");
            if(owner.methods.contains(Type::key(t)+"::method::"+f.token.text))error(owner.source,f.token,Code::duplicate,"Static and instance methods cannot share a name.");
        }
        if (f.receiver) {
            resolve(f.receiver->type,f.receiver->token);
            try {
                auto t = f.receiver->type;
                if (t == error_type) throw Poison{};
                if (((t.kind != Type::aggregate && t.kind != Type::managed_ref) && !(f.interface_requirement && t.kind==Type::interface_ref)) || t.nullable) error(unit->source,f.receiver->token,Code::receiver,"Receiver declarations require a non-nullable struct type.");
                if (!unit->structure_names.contains(t.definition->name) || unit->structure_names.at(t.definition->name)->definition != t.definition)
                    error(unit->source,f.receiver->token,Code::receiver,"Methods must be declared in their value type's owning module; extension methods are deferred.");
                if(t.kind==Type::interface_ref && !f.interface_requirement) error(unit->source,f.token,Code::receiver,"Interfaces declare requirements, not receiver implementations.");
                if(unit->static_fields.contains(Type::key(t)+"::static::"+f.token.text) || unit->static_methods.contains(Type::key(t)+"::static::"+f.token.text))error(unit->source,f.token,Code::duplicate,"Instance method conflicts with a static member.");
                f.initializer = f.token.text == "init";
                if(f.token.text=="deinit" && (t.kind!=Type::managed_ref || f.result!=Type::none || !f.parameters.empty()))error(unit->source,f.token,Code::initializer_deferred,"deinit requires a managed receiver, no arguments and no result.");
                if (f.initializer && f.result != Type::none && !ir::fallible_initializer_result(f.result))
                    error(unit->source,f.token,Code::type_mismatch,"An initializer result must be result<bool, e>; use Ok(true) for successful completion.");
                for (const auto& field : t.definition->fields) if (field.name == f.token.text)
                    error(unit->source,f.token,Code::duplicate,"Method name conflicts with field '" + f.token.text + "'.");
                if(f.interface_requirement) {
                    auto check=[&](Type type,const Token& site) {
                        if(ir::nullable_value(type))type=ir::nullable_payload(type);
                        if(type.definition && ((f.visibility=="public" && type.definition->visibility!="public") ||
                                               (f.visibility=="internal" && type.definition->visibility=="private")))
                            error(unit->source,site,Code::visibility,"An interface requirement cannot expose a less-visible type.");
                    };
                    for(const auto& parameter:f.parameters)check(parameter.type,f.token);
                    if(f.returns.empty())check(f.result,f.token);
                    else for(const auto& component:f.returns)check(component.type,component.site);
                }
                f.symbol = t.definition->identity + "::method::" + (f.concrete_method_name.empty()?f.token.text:f.concrete_method_name);
                if (!unit->methods.emplace(f.symbol,&f).second) error(unit->source,f.token,Code::duplicate,"Duplicate receiver method '" + f.token.text + "'.");
                if (f.initializer) unit->structure_names.at(t.definition->name)->initializer = &f;
            } catch (const CompilationError& failure) { diagnostics.add(failure); f.parse_failed = true; }
              catch (const Poison&) { f.parse_failed = true; }
        }
    };
    bool preparing_relationships=false;
    root.prepare_relationships=[&](Type left,Type right,const SourceFile& location,const Token& token){
        if(headers_ready)return;
        if(preparing_relationships)error(location,token,Code::constant_cycle,"Cyclic dependency between has and declaration headers.");
        preparing_relationships=true;
        try {
            for(auto type:{left,right})for(auto& [id,owner]:units)for(auto& structure:owner->structures)
                if(structure.definition==type.definition)layout(*owner,structure);
            for(auto& [id,owner]:units)for(auto& function:owner->functions)if(!function.prepared && (function.receiver || function.interface_requirement))prepare_function(*owner,function);
            root.member_catalog=std::make_shared<ir::Module>(member_catalog(units,entry));
            preparing_relationships=false;
        }catch(...){preparing_relationships=false;throw;}
    };
    // Install every unit's lazy resolver before any semantic declaration pass
    // can discover a global. Actual initializer evaluation keeps its later phase,
    // after layouts, headers, constants and defaults have been prepared.
    for (auto& [id, owner] : units) {
        owner->resolve_global = [&, u = owner.get()](Global& value) {
            if (value.state == 2) {
                return;
            }
            if (value.state == 1) {
                if (value.type != Type::none) {
                    return;
                }
                error(u->source, value.token, Code::initializer,
                      "Global type inference cycle requires an explicit type annotation.");
            }
            if (value.state == 3) {
                throw Poison{};
            }
            Depth guard(diagnostics.semantic_depth(), u->source, value.token, 128);
            value.state = 1;
            try {
                if (value.initializer->body.front().annotation) {
                    value.type = resolve_type(*value.initializer->body.front().annotation,
                                              *u, root, units, value.token);
                }
                prepare_function(*u,*value.initializer);
                semantics.analyze_global(*u,value);
                value.cell = u->make_capture_record(value.identity + "::$global_cell", value.type, true);
                if(value.readonly)std::const_pointer_cast<ir::ValueType>(value.cell.definition)->fields[0].readonly=true;
                value.state = 2;
            } catch (...) {
                value.state = 3;
                throw;
            }
        };
    }
    for (auto& [id,unit] : units) for (auto& structure : unit->structures) {
        try { layout(*unit,structure); }
        catch (const CompilationError& failure) { diagnostics.add(failure); }
        catch (const Poison&) {}
    }
    bool pending_headers=true;
    while(pending_headers) {pending_headers=false;for(auto& [id,u]:units)for(auto& f:u->functions)if(!f.prepared){prepare_function(*u,f);pending_headers=true;}}
    for(const auto& [id,u]:units)if(u->c_primitives && u->c_primitives->physical_source()) {
        const auto& admission=*u->c_primitives;
        for(const auto& f:u->functions) {
            std::vector<Type> parameters;for(const auto& p:f.parameters)parameters.push_back(p.type);
            if(!f.c_primitive || !admission.signature(admission.bindings().at(*f.c_primitive),parameters,f.result))
                error(u->source,f.token,Code::type_mismatch,"Physical native signature differs from the admitted descriptor.");
        }
        for(const auto& resource:admission.resources()) {
            auto found=u->structure_names.find(resource.name);
            if(found==u->structure_names.end() || !admission.resource_type(Type::nominal(found->second->definition)))
                error(u->source,{TokenKind::name,resource.name,0},Code::type_mismatch,"Physical native resource differs from the admitted descriptor.");
        }
        for(unsigned n=0;n<admission.records().size();++n) {
            const auto& record=admission.records()[n];auto found=u->structure_names.find(record.name);
            CPrimitiveAdmission::Shape shape;shape.record=n+1;
            if(found==u->structure_names.end() || !admission.matches(shape,Type::nominal(found->second->definition)))
                error(u->source,{TokenKind::name,record.name,0},Code::type_mismatch,"Physical native record differs from the admitted descriptor.");
        }
    }
    headers_ready=true;
    root.prepare_relationships=[](Type,Type,const SourceFile&,const Token&){};
    validate_inheritance(units,root,diagnostics);
    root.member_catalog=std::make_shared<ir::Module>(member_catalog(units,entry));
    root.prepare_relationships=[&](Type left,Type right,const SourceFile& source,const Token& token) {
        const bool preparing=preparing_generic_interfaces;
        complete_generic_interface_headers();
        // A field/default/extent may query another interface while its enclosing
        // generic signature is being prepared. Complete that dependency first;
        // querying the unfinished signature itself retains the existing cycle diagnostic.
        if(preparing)for(auto type:{left,right})if(type.kind==Type::interface_ref)
            for(const auto& [id,module]:units)for(const auto& f:module->functions)
                if(f.interface_requirement && f.receiver->type.definition==type.definition &&
                   std::none_of(root.member_catalog->interface_requirements.begin(),root.member_catalog->interface_requirements.end(),[&](const auto& header){return header.name==f.symbol;}))
                    error(source,token,Code::constant_cycle,"Cyclic dependency between has and declaration headers.");
    };
#include "generic_declarations.inc"
    complete_generic_constraints();
    for(auto& [id,u]:units)for(auto& structure:u->structures)try {u->resolve_numeric(structure);}
        catch(const CompilationError& failure){diagnostics.add(failure);}catch(const Poison&){}
    for (auto& [id,unit] : units) for (auto& structure : unit->structures) {
        try { defaults(*unit,structure); }
        catch (const CompilationError& failure) { diagnostics.add(failure); }
        catch (const Poison&) {}
    }
    for (auto& [id, unit] : units) for (auto& c : unit->constants) {
        if (diagnostics.full()) break;
        try { evaluate(*unit, c); }
        catch (const CompilationError& failure) { diagnostics.add(failure); }
        catch (const Poison&) {}
    }
    for (auto& [id,unit] : units) for (auto& f : unit->functions) {
        if (f.parse_failed) continue;
        try { semantics.parameter_defaults(*unit,f); }
        catch (const CompilationError& failure) { diagnostics.add(failure); }
        catch (const Poison&) {}
    }
    for (auto& [id,unit] : units) for (auto& f : unit->functions) {
        if (f.parse_failed) continue;
        try { semantics.initializer_fields(*unit,f); }
        catch (const CompilationError& failure) { diagnostics.add(failure); }
        catch (const Poison&) {}
    }
    for (auto& [id, owner] : units) for (auto& value : owner->globals) if (value.initializer->body.front().annotation) {
        try { value.type = resolve_type(*value.initializer->body.front().annotation, *owner, root, units, value.token); }
        catch (const CompilationError& failure) { diagnostics.add(failure); value.state = 3; }
        catch (const Poison&) { value.state = 3; }
    }
    for (auto& [id, owner] : units) for (auto& value : owner->globals) {
        try { owner->resolve_global(value); }
        catch (const CompilationError& failure) { diagnostics.add(failure); }
        catch (const Poison&) {}
    }
    const bool presentation=diagnostics.recovery && diagnostics.recovery->presentation;
    if(presentation && diagnostics.size()!=parse_errors)diagnostics.finish();
    bool pending_bodies=true;
    while(pending_bodies && !diagnostics.full()) {
        pending_bodies=false;
        for(auto& [id,u]:units) {
            for(const auto& f:u->functions)if(!f.analyzed && !f.interface_requirement){pending_bodies=true;break;}
            for(auto& value:u->globals)if(value.state==0) {u->resolve_global(value);pending_bodies=true;}
            semantics.analyze_unit(*u);
            // Body specialization can introduce numeric enum types after the
            // header/default pass. Their discriminants must be ready before
            // runtime conversions and enum metadata reach ordinary lowering.
            for(auto& structure:u->structures)if(structure.definition->enum_backing && !structure.numeric_state) {
                pending_bodies=true;
                try {u->resolve_numeric(structure);}
                catch(const CompilationError& failure){diagnostics.add(failure);}catch(const Poison&){}
            }
        }
    }
    for(auto& [id,u]:units)u->globals.sort([](const Global& a,const Global& b){return a.token.offset!=b.token.offset?a.token.offset<b.token.offset:a.identity<b.identity;});
    if(presentation && (diagnostics.failed() || diagnostics.recovery->repaired) && diagnostics.presentation_errors()) {
        diagnostics.recovery->repaired=true;
        return; // Typed holes/poisoned defaults have presentation, never ownership/IR contracts.
    }
    diagnostics.finish();
    try {root.member_catalog->interface_bindings=ir::interface_universe(*root.member_catalog);}
    catch(const std::length_error& failure) {error(root.source,root.structures.empty()?Token{TokenKind::end,"",0}:root.structures.front().token,Code::limit,failure.what(),"Split unrelated providers into separately compiled projects.");}
    std::map<std::string,Function*> functions;
    std::map<std::string,std::set<std::string>> dependencies;
    std::vector<std::string> pending;
    for (auto& [id,u] : units) for (auto& f : u->functions) {
        functions.emplace(f.symbol,&f);
        if (f.initializer || f.global_initializer) pending.push_back(f.symbol);
        std::vector<const Statement*> statements; for (const auto& s : f.body) statements.push_back(&s);
        while (!statements.empty()) {
            auto* statement=statements.back(); statements.pop_back();
            for (const auto& child : statement->body) statements.push_back(&child);
            for (const auto& child : statement->otherwise) statements.push_back(&child);
            if (statement->kind == Statement::Kind::deferred && statement->expression && !statement->expression->target.empty()) dependencies[f.symbol].insert(statement->expression->target);
            if (!statement->compound_target.empty()) dependencies[f.symbol].insert(statement->compound_target);
            std::vector<const Expr*> expressions; if (statement->selection) expressions.push_back(statement->selection.get()); if (statement->compound_conversion) expressions.push_back(statement->compound_conversion.get()); if(statement->index) expressions.push_back(statement->index.get()); if (statement->expression) expressions.push_back(statement->expression.get());
            while (!expressions.empty()) {
                auto* e=expressions.back(); expressions.pop_back();
                for (const auto& part : e->parts) expressions.push_back(part.get());
                for (const auto& part : e->omitted) expressions.push_back(part.get());
                if (e->kind==Expr::Kind::call || e->kind==Expr::Kind::method_call || e->property) dependencies[f.symbol].insert(e->target);
                if (e->property && !e->setter.empty()) dependencies[f.symbol].insert(e->setter);
                if (e->kind == Expr::Kind::function_call && !e->parts.empty()) {
                    auto signature = e->parts[0]->type;
                    for (const auto& [id, owner] : units) for (const auto& candidate : owner->functions) {
                        if (candidate.global_initializer || candidate.asynchronous || candidate.receiver || !signature.definition || !signature.definition->callable) continue;
                        auto first = candidate.anonymous ? 1u : 0u;
                        if (candidate.parameters.size() != signature.definition->call_parameters.size() + first || candidate.result != Type(*signature.definition->call_result)) continue;
                        bool matches = true;
                        for (unsigned n = first; n < candidate.parameters.size(); ++n) matches &= candidate.parameters[n].type == Type(signature.definition->call_parameters[n-first]);
                        if (matches) dependencies[f.symbol].insert(candidate.symbol);
                    }
                }
                if(!e->virtual_slot.empty() && !e->super_call) for(const auto& [id,owner]:units) for(const auto& target:owner->functions)
                    if(target.virtual_slot==e->virtual_slot) dependencies[f.symbol].insert(target.symbol);
                if (!e->initializer.empty()) dependencies[f.symbol].insert(e->initializer);
            }
        }
    }
    for(auto& [id,u]:units) for(auto& f:u->functions) {
        if(f.compile_time)pending.push_back(f.symbol);
        std::vector<const Expr*> expressions;
        if(f.base_prelude) expressions.push_back(f.base_prelude.get());
        for(const auto& child:f.initialization_defaults) expressions.push_back(child.get());
        while(!expressions.empty()) {
            const auto* e=expressions.back();expressions.pop_back();
            for(const auto& child:e->parts) expressions.push_back(child.get());
            for(const auto& child:e->omitted) expressions.push_back(child.get());
            if(!e->target.empty()) dependencies[f.symbol].insert(e->target);
            if(!e->initializer.empty()) dependencies[f.symbol].insert(e->initializer);
            if(!e->virtual_slot.empty() && !e->super_call) for(const auto& [id,owner]:units) for(const auto& target:owner->functions)
                if(target.virtual_slot==e->virtual_slot) dependencies[f.symbol].insert(target.symbol);
        }
    }
    for(const auto& binding:root.member_catalog->interface_bindings) for(const auto& [requirement,route]:binding.methods) {
        dependencies[requirement].insert(route.second);
        const auto& selected=*functions.at(route.second);
        if(!selected.virtual_slot.empty()) for(const auto& [name,target]:functions) if(target->virtual_slot==selected.virtual_slot) dependencies[requirement].insert(name);
    }
    std::set<std::string> startup;
    std::vector<std::string> startup_pending;
    for (const auto& [name, function] : functions) if (function->global_initializer) startup_pending.push_back(name);
    while (!startup_pending.empty()) {
        auto name = startup_pending.back(); startup_pending.pop_back();
        if (!functions.contains(name) || !startup.insert(name).second) continue;
        const auto& function = *functions.at(name);
        if (function.asynchronous || function.c_primitive ||
            (!function.native_symbol.empty() && native_binding(function.native_symbol).name == "register")) {
            for (const auto& [id, owner] : units) if (owner->declarations.contains(function.token.text) && owner->declarations.at(function.token.text) == &function)
                error(owner->source, function.token, Code::initializer, function.c_primitive ?
                    "C primitive calls require a completed application startup." :
                    "Global initialization requires synchronous calls without native callback registration.");
        }
        for (const auto& target : dependencies[name]) startup_pending.push_back(target);
    }
    std::set<std::string> frozen;
    while (!pending.empty()) {
        auto key=pending.back(); pending.pop_back();
        if (!functions.contains(key) || !frozen.insert(key).second) continue;
        auto& f=*functions.at(key); f.initialization_contract=f.body_syntax;
        for (const auto& dependency : dependencies[key]) pending.push_back(dependency);
    }
    for (auto& [id,f] : functions) for (const auto& p : f->parameters)
        f->initialization_contract += std::to_string(p.token.text.size()) + ":" + p.token.text + std::to_string(p.default_syntax.size()) + ":" + p.default_syntax;
    if (const auto provider = std::find_if(units.begin(), units.end(),
            [](const auto& unit) { return bool(unit.second->c_primitives); }); provider != units.end()) {
        const auto& dynamic = provider->second->c_primitives->dynamic();
        for (const auto& [id, unit] : units) for (const auto& f : unit->functions)
            if (f.asynchronous && !(dynamic && dynamic->conventional() && standard_module(id)))
                error(unit->source, f.token, Code::unsupported_type, "C primitive provider graphs require synchronous execution.");
    }
    // Recovered trees and AST-only poisoned types can never enter capability analysis or IR.
    diagnostics.finish();
    analyze_capabilities(units,diagnostics);
    diagnostics.finish();
    if(diagnostics.editor && diagnostics.editor->receiver)
        semantics.finish_editor_receiver(*diagnostics.editor->owner,*diagnostics.editor->function);
    analyze_construction(units,diagnostics);
    diagnostics.finish();
    check_string_pool(units);
    for(auto& [id,u]:units)for(auto& f:u->functions)if(f.asynchronous){f.task=u->make_task(f.result);if(!ir::async_boundary_safe(f.result))error(u->source,f.token,Code::unsupported_type,"Callable-containing results cannot escape an async task in Delivery 1.");for(const auto& parameter:f.parameters)if(!ir::async_boundary_safe(parameter.type))error(u->source,parameter.token,Code::unsupported_type,"Callable-containing or borrowed arguments cannot cross an async task boundary in Delivery 1.");if(f.receiver && !ir::async_boundary_safe(f.receiver->type))error(u->source,f.token,Code::unsupported_type,"Callable-containing managed receivers cannot cross an async task boundary in Delivery 1.");if(f.initializer || f.accessor || (f.receiver && f.token.text=="deinit") || f.virtual_method || f.overriding || f.interface_requirement || (f.receiver && f.receiver->type.kind!=Type::managed_ref))error(u->source,f.token,Code::unsupported_type,"Async lifecycle methods, accessors, value receivers and virtual/interface methods require a reviewed contract.");}
    root.prepare_relationships=[](Type,Type,const SourceFile&,const Token&){};
    for(auto& [id,u]:units) {u->resolve_numeric={};u->resolve_global={};u->make_task={};u->make_cleanup={};u->make_capture_record={};u->make_callable={};u->resolve_callable={};u->resolve_result={};u->make_result={};u->make_option={};u->make_nullable={};u->resolve_array={};u->resolve_application={};u->specialize_function={};u->specialize_structure={};} // No closures into this validation frame escape.
}

}

using namespace frontend;

std::shared_ptr<const ValidatedAnalysis> validated_source(const SourceFile&);
std::shared_ptr<const ValidatedAnalysis> validated_project(const std::filesystem::path&, const SourceOverrides& = {}, bool verify_disk = false,const NativeProject* = nullptr,EditorProbe* = nullptr,CPrimitiveAdmission::Owner = {},const std::string* document = nullptr,EditorRecovery* = nullptr);
// This factory is the only way to construct a successful result.
struct AnalysisAccess {
    static std::shared_ptr<const ValidatedAnalysis> build(std::map<std::string, std::unique_ptr<Unit>> units, const std::string& entry,std::vector<Diagnostic> warnings={},const std::string& context_key={},std::shared_ptr<Unit> library_scope={},ApplicationMetadata::Owner metadata={},bool native_authorized=true,std::shared_ptr<const project::LockedDependencies> dependencies={},bool editor_only=false,bool presentation=false) {
        observation::Scope timing(observation::Phase::analysis_assembly);
        auto result = std::shared_ptr<ValidatedAnalysis>(new ValidatedAnalysis);result->warnings_=std::move(warnings);
        result->entry_ = entry;
        result->application_metadata_ = std::move(metadata);
        result->locked_dependencies_ = dependencies;
        const auto& selected=library_scope?*library_scope:*units.at(entry);
        result->executable_=!library_scope && !editor_only;
        const auto* app=application_entry(selected);
        result->application_=app!=nullptr || (!selected.globals.empty() && !selected.declarations.contains("entry") && !selected.declarations.contains("main"));
        result->entry_symbol_=result->executable_?entry+(result->application_?"::$app_adapter":"::entry"):std::string{};
        // Builtin declaration snapshots provide stable canonical hover/reference
        // identity, but never masquerade as filesystem navigation/edit targets.
        for(auto type:{Type::i8,Type::u8,Type::i16,Type::u16,Type::i32,Type::u32,Type::i64,Type::u64,Type::f32,Type::f64})for(const auto& api:numeric::members()) {
            const std::string name(api.name);auto text=std::make_shared<const SourceFile>("<numeric prelude>",name);
            SourceSpan site{text,"$numeric",0,name.size()};
            AnalyzedDeclaration record{api.callable?AnalyzedDeclaration::Kind::function:AnalyzedDeclaration::Kind::module_constant,numeric::symbol(type,api.name),"$numeric","public",site,{},type,{}};
            record.builtin=true;record.static_member=true;record.documentation=numeric_documentation(api);
            for(auto parameter:api.parameters){record.parameters.push_back(type);record.written_parameters.push_back({std::string(parameter),type,site});}
            if(!api.callable){record.value=numeric::limit(type,api.name=="max_value");record.editor_symbol_kind=7;record.readonly=true;}
            result->editor_declarations_.push_back(std::move(record));
        }
        auto primitive_identity=[](Type type){return "$core::type::"+ir::type_name(type);};
        for(auto type:{Type::i8,Type::u8,Type::i16,Type::u16,Type::i32,Type::u32,Type::i64,Type::u64,Type::f32,Type::f64,Type::boolean,Type::string}) {
            auto name=ir::type_name(type);auto text=std::make_shared<const SourceFile>("<core prelude>",name);
            AnalyzedDeclaration record{AnalyzedDeclaration::Kind::value_structure,primitive_identity(type),"$core","public",{text,"$core",0,name.size()},{},type,{}};
            record.builtin=true;result->editor_declarations_.push_back(std::move(record));
        }
        for(const auto& [id,u]:units)for(const auto& [symbol,record]:u->builtins)if(std::none_of(result->editor_declarations_.begin(),result->editor_declarations_.end(),[&](const auto& d){return d.identity==symbol;}))result->editor_declarations_.push_back(record);
        result->contract_="18:numeric-prelude-v1:20:text-collections-v1:unicode-case-16-v1:display-v1:console-v2:terminal-v2:application-args-v2:host-primitives-v1:"+result->entry_symbol_;
        if(!context_key.empty())result->contract_+="|project-context-v1:"+context_key;
        for (const auto& [id,u] : units) if (u->c_primitives) {
            result->c_primitives_ = u->c_primitives;
            const auto& bytes = u->c_primitives->descriptor();
            result->contract_ += "|c-primitive:" + std::to_string(bytes.size()) + ":" + bytes;
        }
        for(const auto& [id,u]:units)for(const auto& f:u->functions)if(!f.native_symbol.empty())result->contract_+=":"+id+":"+f.native_symbol;

        result->contract_+=std::string(formatting::abi_version)+"|"+std::string(temporal::abi_version);
        auto graph = std::make_shared<const std::map<std::string, std::unique_ptr<Unit>>>(std::move(units));
        auto& context=library_scope?*library_scope:*graph->at(entry);
        auto field = [](std::string& out, const std::string& value) { out += std::to_string(value.size()) + ":" + value; };
        std::map<std::string,std::string> source_identities;
        for(const auto& [id,u]:*graph) {
            for(const auto& f:u->functions)if(!f.generic_origin.empty())source_identities.emplace(f.symbol,f.generic_origin);
            for(const auto& s:u->structures)if(s.generic_origin)source_identities.emplace(s.definition->identity,s.generic_origin->definition->identity);
        }
        auto source_identity=[&](const std::string& key) {
            auto prefix=key;
            while(true) {
                if(auto found=source_identities.find(prefix);found!=source_identities.end())return found->second+key.substr(prefix.size());
                auto end=prefix.rfind("::");if(end==std::string::npos)return key;prefix.resize(end);
            }
        };
        std::map<std::string,std::string> owner_names;
        for(const auto& [id,u]:*graph)for(const auto& structure:u->structures)owner_names.emplace(structure.definition->identity,ir::type_name(Type(structure.definition,structure.definition->managed?Type::managed_ref:Type::aggregate)));
        std::map<std::string,std::string> signatures;
        for(const auto& [id,u]:*graph)for(const auto& f:u->functions)if(!f.operator_glyph.empty() || f.receiver || f.static_owner || random_numeric_function(f) || !f.returns.empty() || !f.generic_origin.empty())signatures.emplace(f.symbol,editor_signature(f));
        for(const auto& [id,u]:*graph)for(const auto& value:u->globals)if(value.static_owner!=Type::none)
            signatures.emplace(value.identity,value.visibility+" static "+std::string(value.readonly?"const ":"")+ir::type_name(value.static_owner)+"."+value.token.text+": "+ir::type_name(value.type));
        // Hook calls already selected in ordinary display adapter bodies supply
        // navigation. No editor-side method lookup or runtime hook search.
        for(const auto& [id,u]:*graph)for(const auto& structure:u->structures)if(structure.generic_origin && !structure.case_tokens.empty()) {
            const auto type=Type::nominal(structure.definition);const auto label=ir::type_name(type);
            auto heading=std::string(structure.definition->enum_flags?"flags enum ":"enum ")+label;
            if(structure.definition->enum_backing)heading+="("+ir::type_name(*structure.definition->enum_backing)+")";
            signatures.emplace(structure.definition->identity,std::move(heading));
            for(const auto& member:structure.definition->numeric_members)
                signatures.emplace(structure.definition->identity+"::variant::"+member.name,label+"."+member.name+" = "+numeric::integer_text(member.bits,*structure.definition->enum_backing)+" ("+ir::type_name(*structure.definition->enum_backing)+")");
            for(const auto& variant:structure.definition->cases) {
                std::string signature=variant.name;if(variant.count)signature+='(';
                for(unsigned n=0;n<variant.count;++n){if(n)signature+=", ";signature+=structure.fields[variant.begin+n].token.text+": "+ir::type_name(ir::field_type(type,variant.begin+n));}
                if(variant.count)signature+=')';signature+=" -> "+label;
                signatures.emplace(structure.definition->identity+"::variant::"+variant.name,std::move(signature));
            }
        }
        std::map<std::string,std::map<std::string,std::string>> display_hooks;
        for(const auto& [id,u]:*graph)for(const auto& f:u->functions)if(f.display_adapter) {
            std::vector<const Statement*> statements;for(const auto& s:f.body)statements.push_back(&s);
            std::vector<const Expr*> expressions;
            while(!statements.empty()){auto* s=statements.back();statements.pop_back();if(s->expression)expressions.push_back(s->expression.get());for(const auto& child:s->body)statements.push_back(&child);for(const auto& child:s->otherwise)statements.push_back(&child);}
            while(!expressions.empty()){auto* e=expressions.back();expressions.pop_back();if(e->kind==Expr::Kind::method_call && (e->token.text=="string" || e->token.text=="format") && !e->target.empty())display_hooks[f.symbol].emplace(e->token.text,e->target);for(const auto& part:e->parts)expressions.push_back(part.get());for(const auto& part:e->omitted)expressions.push_back(part.get());}
        }
        std::set<std::string> visible_declarations;
        std::set<std::tuple<std::string,std::string,std::string,std::size_t,std::size_t,bool>> visible_references;
        std::set<std::string> exact_references;
        auto declaration = [&](AnalyzedDeclaration d,bool native_contract=true,bool visible=true) {
            if(d.source.source)d.documentation=source_documentation(*d.source.source,d.source.begin);
            for(auto& p:d.documentation.parameters)p.source.module_identity=d.module;
            if(d.documentation.returns)d.documentation.returns->source.module_identity=d.module;
            if (d.owner.empty()) d.owner = d.module;
            if(native_contract) {
            field(result->contract_, "declaration"); field(result->contract_, d.identity); field(result->contract_, d.visibility);
            field(result->contract_,d.owner);field(result->contract_,d.operator_glyph);
            field(result->contract_,d.static_member?"static":"ordinary");
            field(result->contract_,d.generic_template?"template":"concrete");
            if(!d.operator_glyph.empty())field(result->contract_,d.constant_syntax);
            field(result->contract_, std::to_string(static_cast<int>(d.kind)));
            field(result->contract_, d.type.kind == Type::aggregate ? d.type.definition->identity : ir::type_name(d.type));
            field(result->contract_,ir::capability_key(d.type));
            field(result->contract_, std::to_string(d.parameters.size()));
            for (auto t : d.parameters) {
                field(result->contract_, t.kind == Type::aggregate ? t.definition->identity : ir::type_name(t));
                field(result->contract_,ir::capability_key(t));
            }
            field(result->contract_,d.receiver.kind == Type::none ? "" : ir::type_name(d.receiver));
            field(result->contract_,ir::capability_key(d.receiver));
            field(result->contract_,d.capability_contract);
            field(result->contract_,d.using_field ? "using" : "ordinary-field");
            field(result->contract_,d.base_identity);field(result->contract_,d.virtual_slot);field(result->contract_,d.override_target);
            for (const auto& mask:d.construction_parameters) {
                field(result->contract_,"construction-context");
                for (auto parameter:mask) field(result->contract_,std::to_string(parameter));
            }
            for (const auto& name:d.required_initialization_fields) field(result->contract_,name);
            field(result->contract_,d.receiver_mutates ? "mutating" : "nonmutating");
            field(result->contract_,d.initialization_contract);
            if (d.kind != AnalyzedDeclaration::Kind::function && d.kind != AnalyzedDeclaration::Kind::receiver_method && d.kind!=AnalyzedDeclaration::Kind::interface_requirement && d.kind != AnalyzedDeclaration::Kind::initializer && d.kind != AnalyzedDeclaration::Kind::operator_function) {
                field(result->constants_, d.identity); field(result->constants_, d.visibility);
                field(result->constants_,d.using_field ? "using" : "ordinary-field");
                field(result->constants_, d.type.kind == Type::aggregate ? d.type.definition->identity : ir::type_name(d.type)); field(result->constants_, d.constant_syntax);
            }
            }
            d.identity=source_identity(d.identity);d.owner=source_identity(d.owner);d.virtual_slot=source_identity(d.virtual_slot);d.override_target=source_identity(d.override_target);
            if(visible && visible_declarations.insert(d.identity).second)result->declarations_.push_back(std::move(d));
        };
        auto format_specification = [](const Expr& e)->std::optional<std::string> {
            if(e.format_specification)return e.format_specification;
            if(e.kind==Expr::Kind::method_call && e.token.text=="format" && e.parts.size()==2 && e.parts[1]->type==Type::string)return format_specification_syntax(*e.parts[1]);
            return {};
        };
        auto reference = [&](const std::string& from, const std::string& to, const Expr& e, const Unit& u, bool implicit = false, bool native_contract = true, bool navigation_only = false, bool semantic_only = false) {
            implicit |= from.find("::$json$")!=std::string::npos || from.find("::$enum_")!=std::string::npos;
            auto location = span(u.source,e.token); location.module_identity = u.identity;
            auto spelling = e.qualifier + "::" + e.token.text;
            auto origin=source_identity(from),target=source_identity(to);
            std::string binding;
            if(native_contract){field(binding,from);field(binding,to);if(auto spec=format_specification(e))field(binding,*spec);}
            std::string presentation=signatures.contains(to)?signatures.at(to):"";
            if(e.kind==Expr::Kind::construction && e.constructed_type!=Type::none && to==e.initializer && !presentation.empty()) {
                const auto open=presentation.find('(');
                const auto end=presentation.find('\n');
                const auto result=presentation.rfind(") -> ",end);
                if(result!=std::string::npos)presentation.replace(result+5,(end==std::string::npos?presentation.size():end)-(result+5),ir::type_name(e.type));
                if(open!=std::string::npos)presentation.replace(0,open,ir::type_name(e.constructed_type));
            }

            if(!e.source_target.empty() && e.source_target==to && e.target.starts_with("/@stdlib/random.void::") && signatures.contains(e.target))presentation=signatures.at(e.target);
            if(e.kind==Expr::Kind::field && !e.parts.empty() && e.parts[0]->type.definition && e.slot<e.parts[0]->type.definition->fields.size()) {
                const auto& member=e.parts[0]->type.definition->fields[e.slot];
                if(to==member.owner+"::field::"+member.name)presentation=(member.visibility=="public"?"":member.visibility+" ")+(member.readonly?"const ":"")+member.name+": "+ir::type_name(e.type)+" (declared by "+(owner_names.contains(member.owner)?owner_names.at(member.owner):member.owner)+")";
            }
            const bool written=visible_references.emplace(origin,target,u.identity,location.begin,location.end,implicit).second;
            std::string exact;field(exact,origin);field(exact,target);field(exact,u.identity);field(exact,std::to_string(location.begin));field(exact,std::to_string(location.end));field(exact,binding);field(exact,implicit?"implicit":"written");
            if(exact_references.insert(exact).second)
                (native_contract ? result->references_ : result->editor_references_).push_back({origin,target,spelling,std::move(location),implicit || !written,std::move(binding),std::move(presentation),navigation_only,semantic_only});
            if(native_contract) {field(result->contract_,"reference"); field(result->contract_,from); field(result->contract_,to);}

        };
        for (const auto& [id, unit] : *graph) {
            AnalyzedModule module{id,unit->source,{},{},unit->conditions,unit->conditional_regions};
            field(result->contract_,unit->conditions.key());
            for(const auto& [alias,targets]:unit->qualifiers)
                if(targets.size()==1)module.qualifiers.emplace(alias,targets.front()->identity);
            field(result->contract_,"module"); field(result->contract_,id);
            auto synthetic_types=unit->synthetic_types;std::sort(synthetic_types.begin(),synthetic_types.end(),[](auto a,auto b){return Type::key(a)<Type::key(b);});
            for(auto type:synthetic_types){
                field(result->contract_,"compiler-value-layout");field(result->contract_,Type::key(type));auto base=type.definition->base.lock();field(result->contract_,base?base->identity:"");
                for(const auto& member:type.definition->fields){field(result->contract_,member.name);field(result->contract_,member.visibility);field(result->contract_,member.readonly?"const":"var");field(result->contract_,ir::type_name(member.type));field(result->contract_,ir::capability_key(member.type));}
            }
            // Canonical binding records: offsets and written path normalization are irrelevant.
            std::set<std::string> imports;
            for (const auto& i : unit->imports) {
                module.imports.push_back(i.target);
                std::string binding; field(binding,i.target);
                imports.insert(std::move(binding));
            }
            std::sort(module.imports.begin(),module.imports.end());
            module.imports.erase(std::unique(module.imports.begin(),module.imports.end()),module.imports.end());
            field(result->contract_,std::to_string(imports.size()));
            for (const auto& i : imports) field(result->contract_,i);
            result->modules_.push_back(std::move(module));
            auto location = [&](const Token& t) { auto l = span(unit->source,t); l.module_identity=id; return l; };
            auto return_components = [&](const Function& f) {
                if(!f.generic_origin.empty())return;
                for(const auto& component:f.returns)if(component.name) {
                    const auto identity=f.symbol+"::$return::"+component.name->text;
                    auto type=f.generics.empty()?component.type:editor_pattern_type(component.type);
                    result->editor_declarations_.push_back({AnalyzedDeclaration::Kind::return_component,
                        identity,id,"local",location(*component.name),{},type,{},{},f.symbol});
                }
            };
            auto module_location=location({TokenKind::end,"",0});module_location.end=0;
            result->editor_declarations_.push_back({AnalyzedDeclaration::Kind::import_alias,id+"::$module",id,"private",module_location,{},Type::none,{},{},id});
            for(const auto& import:unit->imports) {
                Expr use(Expr::Kind::name,import.path);
                reference(id+"::$module",import.target+"::$module",use,*unit,false,false);
            }
            std::set<std::string> aliases;
            for(const auto& import:unit->imports)if(import.explicit_alias && aliases.insert(import.alias.text).second)
                result->editor_declarations_.push_back({AnalyzedDeclaration::Kind::import_alias,id+"::import::"+import.alias.text,id,"private",location(import.alias),{},Type::none,{},{},id});
            auto namespace_reference=[&](const std::string& owner,const Token& token,bool implicit=false,bool semantic_only=false) {
                auto binding=unit->qualifiers.find(token.text);
                if(binding==unit->qualifiers.end() || binding->second.size()!=1)return;
                Expr use(Expr::Kind::name,token);
                auto target=aliases.contains(token.text)?id+"::import::"+token.text:binding->second.front()->identity+"::$module";
                reference(owner,target,use,*unit,implicit,false,false,semantic_only);
            };
            auto primitive_reference=[&](const std::string& owner,const Token& token,Type type) {
                Expr use(Expr::Kind::name,token);reference(owner,primitive_identity(type),use,*unit,false,false);
            };
            auto enum_type_reference=[&](const std::string& owner,const TypeUse* use,bool implicit) {
                if(!use || !use->type.definition)return;
                if(use->qualifier)namespace_reference(owner,*use->qualifier,implicit);
                Expr name(Expr::Kind::name,use->token);
                reference(owner,use->type.definition->identity,name,*unit,implicit,false);
            };
#include "generic_tooling.inc"
            std::function<void(const Expr&,const std::string&,const std::map<unsigned,std::string>&,bool)> visit_expression;
            auto visit_patterns = [&](const Pattern& pattern,const std::string& owner,const std::map<unsigned,std::string>& locals,bool implicit) {
                std::vector<const Pattern*> patterns{&pattern};
                while(!patterns.empty()) {
                    auto* p=patterns.back();patterns.pop_back();
                    enum_type_reference(owner,p->enum_type_use.get(),implicit);
                    if(p->kind==Pattern::Kind::constructor && !p->variant_token.text.empty())frontend_case([&] {
                        std::string target;
                        if(p->nullable)target="$nullable::"+Type::key(p->type)+"|"+ir::capability_key(p->type)+"::Ok";
                        else if(p->type.definition && !p->type.definition->cases.empty()) {
                            const auto& variant=p->type.definition->cases.at(p->tag);
                            if(p->variant_token.text==variant.name)target=p->type.definition->result_type?"$result::"+Type::key(p->type)+"|"+ir::capability_key(p->type)+"::"+variant.name:p->type.definition->identity+"::variant::"+variant.name;
                        }
                        if(!target.empty()){Expr use(Expr::Kind::name,p->variant_token);reference(owner,target,use,*unit,implicit,false);}
                    });
                    for(const auto& value:p->values)visit_expression(*value,owner,locals,implicit);
                    for(const auto& child:p->children)patterns.push_back(&child);
                }
            };
            auto expression = [&](const Expr& e, const std::string& owner, const std::map<unsigned,std::string>& locals) { visit_expression(e,owner,locals,owner.find("::$display$")!=std::string::npos || owner.find("::$enum_")!=std::string::npos); };
            visit_expression = [&](const Expr& e, const std::string& owner, const auto& locals, bool implicit) {
                const bool implicit_site=implicit || e.implicit;
                const bool collection_kernel=e.target.starts_with("$array.") || e.target.starts_with("$dict.");
                enum_type_reference(owner,e.enum_type_use.get(),implicit_site);
                if(auto spec=format_specification(e)) {
                    field(result->contract_,"format-specification");field(result->contract_,owner);field(result->contract_,*spec);
                    reference(owner,"$display.specification",e,*unit,true);
                }
                if(!e.qualifier.empty())namespace_reference(owner,e.qualifier_token,implicit_site,e.kind==Expr::Kind::editor_hole);
                if(!e.source_target.empty()) {
                    reference(owner,e.source_target,e,*unit,implicit_site,!e.source_target.starts_with("$result::"));
                    if(!e.target.empty() && !collection_kernel)reference(owner,e.target,e,*unit,true);
                    if(!implicit_site && e.source_target.starts_with("$display.") && display_hooks.contains(e.target)) {
                        std::set<std::string> names{e.token.text};
                        if(e.token.text=="format") {
                            names={"string","format"};
                            for(unsigned n=0;n<e.parts.size();++n)if(n<e.argument_indexes.size() && e.argument_indexes[n]==1 && e.parts[n]->kind==Expr::Kind::literal) {
                                names.clear();
                                try{names.insert(formatting::split_layout(e.parts[n]->aggregate_value.text).pattern.empty()?"string":"format");}catch(const formatting::Error&){} // Fails before calling either hook.
                            }
                        }
                        const auto& hooks=display_hooks.at(e.target);
                        for(const auto& name:names)if(auto hook=hooks.find(name);hook!=hooks.end())
                            reference(owner,hook->second,e,*unit,false,false,/*navigation_only=*/true);
                    }
                }
                else if(e.kind==Expr::Kind::conversion && e.op=="interface") {
                    reference(owner,e.type.definition->identity,e,*unit,true);
                    for(const auto& binding:context.member_catalog->interface_bindings) if(binding.key==e.target)
                        for(const auto& [requirement,route]:binding.methods) {reference(owner,requirement,e,*unit,true);reference(owner,route.second,e,*unit,true);}
                } else if (!e.target.empty() && e.target!="$interpolation" && e.target!="$hole" && !collection_kernel) reference(owner,e.target,e,*unit,implicit_site);
                else if (e.kind == Expr::Kind::name && (!presentation || (e.type!=Type::none && e.type!=error_type)) &&
                         locals.contains(e.slot)) reference(owner,locals.at(e.slot),e,*unit,implicit_site,locals.at(e.slot).find("::const::")!=std::string::npos && locals.at(e.slot).substr(0,locals.at(e.slot).find("::const::"))==owner.substr(0,owner.find("::const::")));
                if(!e.virtual_slot.empty() && !e.super_call) for(const auto& [id,target_unit]:*graph) for(const auto& target:target_unit->functions)
                    if(target.virtual_slot==e.virtual_slot) reference(owner,target.symbol,e,*unit,true);
                if (!e.initializer.empty()) reference(owner,e.initializer,e,*unit,implicit);
                if(!e.setter.empty())reference(owner,e.setter,e,*unit,true);
                if(!e.display_adapter.empty())reference(owner,e.display_adapter,e,*unit,true);
                if(!e.argument_indexes.empty() && !e.target.starts_with("$") &&
                   std::any_of(e.names.begin(),e.names.end(),[](const auto& name){return !name.text.empty();}))
                  for(const auto& [target_id,target_unit]:*graph)for(const auto& target:target_unit->functions)if(target.symbol==e.target && target.generic_origin.empty()) {
                    for(unsigned n=0;n<e.names.size();++n)if(!e.names[n].text.empty()) {
                        const auto& parameter=target.parameters.at(e.argument_indexes.at(n));
                        Expr use(Expr::Kind::name,e.names[n]);reference(owner,target.symbol+"::local::"+std::to_string(parameter.slot),use,*unit,false,false);
                    }
                }

                if ((e.kind == Expr::Kind::construction || e.kind==Expr::Kind::base_initialization) && e.initializer.empty()) for (unsigned n = 0; n < e.names.size(); ++n) {
                    Expr use(Expr::Kind::name,e.names[n]); reference(owner,e.type.definition->fields.at(e.field_indexes[n]).owner + "::field::" + e.names[n].text,use,*unit);
                }
                for(const auto& p:e.patterns)visit_patterns(p,owner,locals,implicit);
                for (const auto& part : e.parts) visit_expression(*part,owner,locals,implicit);
                for (const auto& part : e.omitted) visit_expression(*part,owner,locals,true);
            };
            for(const auto& array:unit->arrays) if(array.extent && !array.template_syntax) {
                expression(*array.extent,id,{});
                field(result->contract_,"array-extent");field(result->contract_,ir::type_name(array.resolved));
            }
            for (const auto& [token,raw,qualifier] : unit->type_uses) {
                if(raw.kind!=Type::unresolved){primitive_reference(id,token,raw);continue;}
                auto t = resolve_type(raw,*unit,context,*graph,token);
                // Attribute annotation dependencies to their enclosing declaration,
                // keeping invalidation edges between owned declaration identities.
                std::string owner = id; std::size_t nearest = 0;
                auto consider = [&](const Token& declaration, const std::string& key) {
                    if (declaration.offset <= token.offset && (owner == id || declaration.offset >= nearest)) { nearest = declaration.offset; owner = key; }
                };
                for (const auto& f : unit->functions) if(!f.enum_adapter)consider(f.receiver ? f.receiver->token : f.token,f.symbol);
                for (const auto& c : unit->constants) consider(c.declaration.token,id + "::" + c.declaration.name);
                for (const auto& structure : unit->structures) {
                    consider(structure.token,structure.definition->identity);
                    for (const auto& member : structure.fields) consider(member.token,structure.definition->identity + "::field::" + member.name);
                }
                if(qualifier)namespace_reference(owner,*qualifier);
                Expr use(Expr::Kind::name,token); reference(owner,t.definition->identity,use,*unit);
            }
            for(const auto& syntax:unit->results) {
                auto type=syntax.resolved==Type::none?Type(syntax.definition):syntax.resolved;
                const auto key="$result::type::"+(syntax.resolved==Type::none?id+":"+std::to_string(syntax.token.offset):Type::key(type));
                if(visible_declarations.insert(key).second) {
                    auto text=std::make_shared<const SourceFile>("<core prelude>","result");
                    AnalyzedDeclaration record{AnalyzedDeclaration::Kind::value_structure,key,"$core","public",{text,"$core",0,6},{},type,{}};
                    record.builtin=true;record.documentation.description="A result carries either a successful Ok value or an Err error; only the active payload is stored.";
                    record.signature=ir::type_name(type);result->editor_declarations_.push_back(std::move(record));
                }
                Expr use(Expr::Kind::name,syntax.token);reference(id,key,use,*unit,false,false);
            }
            for (const auto& structure : unit->structures) {
                auto key = structure.definition->identity;
                auto type=Type::nominal(structure.definition);
                std::string layout=ir::numeric_enum_layout(type);
                for(const auto& variant:structure.definition->cases){field(layout,"variant");field(layout,variant.name);field(layout,std::to_string(variant.begin));field(layout,std::to_string(variant.count));}
                for (const auto& member : structure.fields) {
                    auto t = *member.annotation; auto identity = key + "::field::" + member.name;
                    field(layout,member.event?"event":member.using_field ? "using" : "ordinary-field"); field(layout,member.visibility);field(layout,member.readonly?"const":"var");field(layout,member.name); field(layout,t.kind == Type::aggregate ? t.definition->identity : ir::type_name(t)); field(layout,member.constant_syntax);
                    AnalyzedDeclaration record{structure.definition->managed ? AnalyzedDeclaration::Kind::managed_field : AnalyzedDeclaration::Kind::value_field,identity,id,member.visibility,location(member.token),{},t,member.constant_syntax,{},key};
                    if(member.source_end>member.token.offset){auto r=location(member.token);r.begin=unit->source.line_starts().at(unit->source.location(member.token.offset).line-1);r.end=member.source_end;record.extent=r;}
                    record.using_field=member.using_field;record.readonly=member.readonly;record.editor_symbol_kind=member.event?24:8; declaration(std::move(record),true,structure.definition->cases.empty());
                    if (member.expression) expression(*member.expression,identity,{});
                }
                for(unsigned n=0;n<structure.definition->numeric_members.size();++n){
                    const auto& member=structure.definition->numeric_members[n];
                    const auto value=numeric::integer_text(member.bits,*structure.definition->enum_backing);
                    AnalyzedDeclaration record{AnalyzedDeclaration::Kind::module_constant,key+"::variant::"+member.name,id,structure.visibility,location(structure.case_tokens[n]),{},type,value,member.bits,key};
                    record.editor_symbol_kind=22;
                    record.signature=ir::type_name(type)+"."+member.name+" = "+value+" ("+ir::type_name(*structure.definition->enum_backing)+")";
                    declaration(std::move(record));
                    if(structure.numeric_expressions[n])expression(*structure.numeric_expressions[n],key+"::variant::"+member.name,{});
                }
                for(unsigned n=0;n<structure.definition->cases.size();++n){const auto& variant=structure.definition->cases[n];std::vector<Type> parameters;for(unsigned field=0;field<variant.count;++field)parameters.push_back(ir::field_type(Type(structure.definition),variant.begin+field));
                    AnalyzedDeclaration record{variant.count?AnalyzedDeclaration::Kind::function:AnalyzedDeclaration::Kind::module_constant,key+"::variant::"+variant.name,id,structure.visibility,location(structure.case_tokens[n]),parameters,Type(structure.definition),std::to_string(n),variant.count?std::optional<std::uint64_t>{}:std::optional<std::uint64_t>{n},key};
                    for(unsigned i=0;i<variant.count;++i){const auto& member=structure.fields[variant.begin+i];record.written_parameters.push_back({member.token.text,parameters[i],location(member.token)});}
                    record.editor_symbol_kind=22;declaration(std::move(record));
                }
                AnalyzedDeclaration record{structure.definition->interface_type ? AnalyzedDeclaration::Kind::interface_type : structure.definition->managed ? AnalyzedDeclaration::Kind::managed_structure : AnalyzedDeclaration::Kind::value_structure,key,id,structure.visibility,location(structure.token),{},type,layout,{},id};
                if(structure.source_end>structure.token.offset){auto r=location(structure.token);r.begin=unit->source.line_starts().at(unit->source.location(structure.token.offset).line-1);r.end=structure.source_end;record.extent=r;}
                if(!structure.case_tokens.empty())record.editor_symbol_kind=10;
                if(structure.definition->enum_backing)record.signature=std::string(structure.definition->enum_flags?"flags enum ":"enum ")+structure.token.text+"("+ir::type_name(*structure.definition->enum_backing)+")";
                if(auto base=structure.definition->base.lock()) record.base_identity=base->identity;
                declaration(std::move(record));
            }
            for (const auto& value : unit->globals) {
                AnalyzedDeclaration record{AnalyzedDeclaration::Kind::global_variable, value.identity, id, value.visibility, location(value.token), {}, value.type, value.syntax};
                record.rename_safe = true;record.readonly=value.readonly;
                if(value.static_owner!=Type::none){record.static_member=true;record.owner=Type::key(value.static_owner);record.editor_symbol_kind=8;}
                record.signature=value.visibility+(record.static_member?" static ":" ")+std::string(value.readonly?"const ":"var ")+(record.static_member?ir::type_name(value.static_owner)+".":"")+value.token.text+": "+ir::type_name(value.type);
                declaration(std::move(record));
            }
            for (const auto& c : unit->constants) {
                auto key = id + "::" + c.declaration.name;
                AnalyzedDeclaration constant_record{AnalyzedDeclaration::Kind::module_constant,key,id,c.visibility,location(c.declaration.token),{},c.type,c.declaration.constant_syntax,(c.type.kind == Type::aggregate || ir::collection_type(c.type) || c.type==Type::string) ? std::optional<std::uint64_t>{} : std::optional<std::uint64_t>{c.value}};
                if(c.declaration.source_end>c.declaration.token.offset){auto r=location(c.declaration.token);r.begin=unit->source.line_starts().at(unit->source.location(c.declaration.token.offset).line-1);r.end=c.declaration.source_end;constant_record.extent=r;}
                constant_record.rename_safe=true;declaration(std::move(constant_record));
                expression(*c.declaration.expression,key,{});
            }
            for (const auto& f : unit->functions) {
                std::vector<Type> parameters; for (const auto& p : f.parameters) parameters.push_back(p.type);
                AnalyzedDeclaration method_record{!f.operator_glyph.empty() ? AnalyzedDeclaration::Kind::operator_function : f.interface_requirement ? AnalyzedDeclaration::Kind::interface_requirement : f.initializer ? AnalyzedDeclaration::Kind::initializer : f.receiver ? AnalyzedDeclaration::Kind::receiver_method : AnalyzedDeclaration::Kind::function,f.symbol,id,f.visibility,location(f.token),parameters,f.result,{}};
                if(f.static_owner){method_record.owner=Type::key(f.static_owner->type);method_record.static_member=true;method_record.editor_symbol_kind=6;}
                if (f.receiver) { method_record.owner = f.receiver->type.definition->identity; method_record.receiver = f.receiver->type; method_record.receiver_mutates = f.receiver_mutates; }
                if(!f.operator_glyph.empty()){method_record.owner=Type::key(f.operator_owner);method_record.operator_glyph=f.operator_glyph;method_record.editor_symbol_kind=25;}
                if(f.accessor) {
                    method_record.editor_symbol_kind=7;
                    method_record.readonly=f.accessor==1 && std::none_of(unit->functions.begin(),unit->functions.end(),[&](const auto& other){return other.accessor==2 && other.token.offset==f.token.offset;});
                }
                method_record.declaration_only=!f.native_symbol.empty() || f.c_primitive;
                method_record.rename_safe=f.operator_glyph.empty() && !f.receiver && !f.anonymous && f.generic_origin.empty() && f.generics.empty() && f.token.text!="entry" && f.token.text!="$application" && !f.global_initializer;
                method_record.signature=editor_signature(f);
                method_record.initialization_contract=f.initialization_contract;
                method_record.virtual_slot=f.virtual_slot;
                if(f.overriding) method_record.override_target=f.virtual_slot;
                method_record.capability_contract=f.capability_contract+(f.asynchronous?";async-effect":"");
                for (const auto& [mask,context]:f.construction_contexts)
                    method_record.construction_parameters.emplace_back(mask.begin(),mask.end());
                if (f.initializer && f.receiver->type.kind==Type::managed_ref)
                    for (const auto& member:f.receiver->type.definition->fields) {
                        Type t=member.type;
                        if (ir::requires_initialization(t)) method_record.required_initialization_fields.push_back(member.name);
                    }
                if (f.initializer) for (const auto& p : f.parameters) {
                    AnalyzedParameter parameter{p.token.text,p.type,location(p.token)};
                    if (p.default_expression) { auto range=location(p.default_expression->token); range.begin=p.default_begin; range.end=p.default_end; parameter.default_source=std::move(range); }
                    method_record.initializer_parameters.push_back(std::move(parameter));
                }
                for(const auto& p:f.parameters) {
                    AnalyzedParameter parameter{p.token.text,p.type,location(p.token)};
                    if(p.default_expression){auto r=location(p.token);r.begin=p.default_begin;r.end=p.default_end;parameter.default_source=r;}
                    method_record.written_parameters.push_back(std::move(parameter));
                }
                if(f.source_end>f.token.offset){auto r=location(f.token);r.begin=unit->source.line_starts().at(unit->source.location(f.token.offset).line-1);r.end=f.source_end;
                    if(f.accessor)for(const auto& other:unit->functions)if(other.accessor && other.token.offset==f.token.offset)r.end=std::max(r.end,other.source_end);
                    method_record.extent=r;}
                declaration(std::move(method_record),true,!f.anonymous && !f.display_adapter && !f.json_adapter && !f.enum_adapter && !f.global_initializer && f.token.text!="$application");
                return_components(f);
                if(!presentation && !f.asynchronous && !f.anonymous && f.generic_origin.empty() && !f.display_adapter &&
                   !f.json_adapter && !f.global_initializer && !f.accessor && valid_editor_identifier(f.token.text)) {
                    auto binding=[&](const Token& token,unsigned slot,unsigned argument,unsigned scope,const Token* initialization=nullptr) {
                        const auto type=f.locals.at(slot);
                        if(f.captured_locals.contains(slot) || !valid_editor_identifier(token.text) ||
                           !(numeric::number(type) || type==Type::boolean))return;
                        result->debug_bindings_[f.symbol].push_back({slot,argument,token.text,unit->source.location(token.offset),scope,unit->source.location(initialization?initialization->offset:token.offset)});
                    };
                    for(unsigned n=0;n<f.parameters.size();++n)
                        binding(f.parameters[n].token,f.parameters[n].slot,n+1+unsigned(f.receiver.has_value()),0);
                    // Walk authored lexical bodies without adding recursive native frames.
                    // Each declaration starts a scope through the end of its body: a
                    // later declaration cannot expose an uninitialized/shadowed slot.
                    struct Body {const std::vector<Statement>* statements; std::size_t end; unsigned parent;};
                    std::vector<Body> bodies{{&f.body,f.source_end,0}};
                    auto& scopes=result->debug_scopes_[f.symbol];
                    while(!bodies.empty()) {
                        const auto body=bodies.back();bodies.pop_back();auto scope=body.parent;
                        const auto& statements=*body.statements;
                        for(std::size_t n=0;n<statements.size();++n) {
                            const auto& statement=statements[n];
                            auto nested=scope;
                            const auto end=n+1<statements.size()?statements[n+1].token.offset:body.end;
                            auto enter=[&](std::size_t begin,std::size_t finish,unsigned parent) {
                                scopes.push_back({parent,unit->source.location(begin),unit->source.location(finish)});
                                return static_cast<unsigned>(scopes.size());
                            };
                            if(statement.kind==Statement::Kind::variable || statement.kind==Statement::Kind::destructure) {
                                scope=enter(statement.token.offset,body.end,scope);nested=scope;
                                if(statement.kind==Statement::Kind::variable)binding(statement.token,statement.slot,0,scope);
                                else if(statement.expression->type!=error_type)
                                    for(const auto& local:statement.bindings)binding(local.token,local.slot,0,scope);
                            }
                            if(!statement.body.empty()) {
                                // Loop binders initialize at the loop token in lowered IR.
                                auto child=enter(statement.token.offset,
                                    statement.otherwise.empty()?end:statement.otherwise.front().token.offset,nested);
                                if(statement.kind==Statement::Kind::loop && !statement.binder.text.empty()) {
                                    binding(statement.binder,statement.slot,0,child,&statement.token);
                                    if(!statement.index_binder.text.empty())binding(statement.index_binder,statement.index_slot,0,child,&statement.token);
                                }
                                bodies.push_back({&statement.body,statement.otherwise.empty()?end:statement.otherwise.front().token.offset,child});
                            }
                            if(!statement.otherwise.empty()) {
                                const auto child=enter(statement.otherwise.front().token.offset,end,nested);
                                bodies.push_back({&statement.otherwise,end,child});
                            }
                        }
                    }
                }
                if(f.overriding) {Expr use(Expr::Kind::name,f.token);reference(f.symbol,f.virtual_slot,use,*unit,true);}
                for (const auto& p : f.parameters) if (p.default_expression) expression(*p.default_expression,f.symbol,{});
                std::map<unsigned,std::string> locals;
                for(const auto& binding:f.editor_bindings) {
                    if(presentation && f.locals.at(binding.slot)==error_type)continue;
                    if(!f.generic_origin.empty() || f.display_adapter || f.json_adapter || f.enum_adapter || binding.token.text.starts_with("$iterator$") || (f.global_initializer && !valid_editor_identifier(binding.token.text)))continue;
                    const bool parameter=(f.receiver && f.receiver->slot==binding.slot) ||
                        std::any_of(f.parameters.begin(),f.parameters.end(),[&](const auto& p){return p.slot==binding.slot;});
                    if(binding.readonly && !parameter) continue;
                    auto key=source_identity(f.symbol)+"::local::"+std::to_string(binding.slot);
                    auto kind=parameter ? AnalyzedDeclaration::Kind::parameter : AnalyzedDeclaration::Kind::local_variable;
                    AnalyzedDeclaration record{kind,key,id,"local",location(binding.token),{},f.locals.at(binding.slot),{},{},source_identity(f.symbol)};
                    record.readonly=binding.readonly;
                    record.rename_safe=!f.receiver && !f.anonymous && f.lambdas.empty() && f.captures.empty() && valid_editor_identifier(binding.token.text);
                    result->editor_declarations_.push_back(std::move(record));
                    locals.emplace(binding.slot,key);
                }
                for(const auto& [slot,capture]:f.capture_inputs) {
                    if(capture>=f.captures.size())continue; // Bound receiver environments have no written capture.
                    const auto& token=f.captures.at(capture).token;
                    for(const auto* records:{&result->declarations_,&result->editor_declarations_})for(const auto& record:*records)
                        if(record.module==id && record.source.begin==token.offset && record.source.end==token.offset+token.text.size())locals.emplace(slot,record.identity);
                }
                std::map<std::string,unsigned> names;
                std::function<void(const std::vector<Statement>&)> body;
                body = [&](const auto& statements) {
                    for (const auto& s : statements) {
                        if(presentation && !s.editor_checked) {
                            body(s.body);body(s.otherwise);
                            continue;
                        }
                        auto owner = f.symbol;
                        if (s.readonly) {
                            owner += "::const::" + s.name + "::" + std::to_string(names[s.name]++);
                            AnalyzedDeclaration record{AnalyzedDeclaration::Kind::local_constant,owner,id,"local",location(s.token),{},f.locals.at(s.slot),s.constant_syntax,{},f.symbol};
                            record.rename_safe=!f.receiver && !f.anonymous && f.generic_origin.empty() && f.lambdas.empty() && f.captures.empty();
                            declaration(std::move(record));
                        }
                        if(s.kind==Statement::Kind::assignment && locals.contains(s.slot) && !s.selection) {
                            Expr use(Expr::Kind::name,s.token);reference(owner,locals.at(s.slot),use,*unit,false,false);
                        }
                        if (s.kind == Statement::Kind::assignment && !s.fields.empty() &&
                            s.field_indexes.size()==s.fields.size()) {
                            auto t = f.locals.at(s.slot);
                            for (unsigned n = 0; n < s.fields.size(); ++n) {
                                Expr use(Expr::Kind::name,s.fields[n]); reference(owner,t.definition->fields.at(s.field_indexes[n]).owner + "::field::" + s.fields[n].text,use,*unit,s.implicit_fields[n]);
                                t = t.definition->fields.at(s.field_indexes[n]).type;
                            }
                        }
                        if(!s.compound_target.empty()){Expr use(Expr::Kind::name,s.compound_token);reference(owner,s.compound_target,use,*unit);}
                        if(s.compound_conversion)expression(*s.compound_conversion,owner,locals);
                        if (s.selection) expression(*s.selection,owner,locals);
                        if (s.range_end) expression(*s.range_end,owner,locals);
                        if (s.index) expression(*s.index,owner,locals);
                        if (s.expression) expression(*s.expression,owner,locals);
                        if(s.pattern)visit_patterns(*s.pattern,owner,locals,false);
                        if (s.readonly) locals.emplace(s.slot,owner);
                        body(s.body); body(s.otherwise);
                    }
                };
                if(f.base_prelude) expression(*f.base_prelude,f.symbol,locals);
                for(const auto& child:f.initialization_defaults) visit_expression(*child,f.symbol,locals,true);
                body(f.body);
            }
        }
        for(const auto& binding:context.member_catalog->interface_bindings) {
            field(result->contract_,"interface-binding");field(result->contract_,binding.key);
            for(const auto& [requirement,route]:binding.methods) {
                const auto& owner=*graph->at(binding.interface.definition->module_identity);
                Expr use(Expr::Kind::name,Token{TokenKind::name,binding.interface.definition->name,binding.interface.definition->source_offset});
                reference(requirement,route.second,use,owner,true);
            }
        }
        // One generic source expression may be instantiated with several exact
        // contracts. Keep one written reference per target, but present every
        // observed concrete signature instead of whichever instance came first.
        using PresentationKey=std::tuple<std::string,std::string,std::string,std::size_t,std::size_t>;
        std::map<PresentationKey,std::set<std::string>> presentations;
        auto presentation_key=[](const AnalyzedReference& r){return PresentationKey{r.from,r.to,r.source.module_identity,r.source.begin,r.source.end};};
        for(const auto* records:{&result->references_,&result->editor_references_})for(const auto& r:*records)if(!r.signature.empty())presentations[presentation_key(r)].insert(r.signature);
        for(auto* records:{&result->references_,&result->editor_references_})for(auto& r:*records)if(!r.implicit && !r.signature.empty()) {
            const auto& alternatives=presentations.at(presentation_key(r));
            if(alternatives.size()>1){r.signature="Selected specializations:";for(const auto& label:alternatives)r.signature+="\n\n"+label;}
        }
        field(result->contract_,result->constants_);
        result->lower_ = [graph,entry,library_scope,native_authorized,editor_only,dependencies=std::move(dependencies)] {
            if(editor_only)throw std::invalid_argument("Editor document analysis cannot lower IR.");
            if(library_scope)throw std::invalid_argument("Library analysis has no executable entry or supported IR build operation.");
            if(!native_authorized)throw std::invalid_argument("Native project execution requires explicit host authorization.");
            return lower_graph(*graph,entry,dependencies);
        };
        return result;
    }
};
ir::Module ValidatedAnalysis::lower(ir::Emission emission) const {
    observation::Scope timing(observation::Phase::typed_ir);
    auto module = lower_(); module.constant_contract = constants_; module.analysis = shared_from_this();
    module.application_metadata = application_metadata_;
    // The complete graph has already passed checking and typed-IR validation.
    if(emission==ir::Emission::entry)ir::retain_entry(module);
    return module;
}
AnalysisResult analyze_source(const SourceFile& source) {
    try { auto program=validated_source(source);return {program,program->warnings()}; }
    catch (const CompilationError& e) { return {nullptr,e.diagnostics()}; }
}
AnalysisResult analyze_project(const std::filesystem::path& entry,const NativeProject& bindings) {
    try{validate_native_project(bindings);require_module_identity(bindings.module);auto root=std::filesystem::absolute(entry).parent_path();if(std::filesystem::exists(root/utf8_path(bindings.module).relative_path()))throw std::invalid_argument("Native descriptor module collides with a project source file");SourceOverrides overrides;
        overrides.emplace(bindings.module,SourceFile(path_utf8(root/utf8_path(bindings.module).relative_path()),native_project_source(bindings)));
        auto program=validated_project(entry,overrides,true,&bindings);return {program,program->warnings()};
    }catch(const CompilationError& e){return {nullptr,e.diagnostics()};}
}
AnalysisResult analyze_project(const std::filesystem::path& entry, CPrimitiveAdmission::Owner admission) {
    if (!admission) throw std::invalid_argument("C primitive project requires explicit host admission");
    if ("/" + path_utf8(entry.filename()) == admission->module())
        throw std::invalid_argument("A C primitive declaration module cannot be the project entry");
    SourceOverrides overrides;
    if(!admission->physical_source())
        overrides.emplace(admission->module(), SourceFile("<host binding " + admission->module() + ">", admission->source()));
    try {
        auto program = validated_project(entry, overrides, true, nullptr, nullptr, std::move(admission));
        return {program, program->warnings()};
    } catch (const CompilationError& failure) {
        return {nullptr, failure.diagnostics()};
    }
}
AnalysisResult analyze_project(const std::filesystem::path& entry) {
    try { auto program=validated_project(entry);return {program,program->warnings()}; }
    catch (const CompilationError& e) { return {nullptr,e.diagnostics()}; }
}
SourceFile read_source(const std::filesystem::path& path) {
    observation::Scope timing(observation::Phase::source_loading);
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw SourceReadError(path_utf8(path), "Cannot read source file.");
    std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (stream.bad()) throw SourceReadError(path_utf8(path), "Cannot finish reading source file.");
    return {path_utf8(path), std::move(text)};
}
std::shared_ptr<const ValidatedAnalysis> validated_source(const SourceFile& source) {
    std::map<std::string, std::unique_ptr<Unit>> units;
    auto identity = source.path().empty() ? std::string("<source>") : source.path();
    auto unit = std::make_unique<Unit>(source, identity);
    Diagnostics diagnostics; diagnostics.bind(source, identity);
    { observation::Scope timing(observation::Phase::lex_parse);
      parse_unit(*unit, diagnostics); }
    if (!unit->imports.empty()) {
        try { error(source, unit->imports.front().path, Code::import_context, "Imports need a filesystem project context.", "Use compile_project or the check/run/emit CLI with a source filename."); }
        catch (const CompilationError& failure) { diagnostics.add(failure); }
        unit->failed_import = true;
    }
    units.emplace(identity, std::move(unit));
    validate_graph(units, identity, diagnostics);
    return AnalysisAccess::build(std::move(units), identity,diagnostics.warnings());
}
std::shared_ptr<const ValidatedAnalysis> validated_project(const std::filesystem::path& entry, const SourceOverrides& overrides, bool verify_disk,const NativeProject* bindings,EditorProbe* editor,CPrimitiveAdmission::Owner primitives,const std::string* document,EditorRecovery* recovery) {
    for (const auto& [id, source] : overrides) { (void)source; require_module_identity(id); }
    std::error_code ec;
    auto absolute = std::filesystem::absolute(entry, ec);
    const auto& location=ec?entry:absolute;
    // A diagnostic anchor is needed even before the entry file can be opened.
    const auto seed=document?*document:"/" + path_utf8(location.filename());
    Unit anchor(SourceFile(path_utf8(location.parent_path()/utf8_path(seed).relative_path()), ""), "/");
    Diagnostics diagnostics; diagnostics.editor=editor;diagnostics.recovery=recovery;diagnostics.bind(anchor.source,seed);
    try {
    if (ec) error(anchor.source, {TokenKind::end, "", 0}, Code::import_read, "Cannot resolve the entry source path.");
    auto root = std::filesystem::canonical(absolute.parent_path(), ec);
    if (ec) error(anchor.source, {TokenKind::end, "", 0}, Code::import_read, "Cannot resolve the project root directory.", "Filesystem error " + std::to_string(ec.value()) + " (" + ec.category().name() + "): " + ec.message() + ". Check that the entry's parent directory exists and is readable.");
    SourceGraph graph(root, diagnostics, overrides,bindings,primitives);
    auto& first = [&]() -> Unit& {
        try { return graph.load(anchor, {TokenKind::path, seed, 0}); }
        catch (const CompilationError& failure) { diagnostics.add(failure); diagnostics.finish(); throw; }
    }();
    if(editor)editor->result.inputs_current=graph.input_check();
    // A disk read is not an atomic filesystem transaction. Re-discover paths and
    // compare exact bytes before accepting this captured graph. Overlays are frozen.
    if (verify_disk) {
        Diagnostics check;check.recovery=recovery;check.editor=editor;SourceGraph again(root, check, overrides,bindings,primitives);
        check.bind(anchor.source,seed);
        try {
            again.load(anchor, {TokenKind::path, first.identity, 0});
            // For ordinary checking, report repeated parse errors from the
            // original graph alongside any independent semantic diagnostics.
            // Keep the existing parse gates for completion and presentation.
            if(editor || recovery || !diagnostics.same_errors(check))check.finish_parse();
        } catch(const CompilationError& failure) { throw check.owned_failure(failure); }
        bool same = graph.units.size() == again.units.size();
        for (const auto& [id, unit] : graph.units) {
            auto next = again.units.find(id);
            same &= next != again.units.end() && unit->source.text() == next->second->source.text() && unit->source.path()==next->second->source.path();
        }
        if (!same) error(anchor.source, {TokenKind::end, "", 0}, Code::import_read,
            "Project sources changed during analysis.", "Retry analysis with a stable filesystem or open document overlays.");
    }
    validate_graph(graph.units, first.identity, diagnostics);
    if(editor && editor->operator_context) {
        std::map<std::string,EditorCandidate> candidates;
        for(auto& candidate:editor->result.candidates)candidates.emplace(candidate.name,std::move(candidate));
        editor->result.candidates.clear();
        for(auto& [name,candidate]:candidates) {
            if(editor->candidate_occurrences.at(name)!=editor->operator_occurrences)continue;
            std::set<std::string> details,docs;
            for(const auto& [id,u]:graph.units)for(const auto& f:u->functions)if(editor->candidate_targets.at(name).contains(f.symbol)) {
                details.insert(editor_signature(f));auto doc=source_documentation(u->source,f.token.offset).text(f.result!=Type::none);if(!doc.empty())docs.insert(std::move(doc));
            }
            if(editor->candidate_targets.at(name).contains("$collection_equal")) {
                details.insert("Collection content equality -> bool");
                docs.insert("Compares sequence elements in order, or dictionary key/value contents independently of insertion order, using existing element equality. != negates the same comparison.");
            }
            candidate.detail=details.size()>1?"Selected specializations:":"";
            for(const auto& detail:details){if(!candidate.detail.empty())candidate.detail+="\n\n";candidate.detail+=detail;}
            candidate.documentation.clear();for(const auto& doc:docs){if(!candidate.documentation.empty())candidate.documentation+="\n\n";candidate.documentation+=doc;}
            editor->result.candidates.push_back(std::move(candidate));
        }
    }
    auto identity = first.identity;
    if(editor && recovery)return {};
    return AnalysisAccess::build(std::move(graph.units), identity,diagnostics.warnings(),{}, {}, {}, true, {},
        document || recovery,recovery && recovery->presentation);
    } catch(const CompilationError& failure) { throw diagnostics.owned_failure(failure); }
}
void require_module_identity(const std::string& identity) {
    Unit anchor(SourceFile("<document identity>", ""), "/");
    if (standard_module(identity) || package_source(identity) || identity.empty() || identity[0] != '/' || normalize(anchor, {TokenKind::path, identity, 0}) != identity)
        throw std::invalid_argument("Document identity must be an exact canonical root-relative .void path");
}
AnalysisResult analyze_project(const std::filesystem::path& entry, const SourceOverrides& overrides) {
    try {auto program=validated_project(entry,overrides,true);return {program,program->warnings()};}
    catch (const CompilationError& failure) { return {{}, failure.diagnostics()}; }
}
#include "project_selection.inc"
std::string inspect_native_provider(const SourceFile& source,const BuildConditions& conditions) {
    Diagnostics diagnostics;
    Unit unit(source,"/candidate.void");
    parse_unit(unit,diagnostics,unit.identity,conditions);
    diagnostics.finish();
    return unit.c_provider;
}

std::set<std::string> inspect_project_imports(const std::filesystem::path& root,
                                            const std::optional<std::string>& manifest) {
    SourceOverrides sources;
    ManifestOverrides manifests;
    if(manifest)manifests.emplace("/void.project.toml",SourceFile(path_utf8(root/"void.project.toml"),*manifest));
    ProjectCapture capture(root,sources,manifests,nullptr,false,true);
    std::set<std::string> used, visited;
    std::function<void(Unit&)> walk=[&](Unit& unit) {
        if(!visited.insert(unit.identity).second)return;
        for(const auto& import:unit.imports) {
            if(import.path.kind!=TokenKind::path && import.path.text.starts_with("pkg/")) {
                auto alias=import.path.text.substr(4);
                if(!capture.manifest->dependencies().contains(alias))
                    error(unit.source,import.path,Code::import_path,"Undeclared direct package import '"+import.path.text+"'.",
                          "Add an explicit local path or immutable Git source with voidc add.");
                used.insert(std::move(alias));
            } else {
                auto& target=capture.graph.parse(unit,import.path);
                if(!standard_module(target.identity))walk(target);
            }
        }
    };
    if(auto entry=capture.manifest->entry())walk(capture.graph.parse(capture.from,{TokenKind::path,"/"+*entry,0}));
    else for(const auto& id:capture.inventory())walk(capture.graph.parse(capture.from,{TokenKind::path,id,0}));
    capture.diagnostics.finish();
    return used;
}

// Import recovery reads metadata only. Full imports still pass SourceGraph::load.
std::optional<EditorAssistance> import_assistance(const std::filesystem::path& entry,const SourceOverrides& overrides,
    const std::string& module,const SourceFile& source,std::size_t cursor,ProjectCapture* project=nullptr) {
    const auto& text=source.text();
    auto line=cursor?text.find_last_of("\r\n",cursor-1):std::string::npos;line=line==std::string::npos?0:line+1;
    auto start=line;if(!start && text.starts_with("\xef\xbb\xbf"))start=3;
    while(start<text.size() && text[start]==' ')++start;
    if(text.compare(start,3,"use") || start+3>=text.size() || text[start+3]!=' ' || cursor<start+4)return {};
    start+=4;while(start<cursor && text[start]==' ')++start;
    bool quoted=start<text.size() && text[start]=='"';if(quoted)++start;
    EditorAssistance out;out.import_context=true;out.begin=start;out.end=cursor;
    auto stop=start;
    while(stop<text.size() && text[stop]!='"' && text[stop]!='\r' && text[stop]!='\n' && (quoted || (text[stop]!='#' && text[stop]!=' ' && text[stop]!='\t')))++stop;
    if(cursor>stop)return out;
    auto prefix=text.substr(start,cursor-start);
    auto slash=prefix.rfind('/');auto directory=slash==std::string::npos?std::string{}:prefix.substr(0,slash+1);
    auto leaf=slash==std::string::npos?prefix:prefix.substr(slash+1);
    const bool local=!quoted && prefix.starts_with("lib/");
    out.begin=start+directory.size();out.end=stop;
    bool closing=quoted && stop<text.size() && text[stop]=='"';
    if(!quoted) {
        for(char c:prefix)if(!(letter(c)||digit(c)||c=='/'))return out;
        if(prefix.starts_with('/') || prefix.find("//")!=std::string::npos)return out;
    }
    Unit anchor(source,module);Diagnostics diagnostics;SourceGraph legacy(std::filesystem::canonical(project?project->root:entry.parent_path()),diagnostics,overrides);
    auto& graph=project?project->graph:legacy;
    auto token=[&](std::string name){return Token{quoted?TokenKind::path:TokenKind::name,std::move(name),start};};
    const auto* direct=graph.direct_packages(module);
    if(!quoted && directory.starts_with("pkg/")) {
        if(directory=="pkg/" && direct)for(const auto& [alias,id]:*direct)if(identifier(alias) && alias.starts_with(leaf)) {
            if(out.candidates.size()>=256){out.incomplete=true;break;}
            out.candidates.push_back({alias,id,17,alias});
        }
        return out;
    }
    if(!quoted && directory.empty() && direct && !direct->empty() && std::string_view("pkg/").starts_with(leaf))
        out.candidates.push_back({"pkg/","locked packages",19,"pkg/"});
    if(!quoted && directory.empty() && std::string_view("lib/").starts_with(leaf)) {
        try {(void)graph.resolve(anchor,token("lib/_void_discovery_"),true);out.candidates.push_back({"lib/","project-local libraries",19,"lib/"});}catch(const CompilationError&){}
    }
    try {
        auto dummy=directory+"_void_discovery_"+(quoted?".void":"");
        auto [id,physical]=graph.resolve(anchor,token(dummy),true);
        if(id=="/")id.clear();
        std::set<std::string> children;
        if(const auto* owner=graph.package(id)) {
            auto relative=std::string(package_relative_source(id));
            if(!relative.empty())relative+="/";
            for(auto child=owner->sources.lower_bound(relative);child!=owner->sources.end() && child->first.starts_with(relative);++child) {
                const auto name=std::string_view(child->first).substr(relative.size());
                children.emplace(name.substr(0,name.find('/')));
            }
        } else {
            std::error_code ec;
            for(std::filesystem::directory_iterator it(physical,ec),end;!ec && it!=end;it.increment(ec))children.insert(path_utf8(it->path().filename()));
            if(ec && ec!=std::errc::no_such_file_or_directory)return out;
            if(quoted || local)for(const auto& [key,value]:overrides)if(value && key.starts_with(id+"/")) {
                auto child=key.substr(id.size()+1);children.insert(child.substr(0,child.find('/')));
            }
        }
        for(const auto& child:children) {
            if(child=="@stdlib")continue;
            auto file_name=quoted?child:(child.ends_with(".void")?child.substr(0,child.size()-5):child);
            if(!file_name.starts_with(leaf))continue;
            // Resolve each eligible immediate child with the loader's exact-case,
            // portable path, overlay, alias and tombstone checks. Never parse it.
            bool folder=false,file=false;
            try {(void)graph.resolve(anchor,token(directory+child+"/_void_discovery_"+(quoted?".void":"")),true);folder=true;}catch(const CompilationError&){}
            const bool local_root=local && directory=="lib/";
            if((local_root && folder) || (!local_root && child.ends_with(".void")))try {(void)graph.resolve(anchor,token(directory+(quoted || local_root?child:child.substr(0,child.size()-5))));file=true;}catch(const CompilationError&){}
            if(!quoted && !local && directory.empty() && (child=="lib" || child=="pkg"))folder=false;
            if(!quoted) {auto valid_segment=[](const std::string& name){return !name.empty() && letter(name[0]) && std::all_of(name.begin(),name.end(),[](char c){return letter(c)||digit(c);});};if(!valid_segment(file_name))file=false;if(!valid_segment(child))folder=false;}
            if(!folder && !file)continue;
            if(out.candidates.size()>=256){out.incomplete=true;break;}
            if(file)out.candidates.push_back({file_name,quoted?"project source":local?"local library":"standard module",17,file_name+(quoted && !closing?"\"":"")});
            if(folder) {
                if(out.candidates.size()>=256){out.incomplete=true;break;}
                out.candidates.push_back({child+"/","folder",19,child+"/"});
            }
        }
    }catch(const CompilationError&){}
    return out;
}
bool valid_editor_identifier(const std::string& name) {
    return !name.empty() && letter(name[0]) && !reserved.contains(name) &&
        std::all_of(name.begin(),name.end(),[](char c){return letter(c)||digit(c);});
}
std::optional<EditorAssistance> editor_namespace_assistance(const ValidatedAnalysis& known,const SourceFile& source,
    const std::string& module,std::size_t cursor) {
    const auto found=std::find_if(known.modules().begin(),known.modules().end(),[&](const auto& m){return m.identity==module;});
    if(found==known.modules().end() || found->qualifiers.empty() || cursor>source.text().size())return {};
    const auto& text=source.text();
    const auto line=source.location(cursor).line-1;
    const auto start=source.line_starts().at(line);
    const auto newline=text.find_first_of("\r\n",cursor);
    const auto stop=newline==std::string::npos?text.size():newline;
    if(!cursor || text.find("_void_editor_cursor_")!=std::string::npos)return {};
    const auto dot=text.rfind('.',cursor-1);
    if(dot==std::string::npos || dot<start)return {};
    auto end=dot+1;
    while(end<text.size() && (letter(text[end]) || digit(text[end])))++end;
    if(cursor<dot+1 || cursor>end)return {};
    const auto name=std::string_view(text).substr(dot+1,end-dot-1);
    // Use the ordinary lexer and parser to classify this exact selection. A
    // name in a string/comment, type annotation, import or chained value is not
    // a namespace expression merely because its text contains an alias and dot.
    observation::Scope syntax_timing(observation::Phase::lex_parse);
    Diagnostics diagnostics;
    EditorRecovery recovery{module,{}};
    const auto lexed=lex(source,diagnostics,false,&recovery,&found->conditions);
    if(diagnostics.failed())return {};
    auto physical=std::find_if(lexed.begin(),lexed.end(),[&](const auto& token) {
        return token.offset==dot && token.kind==TokenKind::symbol && token.text==".";
    });
    if(physical==lexed.end() || physical==lexed.begin() || std::prev(physical)->kind!=TokenKind::name)return {};
    const auto receiver=std::prev(physical)->text;
    auto target=found->qualifiers.find(receiver);
    if(target==found->qualifiers.end())return {};
    for(const auto& declaration:known.declarations())if(declaration.module==module &&
        declaration.kind!=AnalyzedDeclaration::Kind::import_alias && declaration.source.source &&
        declaration.source.source->text().substr(declaration.source.begin,declaration.source.end-declaration.source.begin)==receiver)
        return {}; // Conservative delegation for any possible local shadow.
    auto repaired=text;
    repaired.replace(dot+1,end-dot-1,"_void_editor_cursor_");
    int parentheses=0;
    for(const auto& token:lexed)if(token.offset>=start && token.offset<stop) {
        if(token.text=="(")++parentheses;else if(token.text==")")--parentheses;
    }
    if(parentheses<0 || parentheses>32)return {};
    if(parentheses)repaired.insert(stop+std::string_view("_void_editor_cursor_").size()-(end-dot-1),static_cast<std::size_t>(parentheses),')');
    SourceFile parsed_source(source.path(),std::move(repaired));
    Unit parsed(parsed_source,module);
    Diagnostics syntax;
    std::vector<Token> syntax_tokens;
    parse_unit(parsed,syntax,module,found->conditions,&syntax_tokens);
    if(syntax.failed())return {};
    // Body edits do not change an imported namespace's declarations. Compare
    // actual compiler tokens outside ordinary function bodies, not timestamps
    // or an ad-hoc import scanner. New imports, types, globals, signatures and
    // entry declarations delegate to the ordinary project probe.
    Unit previous(found->source,module);Diagnostics previous_syntax;
    std::vector<Token> previous_tokens;
    parse_unit(previous,previous_syntax,module,found->conditions,&previous_tokens);
    if(previous_syntax.failed())return {};
    auto headers=[](const SourceFile& source,const std::vector<Token>& tokens,const Unit& unit) {
        std::vector<std::pair<std::size_t,std::size_t>> bodies;
        for(const auto* functions:{&unit.functions,&unit.generic_functions})for(const auto& function:*functions) {
            if(function.body.empty() || function.anonymous || function.global_initializer || function.token.text.starts_with("$"))continue;
            const auto begin=source.line_starts().at(source.location(function.body.front().token.offset).line-1);
            if(function.source_end>begin)bodies.emplace_back(begin,function.source_end);
        }
        std::sort(bodies.begin(),bodies.end());
        std::vector<std::pair<std::size_t,std::size_t>> ranges;
        for(const auto& body:bodies) {
            if(!ranges.empty() && body.first<ranges.back().second)ranges.back().second=std::max(ranges.back().second,body.second);
            else ranges.push_back(body);
        }
        std::vector<std::pair<TokenKind,std::string>> result;
        for(const auto& token:tokens) {
            const auto next=std::upper_bound(ranges.begin(),ranges.end(),token.offset,[](auto offset,const auto& span){return offset<span.first;});
            const bool body=next!=ranges.begin() && (token.offset<std::prev(next)->second ||
                // The lexer supplies a newline at EOF for an unterminated last
                // line. Like its closing dedent, that token belongs to the body;
                // it must not turn an ordinary body edit into a header change.
                ((token.kind==TokenKind::dedent || token.kind==TokenKind::newline) && token.offset==std::prev(next)->second));
            if(!body)result.emplace_back(token.kind,token.text);
        }
        return result;
    };
    if(headers(parsed_source,syntax_tokens,parsed)!=headers(found->source,previous_tokens,previous))return {};
    bool expression=false;
    for(const auto& function:parsed.functions) {
        if(function.receiver && function.receiver->token.text==receiver)return {};
        for(const auto& parameter:function.parameters)if(parameter.token.text==receiver)return {};
        std::vector<const Statement*> statements;
        std::vector<const Expr*> expressions;
        for(const auto& statement:function.body)statements.push_back(&statement);
        while(!statements.empty()) {
            const auto& statement=*statements.back();statements.pop_back();
            if(statement.name==receiver || statement.binder.text==receiver || statement.index_binder.text==receiver)return {};
            for(const auto& binding:statement.bindings)if(binding.token.text==receiver)return {};
            for(const auto& field:statement.fields)if(field.text==receiver)return {};
            if(statement.pattern) {
                std::vector<const Pattern*> pending{&*statement.pattern};
                while(!pending.empty()) {
                    const auto& pattern=*pending.back();pending.pop_back();
                    if(pattern.kind==Pattern::Kind::capture && pattern.token.text==receiver)return {};
                    for(const auto& child:pattern.children)pending.push_back(&child);
                }
            }
            for(const auto* part:{statement.expression.get(),statement.index.get(),statement.selection.get(),statement.range_end.get()})if(part)expressions.push_back(part);
            for(const auto* body:{&statement.body,&statement.otherwise})for(const auto& child:*body)statements.push_back(&child);
        }
        while(!expressions.empty()) {
            const auto& part=*expressions.back();expressions.pop_back();
            if(part.token.text=="_void_editor_cursor_" && (part.kind==Expr::Kind::field || part.kind==Expr::Kind::method_call) &&
                !part.parts.empty() && part.parts.front()->kind==Expr::Kind::name && part.parts.front()->token.text==receiver)expression=true;
            std::vector<const Pattern*> patterns;for(const auto& pattern:part.patterns)patterns.push_back(&pattern);
            while(!patterns.empty()) {
                const auto& pattern=*patterns.back();patterns.pop_back();
                if(pattern.kind==Pattern::Kind::capture && pattern.token.text==receiver)return {};
                for(const auto& child:pattern.children)patterns.push_back(&child);
            }
            for(const auto& child:part.parts)expressions.push_back(child.get());
        }
    }
    if(!expression)return {};
    syntax_timing.finish();
    observation::Scope candidates_timing(observation::EditorDetail::candidates);
    EditorAssistance out;out.begin=dot+1;out.end=end;
    const auto prefix=name.substr(0,cursor-out.begin);
    std::map<std::string,EditorCandidate> candidates;
    auto consider=[&](const AnalyzedDeclaration& d) {
        if(d.static_member || d.module!=target->second || !source_visible(d.visibility,d.module,module) ||
           !d.source.source || d.source.end<=d.source.begin)return;
        const auto label=d.source.source->text().substr(d.source.begin,d.source.end-d.source.begin);
        if(!label.starts_with(prefix) || !valid_editor_identifier(label))return;
        using K=AnalyzedDeclaration::Kind;
        int kind;
        std::string detail;
        switch(d.kind) {
        case K::function: kind=3;break;
        case K::global_variable: kind=6;break;
        case K::module_constant: if(d.editor_symbol_kind==22)return;kind=21;break;
        case K::value_structure: case K::managed_structure: case K::interface_type:
            kind=7;detail=d.generic_template?"generic type":"type";break;
        default:return;
        }
        if(d.builtin)return;
        if(detail.empty())detail=!d.signature.empty() && d.kind==K::function?d.signature:ir::type_name(d.type);
        candidates.try_emplace(label,EditorCandidate{label,std::move(detail),kind,{},
            d.documentation.text(d.kind==K::function && d.type!=Type::none)});
    };
    for(const auto& d:known.declarations())consider(d);
    for(const auto& d:known.editor_declarations())consider(d);
    for(auto& [label,candidate]:candidates) {
        if(out.candidates.size()==2048){out.incomplete=true;break;}
        out.candidates.push_back(std::move(candidate));
    }
    return out;
}
EditorAssistance editor_assistance_impl(const std::filesystem::path& entry,const SourceOverrides& overrides,const std::string& module,std::size_t cursor,ProjectCapture* project=nullptr,const std::optional<std::string>& project_entry={},bool document=false,const EditorDiscovery* discovery=nullptr) {
    const bool locked_source=project && package_source(module);
    if(!locked_source)require_module_identity(module);
    EditorAssistance empty;
    auto selected=overrides.find(module);
    if(selected!=overrides.end() && !selected->second)return empty;
    SourceFile source=locked_source?project->graph.package_snapshot(module):project?project->recapture_source(module):selected!=overrides.end() && selected->second ? *selected->second : read_source(entry.parent_path()/utf8_path(module).relative_path());
    if(project)empty.inputs_current=project_probe_inputs(*project);
    else empty.inputs_current=[entry,overrides,module,source] {
        const auto root=std::filesystem::canonical(entry.parent_path());
        Diagnostics diagnostics;SourceGraph fresh(root,diagnostics,overrides);Unit anchor(source,module);
        const auto [id,path]=fresh.resolve(anchor,{TokenKind::path,module,0});
        const auto found=overrides.find(module);
        return id==module && (found!=overrides.end() && found->second?found->second->text():read_source(path).text())==source.text();
    };
    const auto& text=source.text();
    if(cursor>text.size() || text.find("_void_editor_cursor_")!=std::string::npos) return empty;
    const auto conditions=project?project->graph.conditions_for(module):native_build_conditions();
    if(text.find("compile")!=std::string::npos) {
        std::vector<ConditionalRegion> conditional_regions;
        Diagnostics selection_diagnostics;EditorRecovery selection_recovery{module,{}};
        (void)lex(source,selection_diagnostics,false,&selection_recovery,&conditions,&conditional_regions);
        for(const auto& region:conditional_regions)if(region.begin<=cursor && cursor<=region.end)return empty;
        try {selection_diagnostics.finish();}
        catch(const CompilationError& failure) {
            if(std::any_of(failure.diagnostics().begin(),failure.diagnostics().end(),[](const auto& d){return d.code==Code::conditional;}))return empty;
        }
    }
    if(auto imports=import_assistance(entry,overrides,module,source,cursor,project)) {
        // Import completion also depends on directory membership and candidate
        // path admission. Re-run that metadata-only query, not a semantic bind.
        imports->inputs_current=[check=project?project_probe_inputs(*project):empty.inputs_current,
            root=project?std::optional(project->root):std::nullopt,entry,overrides,
            manifests=project?project->manifest_overlays:ManifestOverrides{},module,source,cursor,expected=*imports] {
            if(!check())return false;
            std::unique_ptr<ProjectCapture> fresh;
            if(root)fresh=std::make_unique<ProjectCapture>(*root,overrides,manifests);
            const auto current=import_assistance(entry,overrides,module,source,cursor,fresh.get());
            if(!current || current->candidates.size()!=expected.candidates.size() || current->incomplete!=expected.incomplete ||
                current->begin!=expected.begin || current->end!=expected.end)return false;
            for(std::size_t n=0;n<current->candidates.size();++n) {
                const auto& a=current->candidates[n];const auto& b=expected.candidates[n];
                if(a.name!=b.name || a.detail!=b.detail || a.kind!=b.kind || a.insert_text!=b.insert_text)return false;
            }
            return true;
        };
        return *imports;
    }
    if(locked_source)return empty;
    EditorRecovery token_recovery{module,{}};
    Diagnostics diagnostics;auto tokens=lex(source,diagnostics,true,&token_recovery,&conditions);
    if(diagnostics.failed()) return empty;
    std::size_t begin=cursor,end=cursor;
    bool format_context=false,operator_context=false;
    for(const auto& t:tokens) {
        auto r=span(source,t);
        if(t.kind==TokenKind::name && r.begin<=cursor && cursor<=r.end){begin=r.begin;end=r.end;break;}
        if(t.format_spec && r.begin<=cursor && cursor<=r.end){begin=r.begin;end=r.end;format_context=true;break;}
        if(t.kind==TokenKind::string && !t.source_width && r.begin<cursor && cursor<r.end){
            auto quotes=text.compare(r.begin,3,"\"\"\"")==0?3u:1u;
            begin=r.begin+quotes;end=r.end-quotes;if(cursor<begin || cursor>end)return empty;
            format_context=true;break;
        }
        if(t.kind==TokenKind::symbol && r.begin<=cursor && cursor<r.end &&
            (t.text=="+" || t.text=="-" || t.text=="*" || t.text=="/" || t.text=="%" || t.text=="==" || t.text=="!=" || t.text=="<" || t.text==">" || t.text=="<=" || t.text==">=" || t.text=="+=" || t.text=="-=" || t.text=="*=" || t.text=="/=" || t.text=="%=")) {
            begin=r.begin;end=r.end;operator_context=true;break;
        }
        if(r.begin<=cursor && cursor<r.end && (t.kind==TokenKind::string || t.kind==TokenKind::path || t.kind==TokenKind::number)) return empty;
    }
    // Reject trivia/comment positions. Only whitespace between compiler tokens is eligible.
    auto line=text.find_last_of("\r\n",cursor ? cursor-1 : 0);line=line==std::string::npos ? 0 : line+1;
    for(std::size_t n=line;n<cursor;++n) if(text[n]=='#') {
        bool literal=false;for(const auto& t:tokens){auto r=span(source,t);if(r.begin<=n && n<r.end)literal=true;}
        if(!literal)return empty;
    }
    empty.begin=begin;empty.end=end;
    auto leading=text.substr(line,begin-line);
    if(leading.find_first_not_of(' ')==std::string::npos) {
        auto prefix=text.substr(begin,cursor-begin);
        for(const auto& keyword:reserved)if(keyword.starts_with(prefix))empty.candidates.push_back({keyword,"keyword",14});
    }
    auto repaired=text;if(!operator_context)repaired.replace(begin,end-begin,"_void_editor_cursor_");
    if(!format_context && !operator_context) {
    // Repair physical delimiters only; interpolation lowering adds synthetic
    // parentheses at expression offsets that are not source characters.
    EditorRecovery physical_recovery{module,{}};
    Diagnostics physical_diagnostics;auto physical=lex(source,physical_diagnostics,false,&physical_recovery,&conditions);
    if(physical_diagnostics.failed())return empty;
    auto stop=text.find_first_of("\r\n",end);if(stop==std::string::npos)stop=text.size();
    // Match across continuation lines. A ')' on this line can close a header
    // from an earlier line, and an existing later ')' must not be duplicated.
    std::vector<const Token*> openings;
    for(const auto& token:physical) {
        if(token.text=="(")openings.push_back(&token);
        else if(token.text==")") {if(openings.empty())return empty;openings.pop_back();}
    }
    const auto missing=std::count_if(openings.begin(),openings.end(),[&](const auto* token){return token->offset<stop;});
    if(missing>32)return empty;
    std::size_t insertion=end;const Token* last=nullptr;
    for(const auto& token:physical) {
        if(token.kind==TokenKind::string && token.offset<end && span(source,token).end>=end){last=&token;insertion=span(source,token).end;}
        if(token.offset<end || token.offset>=stop || token.kind==TokenKind::newline || token.kind==TokenKind::indent || token.kind==TokenKind::dedent || token.kind==TokenKind::end)continue;
        last=&token;insertion=span(source,token).end;
    }
    // Keep the existing conditional colon and leave comments/literal bytes alone.
    if(last && last->text==":")insertion=last->offset;
    if(missing){if(insertion>stop)return empty;repaired.insert(insertion+std::string_view("_void_editor_cursor_").size()-(end-begin),static_cast<std::size_t>(missing),')');}
    }
    auto frozen=overrides;frozen.insert_or_assign(module,SourceFile(source.path(),std::move(repaired)));
    EditorProbe probe{module,begin,{},operator_context,{},{},0,{}};probe.result.begin=begin;probe.result.end=end;
    probe.prefix=operator_context?std::string{}:text.substr(begin,cursor-begin);
    EditorRecovery recovery{module,{}};
    if(begin && !format_context && !operator_context) {
        const auto previous=text.find_last_not_of(" \t",begin-1);
        recovery.member_probe=previous!=std::string::npos && text[previous]=='.';
    }
    try {
        if(project && document) {
            ProjectCapture repaired(project->root,frozen,project->manifest_overlays,nullptr,false,false,project->dependencies.get());
            (void)validated_editor_document(repaired,module,&probe,&recovery);
        } else if(project) {
            // Reuse only derived metadata from this job's admitted capture.
            // ProjectCapture still rereads and verifies the lock, exact bytes,
            // catalog selection and file identities through the normal resolver.
            auto checked=analyze_manifest_project_impl(project->root,project_entry,frozen,project->manifest_overlays,&probe,false,discovery,&recovery,nullptr,project->dependencies.get());
            if(std::any_of(checked.diagnostics.begin(),checked.diagnostics.end(),[](const auto& diagnostic) {
                return diagnostic.code==Code::project_boundary || diagnostic.code==Code::project_changed;
            }))return {};
        } else (void)validated_project(entry,frozen,true,nullptr,&probe,{},document?&module:nullptr,&recovery);
        if(operator_context)return probe.result;
    }
    catch(const EditorProbeFound&) {
        const auto prefix=operator_context?std::string{}:text.substr(begin,cursor-begin);
        std::erase_if(probe.result.candidates,[&](const auto& c){return !c.name.starts_with(prefix);});
        return probe.result;
    } catch(const CompilationError&) {}
    // Once graph work was attempted, the initial source-only/lexical check
    // cannot stand in for the inputs read by binding (or a failed load).
    empty.inputs_current=std::move(probe.result.inputs_current);
    return empty;
}
EditorAssistance editor_assistance(const std::filesystem::path& entry,const SourceOverrides& sources,const std::string& module,std::size_t cursor) {
    return editor_assistance_impl(entry,sources,module,cursor);
}
bool editor_assistance_inputs_current(const EditorAssistance& result) {
    observation::Scope timing(observation::EditorDetail::inputs);
    try {return result.inputs_current && result.inputs_current();}
    catch(const CompilationError&){return false;}
    catch(const std::filesystem::filesystem_error&){return false;}
    catch(const std::invalid_argument&){return false;}
}
EditorAssistance project_editor_assistance(const std::filesystem::path& root,const std::optional<std::string>& entry,const SourceOverrides& sources,const ManifestOverrides& manifests,const std::string& module,std::size_t cursor,const EditorDiscovery& discovery) {
    try {ProjectCapture capture(root,sources,manifests);return editor_assistance_impl(root,sources,module,cursor,&capture,entry,false,&discovery);}
    catch(const CompilationError&){return {};}
    catch(const std::filesystem::filesystem_error&){return {};}
}
EditorAssistance document_editor_assistance(const std::filesystem::path& context,const SourceOverrides& sources,
    const std::string& module,std::size_t cursor,const ManifestOverrides* manifests) {
    try {
        if(!manifests)return editor_assistance_impl(context,sources,module,cursor,nullptr,{},true);
        ProjectCapture capture(context,sources,*manifests);
        return editor_assistance_impl(context,sources,module,cursor,&capture,{},true);
    } catch(const CompilationError&) {return {};}
      catch(const std::filesystem::filesystem_error&) {return {};}
}
std::optional<std::string> format_source(const SourceFile& source, bool insert_spaces) {
    // Parse with the ordinary compiler before proposing edits. This is local
    // syntax validation, never import discovery or a second semantic resolver.
    Diagnostics diagnostics;Unit parsed(source,source.path());parse_unit(parsed,diagnostics);
    if(diagnostics.failed() || !parsed.conditional_regions.empty())return {};
    auto tokens=lex(source,diagnostics,false);
    if(diagnostics.failed())return {};
    const auto& text=source.text();
    struct Edit {std::size_t begin,end;std::string text;};std::vector<Edit> edits;
    unsigned depth=0,brace_depth=0,parentheses=0;std::vector<bool> braces;std::set<std::size_t> lines;
    const Token* previous=nullptr;
    for(const auto& t:tokens) {
        if(t.kind==TokenKind::indent){++depth;continue;}
        if(t.kind==TokenKind::dedent){if(depth)--depth;continue;}
        if(t.kind==TokenKind::newline || t.kind==TokenKind::end)continue;
        if(t.text=="}" && !braces.empty()){if(braces.back() && brace_depth)--brace_depth;braces.pop_back();}
        auto r=span(source,t);
        auto start=text.find_last_of("\r\n",t.offset ? t.offset-1 : 0);start=start==std::string::npos?0:start+1;
        if(lines.insert(start).second) {
            auto indent=start;if(!start && text.starts_with("\xef\xbb\xbf"))indent=3;
            const auto end=indentation_at(text,indent).end;
            if(end==t.offset && !parentheses)edits.push_back({indent,end,std::string((depth+brace_depth)*(insert_spaces?4:1),insert_spaces?' ':'\t')});
        }
        if(previous) {
            auto left=span(source,*previous);
            if(left.end<=r.begin){auto gap=text.substr(left.end,r.begin-left.end);
                if(gap.find_first_not_of(' ')==std::string::npos) {
                    std::string replacement=gap.empty()?"":" ";
                    if(t.text=="," || t.text==":" || t.text==")" || t.text=="]" || t.text=="." || previous->text=="." || previous->text=="(" || previous->text=="[")replacement="";
                    else if(previous->text=="," || previous->text==":" || t.text=="=" || previous->text=="=")replacement=" ";
                    edits.push_back({left.end,r.begin,replacement});
                }
            }
        }
        if(t.text=="{"){bool literal=!previous || previous->text!="defer";braces.push_back(literal);if(literal)++brace_depth;}
        if(t.text=="(")++parentheses;else if(t.text==")" && parentheses)--parentheses;
        previous=&t;
    }
    // Retokenization proves the same structural indentation, including literal
    // blocks. Ambiguous/unsupported relationships produce no edit.
    auto result=text;for(auto it=edits.rbegin();it!=edits.rend();++it)result.replace(it->begin,it->end-it->begin,it->text);
    Diagnostics check;auto after=lex(SourceFile(source.path(),result),check,false);
    auto same_tokens=[](const auto& before,const auto& after) {
        if(before.size()!=after.size())return false;
        for(std::size_t n=0;n<before.size();++n)if(before[n].kind!=after[n].kind || before[n].text!=after[n].text || before[n].decoded!=after[n].decoded)return false;
        return true;
    };
    if(check.failed() || !same_tokens(tokens,after))return {};
    // Literal bytes alone are insufficient: reindenting the physical opening
    // line can change indentation-sensitive bodies inside an unchanged string.
    // Prove the parser sees the identical expanded stream as well.
    auto expanded=lex(SourceFile(source.path(),result),check);
    if(check.failed() || !same_tokens(tokens,expanded))return {};
    return result;
}

ir::Module compile_source(const SourceFile& source) {
    auto result = analyze_source(source);
    if (!result.succeeded()) throw CompilationError(std::move(result.diagnostics));
    return result.program->lower();
}
SourceProgram compile_project(const std::filesystem::path& entry) {
    auto result = analyze_project(entry);
    if (!result.succeeded()) throw CompilationError(std::move(result.diagnostics));
    auto module = result.program->lower();
    for (const auto& m : result.program->modules()) if (m.identity == module.identity) return {m.source,std::move(module)};
    throw std::logic_error("Validated entry absent");
}
namespace {
void entry_signature(const SourceFile& source,const std::string& identity,bool found,bool valid,std::size_t offset) {
    Diagnostics diagnostics;diagnostics.bind(source,identity);
    try {
        if(valid)return;
        if(found)error(source,{TokenKind::name,"entry",offset},Code::entry,"The CLI entry point must have signature entry(value: i32) -> i32.","Other functions can have any supported parameter list; choose an i32 adapter for the CLI.");
        error(source,{TokenKind::end,"",0},Code::entry,"No entry function found.","Define func entry(value: i32) -> i32 to run or emit this file; check accepts libraries without an entry.");
    } catch(const CompilationError& failure){diagnostics.add(failure);diagnostics.finish();}
}
}
void require_source_entry(const ValidatedAnalysis& analysis) {
    if(analysis.application())return;
    for(const auto& module:analysis.modules())if(module.identity==analysis.entry_identity()) {
        for(const auto& declaration:analysis.declarations())if(declaration.identity==module.identity+"::entry" && declaration.kind==AnalyzedDeclaration::Kind::function && !declaration.generic_template) {
            entry_signature(module.source,module.identity,true,declaration.parameters==std::vector<Type>{Type::i32} && declaration.type==Type::i32,declaration.source.begin);return;
        }
        entry_signature(module.source,module.identity,false,false,0);return;
    }
    throw std::logic_error("Validated entry absent");
}
void require_source_entry(const SourceFile& source, const ir::Module& module) {
    for(const auto& f:module.functions)if(f.name==module.entry_symbol) {
        std::size_t offset=0;
        while(offset<source.text().size() && source.location(offset).line<f.source.line)++offset;
        while(offset<source.text().size() && source.location(offset).column<f.source.column)++offset;
        entry_signature(source,module.identity,true,f.parameters==std::vector<Type>{Type::i32} && f.result==Type::i32,offset);return;
    }
    entry_signature(source,module.identity,false,false,0);
}
}

#undef VOID_SEMANTIC_HANDLER
