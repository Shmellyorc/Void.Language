#include "source_graph.hpp"
#include "diagnostics.hpp"
#include "parser.hpp"
#include "stack_guard.hpp"
#include "void/compiler_observation.hpp"
#include "void/console_ops.hpp"
#include "void/path.hpp"
#include <algorithm>

namespace vd::frontend {

std::string folded(std::string text) {
    for (auto& c : text) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return text;
}
bool identifier(const std::string& text) {
    return !text.empty() && letter(text[0]) && !reserved.contains(text) &&
        std::all_of(text.begin(), text.end(), [](char c) { return letter(c) || digit(c); });
}
static void portable_component(const SourceFile& source, const Token& at, const std::string& component) {
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

Diagnostic SourceReadError::record(const std::string& path, const std::string& message) {
    Diagnostic d{Code::import_read}; d.description = message;
    d.primary = {std::make_shared<const SourceFile>(path, ""), {}, 0, 0};
    d.hint = "Check the source path, permissions and storage availability.";
    return d;
}

SourceReadError::SourceReadError(const std::string& path, const std::string& message)
    : CompilationError({record(path, message)}) {}

const SourceGraph::Directory& SourceGraph::directory_entries(const std::filesystem::path& path) {
    auto [entry,fresh]=directories.try_emplace(path);
    if(fresh)for(std::filesystem::directory_iterator it(path,entry->second.error),end;!entry->second.error && it!=end;it.increment(entry->second.error)) {
        auto name=path_utf8(it->path().filename());entry->second.names[folded(name)].insert(name);
    }
    return entry->second;
}

BuildConditions SourceGraph::conditions_for(const std::string& id) const {
    auto result=conditions;
    if(standard_module(id))result.features.clear();
    else if(dependencies && package_source(id))result.features=package(id)->conditions;
    return result;
}

SourceGraph::SourceGraph(std::filesystem::path project_root, Diagnostics& d, const SourceOverrides& o,const NativeProject* bindings,
                         CPrimitiveAdmission::Owner c_primitives)
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

void SourceGraph::generated_primitives(CPrimitiveAdmission::Owner admission) {
    if(!admission->physical_source())
        virtual_children[""][folded(admission->module().substr(1))].insert(admission->module().substr(1));
    primitives = std::move(admission);
}

std::function<bool()> SourceGraph::input_check() const {
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

const project::LockedPackage* SourceGraph::package(std::string_view identity) const {
    if (!dependencies || !identity.starts_with(package_source_prefix)) return nullptr;
    const auto found = dependencies->packages().find(source_domain(identity).substr(package_source_prefix.size()));
    return found == dependencies->packages().end() ? nullptr : &found->second;
}

const project::DependencyEdges* SourceGraph::direct_packages(std::string_view identity) const {
    if (!dependencies || standard_module(identity)) return nullptr;
    if (!package_source(identity)) return &dependencies->direct();
    const auto* selected = package(identity);
    return selected ? &selected->dependencies : nullptr;
}

SourceFile SourceGraph::package_snapshot(const std::string& id) const {
    const auto* selected = package(id);
    if (selected) {
        const auto relative = package_relative_source(id);
        if (auto found = selected->sources.find(relative); found != selected->sources.end())
            return SourceFile(path_utf8(root / utf8_path(selected->locator) / utf8_path(relative)), found->second);
    }
    throw std::invalid_argument("Source is outside the verified package snapshot.");
}

std::string SourceGraph::package_identity(const std::filesystem::path& physical) const {
    if (!dependencies) return {};
    for (const auto& [id, selected] : dependencies->packages()) {
        const auto relative = path_utf8(physical.lexically_relative(root / utf8_path(selected.locator)));
        if (selected.sources.contains(relative)) return std::string(package_source_prefix) + id + "/" + relative;
    }
    return {};
}

std::pair<std::string,std::filesystem::path> SourceGraph::resolve(const Unit& from, const Token& path, bool directory) {
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

Unit& SourceGraph::parse(const Unit& from, const Token& path) {
    auto [id,physical]=resolve(from,path);
    return parse_resolved(from,path,id,physical);
}

Unit& SourceGraph::parse_resolved(const Unit& from,const Token& path,const std::string& id,const std::filesystem::path& physical) {
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

Unit& SourceGraph::load(const Unit& from, const Token& path) {
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
}
