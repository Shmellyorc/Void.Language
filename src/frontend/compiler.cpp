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

// Type lookup follows the same owned graph and direct-import boundary as values.
std::pair<Unit*, Structure*> structure_lookup(Unit& unit, Unit& root,
    const std::map<std::string,std::unique_ptr<Unit>>& units, const std::string& spelling, const Token& token) {
    auto dot = spelling.find('.'); auto name = spelling.substr(dot == std::string::npos ? 0 : dot + 1);
    if (dot == std::string::npos && unit.invalid_declarations.contains(name)) throw Poison{};
    std::map<std::string,std::pair<Unit*,Structure*>> candidates;
    auto add = [&](Unit* owner) {
        auto found = owner->structure_names.find(name);
        if (found != owner->structure_names.end() && source_visible(found->second->visibility,owner->identity,unit.identity))
            candidates.emplace(owner->identity,std::make_pair(owner,found->second));
    };
    if (dot != std::string::npos) {
        auto qualifier = spelling.substr(0,dot); auto found = unit.qualifiers.find(qualifier);
        if (unit.failed_qualifiers.contains(qualifier)) throw Poison{};
        if (found == unit.qualifiers.end()) return {nullptr,nullptr};
        if (found->second.size() != 1) error(unit.source,token,Code::ambiguous,"Ambiguous import qualifier '" + qualifier + "'.");
        add(found->second.front());
    } else {
        add(&unit);
        if (!candidates.empty()) return candidates.begin()->second;
        if (unit.constant_names.contains(name) || unit.declarations.contains(name)) return {nullptr,nullptr};
        if(same_library(unit.identity,root.identity))add(&root);
        for (const auto& import : unit.imports) if (!import.explicit_alias && !import.target.empty()) add(units.at(import.target).get());
        for (const auto& import : unit.imports) if (!import.explicit_alias && !import.target.empty()) {
            auto& owner = *units.at(import.target);
            if (!candidates.empty() && ((owner.declarations.contains(name) && source_visible(owner.declarations.at(name)->visibility,owner.identity,unit.identity)) ||
                (owner.constant_names.contains(name) && source_visible(owner.constant_names.at(name)->visibility,owner.identity,unit.identity))))
                error(unit.source,token,Code::ambiguous,"Ambiguous imported declaration '" + name + "'.","Use explicit aliases.");
        }
    }
    if (candidates.size() > 1) error(unit.source,token,Code::ambiguous,"Ambiguous imported type '" + name + "'.","Use explicit aliases.");
    return candidates.empty() ? std::pair<Unit*,Structure*>{nullptr,nullptr} : candidates.begin()->second;
}
// A qualified constructor wrapper can name its same-named nominal result in
// annotations. Keep this separate from expression lookup: calls must still run
// the wrapper (including its defaults), never construct the underlying record.
std::pair<Unit*, Structure*> annotation_structure_lookup(Unit& unit, Unit& root,
    const std::map<std::string,std::unique_ptr<Unit>>& units, std::string spelling, const Token& token) {
    auto* scope=&unit;
    std::set<const Function*> forwarding;
    while(true) {
        auto result=structure_lookup(*scope,root,units,spelling,token);
        if(result.second) {
            if(source_visible(result.second->visibility,result.first->identity,unit.identity))return result;
            return {};
        }
        const auto dot=spelling.find('.');
        if(dot==std::string::npos)return {};
        auto qualifier=scope->qualifiers.find(spelling.substr(0,dot));
        if(qualifier==scope->qualifiers.end() || qualifier->second.size()!=1)return {};
        auto* owner=qualifier->second.front();
        const auto name=spelling.substr(dot+1);
        auto found=owner->declarations.find(name);
        if(found==owner->declarations.end())return {};
        const auto& function=*found->second;
        const auto type=function.result;
        if(!source_visible(function.visibility,owner->identity,unit.identity) ||
           function.asynchronous || !function.generics.empty() || !function.returns.empty() ||
           type.nullable || type.readonly || !type.definition ||
           (type.kind!=Type::unresolved && type.kind!=Type::aggregate && type.kind!=Type::managed_ref))return {};
        const auto& name_in_result=type.definition->name;
        if(name_in_result.substr(name_in_result.find_last_of('.')+1)!=name)return {};
        if(!forwarding.insert(&function).second || forwarding.size()>128)return {};
        if(type.kind!=Type::unresolved) {
            auto target=units.find(type.definition->module_identity);
            if(target==units.end())return {};
            auto structure=target->second->structure_names.find(name);
            if(structure==target->second->structure_names.end() || structure->second->definition!=type.definition ||
               !source_visible(structure->second->visibility,target->first,unit.identity))return {};
            return {target->second.get(),structure->second};
        }
        scope=owner;spelling=name_in_result;
    }
}
VOID_SEMANTIC_HANDLER void editor_type_candidates(const Type& t,Unit& unit,Unit& root,const std::map<std::string,std::unique_ptr<Unit>>& units,const Token& token) {
    auto& probe=*root.editor;
    if(unit.failed_import){probe.result.candidates.clear();probe.result.signature.clear();probe.result.parameters.clear();throw EditorProbeFound{};}
    auto qualifier=t.definition->name.substr(0,t.definition->name.size()-std::string_view("_void_editor_cursor_").size());
    std::set<std::string> names;
    for(const auto& [id,u]:units)for(const auto& [name,structure]:u->structure_names)names.insert(name);
    for(const auto& name:names)try{
        auto [owner,structure]=annotation_structure_lookup(unit,root,units,qualifier+name,token);
        if(structure && name.starts_with(probe.prefix) && probe.result.candidates.size()<256)probe.result.candidates.push_back({name,structure->generics.empty()?"type":"generic type",7});
    }catch(const CompilationError&){}catch(const Poison&){}
    throw EditorProbeFound{};
}
Type resolve_type(Type t, Unit& unit, Unit& root, const std::map<std::string,std::unique_ptr<Unit>>& units, const Token& token) {
    if(t.nullable && !ir::reference_value(t) && !(t.definition && t.definition->callable)) {
        t.nullable=false;auto payload=resolve_type(t,unit,root,units,token);
        if(ir::nullable_type(payload))return payload;
        if(ir::reference_value(payload) || (payload.definition && payload.definition->callable)){payload.nullable=true;return payload;}
        const auto key=ir::nullable_identity(payload)+"|"+ir::capability_key(payload);
        if(auto found=unit.nullable_types.find(key);found!=unit.nullable_types.end())return found->second;
        if(!unit.make_nullable)throw std::logic_error("Nullable annotation was not resolved during validation");
        return unit.nullable_types.emplace(key,unit.make_nullable(payload)).first->second;
    }
    if(root.editor && unit.identity==root.editor->module && t.kind==Type::unresolved && t.definition->name.ends_with("_void_editor_cursor_") &&
        t.definition->source_offset+t.definition->name.size()-std::string_view("_void_editor_cursor_").size()==root.editor->offset)
        editor_type_candidates(t,unit,root,units,token);
    if(t.definition && t.definition->callable){auto result=unit.resolve_callable(t,token);result.nullable=t.nullable;result.readonly=t.readonly;return result;}
    if(t.definition && t.definition->result_type){auto result=unit.resolve_result(t,token);result.readonly=t.readonly;result.readonly_references=t.readonly_references;return result;}
    if(ir::collection_type(t)) return unit.resolve_array(t,token);
    if (t.kind != Type::unresolved) return t;
    for(auto& application:unit.applications)if(application.definition==t.definition) {
        auto result=application.resolved;
        if(result==Type::none)result=unit.resolve_application(t,token);
        result.nullable=t.nullable;result.readonly=t.readonly;
        if(result.nullable && !ir::reference_value(result))error(unit.source,token,Code::unsupported_type,"Only managed references can be nullable.");
        return result;
    }
    auto [owner, structure] = annotation_structure_lookup(unit,root,units,t.definition->name,token); (void)owner;
    auto location = token; location.offset = t.definition->source_offset; location.text = t.definition->name;
    if (!structure) error(unit.source,location,Code::unsupported_type,"Unsupported type '" + t.definition->name + "'.","Use a numeric type, bool, string or a visible value/managed struct.");
    if(!structure->generics.empty())error(unit.source,token,Code::unsupported_type,"Generic type '"+structure->token.text+"' requires concrete type arguments.");
    if (structure->layout_state == 3) throw Poison{};
    auto result=Type::nominal(structure->definition);
    result.nullable = t.nullable; result.readonly=t.readonly;
    if (result.nullable && !ir::reference_value(result)) error(unit.source,token,Code::unsupported_type,"Only managed references can be nullable in this phase.");
    return result;
}
std::string declaring_field_name(Type owner,unsigned index) {
    const auto& identity=owner.definition->fields.at(index).owner;
    for(auto definition=owner.definition;definition;definition=definition->base.lock())
        if(definition->identity==identity)return display_nominal_names(ir::type_name(Type::nominal(definition)));
    return display_nominal_names(identity);
}
void check_dictionary_key(const SourceFile& source,const Token& use,Type key,const std::map<std::string,std::unique_ptr<Unit>>& units) {
    if(auto owner=ir::dictionary_equality_owner(key)) {
        std::vector<RelatedLocation> related;
        if(auto found=units.find(owner->definition->module_identity);found!=units.end())related.push_back({span(found->second->source,Token{TokenKind::symbol,"==",owner->definition->custom_equality_offset}),"Custom equality declared here."});
        error(source,use,Code::unsupported_type,"Dictionary key contains a type defining custom operator ==.","Use an explicit scalar, string or eligible value key projection; custom equality has no authorized hash contract.",std::move(related));
    }
    if(!ir::dictionary_key_supported(key))error(source,use,Code::unsupported_type,"Unsupported dictionary key type.");
}
class Semantics {
    const SourceFile& source;
    Unit& unit;
    const std::map<std::string, std::unique_ptr<Unit>>& units;
    Unit& root;
    std::function<void(Unit&, Constant&)> evaluate;
    std::function<void(Unit&, Structure&)> defaults;
    Diagnostics& diagnostics;
    bool invalid_function = false;
    struct Variable { Type type; unsigned slot; bool readonly; Token declaration;Function* owner=nullptr; bool task_group=false; };
    std::function<Variable*(const std::string&,bool)> enclosing;
    std::vector<std::map<std::string, Variable>> scopes;
    Function* current = nullptr;
    Statement* editor_statement = nullptr;
    const Function* default_context = nullptr;
    bool inferring_default=false;
    unsigned loops = 0;
    bool suppress_zero = false,pattern_bindings=false,allow_event=false;
    bool visible_name(const std::string& name) const {
        if (unit.structure_names.contains(name) || unit.global_names.contains(name) ||
            unit.constant_names.contains(name) || unit.declarations.contains(name) ||
            unit.qualifiers.contains(name))
            return true;
        auto visible_in = [&](const auto& declarations, const Unit& owner) {
            const auto found = declarations.find(name);
            return found != declarations.end() &&
                source_visible(found->second->visibility, owner.identity, unit.identity);
        };
        // An implicit project root exposes types, globals and non-file-private
        // constants in the same library. Functions still need an import.
        if (same_library(unit.identity, root.identity)) {
            const auto constant = root.constant_names.find(name);
            if ((constant != root.constant_names.end() && !constant->second->file_private) ||
                visible_in(root.structure_names, root) || visible_in(root.global_names, root))
                return true;
        }
        for (const auto& import : unit.imports) if (!import.explicit_alias && !import.target.empty()) {
            const auto& owner = *units.at(import.target);
            if (visible_in(owner.structure_names, owner) || visible_in(owner.global_names, owner) ||
                visible_in(owner.constant_names, owner) || visible_in(owner.declarations, owner))
                return true;
        }
        return false;
    }
    Variable* lookup(const std::string& name,bool capture=true) {
        for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
            auto found = scope->find(name); if (found != scope->end()) return &found->second;
        }
        if(enclosing && current)if(auto* outer=enclosing(name,capture)) {
            if(!capture)return outer;
            const bool snapshot=(outer->owner->receiver && outer->slot==outer->owner->receiver->slot && outer->type.kind==Type::aggregate) ||
                (!outer->owner->operator_glyph.empty() && outer->readonly && std::any_of(outer->owner->parameters.begin(),outer->owner->parameters.end(),[&](const auto& p){return p.slot==outer->slot;}));
            Type cell;
            if(!snapshot && outer->owner->captured_locals.contains(outer->slot))cell=outer->owner->captured_locals.at(outer->slot);
            else {
                cell=unit.make_capture_record(outer->owner->symbol+"::$cell$"+std::to_string(outer->slot),outer->type,true);
                if(!snapshot)outer->owner->captured_locals.emplace(outer->slot,cell);
            }
            const auto slot=static_cast<unsigned>(current->locals.size());current->locals.push_back(outer->type);
            current->capture_inputs.emplace(slot,static_cast<unsigned>(current->captures.size()));current->captured_locals.emplace(slot,cell);
            current->captures.push_back({outer->slot,cell,outer->declaration,snapshot});
            auto [position,inserted]=scopes.front().emplace(name,Variable{outer->type,slot,outer->readonly,outer->declaration,current,outer->task_group});return &position->second;
        }
        return nullptr;
    }
    std::pair<Unit*, Constant*> constant(const Expr& e) {
        if (e.qualifier.empty() && unit.invalid_declarations.contains(e.token.text)) throw Poison{};
        std::map<std::string, std::pair<Unit*, Constant*>> candidates;
        auto add = [&](Unit* owner, bool own = false) {
            if (owner->invalid_declarations.contains(e.token.text)) throw Poison{};
            auto found = owner->constant_names.find(e.token.text);
            if (found != owner->constant_names.end()) {
                if (!own && !source_visible(found->second->visibility,owner->identity,unit.identity))
                    return;
                candidates.emplace(owner->identity, std::make_pair(owner, found->second));
            }
        };
        if (!e.qualifier.empty()) {
            auto q = unit.qualifiers.find(e.qualifier);
            if (unit.failed_qualifiers.contains(e.qualifier)) throw Poison{};
            if (q == unit.qualifiers.end()) error(source, e.qualifier_token, Code::visibility, "Unknown import qualifier '" + e.qualifier + "'.");
            if (q->second.size() != 1) error(source, e.qualifier_token, Code::ambiguous, "Ambiguous import qualifier '" + e.qualifier + "'.");
            add(q->second.front(), q->second.front() == &unit);
            if (candidates.empty() && q->second.front()->parse_failed) throw Poison{};
            if (candidates.empty()) error(source, e.token, Code::visibility, "Unknown or private imported constant '" + e.token.text + "'.");
        } else {
            add(&unit, true);
            if (!candidates.empty()) return candidates.begin()->second;
            if (unit.declarations.contains(e.token.text)) return {nullptr, nullptr};
            if(same_library(unit.identity,root.identity))add(&root, &root == &unit);
            for (const auto& import : unit.imports) if (!import.explicit_alias && !import.target.empty()) add(units.at(import.target).get());
            // A value and a function cannot silently compete for an imported binding.
            for (const auto& import : unit.imports) if (!import.explicit_alias && !import.target.empty()) {
                auto* owner = units.at(import.target).get();
                auto f = owner->declarations.find(e.token.text);
                if (!candidates.empty() && f != owner->declarations.end() && source_visible(f->second->visibility,owner->identity,unit.identity))
                    error(source, e.token, Code::ambiguous, "Ambiguous imported declaration '" + e.token.text + "'.", "Use explicit aliases.");
            }
        }
        if (candidates.size() > 1) error(source, e.token, Code::ambiguous, "Ambiguous imported constant '" + e.token.text + "'.", "Use explicit aliases.");
        return candidates.empty() ? std::pair<Unit*, Constant*>{nullptr, nullptr} : candidates.begin()->second;
    }
    std::pair<Unit*, Global*> global(const Expr& e) {
        std::map<std::string, std::pair<Unit*, Global*>> candidates;
        auto add = [&](Unit* owner) {
            if (owner->invalid_declarations.contains(e.token.text)) throw Poison{};
            auto found = owner->global_names.find(e.token.text);
            if (found != owner->global_names.end() && source_visible(found->second->visibility, owner->identity, unit.identity))
                candidates.emplace(owner->identity, std::make_pair(owner, found->second));
        };
        if (!e.qualifier.empty()) {
            auto found = unit.qualifiers.find(e.qualifier);
            if (found == unit.qualifiers.end()) return {};
            if (found->second.size() != 1) error(source, e.qualifier_token, Code::ambiguous, "Ambiguous import qualifier.");
            add(found->second.front());
        } else {
            add(&unit);
            if (!candidates.empty()) return candidates.begin()->second;
            if (unit.declarations.contains(e.token.text) || unit.constant_names.contains(e.token.text) || unit.structure_names.contains(e.token.text)) return {};
            if (e.kind == Expr::Kind::call && same_library(unit.identity, root.identity) && root.structure_names.contains(e.token.text) && source_visible(root.structure_names.at(e.token.text)->visibility, root.identity, unit.identity)) return {};
            if (same_library(unit.identity, root.identity)) add(&root);
            for (const auto& import : unit.imports) if (!import.explicit_alias && !import.target.empty()) add(units.at(import.target).get());
        }
        if (candidates.size() > 1) error(source, e.token, Code::ambiguous, "Ambiguous imported global '" + e.token.text + "'.", "Use explicit aliases.");
        if (candidates.empty()) return {};
        auto result = candidates.begin()->second;
        if (e.qualifier.empty() && same_library(unit.identity, root.identity) && root.constant_names.contains(e.token.text) && source_visible(root.constant_names.at(e.token.text)->visibility, root.identity, unit.identity))
            error(source, e.token, Code::ambiguous, "Ambiguous imported declaration '" + e.token.text + "'.", "Use explicit aliases.");
        if (e.qualifier.empty()) for (const auto& import : unit.imports) if (!import.explicit_alias && !import.target.empty()) {
            auto& owner = *units.at(import.target);
            if ((owner.constant_names.contains(e.token.text) && source_visible(owner.constant_names.at(e.token.text)->visibility, owner.identity, unit.identity)) ||
                (owner.declarations.contains(e.token.text) && source_visible(owner.declarations.at(e.token.text)->visibility, owner.identity, unit.identity)))
                error(source, e.token, Code::ambiguous, "Ambiguous imported declaration '" + e.token.text + "'.", "Use explicit aliases.");
        }
        return result;
    }
    const Function& resolve(const Expr& e) {
        if (e.qualifier.empty() && unit.invalid_declarations.contains(e.token.text)) throw Poison{};
        if (!e.qualifier.empty()) {
            auto module = unit.qualifiers.find(e.qualifier);
            if (unit.failed_qualifiers.contains(e.qualifier)) throw Poison{};
            if (module == unit.qualifiers.end()) error(source, e.qualifier_token, Code::visibility, "Unknown import qualifier '" + e.qualifier + "'.", "Import the module with this alias before calling it.");
            if (module->second.size() != 1) error(source, e.qualifier_token, Code::ambiguous, "Ambiguous import qualifier '" + e.qualifier + "'.", "Assign distinct explicit aliases with as to modules sharing a filename stem.");
            auto* target = module->second.front();
            if (target->invalid_declarations.contains(e.token.text)) throw Poison{};
            if (target->constant_names.contains(e.token.text)) error(source, e.token, Code::not_callable, "A const value is not callable.");
            auto found = target->declarations.find(e.token.text);
            if (found == target->declarations.end() && target->parse_failed) throw Poison{};
            if (found == target->declarations.end()) error(source, e.token, Code::unknown_function, "Unknown imported function '" + e.qualifier + "." + e.token.text + "'.");
            if (!source_visible(found->second->visibility,target->identity,unit.identity))
                error(source, e.token, Code::visibility, "Function '" + e.token.text + "' is private to " + target->identity + ".", "Call a public function from that file instead.");
            return *found->second;
        }
        if (unit.constant_names.contains(e.token.text)) error(source, e.token, Code::not_callable, "A const value is not callable.");
        auto own = unit.declarations.find(e.token.text);
        if (own != unit.declarations.end()) return *own->second;
        if (constant(e).second) error(source, e.token, Code::not_callable, "A const value is not callable.");
        std::map<std::string, Function*> candidates;
        bool hidden = false;
        for (const auto& import : unit.imports) if (!import.explicit_alias && !import.target.empty()) {
            if (units.at(import.target)->invalid_declarations.contains(e.token.text)) throw Poison{};
            auto found = units.at(import.target)->declarations.find(e.token.text);
            if (found != units.at(import.target)->declarations.end()) {
                if (!source_visible(found->second->visibility,import.target,unit.identity)) hidden = true;
                else candidates.emplace(found->second->symbol, found->second);
            }
        }
        if (candidates.size() > 1) {
            std::string owners;
            for (const auto& [id, f] : candidates) { (void)f; if (!owners.empty()) owners += ", "; owners += id.substr(0, id.rfind("::")); }
            error(source, e.token, Code::ambiguous, "Ambiguous imported function '" + e.token.text + "' from " + owners + ".", "Use a module qualifier or import as an explicit alias.");
        }
        if (!candidates.empty()) return *candidates.begin()->second;
        if (hidden) error(source, e.token, Code::visibility, "Function '" + e.token.text + "' is private to its declaring file.", "Call a public function instead.");
        if (unit.failed_import) throw Poison{};
        error(source, e.token, Code::unknown_function, "Unknown function '" + e.token.text + "'.", "Declare it here or import its declaring file; imports are not transitive exports.");
    }
#include "generic_inference.inc"
    std::string lexical_owner() const {
        if(!current)return {};
        return current->receiver?Type::key(current->receiver->type):current->static_owner?Type::key(current->static_owner->type):current->lexical_owner;
    }
    struct Member {
        std::vector<unsigned> path;
        const Function* function = nullptr;
    };
    VOID_SEMANTIC_HANDLER void const_field_error(Type owner,unsigned index,const Token& token) {
        auto* structure=owned_structure(owner).second;
        const auto& origin=structure->effective_fields.at(index);
        error(source,token,Code::readonly,"Cannot mutate a const stored field outside its direct declaring initializer.",
            "Prepare mutable child state before assigning the field in its owner's initializer.",
            {{span(origin.first->source,origin.second->token),"Const field declared here."}});
    }
    const Function* direct_method(Type receiver, const Token& token) {
        auto key = receiver.definition->identity + "::method::" + token.text;
        for (const auto& [id,owner] : units)
            if (auto found=owner->methods.find(key); found!=owner->methods.end()) return found->second;
        return nullptr;
    }
    bool accessible(const Function& f) {
        for (const auto& [id,owner]:units) if (owner->methods.contains(f.symbol))
            return source_visible(f.visibility,owner->identity,unit.identity) &&
                (owner.get()==&unit || (owner.get()==&root && same_library(unit.identity,root.identity)) || std::any_of(unit.imports.begin(),unit.imports.end(),[&](const auto& i){return i.target==id;}));
        return false;
    }
    std::optional<Member> direct_member(Type t,const Token& token) {
        auto base=t.definition->base.lock();
        for (unsigned n=base ? static_cast<unsigned>(base->fields.size()) : 0;n<t.definition->fields.size();++n)
            if (t.definition->fields[n].name==token.text) return Member{{n},nullptr};
        if (auto* f=direct_method(t,token)) return Member{{},f};
        return {};
    }
    Member member(Type t,const Token& token,bool calling = false) {
        if (t==error_type) throw Poison{};
        if (ir::nullable_type(t)) error(source,token,calling ? Code::receiver : Code::type_mismatch,"Unwrap a nullable reference before member access.","Use a checked as conversion to its non-nullable type.");
        if(t.kind==Type::interface_ref) {
            if(auto* requirement=direct_method(t,token)) {if(!accessible(*requirement)) error(source,token,Code::visibility,"Interface requirement is not accessible; import its original interface module.");return Member{{},requirement};}
            error(source,token,calling ? Code::unknown_method : Code::unknown_identifier,"Interface views expose only their declared method requirements.");
        }
        if (t.kind!=Type::aggregate && t.kind!=Type::managed_ref)
            error(source,token,calling ? Code::receiver : Code::type_mismatch,"Member access requires a struct.");
        auto selected=ir::select_member(*root.member_catalog,t,token.text,unit.identity,nullptr,lexical_owner());
        if(selected.status==ir::MemberSelection::inaccessible) {
            std::vector<RelatedLocation> related;auto owner=t;
            for(auto index:selected.path) {
                auto [module,structure]=owned_structure(owner);
                const auto& origin=structure->effective_fields.at(index);
                related.push_back({span(origin.first->source,origin.second->token),"Stored field declared by "+declaring_field_name(owner,index)+"."});
                owner=ir::field_type(owner,index);
            }
            error(source,token,Code::visibility,"Member '"+token.text+"' is not accessible from this lexical context.","Use an accessible owner operation; stored-field privacy belongs to its declaring struct.",std::move(related));
        }
        if(selected.status==ir::MemberSelection::ambiguous) {
            std::vector<RelatedLocation> related;std::string routes;
            auto [owner,structure]=owned_structure(t);
            for(const auto& [path,method]:selected.candidates) {
                auto [embedded_owner,embedded]=structure->effective_fields.at(path.front());
                auto route=embedded->name+"."+token.text;if(!routes.empty()) routes+=", ";routes+=route;
                related.push_back({span(embedded_owner->source,embedded->token),"Candidate through '"+route+"'."});
                auto child=ir::field_type(t,path.front());auto [child_owner,child_structure]=owned_structure(child);
                auto declaration=method.empty() ? child_structure->effective_fields.at(path.back()).second->token : direct_method(child,token)->token;
                related.push_back({span(child_owner->source,declaration),"Promoted member declared here."});
            }
            error(source,token,Code::ambiguous,"Ambiguous promoted member '"+token.text+"': "+routes+".","Qualify the intended embedded field explicitly; types and signatures do not select a component.",std::move(related));
        }
        if(selected.status==ir::MemberSelection::found) {
            if(selected.method.empty()){auto path_owner=t;for(auto index:selected.path){if(path_owner.definition->native_resource && !(console::session_type(path_owner) && unit.identity==console::source_module))error(source,token,Code::unsupported_type,"Native resource payload is binding-private.");path_owner=ir::field_type(path_owner,index);}}
            const Function* target=nullptr;
            if(!selected.method.empty()) for(const auto& [id,u]:units) for(const auto& f:u->functions) if(f.symbol==selected.method) target=&f;
            return {selected.path,target};
        }
        error(source,token,calling ? Code::unknown_method : Code::unknown_identifier,std::string(calling ? "Unknown method '" : "Unknown field '")+token.text+"' in "+ir::type_name(t)+".");
    }
    void embedded_receiver(std::unique_ptr<Expr>& base,unsigned index,const Token& written) {
        auto t=base->type;
        auto projected=std::make_unique<Expr>(Expr::Kind::field,written);
        projected->token.text=t.definition->fields[index].name;
        projected->slot=index; projected->type=ir::field_type(t,index);
        projected->target=t.definition->fields[index].owner+"::field::"+projected->token.text;
        projected->implicit=true; projected->parts.push_back(std::move(base)); base=std::move(projected);
    }
    bool module_call(const Expr& e) {
        const auto& base = *e.parts[0];
        if (base.kind != Expr::Kind::name || lookup(base.token.text) || global(base).second) return false;
        if (unit.qualifiers.contains(base.token.text) || unit.failed_qualifiers.contains(base.token.text)) return true;
        // Retain the established unknown-qualifier diagnostics for a.b().
        return !constant(base).second;
    }
    // Keep parent-only temporaries off recursively nested ordinary call frames.
    Type super_expression(Expr& e) {
        if (!current || !current->receiver || current->receiver->type.definition->base.expired())
            error(source,e.parts[0]->token,Code::receiver,"super requires a method on a derived managed struct.");
        if (e.token.text=="init") error(source,e.token,Code::initializer,"super.init(...) is permitted only as the first derived initializer prelude.");
        if(e.token.text=="deinit" && current->token.text!="deinit")error(source,e.token,Code::initializer_deferred,"super.deinit is only valid within deinit.");
        const Function* selected=nullptr;
        for (auto base=current->receiver->type.definition->base.lock();base && !selected;base=base->base.lock()) selected=direct_method(Type(base,Type::managed_ref),e.token);
        if (!selected) error(source,e.token,Code::unknown_method,"No real parent method named '"+e.token.text+"'; super does not promote using members.");
        if (!accessible(*selected)) error(source,e.token,Code::visibility,"Parent method is not accessible from this module.");
        e.parts[0]=std::make_unique<Expr>(Expr::Kind::name,current->receiver->token); expression(*e.parts[0]); e.parts[0]->implicit=true;
        e.target=selected->symbol; e.super_call=true; e.virtual_slot=selected->virtual_slot;
        e.receiver_addressable=true; e.slot=current->receiver->slot;
        auto original=std::move(e.parts[0]);
        e.parts[0]=std::make_unique<Expr>(Expr::Kind::conversion,original->token); e.parts[0]->op="upcast"; e.parts[0]->implicit=true; e.parts[0]->type=selected->receiver->type; e.parts[0]->parts.push_back(std::move(original));
        return e.type=call_arguments(e,*selected,1);
    }
    VOID_SEMANTIC_HANDLER Type primitive_base(const Expr& base) {
        if(base.kind!=Expr::Kind::name || lookup(base.token.text) || unit.qualifiers.contains(base.token.text))return Type::none;
        auto type=numeric::primitive(base.token.text);
        auto context=current?current:default_context;
        if(type==Type::none && context && context->substitutions.contains(base.token.text))type=context->substitutions.at(base.token.text);
        return numeric::number(type)?type:Type::none;
    }
    // Keep numeric-only Type and Function temporaries off recursive receiver
    // frames. Valid and rejected chains must still fit the existing 1 MiB gate.
    VOID_SEMANTIC_HANDLER bool primitive_base_selected(const Expr& base) {
        return base.kind==Expr::Kind::name && primitive_base(base)!=Type::none;
    }
    VOID_SEMANTIC_HANDLER std::optional<Type> primitive_anchor(const Expr& base) {return primitive_base(base);}
    VOID_SEMANTIC_HANDLER Type type_numeric_call(Expr& e) {return numeric_call(e,primitive_base(*e.parts[0]),true);}
    VOID_SEMANTIC_HANDLER Type numeric_call(Expr& e,Type type,bool type_level) {
        auto api=numeric::member(e.token.text);
        if(!api || !api->callable)error(source,e.token,type_level?Code::unknown_method:Code::receiver,"No numeric callable named '"+e.token.text+"'.");
        if(!e.type_arguments.empty())error(source,e.token,Code::argument_count,"Numeric prelude members do not take generic type arguments.");
        Function target;target.token=e.token;target.result=type;target.symbol=numeric::symbol(type,e.token.text);
        for(auto name:api->parameters)target.parameters.push_back({Token{TokenKind::name,std::string(name),e.token.offset},type});
        if(type_level)e.parts.erase(e.parts.begin());else {e.names.insert(e.names.begin(),Token{TokenKind::name,"",e.token.offset});e.op="$bound_numeric";}
        e.kind=Expr::Kind::call;e.target=target.symbol;
        auto result=call_arguments(e,target,0);
        return e.type=result;
    }
    VOID_SEMANTIC_HANDLER bool text_type_base(const Expr& base) {
        if(base.kind!=Expr::Kind::name || lookup(base.token.text) || unit.qualifiers.contains(base.token.text))return false;
        auto context=current?current:default_context;
        return base.token.text=="string" || (context && context->substitutions.contains(base.token.text) && context->substitutions.at(base.token.text)==Type::string);
    }
    Type list_type(Type element) {
        auto definition = std::make_shared<ir::ValueType>();
        definition->identity = definition->name = ir::array_identity(element, {});
        definition->element = ir::FieldType(element);
        return Type(definition, Type::growable_array);
    }
    Type byte_list() { return list_type(Type::u8); }
    void builtin_record(const Function& function,const std::string& documentation) {
        auto text=std::make_shared<const SourceFile>("<core prelude>",function.token.text);SourceSpan site{text,"$core",0,function.token.text.size()};
        AnalyzedDeclaration record{AnalyzedDeclaration::Kind::function,function.symbol,"$core","public",site,{},function.result,{}};
        record.builtin=true;record.documentation.description=documentation;
        for(const auto& p:function.parameters){record.parameters.push_back(p.type);record.written_parameters.push_back({p.token.text,p.type,site});}
        unit.builtins.insert_or_assign(function.symbol,std::move(record));
    }
#include "enum_api.inc"
#include "core_members.inc"
    // Signature construction owns a Function-sized temporary. Keep it out of
    // recursive method/field hint frames at the unchanged 1 MiB stack limit.
    VOID_SEMANTIC_HANDLER std::optional<Type> core_member_anchor(const Type& receiver,const Token& token,bool type_level=false,bool property=false) {
        const auto* api=core_member(receiver,token.text,type_level);
        if(!api || api->property!=property)return {};
        return core_signature(*api,receiver,token).result;
    }
    VOID_SEMANTIC_HANDLER Type text_type_call(Expr& e) {return core_call(e,Type::string,true);}
    VOID_SEMANTIC_HANDLER std::optional<Type> text_anchor() {return Type::string;}
    VOID_SEMANTIC_HANDLER std::optional<Type> text_result_anchor() {return unit.make_result(Type::string,Type::string);}
    VOID_SEMANTIC_HANDLER Type text_index_expression(Expr& e) {
        if(!current)error(source,e.token,Code::constant_call,"String indexing requires runtime evaluation.");
        if(!numeric::integer(expression(*e.parts[1])))error(source,e.parts[1]->token,Code::type_mismatch,"String index requires an integer, excluding bool and floats.");e.target="$text.index";return e.type=Type::string;
    }
    VOID_SEMANTIC_HANDLER Type core_call(Expr& e,const Type& receiver,bool type_level=false) {
        if(!current)error(source,e.token,Code::constant_call,"Text and collection APIs require runtime evaluation.");
        if(!e.type_arguments.empty())error(source,e.token,Code::argument_count,"Core members do not take explicit generic arguments.");
        const auto* api=core_member(receiver,e.token.text,type_level);
        if(!api || api->property)error(source,e.token,Code::unknown_method,"Unknown callable member '"+e.token.text+"' in "+ir::type_name(receiver)+".");
        auto target=core_signature(*api,receiver,e.token);
        const auto first=type_level?0u:1u;
        if(type_level)e.parts.erase(e.parts.begin());
        if(api->positional && std::any_of(e.names.begin(),e.names.end(),[](const auto& n){return !n.text.empty();}))
            error(source,e.token,Code::argument_count,"Incorrect positional arguments for collection operation.");
        if(!target.parameters.empty() && target.parameters.front().token.text=="index" && e.parts.size()>first && !editor_cursor(*e.parts[first])) {
            auto index=expression(*e.parts[first]);
            if(!numeric::integer(index))error(source,e.parts[first]->token,Code::type_mismatch,"Collection index requires an integer, excluding bool and floats.");
            target.parameters.front().type=index;
            if(e.token.text=="get")target.symbol=target.symbol.substr(0,target.symbol.rfind("::"))+"::"+ir::capability_key(index);
        }
        core_record(*api,target,receiver,type_level);
        e.target=target.symbol.substr(0,target.symbol.find("::"));e.source_target=target.symbol;
        return e.type=call_arguments(e,target,first);
    }
#include "static_members.inc"
    VOID_SEMANTIC_HANDLER Type method_expression(Expr& e) {
        if(!e.argument_indexes.empty())return e.type;
        if(e.source_target.starts_with("$text.") || e.source_target.starts_with("$safe."))return e.type;
        if(text_type_base(*e.parts[0]))return text_type_call(e);
        if(primitive_base_selected(*e.parts[0]))return type_numeric_call(e);
        if(auto variant=enum_expression(e,true))return *variant;
        bool super=e.parts[0]->kind==Expr::Kind::name && e.parts[0]->token.text=="super" && !lookup("super") && !unit.qualifiers.contains("super") && !unit.failed_qualifiers.contains("super");
        if (super) return super_expression(e);
        if ((e.token.text == "init") && !(e.parts[0]->kind==Expr::Kind::name && unit.qualifiers.contains(e.parts[0]->token.text)))
            error(source,e.token,Code::initializer_deferred,"Lifecycle methods cannot be called directly; construct a new value using its type.");
        if(auto result=static_expression(e,true))return *result;
        if (module_call(e)) {
            e.qualifier = e.parts[0]->token.text; e.qualifier_token = e.parts[0]->token;
            if(editor_cursor(e))editor_candidates(e);
            e.parts.erase(e.parts.begin()); e.kind = Expr::Kind::call;
            try {return call_expression(e);}
            catch(const CompilationError& failure) {return recover_or_report_member(e,failure,Code::visibility);}
        }
        auto event_access=allow_event;allow_event=e.token.text=="emit";auto receiver=expression(*e.parts[0]);allow_event=event_access;
        if(editor_cursor(e))editor_field_candidates(e,receiver);
        if(receiver.definition && receiver.definition->task_result && e.token.text=="cancel"){
            if(e.parts.size()!=1 || !e.type_arguments.empty() || !current)error(source,e.token,Code::unsupported_type,"Task cancellation requires a function activation and no arguments.");
            if(receiver.nullable)error(source,e.token,Code::type_mismatch,"Task cancellation requires a non-null handle.");
            e.kind=Expr::Kind::task_cancel;return e.type=Type::none;
        }
        try {return resolved_method_expression(e,receiver);}
        catch(const CompilationError& failure) {return recover_or_report_member(e,failure,Code::unknown_method);}
    }
    // Resolve the receiver before reserving mutation/member-check temporaries.
    // Deep receiver chains must reach the existing semantic nesting guard on
    // the supported 1 MiB native stack in Debug as well as optimized builds.
    #include "random.inc"
    VOID_SEMANTIC_HANDLER Type resolved_method_expression(Expr& e,Type receiver) {
        if(auto result=enum_value_api(e,receiver,true))return *result;
        if(numeric::number(receiver))return numeric_call(e,receiver,false);
        if(core_member(receiver,e.token.text))return core_call(e,receiver);
        if(e.parts[0]->event){
            bool owner=false;if(current && current->receiver)for(auto type=current->receiver->type.definition;type;type=type->base.lock())owner|=type->identity==e.parts[0]->event_owner;
            if(e.token.text!="emit" || !owner)error(source,e.token,Code::visibility,"Only methods of the event owner may emit it.");
            auto signature=ir::array_element(receiver);if(e.parts.size()!=signature.definition->call_parameters.size()+1 || std::any_of(e.names.begin(),e.names.end(),[](const auto& name){return !name.text.empty();}))error(source,e.token,Code::argument_count,"Incorrect event emission arguments.");
            for(unsigned n=1;n<e.parts.size();++n){auto parameter=Type(signature.definition->call_parameters[n-1]);expect(expression(*e.parts[n],parameter),parameter,e.parts[n]->token);}e.target="$event.emit";return e.type=Type::none;
        }
        if(receiver.definition && receiver.definition->callable && e.token.text=="call"){e.kind=Expr::Kind::function_call;return function_invocation(e);}
        if(receiver.kind==Type::aggregate)root.prepare_relationships(receiver,Type::none,source,e.token);
        if (e.token.text == "init")
            error(source,e.token,Code::initializer_deferred,"Lifecycle methods cannot be called directly; construct a new value using its type.");
        // A stored callable struct uses its ordinary call method. Keep the
        // field/property read in the receiver expression, preserving selection.
        if(!e.property && receiver.definition && !receiver.nullable && (receiver.kind==Type::aggregate || receiver.kind==Type::managed_ref)) {
            auto selected=ir::select_member(*root.member_catalog,receiver,e.token.text,unit.identity,nullptr,lexical_owner());
            bool callable_field=selected.status==ir::MemberSelection::found && selected.method.empty();
            if(!selected.method.empty())for(const auto& f:root.member_catalog->functions)
                if(f.name==selected.method && f.accessor==1 && (f.result.kind==Type::aggregate || f.result.kind==Type::managed_ref || f.result.kind==Type::interface_ref))callable_field=true;
            if(callable_field) {
                Depth depth(diagnostics.semantic_depth(),source,e.token);
                auto field=std::make_unique<Expr>(Expr::Kind::field,e.token);field->parts.push_back(std::move(e.parts[0]));
                auto type=expression(*field);
                if(type.kind!=Type::aggregate && type.kind!=Type::managed_ref && type.kind!=Type::interface_ref)
                    error(source,e.token,Code::not_callable,"Field '"+e.token.text+"' is not callable.");
                e.parts[0]=std::move(field);e.token.source_width=e.token.text.size();e.token.text="call";
                return resolved_method_expression(e,type);
            }
        }
        const auto& target = [&]() -> const Function& {
            auto check_arguments = [&] { for (std::size_t n=1; n<e.parts.size(); ++n) expression(*e.parts[n]); };
            try {
                if(auto* specialized=concrete_method(e,receiver))return *specialized;
                auto selected=member(receiver,e.token,true);
                if (!selected.function) error(source,e.token,Code::not_callable,"Field '"+e.token.text+"' is not callable.");
                for (auto index:selected.path) embedded_receiver(e.parts[0],index,e.token);
                receiver=e.parts[0]->type;
                return random_arity(e,*selected.function,1);
            }
            catch (const CompilationError&) { check_arguments(); throw; }
            catch (const Poison&) { check_arguments(); throw; }
        }();
        if(target.accessor && !e.property)error(source,e.token,Code::not_callable,"A property uses field access, without parentheses.");
        e.target = target.symbol; e.virtual_slot=target.virtual_slot; e.interface_call=target.interface_requirement;
        // Record the resolved place now, then check its permission after the
        // complete method graph has reached its receiver-effect fixed point.
        const Expr* base = e.parts[0].get(); std::vector<unsigned> path;
        while (base->kind == Expr::Kind::field) { path.push_back(base->slot); base = base->parts[0].get(); }
        if (base->kind == Expr::Kind::literal && !base->target.empty()) e.receiver_readonly = true;
        if (base->kind == Expr::Kind::global) { e.receiver_addressable = true; e.receiver_global = true; e.field_indexes.assign(path.rbegin(), path.rend()); }
        if (base->kind == Expr::Kind::name) {
            if (auto* v = lookup(base->token.text)) {
                e.receiver_addressable = true; e.receiver_readonly = v->readonly; e.slot = v->slot;
                e.field_indexes.assign(path.rbegin(),path.rend());
            }
        }
        if (ir::reference_value(receiver)) e.receiver_addressable=true;
        if(e.parts[0]->property && !e.parts[0]->setter.empty()) {
            e.receiver_addressable=e.parts[0]->receiver_addressable;
            e.receiver_readonly=e.parts[0]->receiver_readonly;e.slot=e.parts[0]->slot;
            e.field_indexes=e.parts[0]->field_indexes;
        }
        const Expr* selected=e.parts[0].get();bool indexed_receiver=false;
        while(selected->kind==Expr::Kind::field || selected->kind==Expr::Kind::array_index || selected->property) {
            indexed_receiver|=selected->kind==Expr::Kind::array_index;e.selected_receiver|=selected->property;
            selected=selected->parts[0].get();
        }
        // Indexed inline values still cannot expose a mutable receiver alias.
        // Copy-back would overwrite changes made through aliases in the callee.
        if(indexed_receiver && receiver.kind==Type::aggregate) {e.selected_receiver=false;e.receiver_addressable=false;}
        else if(e.selected_receiver && selected->kind==Expr::Kind::name)if(auto* v=lookup(selected->token.text)) {
            e.receiver_addressable=true;e.slot=v->slot;e.receiver_readonly=v->readonly;
        }
        const Expr* candidate=e.parts[0].get();
        while (candidate->kind==Expr::Kind::field) {
            const auto& field=candidate->parts[0]->type.definition->fields.at(candidate->slot);
            e.receiver_readonly|=field.readonly;
            if (candidate->parts[0]->type.kind==Type::managed_ref) e.receiver_addressable=true;
            candidate=candidate->parts[0].get();
        }
        if (receiver.kind==Type::managed_ref && Type::key(receiver)!=Type::key(target.receiver->type)) {
            auto original=std::move(e.parts[0]);
            e.parts[0]=std::make_unique<Expr>(Expr::Kind::conversion,original->token); e.parts[0]->op="upcast"; e.parts[0]->implicit=true;
            e.parts[0]->type=target.receiver->type; e.parts[0]->type.readonly=receiver.readonly; e.parts[0]->parts.push_back(std::move(original));
        }
        return e.type = call_arguments(e,target,1);
    }
    VOID_SEMANTIC_HANDLER void editor_call(const Expr& e,const Function& target,std::size_t first,const Documentation* selected_docs=nullptr) {
        std::optional<std::size_t> active;
        for(std::size_t n=first;n<e.parts.size() && !active;++n) {
            if(editor_cursor(e.names[n-first])) {active=n;break;}
            std::vector<const Expr*> work{e.parts[n].get()};
            while(!work.empty()) {
                auto* part=work.back();work.pop_back();
                if(part->kind==Expr::Kind::call || part->kind==Expr::Kind::method_call)continue;
                if(editor_cursor(*part)){active=n;break;}
                for(const auto& child:part->parts)work.push_back(child.get());
            }
        }
        if(!active)return;
        auto& out=diagnostics.editor->result;out.parameters.clear();out.signature=(target.static_owner?ir::type_name(target.static_owner->type)+".":std::string{})+random_public_name(target)+"(";
        out.constructor_signature=false;
        out.multiple_return_signature=!target.returns.empty();
        Documentation docs;
        if(selected_docs)docs=*selected_docs;
        else {
            for(const auto& [id,u]:units)for(const auto& f:u->functions)if(&f==&target || (id=="/@stdlib/random.void" && target.symbol.starts_with("/@stdlib/random.void::") && f.token.text==random_public_name(target) && bool(f.receiver)==bool(target.receiver)))docs=source_documentation(u->source,f.token.offset);
            if(auto builtin=unit.builtins.find(target.symbol);builtin!=unit.builtins.end())docs=builtin->second.documentation;
        }
        if(target.symbol.starts_with("$numeric::"))docs=numeric_documentation(*numeric::member(target.token.text));
        out.documentation=docs.text(target.result!=Type::none);
        out.parameter_documentation.clear();
        for(const auto& p:target.parameters) {
            out.parameter_documentation.push_back(docs.parameter(p.token.text));
            auto label=p.token.text+": "+ir::type_name(target.generics.empty()?p.type:editor_pattern_type(p.type));if(p.default_expression)label+=" = …";
            if(!out.parameters.empty())out.signature+=", ";out.signature+=label;out.parameters.push_back(label);
        }
        out.signature+=") -> "+editor_return_type(target);
        out.active_parameter=static_cast<unsigned>(*active-first);
        if(target.parameters.empty() &&
           e.parts.size()==first+1 && editor_cursor(*e.parts[first]) &&
           e.names.size()==1 && e.names.front().text.empty())
            throw EditorProbeFound{};
        if(*active-first<e.names.size() && !e.names[*active-first].text.empty()) {
            auto found=std::find_if(target.parameters.begin(),target.parameters.end(),[&](const auto& p){return p.token.text==e.names[*active-first].text;});
            if(found!=target.parameters.end())out.active_parameter=static_cast<unsigned>(found-target.parameters.begin());
        }
        if(target.symbol.starts_with("$numeric::")) {
            out.documentation=std::string(numeric::member(target.token.text)->documentation);
            if(e.op=="$bound_numeric" && !out.parameters.empty()) {
                out.parameters.erase(out.parameters.begin());out.parameter_documentation.erase(out.parameter_documentation.begin());out.active_parameter=out.active_parameter?out.active_parameter-1:0;
                out.signature=random_public_name(target)+"(";for(const auto& p:out.parameters){if(out.signature.back()!='(')out.signature+=", ";out.signature+=p;}out.signature+=") -> "+ir::type_name(target.result);
            }
        }
        const auto& label=e.names[*active-first];
        const bool editing_label=editor_cursor(label);
        const bool bare_cursor=label.text.empty() && e.parts[*active]->kind==Expr::Kind::name && editor_cursor(*e.parts[*active]);
        if((editing_label || bare_cursor) && !target.positional_only && !invalid_function && !unit.failed_import &&
           !(current && current->parse_failed) && (!selected_docs || !target.generics.empty()) &&
           !target.symbol.starts_with("$result::") && !target.symbol.starts_with("$nullable::")) {
            // Bind every other written argument through the ordinary mapper.
            // The cursor occupies its positional slot but supplies no value.
            const auto mapping=argument_mapping(e,target,first,false,*active);
            std::vector<bool> bound(target.parameters.size());
            for(auto index:mapping)bound[index]=true;
            for(std::size_t index=0;index<target.parameters.size();++index) {
                const auto& parameter=target.parameters[index];
                const auto& name=parameter.token.text;
                if(bound[index] || !valid_editor_identifier(name) || out.candidates.size()>=256)continue;
                out.candidates.push_back({name+" =",ir::type_name(target.generics.empty()?parameter.type:editor_pattern_type(parameter.type)),6,
                    editing_label?name:name+" = ",docs.parameter(name)});
            }
            const bool named_before=std::any_of(e.names.begin(),e.names.begin()+(*active-first),
                [](const auto& argument){return !argument.text.empty();});
            if(editing_label || named_before)throw EditorProbeFound{};
        }
    }
    std::vector<unsigned> argument_mapping(const Expr& e, const Function& target, std::size_t first, bool constructor,
                                         std::optional<std::size_t> cursor = {}) {
        std::vector<bool> bound(target.parameters.size());
        std::vector<unsigned> mapping;
        bool named = false;
        unsigned positional = 0;
        for (std::size_t n = first; n < e.parts.size(); ++n) {
            auto index = positional++;
            const auto& label = e.names[n-first];
            if(cursor==n) {named|=!label.text.empty();continue;}
            if (!label.text.empty()) {
                named = true;
                auto found = std::find_if(target.parameters.begin(),target.parameters.end(),[&](const auto& p) { return p.token.text == label.text; });
                if (found == target.parameters.end()) error(source,label,Code::argument_count,std::string(constructor ? "Unknown initializer parameter '" : "Unknown parameter '") + label.text + "'.");
                index = static_cast<unsigned>(found-target.parameters.begin());
            } else if (named) error(source,e.parts[n]->token,Code::argument_count,"Positional arguments must precede named arguments.");
            if (index >= bound.size()) error(source,e.token,Code::argument_count,"Incorrect argument count for '" + e.token.text + "'.");
            if (bound[index]) error(source,label.text.empty() ? e.parts[n]->token : label,Code::duplicate,constructor ? "Initializer parameter is supplied more than once." : "Parameter is supplied more than once.");
            bound[index] = true; mapping.push_back(index);
        }
        return mapping;
    }

    Type call_arguments(Expr& e, const Function& target, std::size_t first, bool constructor = false) {
        e.compile_target=target.compile_owner?const_cast<Function*>(&target):nullptr;
        if(target.compile_time)prepare_compile_call(root.compile_evaluation,*e.compile_target,source,e.token,false);
        if(!current && !constructor && target.compile_owner && !target.compile_time)error(source,e.token,Code::constant_call,"A compile-time constant cannot call a function unless it is explicitly marked compile func.",{},target.compile_owner?std::vector<RelatedLocation>{{span(target.compile_owner->source,target.token),"Runtime function declared here."}}:std::vector<RelatedLocation>{});
        e.asynchronous_call=target.asynchronous;e.native_symbol=target.native_symbol;
        if(diagnostics.editor && !constructor) {editor_call(e,target,first);if(first==1 && target.token.text=="format" && e.parts.size()==2)editor_format(*e.parts[1],e.parts[0]->type);}
        if(e.interface_call) {
            std::vector<const Expr*> pending;for(std::size_t n=first;n<e.parts.size();++n)pending.push_back(e.parts[n].get());
            while(!pending.empty()){auto* part=pending.back();pending.pop_back();if(part->kind==Expr::Kind::awaited)error(source,part->token,Code::unsupported_type,"await cannot retain a prepared interface receiver across suspension.","Await arguments into locals before selecting the interface method.");for(const auto& child:part->parts)pending.push_back(child.get());}
        }
        auto mapping=argument_mapping(e,target,first,constructor);
        std::vector<bool> bound(target.parameters.size());
        bool invalid=false;
        for(std::size_t n=first;n<e.parts.size();++n) {
            auto index=mapping[n-first];bound[index]=true;
            auto actual = e.op=="$bound_numeric" && n==0 ? e.parts[n]->type : expression(*e.parts[n],target.parameters[index].type);
            if((!target.native_symbol.empty() && native_binding(target.native_symbol).name=="register") && index==1 && e.parts[n]->kind!=Expr::Kind::function_reference)error(source,e.parts[n]->token,Code::unsupported_type,"Native callback registration requires a directly named function or a literal closure created in this generation.");
            const auto expected = target.parameters[index].type;
            if (target.c_primitive && expected.definition && expected.definition->callable && actual != error_type) {
                if (e.parts[n]->kind != Expr::Kind::function_reference || n + 1 != e.parts.size())
                    error(source,e.parts[n]->token,Code::unsupported_type,
                        "A checked C callback must be supplied last as a directly named function, bound method or literal closure from this generation.");
                e.c_callback_argument = index;
            }
            try { expect(actual,target.parameters[index].type,e.parts[n]->token); }
            catch (const CompilationError& failure) { diagnostics.add(failure); invalid = true; }
            invalid |= actual == error_type;
        }
        for (unsigned n=0;n<bound.size();++n) if (!bound[n]) {
            const auto& parameter = target.parameters[n];
            if (!parameter.default_value) {
                std::vector<RelatedLocation> related;
                if (constructor) { auto [owner,structure] = owned_structure(target.receiver->type); (void)structure; related.push_back({span(owner->source,parameter.token),"Required initializer parameter declared here."}); }
                error(source,e.token,Code::argument_count,"Incorrect argument count: missing required argument '" + parameter.token.text + "'.","Supply an explicit constructed value or every required argument.",std::move(related));
            }
            auto value = std::make_unique<Expr>(Expr::Kind::literal,e.token); value->type = parameter.type;
            value->aggregate_value = *parameter.default_value; value->value = parameter.default_value->bits;
            e.parts.push_back(std::move(value)); mapping.push_back(n);
        }
        if(constructor)e.field_indexes=std::move(mapping);else e.argument_indexes=std::move(mapping);
        return invalid ? error_type : target.asynchronous ? unit.make_task(target.result) : target.result;
    }
    std::pair<Unit*,Structure*> owned_structure(Type type) {
        for (const auto& [id,owner] : units) for (auto& structure : owner->structures)
            if (structure.definition == type.definition) return {owner.get(),&structure};
        throw std::logic_error("Missing owned value type");
    }
    void construction_fields(Expr& e, Unit&, Structure& structure, bool runtime, bool initializer_declaration = false) {
        Depth guard(diagnostics.semantic_depth(),source,e.token,128);
        for (unsigned n=0;n<structure.effective_fields.size();++n) {
            if (initializer_declaration && structure.base_type && n<structure.base_type->definition->fields.size()) continue;
            if (!e.initializer.empty() || std::find(e.field_indexes.begin(),e.field_indexes.end(),n)==e.field_indexes.end()) {
                const auto& field = *structure.effective_fields[n].second;
                auto& field_owner=*structure.effective_fields[n].first;
                if(field.expression){
                    if(initializer_declaration && checked_initializer_default(*field.annotation)){
                        auto value=std::make_unique<Expr>(Expr::Kind::literal,field.token);
                        value->type=*field.annotation;value->aggregate_value=structure.defaults.fields.at(n);
                        value->value=value->aggregate_value.bits;value->slot=n;
                        e.omitted.push_back(std::move(value));
                    }
                    continue;
                }
                if(console::endpoint_type(*field.annotation))continue; // Required token has no implicit constructor.
                if(ir::array_type(*field.annotation) && needs_array_recipe(*field.annotation)) {auto value=array_recipe(*field.annotation,initializer_declaration ? field.token : e.token,runtime,field_owner);value->slot=n;e.omitted.push_back(std::move(value));continue;}
                if(field.annotation->kind!=Type::aggregate || (field.annotation->definition && (field.annotation->definition->callable || !field.annotation->definition->cases.empty())))continue;
                auto [child_owner,child] = owned_structure(*field.annotation);
                auto value = std::make_unique<Expr>(Expr::Kind::construction,initializer_declaration ? field.token : e.token); value->slot = n;
                try { construction(*value,*child_owner,*child,runtime,field_owner);
                    if(value->constructed_type!=Type::none)error(source,value->token,Code::initializer,"A fallible child requires an explicitly handled constructor expression.","Supply a completed child, or propagate construction with ? before storing it.");
                }
                catch (CompilationError& failure) {
                    auto errors = failure.diagnostics();
                    for (auto& d : errors) d.related.push_back({span(field_owner.source,field.token),"Default construction required for this field."});
                    throw CompilationError(std::move(errors));
                }
                e.omitted.push_back(std::move(value));
            }
        }
    }
    VOID_SEMANTIC_HANDLER void editor_constructor(Expr& e, const Unit& owner, const Structure& structure, const Unit& access) {
        if(invalid_function || unit.failed_import || (current && current->parse_failed))return;
        const auto cursor_in_argument = [&](std::size_t n) {
            if(editor_cursor(e.names[n]))return true;
            std::vector<const Expr*> pending{e.parts[n].get()};
            while(!pending.empty()) {
                const auto* part=pending.back();pending.pop_back();
                // Leave an inner call to its own binder, even if it has no valid target.
                if(part->kind==Expr::Kind::call || part->kind==Expr::Kind::method_call)continue;
                if(editor_cursor(*part))return true;
                for(const auto& child:part->parts)pending.push_back(child.get());
            }
            return false;
        };
        std::size_t active=0;
        while(active<e.parts.size() && !cursor_in_argument(active))++active;
        if(active==e.parts.size())return;

        const auto* init=structure.initializer;
        std::vector<Parameter> fields;
        if(!init)for(unsigned n=0;n<structure.definition->fields.size();++n) {
            const auto& field=structure.definition->fields[n];
            if(!field.event && ir::field_accessible(*root.member_catalog,e.type,n,access.identity,lexical_owner()))
                fields.push_back({structure.effective_fields[n].second->token,field.type});
        }
        const auto& parameters=init?init->parameters:fields;
        const auto find_parameter=[&](const std::string& name) {
            return static_cast<std::size_t>(std::find_if(parameters.begin(),parameters.end(),[&](const auto& p){return p.token.text==name;})-parameters.begin());
        };
        const auto& label=e.names[active];
        const bool editing_label=editor_cursor(label);
        const bool bare_cursor=label.text.empty() && editor_cursor(*e.parts[active]);
        std::vector<bool> bound(parameters.size());
        bool named=false, positional_active=false;
        for(std::size_t n=0;n<e.parts.size();++n) {
            if(n==active) {
                positional_active=init && !named && label.text.empty();
                named|=!label.text.empty();
                continue;
            }
            const auto& argument=e.names[n].text;
            if(argument.empty() && (!init || named))return;
            named|=!argument.empty();
            const auto index=argument.empty()?n:find_parameter(argument);
            if(index>=bound.size() || bound[index])return;
            bound[index]=true;
        }
        auto index=label.text.empty()?active:find_parameter(label.text);
        if(editing_label || (bare_cursor && (!positional_active || index>=bound.size() || bound[index])))
            index=static_cast<std::size_t>(std::find(bound.begin(),bound.end(),false)-bound.begin());
        if(index>=bound.size() || bound[index] || (!positional_active && label.text.empty() && !bare_cursor))return;

        auto& out=diagnostics.editor->result;
        const auto docs=init?source_documentation(owner.source,init->token.offset):Documentation{};
        out.signature=e.qualifier.empty()?e.token.text:e.qualifier+"."+e.token.text;
        if(!structure.type_arguments.empty()) {
            out.signature+="<";
            for(std::size_t n=0;n<structure.type_arguments.size();++n) {
                if(n)out.signature+=", ";
                out.signature+=ir::type_name(structure.type_arguments[n]);
            }
            out.signature+=">";
        }
        out.signature+="(";
        out.constructor_signature=true;
        out.multiple_return_signature=false;
        out.documentation=docs.text();
        out.parameters.clear();
        out.parameter_documentation.clear();
        out.active_parameter=static_cast<unsigned>(index);
        for(std::size_t n=0;n<parameters.size();++n) {
            const auto& parameter=parameters[n];
            const auto& name=parameter.token.text;
            auto type=ir::type_name(parameter.type);
            auto documentation=docs.parameter(name);
            auto parameter_label=name+": "+type;
            if(parameter.default_expression)parameter_label+=" = …";
            if(n)out.signature+=", ";
            out.signature+=parameter_label;
            out.parameters.push_back(std::move(parameter_label));
            if((editing_label || bare_cursor) && !bound[n] && out.candidates.size()<256)
                out.candidates.push_back({name+" =",std::move(type),init?6:5,editing_label?name:name+" = ",documentation});
            out.parameter_documentation.push_back(std::move(documentation));
        }
        out.signature+=")";
        if(init && ir::fallible_initializer_result(init->result))
            out.signature+=" -> "+ir::type_name(unit.make_result(e.type,ir::field_type(init->result,2)));
        if(editing_label || (bare_cursor && !positional_active))throw EditorProbeFound{};
        if(bare_cursor)editor_name_candidates(*e.parts[active],parameters[index].type);
    }
    void construction(Expr& e, Unit& owner, Structure& structure, bool runtime, Unit& access) {
        if(console::endpoint_type(Type(structure.definition)) && unit.identity!=console::source_module)error(source,e.token,Code::unsupported_type,"Console endpoints are compiler-owned borrowed tokens; use console.stdin/stdout/stderr.");
        if(structure.definition->native_resource && !(console::session_type(Type(structure.definition)) && unit.identity==console::source_module))error(source,e.token,Code::unsupported_type,"Native resources are nonforgeable; use the checked binding factory.");
        if(structure.definition->interface_type) error(source,e.token,Code::initializer,"An interface cannot be constructed; convert a complete conforming managed value.");
        defaults(owner,structure);
        if (structure.definition->managed && !runtime) error(source,e.token,Code::constant_call,"Managed allocation is not a compile-time expression.");
        e.kind = Expr::Kind::construction; e.type = Type(structure.definition,structure.definition->interface_type ? Type::interface_ref : structure.definition->managed ? Type::managed_ref : Type::aggregate); e.target = structure.definition->identity;
        e.aggregate_value = structure.defaults;
        auto* init = structure.initializer;
        const bool fallible=init && ir::fallible_initializer_result(init->result);
        if (init) {
            if (!runtime) error(source,e.token,Code::constant_call,"Compile-time expressions cannot execute custom initialization.","Construct this value in a function or local const.",{{span(owner.source,init->token),"Runtime initializer declared here."}});
            if (!source_visible(init->visibility,owner.identity,access.identity)) error(source,e.token,Code::visibility,"Initializer is private to " + owner.identity + ".");
        }
        if(diagnostics.editor)editor_constructor(e,owner,structure,access);
        if(init) {
            e.initializer = init->symbol;
            if (call_arguments(e,*init,0,true) == error_type) e.type = error_type;
        } else {
            std::set<unsigned> initialized;
            bool invalid=false;
            for (unsigned n=0;n<e.parts.size();++n) {
                if (e.names[n].text.empty()) error(source,e.parts[n]->token,Code::argument_count,"Construction requires named fields.","Use field = expression for a type without init.");
                auto& fields = structure.definition->fields;
                auto field = std::find_if(fields.begin(),fields.end(),[&](const auto& f) { return f.name == e.names[n].text; });
                if(field!=fields.end() && field->event)error(source,e.names[n],Code::initializer,"Events always start empty; subscribe after construction.");
                if (field == fields.end()) error(source,e.names[n],Code::unknown_identifier,"Unknown constructor field '" + e.names[n].text + "'.");
                auto index = static_cast<unsigned>(field-fields.begin());
                if(!ir::field_accessible(*root.member_catalog,Type(structure.definition,structure.definition->managed?Type::managed_ref:Type::aggregate),index,access.identity,lexical_owner()))
                    error(source,e.names[n],Code::visibility,"Constructor field '"+field->name+"' is not accessible from this lexical owner.");
                if (!initialized.insert(index).second) error(source,e.names[n],Code::duplicate,"Duplicate initializer for field '" + field->name + "'.");
                e.field_indexes.push_back(index);
                auto actual=expression(*e.parts[n],field->type);
                invalid|=actual==error_type;
                expect(actual,field->type,e.parts[n]->token);
            }
            if (invalid) throw Poison{};
        }
        if (!structure.initializer) for (unsigned n=0;n<structure.effective_fields.size();++n) {
            const auto& field=*structure.effective_fields[n].second; auto& field_owner=*structure.effective_fields[n].first;
            Type t = *field.annotation;
            const bool complete_default = field.expression && t.definition &&
                (t.kind==Type::numeric_enum || t.definition->callable || !t.definition->cases.empty() || console::endpoint_type(t));
            if (ir::requires_initialization(t) && !complete_default && std::find(e.field_indexes.begin(),e.field_indexes.end(),n)==e.field_indexes.end()) {
                if(!ir::field_accessible(*root.member_catalog,Type(structure.definition,structure.definition->managed?Type::managed_ref:Type::aggregate),n,access.identity,lexical_owner()))
                    error(source,e.token,Code::visibility,"An inaccessible required field cannot be supplied by this caller: "+field.name+".","Provide an accessible owner initializer or factory.",{{span(field_owner.source,field.token),"Required field declared here."}});
                error(source,e.token,Code::argument_count,(t.kind==Type::numeric_enum?"A numeric enum field requires an explicit value: ":"A non-nullable reference field requires an explicit value: ") + field.name + ".",{},{{span(field_owner.source,field.token),"Required field declared here."}});
            }
        }
        if (!structure.initializer || !structure.definition->managed) construction_fields(e,owner,structure,runtime);
        if(fallible) {
            e.constructed_type=e.type;e.initializer_result=init->result;
            e.type=unit.make_result(e.constructed_type,ir::field_type(init->result,2));
        }
    }
    void expect(Type actual, Type expected, const Token& token) {
        if (actual == error_type || expected == error_type) return;
        if (actual.kind == Type::managed_ref && expected.kind == Type::managed_ref && ir::subtype(actual,expected) && (!actual.nullable || expected.nullable)) return;
        if(actual.kind==Type::interface_ref && expected.kind==Type::interface_ref && Type::key(actual)==Type::key(expected) && (!actual.nullable || expected.nullable)) return;
        if(actual.definition && expected.definition && actual.definition->callable && expected.definition->callable && Type::key(actual)==Type::key(expected) && (!actual.nullable || expected.nullable))return;
        if (actual != expected) error(source, token, Code::type_mismatch, "Type mismatch: expected " + std::string(ir::type_name(expected)) + ", received " + ir::type_name(actual) + ".", {}, {}, ir::type_name(expected), ir::type_name(actual));
    }
    void check_local_name(const Token& token, bool poison_previous) {
        // The approved random constructor and module seeder both spell `seed`.
        // Permit only this canonical initializer parameter to shadow that function.
        const bool random_seed_parameter = unit.identity == "/@stdlib/random.void" &&
            current && current->initializer && current->receiver &&
            Type::key(current->receiver->type) == "/@stdlib/random.void::generator" &&
            token.text == "seed" && !current->parameters.empty() &&
            token.offset == current->parameters[0].token.offset;
        auto* previous=lookup(token.text,poison_previous);
        if (previous || (!random_seed_parameter && visible_name(token.text))) {
            std::vector<RelatedLocation> related;
            if (previous) {
                related.push_back({span(source, previous->declaration), "Previously declared"});
                if(poison_previous)previous->type = error_type;
            }
            else {
                auto add = [&](const Unit& owner) {
                    if (auto c = owner.constant_names.find(token.text); c != owner.constant_names.end() && source_visible(c->second->visibility,owner.identity,unit.identity))
                        related.push_back({span(owner.source, c->second->declaration.token), "Previously declared"});
                    if (auto f = owner.declarations.find(token.text); f != owner.declarations.end() && source_visible(f->second->visibility,owner.identity,unit.identity))
                        related.push_back({span(owner.source, f->second->token), "Previously declared"});
                };
                add(unit); if (&root != &unit && same_library(unit.identity,root.identity)) add(root);
                for (const auto& import : unit.imports) if (!import.target.empty() && !import.explicit_alias) add(*units.at(import.target));
            }
            error(source, token, Code::duplicate, "Duplicate declaration or shadowing of '" + token.text + "'.", "Assign to the existing variable, or choose a distinct name.", std::move(related));
        }
    }
    unsigned declare(const Token& token, Type type, bool readonly = false) {
        check_local_name(token,true);
        auto slot = static_cast<unsigned>(current->locals.size()); current->locals.push_back(type);
        current->editor_bindings.push_back({token,slot,readonly});
        scopes.back().emplace(token.text, Variable{type, slot, readonly, token,current}); return slot;
    }
    // Qualified lookup needs a temporary AST node only on this cold path.
    // Keep it out of every recursive receiver-chain hint's native stack frame.
    VOID_SEMANTIC_HANDLER std::optional<Type> qualified_anchor(Expr& e,Expr::Kind kind) {
        Expr qualified(kind,e.token);qualified.qualifier=e.parts[0]->token.text;qualified.qualifier_token=e.parts[0]->token;
        if(kind==Expr::Kind::call) {
            auto [owner,structure]=structure_lookup(unit,root,units,qualified.qualifier+"."+e.token.text,qualified.qualifier_token);
            if(structure){auto& concrete=concrete_structure(e,*owner,*structure,1);return Type(concrete.definition,concrete.definition->managed?Type::managed_ref:Type::aggregate);}
            const auto& target=concrete_function(e,resolve(qualified),1);
            return call_result_anchor(target);
        }
        return anchor(qualified);
    }
    VOID_SEMANTIC_HANDLER std::optional<Type> operator_anchor(Expr& e) {
        using K=Expr::Kind;
            std::vector<std::optional<Type>> hints;for(auto& part:e.parts)hints.push_back(anchor(*part));
            std::vector<Type> operands;bool known=true;
            for(unsigned n=0;n<e.parts.size();++n){auto type=hints[n];auto& part=*e.parts[n];
                if(!type && part.token.text=="null" && hints.size()==2 && hints[1-n] && (ir::reference_value(*hints[1-n]) || (hints[1-n]->definition && hints[1-n]->definition->callable))){type=hints[1-n];type->nullable=true;}
                if(!type && part.kind==K::literal && !part.literal_text.empty()){auto opposite=hints.size()==2?hints[1-n]:std::optional<Type>{};type=opposite && numeric::number(*opposite)?*opposite:decimal(part)?Type::f32:Type::i32;}
                if(!type){known=false;break;}operands.push_back(*type);
            }
            if(known){auto selected=ir::select_operator(*root.member_catalog,e.op,operands,unit.identity,lexical_owner(),operator_generic_arguments());if(selected.status==ir::MemberSelection::found)for(const auto& f:root.member_catalog->functions)if(f.name==selected.method)return f.result;}
        // Builtin arithmetic uses the original child anchors. Do not walk the
        // same children again after failing to select a custom operator.
        for(const auto& hint:hints)if(hint)return hint;
        return {};
    }
    // Keep name/global resolution temporaries out of recursive hint frames so
    // the unchanged semantic-depth guard fits the supported 1 MiB stack.
    VOID_SEMANTIC_HANDLER std::optional<Type> name_anchor(Expr& e) {
        if (auto [owner, value] = global(e); value) {
            // Pure expressions cannot read runtime globals. In particular,
            // a type hint must not start runtime initializer analysis while
            // declaration layouts and constant defaults are being prepared.
            if (value->type == Type::none) {
                if (!current) {
                    return {};
                }
                owner->resolve_global(*value);
            }
            return value->type;
        }
        if (auto* local = e.qualifier.empty() ? lookup(e.token.text) : nullptr) {
            return local->type;
        }

        auto [owner, constant_value] = constant(e);
        if (constant_value) {
            evaluate(*owner, *constant_value);
            return constant_value->type;
        }
        return {};
    }

    template<class Target> VOID_SEMANTIC_HANDLER std::optional<Type> call_result_anchor(const Target& target) {
        if(ir::multiple_return_type(target.result))return {};
        if(target.asynchronous)return unit.make_task(target.result);
        return target.result;
    }
    // These projections are type-only. Keep their Type temporaries out of
    // every recursive receiver hint's frame, including in Debug builds.
    VOID_SEMANTIC_HANDLER std::optional<Type> projected_anchor(Expr& e) {
        auto operand=anchor(*e.parts[0]);
        if(!operand)return {};
        if(e.kind==Expr::Kind::spawned)return unit.make_task(*operand);
        if(e.kind==Expr::Kind::propagate)
            return operand->definition && operand->definition->result_type
                ? std::optional<Type>(ir::field_type(*operand,1)) : std::nullopt;
        if(!operand->definition || !operand->definition->task_result ||
           operand->kind!=Type::managed_ref || operand->nullable || operand->construction)return {};
        return Type(*operand->definition->task_result)==Type::none ? Type::none : ir::field_type(*operand,1);
    }
    VOID_SEMANTIC_HANDLER std::optional<Type> choice_anchor(Expr& e) {
        // The condition may introduce pattern bindings. Ordinary checking owns
        // those bindings; a hint only considers the value branches.
        if(auto branch=anchor(*e.parts[1]))return branch;
        return anchor(*e.parts[2]);
    }

    // Separate handlers keep the recursive dispatcher small at the supported
    // 1 MiB stack size without changing its shared semantic-depth budget.
    VOID_SEMANTIC_HANDLER std::optional<Type> indexed_anchor(Expr& e) {
        using K=Expr::Kind;
        auto owner=anchor(*e.parts[0]);
        if(owner && owner->kind==Type::string && e.kind==K::array_index)return text_anchor();
        if(owner && owner->kind==Type::dictionary && e.kind==K::array_index)return ir::dictionary_value(*owner);
        if(owner && ir::array_type(*owner)) return e.kind==K::array_slice?slice_type(*owner):ir::array_element(*owner);
        return {};
    }
    VOID_SEMANTIC_HANDLER std::optional<Type> method_anchor(Expr& e) {
        if(auto result=static_expression(e,true,true))return result;
        using K=Expr::Kind;
        if(e.source_target.starts_with("$text.") || e.source_target.starts_with("$safe."))return e.type;
        if(text_type_base(*e.parts[0]))return core_member_anchor(Type::string,e.token,true);
        if(primitive_base_selected(*e.parts[0]))return primitive_anchor(*e.parts[0]);
        if (module_call(e)) {
            return qualified_anchor(e,K::call);
        }
        auto receiver = anchor(*e.parts[0]);
        if (!receiver) return std::nullopt;
        if(auto result=core_member_anchor(*receiver,e.token))return result;
        if(numeric::number(*receiver) && numeric::member(e.token.text))return *receiver;
        if(auto* specialized=concrete_method(e,*receiver))return call_result_anchor(*specialized);
        auto selected=member(*receiver,e.token,true);
        if(selected.function)return call_result_anchor(*selected.function);
        return {};
    }
    Type constructor_result(Type type,Structure& structure) {
        return structure.initializer && ir::fallible_initializer_result(structure.initializer->result)
            ? unit.make_result(type,ir::field_type(structure.initializer->result,2)) : type;
    }
    VOID_SEMANTIC_HANDLER std::optional<Type> call_anchor(Expr& e) {
        if(e.token.text=="$interpolation" || e.token.text=="$hole")return text_anchor();
        if(e.target.starts_with("$display.") || display_prelude(e))return e.token.text=="string"?text_anchor():text_result_anchor();
        if(e.op=="$static_call" || e.target.starts_with("$numeric::") || !e.argument_indexes.empty())return e.type;
        if(auto* value=lookup(e.token.text);value && value->type.definition && value->type.definition->callable)return Type(*value->type.definition->call_result);
        if(current && e.qualifier.empty() && !lookup(e.token.text) && current->substitutions.contains(e.token.text)) {
            auto type=current->substitutions.at(e.token.text);
            if(type.definition && (type.kind==Type::aggregate || type.kind==Type::managed_ref) && !type.nullable && type.definition->cases.empty() && !type.definition->callable) {
                auto [owner,structure]=owned_structure(type);return constructor_result(type,*structure);
            }
            return type;
        }
        auto [owner, structure] = structure_lookup(unit,root,units,e.qualifier.empty() ? e.token.text : e.qualifier + "." + e.token.text,e.qualifier.empty() ? e.token : e.qualifier_token);
        if (structure) {auto& concrete=concrete_structure(e,*owner,*structure);return constructor_result(Type(concrete.definition,concrete.definition->interface_type?Type::interface_ref:concrete.definition->managed?Type::managed_ref:Type::aggregate),concrete);}
        const auto& target=concrete_function(e,resolve(e));
        return call_result_anchor(target);
    }
    VOID_SEMANTIC_HANDLER std::optional<Type> field_anchor(Expr& e) {
        if(auto result=static_expression(e,false,true))return result;
        using K=Expr::Kind;
        if (auto [owner, declaration] = enum_declaration(e); declaration) {
            return Type::nominal(declaration->definition);
        }
        if(primitive_base_selected(*e.parts[0]))return primitive_anchor(*e.parts[0]);
        const auto& operand = *e.parts[0];
        if (operand.kind == K::name && !lookup(operand.token.text) && unit.qualifiers.contains(operand.token.text)) {
            return qualified_anchor(e,K::name);
        }
        auto base = anchor(*e.parts[0]);
        if(base && enum_value_type(*base) && !direct_method(*base,e.token)) {
            if(e.token.text=="name")return Type::string;
            if(e.token.text=="value" && base->kind==Type::numeric_enum)return Type(*base->definition->enum_backing);
        }
        if(base)if(auto result=core_member_anchor(*base,e.token,false,true))return result;
        if (base && (base->kind == Type::aggregate || base->kind == Type::managed_ref)) {
            auto selected=member(*base,e.token);
            if (!selected.function) { auto t=*base; for (auto index:selected.path) t=ir::field_type(t,index); return t; }
            if(selected.function->accessor==1)return selected.function->result;
        }
        return {};
    }

    // Hints resolve typed anchors without prematurely defaulting untyped literals.
    std::optional<Type> anchor(Expr& e) {
        try {
        // Hints share the semantic budget; an exhausted hint falls back to
        // ordinary checking, which reports the existing nesting diagnostic.
        Depth guard(diagnostics.semantic_depth(),source,e.token,256);
        using K = Expr::Kind;
        if (e.kind == K::literal) {
            if (e.token.text == "null") return {};
            if (e.literal_text.empty()) return e.type;
            if (e.literal_text.back() == 'f') return Type::f32;
            if (e.literal_text.back() == 'd') return Type::f64;
            return {};
        }
        if (e.kind == K::conversion) {
            if(e.token.text!="as?" || !e.type_arguments.empty())return resolve_type(e.type,unit,root,units,e.token);
            return fallible_anchor(e);
        }
        if (e.kind == K::global) return e.type;
        if (e.kind==K::spawned || e.kind==K::propagate || e.kind==K::awaited)return projected_anchor(e);
        if (e.kind==K::choice)return choice_anchor(e);
        if (e.kind == K::name) {
            return name_anchor(e);
        }
        if(e.kind==K::array_index || e.kind==K::array_slice)return indexed_anchor(e);
        if(e.kind==K::array_literal || e.kind==K::dictionary_literal) return {};
        if(e.kind == K::method_call)return method_anchor(e);
        if(e.kind == K::call)return call_anchor(e);
        if(e.kind == K::field)return field_anchor(e);
        if((e.kind==K::binary || e.kind==K::unary) && (e.op=="+" || e.op=="-" || e.op=="*" || e.op=="/" || e.op=="%"))return operator_anchor(e);
        if (e.op == "not" || e.op == "and" || e.op == "or" || e.op == "==" || e.op == "!=" || e.op == "<" || e.op == "<=" || e.op == ">" || e.op == ">=") return Type::boolean;
        if(e.kind==K::binary && (e.op=="<<" || e.op==">>"))return anchor(*e.parts[0]);
        for (auto& part : e.parts) if (auto t = anchor(*part)) return t;
        return {};
        } catch (const CompilationError&) { return {}; }
          catch (const Poison&) { return {}; }
          catch (const std::invalid_argument&) { return {}; }
    }
    bool decimal(const Expr& e) {
        if (e.kind == Expr::Kind::literal && e.literal_text.find('.') != std::string::npos) return true;
        for (auto& part : e.parts) if (decimal(*part)) return true;
        return false;
    }
    // Apply contextual rewrites only after the recursive handler has returned.
    Type interface_conversion(Expr& e,Type from,Type target) {
        if(from.nullable && !target.nullable) error(source,e.token,Code::type_mismatch,"Unwrap the concrete nullable reference before converting to a non-nullable interface.");
        auto concrete=from;concrete.nullable=false;
        ir::Module::Binding binding;
        try { binding=ir::structural_binding(*root.member_catalog,concrete,target,unit.identity); }
        catch(const std::invalid_argument& failure) {error(source,e.token,Code::type_mismatch,failure.what());}
        catch(const std::length_error& failure) {error(source,e.token,Code::limit,failure.what());}
        if(!current && !(from.nullable && target.nullable && constant_value(source,e,root.compile_evaluation).bits==0)) error(source,e.token,Code::constant_call,"Interface bindings are runtime views; only nullable all-null templates are compile-time values.");
        auto& bindings=root.member_catalog->interface_bindings;
        if(std::none_of(bindings.begin(),bindings.end(),[&](const auto& b){return b.key==binding.key;})) bindings.push_back(binding);
        auto operand=std::make_unique<Expr>(std::move(e));
        e=Expr(Expr::Kind::conversion,operand->token);e.op="interface";e.target=binding.key;e.implicit=true;
        e.type=target;e.type.readonly=from.readonly || ir::binding_requires_readonly(binding);e.parts.push_back(std::move(operand));return e.type;
    }
    VOID_SEMANTIC_HANDLER Type contextual_conversion(Expr& e, Type type, const std::optional<Type>& expected) {
        if(expected && ir::nullable_value(*expected) && type!=error_type && !ir::nullable_value(type)) {
            auto payload=ir::nullable_payload(*expected);
            type=contextual_conversion(e,type,payload);expect(type,payload,e.token);
            auto operand=std::make_unique<Expr>(std::move(e));
            e=Expr(Expr::Kind::conversion,operand->token);e.op="$nullable_lift";e.implicit=true;
            e.type=ir::with_field_type(*expected,1,type);e.parts.push_back(std::move(operand));return e.type;
        }
        if(expected && expected->kind==Type::interface_ref && type!=error_type && type.kind!=Type::interface_ref) return interface_conversion(e,type,*expected);
        if(expected && type!=error_type && expected->definition && expected->definition->callable && type.definition && !type.definition->callable && !type.nullable && (type.kind==Type::aggregate || type.kind==Type::managed_ref || type.kind==Type::interface_ref)){
            return bind_method(e,Token{TokenKind::name,"call",e.token.offset},*expected);
        }
        if (expected && type!=error_type && ir::subtype(type,*expected) && (!type.nullable || expected->nullable) && Type::key(type)!=Type::key(*expected)) {
            auto operand=std::make_unique<Expr>(std::move(e));
            e=Expr(Expr::Kind::conversion,operand->token); e.op="upcast"; e.implicit=true;
            e.type=*expected; e.type.readonly=type.readonly; e.parts.push_back(std::move(operand)); type=e.type;
        }
        return type;
    }
    VOID_SEMANTIC_HANDLER Type nullable_expression(Expr& e,Type expected) {
        const auto known=anchor(e);
        const bool nullable_context=e.token.text=="null" || e.kind==Expr::Kind::choice || e.kind==Expr::Kind::match || (known && ir::nullable_type(*known));
        return expression_impl(e,nullable_context?expected:ir::nullable_payload(expected));
    }
    Type expression(Expr& e, std::optional<Type> expected = {}, bool destructuring = false) {
        struct RestoreBindings {
            bool& flag;
            bool saved;
            ~RestoreBindings() { flag=saved; }
        } restore{pattern_bindings,pattern_bindings};
        try {
            Depth guard(diagnostics.semantic_depth(), source, e.token, 256);
            auto outer_bindings=pattern_bindings;if(e.kind!=Expr::Kind::test)pattern_bindings=false;
            auto type = expected && ir::nullable_value(*expected)?nullable_expression(e,*expected):expression_impl(e,expected);pattern_bindings=outer_bindings;
            if(ir::multiple_return_type(type) && (!destructuring ||
               (e.kind!=Expr::Kind::call && e.kind!=Expr::Kind::method_call)))
                error(source,e.token,Code::invalid_value,"A multiple-return call requires a destructuring declaration.");
            type = contextual_conversion(e,type,expected);
            if (type == error_type) { e.type = error_type; invalid_function = true; }
            return type;
        } catch (const CompilationError& failure) { diagnostics.add(failure); }
          catch (const Poison&) {}
        invalid_function = true;
        return e.type = error_type;
    }
    VOID_SEMANTIC_HANDLER Type spawned_expression(Expr& e) {
        if(!current || !current->asynchronous || current->deferred)error(source,e.token,Code::unsupported_type,"spawn requires an async body outside deferred cleanup.");
        auto result=expression(*e.parts[0]);auto& call=*e.parts[0];
        if(call.kind!=Expr::Kind::call || call.asynchronous_call || call.target.empty() || call.target.starts_with("$"))error(source,e.token,Code::unsupported_type,"spawn requires a directly named synchronous noncapturing function.");
        std::vector<Type> transfer{result};for(const auto& part:call.parts)transfer.push_back(part->type);
        while(!transfer.empty()){auto type=transfer.back();transfer.pop_back();if(!ir::worker_transfer_safe(type))error(source,e.token,Code::unsupported_type,"spawn transfers only scalars, immutable strings and recursively safe fixed values.");
            if(type.kind==Type::aggregate){Token deinit=e.token;deinit.text="deinit";if(direct_method(type,deinit))error(source,e.token,Code::unsupported_type,"Native/resource wrappers cannot cross a worker boundary.");for(const auto& field:type.definition->fields)transfer.push_back(field.type);}
            else if(type.kind==Type::fixed_array)transfer.push_back(ir::array_element(type));
        }
        return e.type=unit.make_task(result);
    }
    VOID_SEMANTIC_HANDLER Type awaited_expression(Expr& e) {
        if(!current || !current->asynchronous || current->deferred)error(source,e.token,Code::unsupported_type,"await requires an async function and is not allowed in deferred code.");
        auto task=expression(*e.parts[0]);if(!task.definition || !task.definition->task_result || task.kind!=Type::managed_ref || task.nullable || task.construction)error(source,e.token,Code::type_mismatch,"await requires a non-null inferred task handle.");
        return e.type=Type(*task.definition->task_result)==Type::none?Type::none:ir::field_type(task,1);
    }
    // Keep the recursive dispatch frame small: reserving every handler's
    // temporaries here exhausts a 1 MiB Debug stack before the depth guard.
    bool editor_cursor(const Token& token) const {
        return diagnostics.editor && diagnostics.editor->module==unit.identity &&
            diagnostics.editor->offset==token.offset && token.text=="_void_editor_cursor_";
    }
    bool editor_cursor(const Expr& e) const {return editor_cursor(e.token);}
    bool editor_mutable_storage(const Expr& expression) {
        const Expr* selected=&expression;
        while(selected->kind==Expr::Kind::field || (selected->kind==Expr::Kind::method_call && selected->property)) {
            if(selected->property) {if(selected->setter.empty())return false;}
            else if(selected->parts[0]->type.definition->fields.at(selected->slot).readonly)return false;
            if(selected->parts[0]->type.kind==Type::managed_ref)return !ir::readonly_paths(selected->parts[0]->type).contains({});
            selected=selected->parts[0].get();
        }
        if(selected->kind==Expr::Kind::name) {
            if(auto* variable=lookup(selected->token.text))return !variable->readonly;
            return true; // Finalized probes use the captured storage permission.
        }
        return selected->kind==Expr::Kind::global;
    }
    bool editor_method_available(const Function& function,Type receiver,const std::vector<unsigned>& path,const Expr& expression) {
        const bool storage=receiver.kind==Type::managed_ref || (editor_mutable_storage(expression) &&
            (!diagnostics.editor->finalizing || diagnostics.editor->receiver_addressable));
        bool readonly=receiver.readonly;
        for(auto field:path){readonly|=receiver.definition->fields[field].readonly;receiver=ir::field_type(receiver,field);}
        const bool restricted=readonly || !ir::readonly_paths(receiver).empty();
        if(!diagnostics.editor->finalizing)return !restricted;
        if(function.receiver_mutates && (readonly || (receiver.kind==Type::aggregate && !storage)))return false;
        const auto& effects=function.virtual_slot.empty()?function.reference_effects:function.virtual_effects;
        const auto paths=ir::readonly_paths(receiver);
        for(const auto* requirements:{&effects.mutations,&effects.retained,&effects.required_mutable})
            for(const auto& origin:*requirements)if(origin.parameter==0 && paths.contains(origin.path))return false;
        return true;
    }
    VOID_SEMANTIC_HANDLER void editor_candidates(Expr& e, std::optional<Type> receiver = {}) {
        observation::Scope timing(observation::EditorDetail::candidates);
        auto& out=diagnostics.editor->result;
        // A broken local context or import must never manufacture authority.
        if (invalid_function || unit.failed_import || (current && current->parse_failed)) {out.candidates.clear();out.signature.clear();out.parameters.clear();throw EditorProbeFound{};}
        std::map<std::string,EditorCandidate> candidates;
        auto add=[&](std::string name,Type type,int kind=6) -> EditorCandidate* {
            if((!valid_editor_identifier(name) && !(receiver && name=="string")) || type==error_type)return nullptr;
            return &candidates.emplace(name,EditorCandidate{name,ir::type_name(type),kind}).first->second;
        };
        if(receiver) {
            auto t=*receiver;
            if(t.kind==Type::string || ir::collection_type(t)) {
                const bool type_level=text_type_base(*e.parts[0]);
                for(const auto& api:core_members)if(core_member(t,api.name,type_level) &&
                    (!api.mutates || (!t.readonly && !ir::readonly_paths(t).contains({})))) {
                    auto token=e.token;token.text=api.name;auto target=core_signature(api,t,token);
                    core_record(api,target,t,type_level);
                    if(auto* candidate=add(api.name,target.result,api.property?10:2)) {
                        candidate->detail=api.property?ir::type_name(target.result):editor_signature(target);
                        candidate->documentation=unit.builtins.at(target.symbol).documentation.text(target.result!=Type::none);
                    }
                }
                for(auto& [name,candidate]:candidates)out.candidates.push_back(std::move(candidate));throw EditorProbeFound{};
            }
            if(numeric::number(t)) {
                const bool type_level=primitive_base(*e.parts[0])!=Type::none;
                for(const auto& api:numeric::members())if(type_level || api.callable) {
                    std::string name(api.name);if(auto* candidate=add(name,t,api.callable?2:21))candidate->documentation=std::string(api.documentation);
                }
                for(auto& [name,candidate]:candidates)out.candidates.push_back(std::move(candidate));
                throw EditorProbeFound{};
            }
            if(ir::nullable_type(t) || !t.definition || (t.definition->native_resource && !console::session_type(t)) || t.definition->callable || !t.definition->cases.empty()) throw EditorProbeFound{};
            std::set<std::string> names;
            for(const auto& [id,u]:units) {
                for(const auto& structure:u->structures) for(const auto& f:structure.definition->fields) names.insert(f.name);
                for(const auto& f:u->functions) if(f.receiver && !f.initializer && f.accessor!=2 && f.token.text!="deinit") names.insert(f.token.text);
                for (const auto& function : u->generic_functions) {
                    if (function.receiver && !function.generics.empty()) {
                        names.insert(function.token.text);
                    }
                }
            }
            for(const auto& name:names) try {
                if(!name.starts_with(diagnostics.editor->prefix))continue;
                auto token = e.token;
                token.text = name;
                auto [owner, generic] = generic_method_declaration(t, token);

                if (generic) {
                    // Inspect the original header without instantiating a body
                    // or granting mutation through an unchecked readonly view.
                    if (t.readonly || !ir::readonly_paths(t).empty()) {
                        continue;
                    }

                    auto result = generic->result;

                    if (ir::collection_type(result)) {
                        result.kind = Type::unresolved;
                    }

                    if (auto* candidate = add(name, result, 2)) {
                        candidate->detail=editor_signature(*generic);
                        candidate->documentation = source_documentation(
                            owner->source, generic->token.offset).text(generic->result != Type::none);
                    }

                    continue;
                }

                auto selected = member(t, token);
                if(selected.function) {
                    if(!editor_method_available(*selected.function,t,selected.path,*e.parts[0]))continue;
                    if(auto* candidate=add(name,selected.function->result,selected.function->accessor==1?10:2)) {
                        candidate->detail=editor_signature(*selected.function);
                        for(const auto& [id,u]:units)for(const auto& f:u->functions)if(&f==selected.function)candidate->documentation=source_documentation(u->source,f.token.offset).text(f.result!=Type::none);
                    }
                } else {
                    if(t.definition->native_resource)continue; // Source methods remain visible; native payloads do not.
                    auto type=t;bool readonly=false;std::string declaring;
                    Documentation docs;
                    for(auto field:selected.path){readonly|=type.definition->fields[field].readonly;declaring=declaring_field_name(type,field);
                        auto [owner,structure]=owned_structure(type);const auto& origin=structure->effective_fields.at(field);
                        docs=source_documentation(origin.first->source,origin.second->token.offset);type=ir::field_type(type,field);}
                    if(auto* candidate=add(name,type,5)){if(readonly)candidate->detail="const "+candidate->detail;candidate->documentation=docs.text();
                        if(candidate->documentation.empty())candidate->documentation=display_nominal_names("Stored field declared by "+declaring+".");}
                }
            } catch(const CompilationError&) {} catch(const Poison&) {}
        } else {
            if(e.qualifier.empty()) for(const auto& scope:scopes) for(const auto& [name,v]:scope) if(!v.task_group) add(name,v.type);
            std::set<std::string> names;
            auto collect=[&](const Unit& u) {
                for(const auto& [name,f]:u.declarations) names.insert(name);
                for(const auto& [name,c]:u.constant_names) names.insert(name);
                for(const auto& [name,value]:u.global_names) names.insert(name);
                for(const auto& [name,structure]:u.structure_names) names.insert(name);
            };
            if(e.qualifier.empty()) {
                for(const auto& [id,u]:units)collect(*u);
            } else if(auto found=unit.qualifiers.find(e.qualifier);
                      found!=unit.qualifiers.end() && found->second.size()==1) {
                collect(*found->second.front());
            }
            for(const auto& name:names) {
                if(!name.starts_with(diagnostics.editor->prefix) || !valid_editor_identifier(name) || candidates.contains(name)) continue;
                Expr test(Expr::Kind::name,e.token);test.token.text=name;test.qualifier=e.qualifier;test.qualifier_token=e.qualifier_token;
                try {
                    auto [owner, value] = global(test);
                    if (value) {
                        // Completion follows the same pure/runtime boundary as
                        // name_expression, without inferring illegal candidates.
                        if (!current) {
                            continue;
                        }
                        if (value->type == Type::none) {
                            owner->resolve_global(*value);
                        }
                        if (auto* candidate = add(name, value->type, 6)) {
                            candidate->documentation = source_documentation(
                                owner->source, value->token.offset).text();
                        }
                        continue;
                    }
                } catch (const CompilationError&) {
                } catch (const Poison&) {
                }
                try {
                    const auto& function = resolve(test);
                    auto result = function.result;

                    // Generic collection headers still hold source patterns;
                    // their element layouts are resolved only on specialization.
                    if (!function.generics.empty() && ir::collection_type(result)) {
                        result.kind = Type::unresolved;
                    }

                    if (auto* candidate = add(name, result, 3)) {
                        candidate->detail=editor_signature(function);
                        for (const auto& [id, owner] : units) {
                            if (owner->declarations.contains(function.token.text) &&
                                owner->declarations.at(function.token.text) == &function) {
                                candidate->documentation = source_documentation(
                                    owner->source, function.token.offset).text(function.result != Type::none);
                            }
                        }
                    }

                    continue;
                } catch (const CompilationError&) {
                } catch (const Poison&) {
                }
                try {auto [owner,c]=constant(test);if(c) {evaluate(*owner,*c);if(auto* candidate=add(name,c->type,21))candidate->documentation=source_documentation(owner->source,c->declaration.token.offset).text();continue;}} catch(const CompilationError&) {} catch(const Poison&) {}
                try {auto [owner,structure]=structure_lookup(unit,root,units,e.qualifier.empty()?name:e.qualifier+"."+name,e.token);
                    if(structure && structure->layout_state!=3)candidates.emplace(name,EditorCandidate{name,structure->generics.empty()?"type":"generic type",7,{},source_documentation(owner->source,structure->token.offset).text()});
                } catch(const CompilationError&){}catch(const Poison&){}
            }
            if(e.qualifier.empty()) {
                for(const auto& [name,owners]:unit.qualifiers) candidates.emplace(name,EditorCandidate{name,"import",9});
                for(const auto& name:{"true","false","null","not","match","if","func"}) candidates.emplace(name,EditorCandidate{name,"keyword",14});
                for(const auto& name:{"string","format","print"})if(!candidates.contains(name))candidates.emplace(name,EditorCandidate{name,name==std::string("print")?"print(*values, sep = space, end = newline, file = console.stdout, flush = false)":name==std::string("string")?"string(value) -> string":"format(value, spec: string) -> result<string, string>",3,{},"Invariant type-aware display and formatting."});
                if(current && current->asynchronous)for(const auto& name:{"await","spawn"})candidates.emplace(name,EditorCandidate{name,"keyword",14});
            }
        }
        const auto limit=e.qualifier.empty()?256u:2048u;
        for(auto& [name,candidate]:candidates) {
            if(!name.starts_with(diagnostics.editor->prefix))continue;
            if(out.candidates.size()>=limit){out.incomplete=!e.qualifier.empty();break;}
            out.candidates.push_back(std::move(candidate));
        }
        throw EditorProbeFound{};
    }
    // Borrow the expected type: even an unused by-value probe argument reserves
    // copy storage in recursive Debug frames that must fit the 1 MiB stack.
    VOID_SEMANTIC_HANDLER void editor_name_candidates(Expr& e,const std::optional<Type>& expected) {
        if(e.qualifier.empty() && expected && expected->definition && expected->definition->result_type)
            for(unsigned tag=0;tag<2;++tag) {
                const auto name=tag?"Err":"Ok";auto token=e.token;token.text=name;
                auto target=result_editor_target(*expected,tag,token);
                diagnostics.editor->result.candidates.push_back({name,std::string(name)+"("+(tag?"error: ":"value: ")+ir::type_name(target.parameters[0].type)+") -> "+ir::type_name(*expected),3,{},unit.builtins.at(target.symbol).documentation.text(true)});
            }
        editor_candidates(e);
    }
    VOID_SEMANTIC_HANDLER void editor_field_candidates(Expr& e,const Type& receiver) {
        auto type=receiver;
        if(e.parts[0]->kind==Expr::Kind::name)if(auto* variable=lookup(e.parts[0]->token.text))type.readonly|=variable->readonly;
        if(ir::nullable_type(type))throw EditorProbeFound{};
        auto& probe=*diagnostics.editor;
        if(current && editor_statement && !current->global_initializer && !current->initializer && !probe.finalizing &&
           !invalid_function && !unit.failed_import && !current->parse_failed) {
            const bool addressable=type.kind!=Type::aggregate || editor_mutable_storage(*e.parts[0]);
            // Nominally mutable bindings can acquire readonly authority from
            // earlier calls/aliases. Stabilize every reference-bearing receiver,
            // even when its immediate syntax contains no call or readonly token.
            bool effects=type.readonly || !ir::reference_paths(type).empty() || !addressable;
            std::vector<const Expr*> pending{e.parts[0].get()};
            while(!pending.empty()) {
                const auto* part=pending.back();pending.pop_back();
                effects|=part->kind==Expr::Kind::method_call || part->kind==Expr::Kind::call || part->kind==Expr::Kind::function_call;
                for(const auto& child:part->parts)pending.push_back(child.get());
            }
            // Argument checking can own temporary rewrites. Capture the receiver
            // before unwinding them; retain by its owning checked statement.
            if(effects)throw EditorReceiverFound{editor_statement,std::move(e.parts.front()),e.token,type.readonly,addressable};
        }
        if(enum_value_type(type)) {
            std::vector<std::string> names{"name","is_defined"};if(type.kind==Type::numeric_enum)names.push_back("value");
            if(type.kind==Type::numeric_enum && type.definition->enum_flags)for(auto name:{"has","has_any","with","without","toggled"})names.push_back(name);
            if(invalid_function || unit.failed_import || (current && current->parse_failed))throw EditorProbeFound{};
            std::set<std::string> explicit_names;
            for(const auto& [id,owner]:units)for(const auto& function:owner->functions)
                if(function.receiver && function.receiver->type.definition==type.definition && !function.initializer && !function.accessor && accessible(function)) {
                    explicit_names.insert(function.token.text);
                    if(editor_method_available(function,type,{},*e.parts[0]))diagnostics.editor->result.candidates.push_back({function.token.text,editor_signature(function),2,function.symbol,source_documentation(owner->source,function.token.offset).text(function.result!=Type::none)});
                }
            for(const auto& name:names){if(explicit_names.contains(name))continue;auto site=e.token;site.text=name;auto target=enum_api_signature(type,site,name);auto& record=unit.builtins.at(target.symbol);diagnostics.editor->result.candidates.push_back({name,(name=="name" || name=="value")?ir::type_name(target.result):editor_signature(target),name=="name" || name=="value"?10:2,target.symbol,record.documentation.text(true)});}
            throw EditorProbeFound{};
        }
        editor_candidates(e,type);
    }
    // Borrow context through dispatch; handlers that infer a collection context
    // own their copy. Per-branch copies inflate every recursive Debug frame.
    Type expression_impl(Expr& e, const std::optional<Type>& expected) {
        if(editor_cursor(e) && e.kind==Expr::Kind::name) editor_name_candidates(e,expected);
        switch (e.kind) {
        case Expr::Kind::spawned:return spawned_expression(e);
        case Expr::Kind::awaited:return awaited_expression(e);
        case Expr::Kind::lambda:return lambda_expression(e);
        case Expr::Kind::task_cancel:case Expr::Kind::capture_reference:case Expr::Kind::function_null: case Expr::Kind::function_reference:return e.type;
        case Expr::Kind::function_call:return function_invocation(e);
        case Expr::Kind::test:return test_expression(e);
        case Expr::Kind::match:return match_expression(e,expected);
        case Expr::Kind::choice:return choice_expression(e,expected);
        case Expr::Kind::has:return has_expression(e);
        case Expr::Kind::sum:case Expr::Kind::editor_hole:return e.type;
        case Expr::Kind::propagate:return propagate_expression(e);
        case Expr::Kind::construction: return e.type; // An inferred literal may revisit its already checked constructor.
        case Expr::Kind::dictionary_literal: return dictionary_expression(e,expected);
        case Expr::Kind::array_literal: return array_expression(e,expected);
        case Expr::Kind::array_default: return array_default_expression(e);
        case Expr::Kind::array_index: return index_expression(e);
        case Expr::Kind::array_slice: return slice_expression(e);
        case Expr::Kind::literal: return literal_expression(e, expected);
        case Expr::Kind::global: return e.type;
        case Expr::Kind::name:
            if(e.token.text=="None" && e.qualifier.empty())return option_expression(e,expected);
            return name_expression(e);
        case Expr::Kind::field: return field_expression(e, expected);
        case Expr::Kind::call:
            if(e.token.text=="Ok" || e.token.text=="Err")return result_expression(e,expected);
            if((e.token.text=="Some" || e.token.text=="None") && e.qualifier.empty())return option_expression(e,expected);
            return call_expression(e);
        case Expr::Kind::method_call: return e.op=="$iteration" || e.op=="$async_iteration_next" ? iteration_expression(e) : method_expression(e);
        case Expr::Kind::conversion: return conversion_expression(e);
        case Expr::Kind::unary: return unary_expression(e, expected);
        default: return binary_expression(e, expected);
        }
    }
    // Fixed-array temporaries belong to this case, not every recursive dispatch.
    VOID_SEMANTIC_HANDLER Type array_default_expression(Expr& e) {
        if(e.type.kind!=Type::fixed_array || e.parts.size()!=1)
            error(source,e.token,Code::unsupported_type,"Invalid generated fixed-array recipe.");
        expect(expression(*e.parts[0],ir::array_element(e.type)),ir::array_element(e.type),e.token);
        return e.type;
    }
    VOID_SEMANTIC_HANDLER Type test_expression(Expr& e){
        if(!current)error(source,e.token,Code::constant_call,"Patterns require a function body; compile-time pattern evaluation is not implemented.");
        auto subject=expression(*e.parts[0]);if(!pattern_bindings)scopes.emplace_back();check_pattern(e.patterns[0],subject);if(!pattern_bindings)scopes.pop_back();return e.type=Type::boolean;
    }
    VOID_SEMANTIC_HANDLER Type match_expression(Expr& e,const std::optional<Type>& expected){
        if(!current)error(source,e.token,Code::constant_call,"Patterns require a function body; compile-time pattern evaluation is not implemented.");return check_match(e,expected);
    }
    VOID_SEMANTIC_HANDLER Type propagate_expression(Expr& e){
        auto type=expression(*e.parts[0]);
        if(current && current->deferred)error(source,e.token,Code::invalid_value,"? cannot propagate out of deferred code.");
        if(!current || !type.definition || !type.definition->result_type || !current->result.definition || !current->result.definition->result_type)error(source,e.token,Code::type_mismatch,"? requires a result value in a result-returning function.");
        if (ir::field_type(type, 2) != ir::field_type(current->result, 2)) {
            error(source, e.token, Code::type_mismatch,
                  "Result propagation requires exactly matching error types.");
        }
        return e.type = ir::field_type(type, 1);
    }
#include "pattern_semantics.inc"
    VOID_SEMANTIC_HANDLER Type lambda_expression(Expr& e) {
        if(!current)error(source,e.token,Code::constant_call,"A closure needs a function activation.");
        auto parsed=current->lambdas.begin();std::advance(parsed,e.slot);unit.functions.push_back(std::move(*parsed));auto& lambda=unit.functions.back();
        lambda.lexical_owner=lexical_owner();lambda.lexical_parent=current->symbol;
        lambda.symbol=current->symbol+"::$lambda$"+std::to_string(e.slot);lambda.concrete_name=lambda.symbol.substr(unit.identity.size()+2);lambda.display_name="<lambda>";lambda.prepared=true;lambda.instantiation_context=current->instantiation_context;lambda.specialization_depth=current->specialization_depth;lambda.substitutions=current->substitutions;lambda.substitution_sources=current->substitution_sources;
        for(auto& parameter:lambda.parameters)parameter.type=resolve_type(parameter.type,unit,root,units,parameter.token);lambda.result=resolve_type(lambda.result,unit,root,units,e.token);
        std::vector<Type> parameters;for(const auto& parameter:lambda.parameters)parameters.push_back(parameter.type);
        lambda.parameters.insert(lambda.parameters.begin(),Parameter{Token{TokenKind::name,"$environment",e.token.offset},Type::none});
        Semantics checker(unit,units,root,evaluate,diagnostics,defaults);checker.enclosing=[&](const std::string& name,bool capture){return lookup(name,capture);};checker.analyze_one(lambda);
        auto environment=unit.make_capture_record(lambda.symbol+"::$captures",Type::none,false);
        auto definition=std::const_pointer_cast<ir::ValueType>(environment.definition);
        for(unsigned n=0;n<lambda.captures.size();++n){const auto& capture=lambda.captures[n];definition->fields.push_back({"$capture$"+std::to_string(n),capture.cell,definition->identity});auto reference=std::make_unique<Expr>(Expr::Kind::capture_reference,capture.token);reference->slot=capture.outer_slot;reference->type=capture.cell;reference->implicit=capture.snapshot;e.parts.push_back(std::move(reference));}
        lambda.parameters[0].type=environment;lambda.locals[lambda.parameters[0].slot]=environment;
        root.member_catalog->functions.push_back(member_header_for_lambda(lambda));
        e.environment=environment;e.implicit=true;e.kind=Expr::Kind::function_reference;e.target=lambda.symbol;return e.type=unit.make_callable(parameters,lambda.result);
    }
    ir::Function member_header_for_lambda(const Function& function) {
        ir::Function header;header.name=function.symbol;header.module_identity=unit.identity;header.result=function.result;header.bound_receiver=function.bound_receiver;header.closure=true;header.parameters.clear();for(const auto& parameter:function.parameters)header.parameters.push_back(parameter.type);header.visibility="private";return header;
    }
    VOID_SEMANTIC_HANDLER Type bind_method(Expr& value,const Token& method,Type signature,std::vector<Type> type_arguments={}){
        if(!current)error(source,method,Code::constant_call,"Bound callbacks require a function activation; use a named function in a pure template.");
        auto receiver=value.type;auto original=std::make_unique<Expr>(std::move(value));
        Member selected{};if(receiver.kind==Type::interface_ref || ir::select_member(*root.member_catalog,receiver,method.text,unit.identity,nullptr,lexical_owner()).status!=ir::MemberSelection::missing)selected=member(receiver,method);
        if(selected.function){
            for(auto field:selected.path)embedded_receiver(original,field,method);receiver=original->type;
            auto* target=selected.function;
            if(!target->returns.empty())error(source,method,Code::unsupported_type,"A multiple-return method cannot be used as a function value.");
            if(!target->virtual_slot.empty())for(auto& [id,owner]:units)for(auto& function:owner->functions)if(function.symbol==target->virtual_slot)target=&function;
            auto canonical=target->receiver->type;
            if(receiver.kind==Type::managed_ref && canonical.kind==Type::managed_ref && Type::key(receiver)!=Type::key(canonical)){
                auto source=std::move(original);original=std::make_unique<Expr>(Expr::Kind::conversion,source->token);original->op="upcast";original->implicit=true;canonical.readonly=receiver.readonly;original->type=canonical;original->parts.push_back(std::move(source));receiver=canonical;
            }
        }
        const bool direct=receiver.kind==Type::managed_ref || receiver.kind==Type::interface_ref;
        auto identity=current->symbol+"::$bound$"+std::to_string(current->binding_ordinal++)+"$"+method.text;
        if(direct)identity=unit.identity+"::$bound$"+Type::key(receiver)+"$"+method.text+"$"+Type::key(signature);
        for(auto argument:type_arguments){auto key=ir::array_identity(resolve_type(argument,unit,root,units,method),{});identity+="$"+std::to_string(key.size())+":"+key;}
        Function* existing=nullptr;for(auto& f:unit.functions)if(f.symbol==identity)existing=&f;
        if(!existing){
            Function adapter;adapter.token=method;adapter.symbol=identity;adapter.concrete_name=identity.substr(unit.identity.size()+2);adapter.display_name="<bound "+method.text+">";adapter.visibility="private";adapter.prepared=true;adapter.instantiation_context=current->instantiation_context;adapter.specialization_depth=current->specialization_depth;adapter.result=*signature.definition->call_result;
            Token receiver_name{TokenKind::name,"$bound_receiver",method.offset};adapter.parameters.push_back({receiver_name,receiver});
            auto call=std::make_unique<Expr>(Expr::Kind::method_call,method);call->type_arguments=type_arguments;call->parts.push_back(std::make_unique<Expr>(Expr::Kind::name,receiver_name));
            for(unsigned n=0;n<signature.definition->call_parameters.size();++n){Token name{TokenKind::name,"$argument$"+std::to_string(n),method.offset};adapter.parameters.push_back({name,signature.definition->call_parameters[n]});call->parts.push_back(std::make_unique<Expr>(Expr::Kind::name,name));call->names.push_back({});}
            Statement invoke(adapter.result==Type::none?Statement::Kind::expression:Statement::Kind::returned,method);invoke.expression=std::move(call);adapter.body.push_back(std::move(invoke));unit.functions.push_back(std::move(adapter));existing=&unit.functions.back();
            Semantics checker(unit,units,root,evaluate,diagnostics,defaults);checker.analyze_one(*existing);existing->anonymous=true;
            {
                existing->bound_receiver=direct;
                auto environment=unit.make_capture_record(identity+"::$receiver",receiver,true);std::const_pointer_cast<ir::ValueType>(environment.definition)->base=signature.definition->fields[1].type.target;
                existing->parameters[0].type=environment;existing->locals[0]=environment;
                auto& invoke=*existing->body[0].expression;auto field=std::make_unique<Expr>(Expr::Kind::field,receiver_name);field->slot=0;field->type=receiver;field->parts=std::move(invoke.parts[0]->parts);field->parts.push_back(std::make_unique<Expr>(Expr::Kind::name,receiver_name));field->parts[0]->type=environment;field->parts[0]->slot=0;invoke.parts[0]=std::move(field);invoke.field_indexes.insert(invoke.field_indexes.begin(),0);
            }
            root.member_catalog->functions.push_back(member_header_for_lambda(*existing));
        }
        value=Expr(Expr::Kind::function_reference,method);value.type=signature;value.target=identity;value.environment=existing->parameters[0].type;value.op="value_bound";value.implicit=true;value.parts.push_back(std::move(original));return signature;
    }
    VOID_SEMANTIC_HANDLER void deferred_call(Statement& statement) {
        auto& call=*statement.expression;
        if(expression(call)==error_type)return;
        if(call.kind!=Expr::Kind::call && call.kind!=Expr::Kind::method_call && call.kind!=Expr::Kind::function_call)error(source,statement.token,Code::invalid_value,"defer requires a call or a deferred block.");
        Function thunk;thunk.anonymous=true;thunk.deferred=true;thunk.prepared=thunk.analyzed=true;thunk.token=statement.token;thunk.token.text="$deferred$"+std::to_string(unit.functions.size());thunk.symbol=current->symbol+"::"+thunk.token.text;thunk.concrete_name=thunk.symbol.substr(unit.identity.size()+2);thunk.display_name="<defer>";thunk.visibility="private";
        auto environment=unit.make_capture_record(thunk.symbol+"::$captures",Type::none,false);auto definition=std::const_pointer_cast<ir::ValueType>(environment.definition);
        thunk.parameters.push_back({Token{TokenKind::name,"$environment",statement.token.offset},environment});thunk.locals.push_back(environment);
        auto reference=std::make_unique<Expr>(Expr::Kind::function_reference,statement.token);reference->target=thunk.symbol;reference->type=unit.make_callable({},Type::none);reference->environment=environment;reference->implicit=true;
        for(unsigned n=0;n<call.parts.size();++n){
            // print retains display wrappers for delayed rendering. Capture the
            // argument inside each wrapper, not the wrapper itself; replacing
            // it with a name loses the lowering plan (and bare null is constant).
            const bool printed=call.target=="$console.print" && call.argument_indexes[n]<call.slot;
            if(printed && call.parts[n]->op=="$print.null")continue;
            auto& part=printed?call.parts[n]->parts[0]:call.parts[n];auto capture=reference->parts.size();
            auto type=part->type;auto cell=unit.make_capture_record(thunk.symbol+"::$cell$"+std::to_string(n),type,true);definition->fields.push_back({"$capture$"+std::to_string(n),cell,definition->identity});
            auto snapshot=std::make_unique<Expr>(Expr::Kind::capture_reference,part->token);snapshot->type=cell;snapshot->implicit=true;snapshot->parts.push_back(std::move(part));reference->parts.push_back(std::move(snapshot));
            auto slot=static_cast<unsigned>(thunk.locals.size());thunk.locals.push_back(type);thunk.captured_locals.emplace(slot,cell);thunk.capture_inputs.emplace(slot,capture);
            part=std::make_unique<Expr>(Expr::Kind::name,reference->parts.back()->token);part->type=type;part->slot=slot;
        }
        call.receiver_addressable=false;call.selected_receiver=false;
        Statement invoke(Statement::Kind::expression,statement.token);invoke.expression=std::move(statement.expression);thunk.body.push_back(std::move(invoke));statement.expression=std::move(reference);unit.functions.push_back(std::move(thunk));root.member_catalog->functions.push_back(member_header_for_lambda(unit.functions.back()));
    }
    VOID_SEMANTIC_HANDLER Type function_invocation(Expr& e) {
        auto type=expression(*e.parts[0]);if(!type.definition || !type.definition->callable)error(source,e.token,Code::not_callable,"Value is not a function.");
        const auto& parameters=type.definition->call_parameters;
        if(e.parts.size()!=parameters.size()+1 || std::any_of(e.names.begin(),e.names.end(),[](const auto& name){return !name.text.empty();}))error(source,e.token,Code::argument_count,"Incorrect positional function-value arguments.");
        for(unsigned n=0;n<parameters.size();++n)expect(expression(*e.parts[n+1],Type(parameters[n])),Type(parameters[n]),e.parts[n+1]->token);
        return e.type=Type(*type.definition->call_result);
    }
    Type sum_expression(Expr& e,Type type,unsigned tag,unsigned first) {
        const auto& variant=type.definition->cases.at(tag);
        // Dirty calls may not yet have all payloads. Visit the written arguments
        // with their real context before reporting arity, just as ordinary calls do.
        if(diagnostics.editor && e.parts.size()-first!=variant.count)
            for(unsigned n=0;n<std::min<std::size_t>(e.parts.size()-first,variant.count);++n)expression(*e.parts[first+n],ir::field_type(type,variant.begin+n));
        if(e.parts.size()-first!=variant.count || std::any_of(e.names.begin(),e.names.end(),[](const auto& name){return !name.text.empty();}))error(source,e.token,Code::argument_count,"Incorrect variant payload arguments.");
        for(unsigned n=0;n<variant.count;++n)expect(expression(*e.parts[first+n],ir::field_type(type,variant.begin+n)),ir::field_type(type,variant.begin+n),e.parts[first+n]->token);
        if(first)e.parts.erase(e.parts.begin(),e.parts.begin()+first);
        e.kind=Expr::Kind::sum;e.slot=tag;return e.type=type;
    }
    Function result_editor_target(Type type,unsigned tag,Token token) {
        Function target;target.token=token;target.symbol=std::string(ir::nullable_type(type)?"$nullable::":"$result::")+Type::key(type)+"|"+ir::capability_key(type)+"::"+(tag?"Err":"Ok");target.result=type;
        auto payload=ir::nullable_type(type)?ir::nullable_payload(type):ir::field_type(type,tag?2:1);
        target.parameters.push_back({Token{TokenKind::name,tag?"error":"value",token.offset},payload});
        builtin_record(target,ir::nullable_type(type)?"Matches a present nullable value and exposes its non-null payload.":tag?"The error variant of result; contains the failure payload.":"The success variant of result; contains the successful value. Ok(null) is successful.");
        unit.builtins.at(target.symbol).documentation.parameters.push_back({tag?"error":"value",ir::nullable_type(type)?"The present non-null value.":tag?"The error carried by Err.":"The successful value carried by Ok.",{}});
        return target;
    }
    // Keep the result signature's Function temporary out of the recursive
    // expression dispatcher, including when Release would inline this handler.
    VOID_SEMANTIC_HANDLER Type result_expression(Expr& e,const std::optional<Type>& expected) {
        if(!expected || !expected->definition || !expected->definition->result_type)error(source,e.token,Code::type_mismatch,"Ok/Err require a contextual result<t, e> type.","Annotate the variable or the enclosing function result.");
        auto target=result_editor_target(*expected,e.token.text=="Ok"?0:1,e.token);
        if(diagnostics.editor)editor_call(e,target,0);
        e.source_target=target.symbol;
        return sum_expression(e,*expected,e.token.text=="Ok"?0:1,0);
    }
    VOID_SEMANTIC_HANDLER Type option_expression(Expr& e,const std::optional<Type>& expected) {
        if(!expected || !expected->definition || !expected->definition->option_type)
            error(source,e.token,Code::type_mismatch,"Some and None require a contextual option<T> type.");
        const bool some=e.token.text=="Some";
        if(!some && e.kind!=Expr::Kind::name)error(source,e.token,Code::syntax,"Write None without parentheses.");
        return sum_expression(e,*expected,some?1u:0u,0);
    }
    std::pair<Unit*, Structure*> enum_declaration(Expr& e,const Type* context=nullptr) {
        auto& base = *e.parts[0];
        std::string spelling;
        if (base.kind == Expr::Kind::name && !lookup(base.token.text)) {
            spelling = base.token.text;
        } else if (base.kind == Expr::Kind::field &&
                   base.parts[0]->kind == Expr::Kind::name &&
                   !lookup(base.parts[0]->token.text) &&
                   unit.qualifiers.contains(base.parts[0]->token.text)) {
            spelling = base.parts[0]->token.text + "." + base.token.text;
        }
        if (spelling.empty()) {
            return {};
        }
        auto selected = structure_lookup(unit, root, units, spelling, base.token);
        if(!selected.second || (selected.second->definition->cases.empty() && !selected.second->definition->enum_backing))return {};
        if(!selected.second->generics.empty()) {
            auto& declaration=*selected.second;
            if(base.type_arguments.empty()) {
                if(context)for(auto& candidate:selected.first->structures)
                    if(candidate.generic_origin==&declaration && candidate.definition==context->definition){selected.second=&candidate;break;}
                if(selected.second==&declaration) {
                    const auto& variants=declaration.definition->cases;
                    auto variant=std::find_if(variants.begin(),variants.end(),[&](const auto& c){return c.name==e.token.text;});
                    if(e.kind!=Expr::Kind::method_call || variant==variants.end() || e.parts.size()!=variant->count+1)
                        error(source,base.token,Code::argument_count,"Generic enum cases require concrete type arguments or an inferable payload.");
                    std::vector<std::pair<Type,Expr*>> supplied;
                    for(unsigned n=0;n<variant->count;++n)supplied.emplace_back(*declaration.fields[variant->begin+n].annotation,e.parts[n+1].get());
                    auto arguments=infer_arguments(*selected.first,declaration.generics,{},supplied,base.token);
                    selected.second=&unit.specialize_structure(unit,*selected.first,declaration,arguments,base.token);
                }
            } else {
                std::vector<Type> arguments;
                for(auto type:base.type_arguments)arguments.push_back(resolve_type(type,unit,root,units,base.token));
                selected.second=&unit.specialize_structure(unit,*selected.first,declaration,arguments,base.token);
            }
        } else if(selected.second && !base.type_arguments.empty())error(source,base.token,Code::unsupported_type,"This enum is not generic.");
        if (!selected.second || (selected.second->definition->cases.empty() && !selected.second->definition->enum_backing)) {
            return {};
        }
        return selected;
    }

    VOID_SEMANTIC_HANDLER void editor_enum_candidates(Unit& owner, const Structure& structure,const Token& site) {
        auto& out=diagnostics.editor->result;
        // A rejected declaration can leave no binding in the scope. Do not
        // fall through to an enum if an earlier error makes that lookup uncertain.
        const auto scope_start=current?current->token.offset:0;
        if(!invalid_function && !unit.failed_import && !diagnostics.within(source,scope_start,site.offset+1)) {
            const auto type=Type::nominal(structure.definition);
            if(std::none_of(type.definition->cases.begin(),type.definition->cases.end(),[](const auto& c){return c.count!=0 || c.name=="values";}) &&
               std::none_of(type.definition->numeric_members.begin(),type.definition->numeric_members.end(),[](const auto& m){return m.name=="values";})) {
                auto token=site;token.text="values";auto target=enum_api_signature(type,token,"values");out.candidates.push_back({"values",editor_signature(target),2,target.symbol,unit.builtins.at(target.symbol).documentation.text(true)});
            }
            if(structure.definition->enum_backing) {
                const auto& members=structure.definition->numeric_members;
                for(unsigned n=0;n<members.size();++n) {
                    const auto& member=members[n];
                    auto detail=ir::type_name(type)+" = "+numeric::integer_text(member.bits,*structure.definition->enum_backing);
                    out.candidates.push_back({member.name,detail,20,{},source_documentation(owner.source,structure.case_tokens[n].offset).text(false)});
                }
            } else {
                const auto& variants=structure.definition->cases;
                for(unsigned n=0;n<variants.size();++n) {
                    const auto& variant=variants[n];
                    std::string signature=variant.name;
                    if(variant.count) {
                        signature+='(';
                        for(unsigned i=0;i<variant.count;++i) {
                            if(i)signature+=", ";
                            const auto index=variant.begin+i;
                            signature+=structure.fields[index].token.text+": "+ir::type_name(ir::field_type(type,index));
                        }
                        signature+=')';
                    }
                    signature+=" -> "+ir::type_name(type);
                    out.candidates.push_back({variant.name,signature,20,{},source_documentation(owner.source,structure.case_tokens[n].offset).text(true)});
                }
            }
        }
        throw EditorProbeFound{};
    }

    bool enum_recovery_available() const {
        return diagnostics.recovery && !diagnostics.editor && diagnostics.recovery->module==unit.identity &&
            diagnostics.recovery->available() && !invalid_function && !unit.failed_import;
    }

    VOID_SEMANTIC_HANDLER Type missing_enum_member(Expr& e,const Structure& structure,Type type,bool call) {
        const auto prefix=[&](const auto& member){return member.name.starts_with(e.token.text);};
        const bool partial=!call && !invalid_function && !unit.failed_import &&
            (std::any_of(structure.definition->cases.begin(),structure.definition->cases.end(),prefix) ||
             std::any_of(structure.definition->numeric_members.begin(),structure.definition->numeric_members.end(),prefix));
        if(partial && enum_recovery_available()) {
            diagnostics.recovery->record();
            e.kind=Expr::Kind::editor_hole;e.parts.clear();return e.type=type;
        }
        Diagnostic diagnostic(Code::unknown_identifier);
        diagnostic.primary=span(source,e.token);
        diagnostic.description=std::string(structure.definition->enum_backing?"Unknown numeric enum member '":"Unknown enum variant '")+e.token.text+"'.";
        diagnostic.editor_enum_recovery=partial;
        throw CompilationError({std::move(diagnostic)});
    }

    VOID_SEMANTIC_HANDLER std::optional<Type> enum_expression(Expr& e,bool call,const Type* subject=nullptr) {
        auto [owner, structure] = enum_declaration(e,subject);
        if(structure && structure->definition->enum_backing && owner->resolve_numeric)owner->resolve_numeric(*structure);
        if (!structure) {
            return {};
        }
        auto type=Type::nominal(structure->definition);
        const bool probe=editor_cursor(e);
        if(subject && type!=*subject) {
            if(probe)throw EditorProbeFound{};
            error(source,e.token,Code::type_mismatch,"Enum pattern or incomplete return has a different nominal type.");
        }
        if(e.token.text=="values" &&
           std::none_of(type.definition->cases.begin(),type.definition->cases.end(),[](const auto& c){return c.name=="values";}) &&
           std::none_of(type.definition->numeric_members.begin(),type.definition->numeric_members.end(),[](const auto& m){return m.name=="values";})) {
            if(!call || e.parts.size()!=1)error(source,e.token,Code::argument_count,"Enum values() takes no arguments.");
            if(std::any_of(type.definition->cases.begin(),type.definition->cases.end(),[](const auto& c){return c.count!=0;}))
                error(source,e.token,Code::unsupported_type,"values() cannot invent payloads; use it with numeric or payload-free enums.");
            auto target=enum_api_signature(type,e.token,"values");e.source_target=target.symbol;e.kind=Expr::Kind::array_literal;e.parts.clear();e.names.clear();
            if(type.kind==Type::numeric_enum)for(const auto& member:type.definition->numeric_members){auto value=std::make_unique<Expr>(Expr::Kind::literal,e.token);value->type=type;value->value=member.bits;e.parts.push_back(std::move(value));}
            else for(unsigned n=0;n<type.definition->cases.size();++n){auto value=std::make_unique<Expr>(Expr::Kind::sum,e.token);value->type=type;value->slot=n;e.parts.push_back(std::move(value));}
            return array_expression(e,target.result);
        }
        if(probe)editor_enum_candidates(*owner,*structure,e.token);
        const auto& receiver=*e.parts.front();
        e.enum_type_use=std::make_unique<TypeUse>(TypeUse{receiver.token,type,
            receiver.kind==Expr::Kind::field?std::optional<Token>{receiver.parts.front()->token}:std::nullopt});
        if(structure->definition->enum_backing) {
            const auto& members=structure->definition->numeric_members;
            auto found=std::find_if(members.begin(),members.end(),[&](const auto& member){return member.name==e.token.text;});
            if(found==members.end())return missing_enum_member(e,*structure,type,call);
            if(call)error(source,e.token,Code::not_callable,"Numeric enum members are values.");
            e.kind=Expr::Kind::literal;e.parts.clear();e.value=found->bits;e.target=Type::key(type)+"::variant::"+found->name;
            return e.type=type;
        }
        const auto& variants=structure->definition->cases;
        auto found=std::find_if(variants.begin(),variants.end(),[&](const auto& variant){return variant.name==e.token.text;});
        if(found==variants.end())return missing_enum_member(e,*structure,type,call);
        if(!call && found->count)error(source,e.token,Code::argument_count,"Variant requires its payload arguments.");
        if(diagnostics.editor && call){Function target;target.token=e.token;target.result=Type(structure->definition);for(unsigned i=0;i<found->count;++i){const auto& field=structure->fields[found->begin+i];target.parameters.push_back({field.token,ir::field_type(target.result,found->begin+i)});}auto docs=source_documentation(owner->source,structure->case_tokens[found-variants.begin()].offset);editor_call(e,target,1,&docs);}
        auto result=sum_expression(e,Type(structure->definition),static_cast<unsigned>(found-variants.begin()),1);e.target=structure->definition->identity+"::variant::"+found->name;return result;
    }
    VOID_SEMANTIC_HANDLER Type dictionary_expression(Expr& e,const std::optional<Type>& context) {
        auto expected=context;
        if(!expected){
            if(e.parts.empty())error(source,e.token,Code::unsupported_type,"Empty dictionary literal requires a typed context.");
            std::optional<Type> key,value;for(unsigned n=0;n<e.parts.size();n+=2){if(!key)key=anchor(*e.parts[n]);if(!value)value=anchor(*e.parts[n+1]);}
            if(!key)key=expression(*e.parts[0]);if(!value)value=expression(*e.parts[1]);
            if(*key==Type::null_value || *value==Type::null_value || *key==Type::none || *value==Type::none)error(source,e.token,Code::unsupported_type,"Dictionary entries require concrete types or a typed context.");expected=unit.make_dictionary(*key,*value);
        }
        if(expected->kind!=Type::dictionary)error(source,e.token,Code::type_mismatch,"Dictionary literal requires dictionary context.");
        check_dictionary_key(source,e.token,ir::dictionary_key(*expected),units);
        for(unsigned n=0;n<e.parts.size();n+=2){expect(expression(*e.parts[n],ir::dictionary_key(*expected)),ir::dictionary_key(*expected),e.parts[n]->token);expect(expression(*e.parts[n+1],ir::dictionary_value(*expected)),ir::dictionary_value(*expected),e.parts[n+1]->token);}
        return e.type=*expected;
    }
    VOID_SEMANTIC_HANDLER Type array_expression(Expr& e,const std::optional<Type>& context) {
        auto expected=context;
        if(!expected) {
            if(e.parts.empty())error(source,e.token,Code::unsupported_type,"An empty array literal requires contextual typing.");
            std::optional<Type> element;
            for(auto& part:e.parts) if(auto hint=anchor(*part)) {element=hint;break;}
            if(!element)element=expression(*e.parts.front());
            if(*element==Type::none || *element==Type::null_value)error(source,e.token,Code::unsupported_type,"Array element requires a concrete value type.");
            *element=ir::with_readonly_paths(*element,{});auto definition=std::make_shared<ir::ValueType>();definition->identity=ir::array_identity(*element,{});definition->name=definition->identity;definition->element=ir::FieldType(*element);
            expected=Type(definition,Type::growable_array);
        }
        if(!ir::array_type(*expected)) error(source,e.token,Code::unsupported_type,"Array literals require a fixed or growable array context.");
        if(expected->kind==Type::fixed_array && e.parts.size()!=*expected->definition->extent) error(source,e.token,Code::type_mismatch,"Fixed-array literal requires exactly "+std::to_string(*expected->definition->extent)+" elements.");
        for(auto& part:e.parts) expect(expression(*part,ir::array_element(*expected)),ir::array_element(*expected),part->token);
        return e.type=*expected;
    }
    Type slice_type(Type source) {
        auto element=ir::array_element(source);auto definition=std::make_shared<ir::ValueType>();definition->identity=definition->name=ir::array_identity(element,{});
        // Structural element definitions are authority-free. Project the const
        // source's child permissions onto the new list type below, never embed
        // those permissions in its shared physical element definition.
        definition->element=ir::FieldType(ir::with_readonly_paths(element,{}));
        Type result(definition,Type::growable_array);ir::ReferencePaths paths;
        for(auto path:ir::readonly_paths(element)){path.insert(path.begin(),ir::array_element_path);paths.insert(std::move(path));}
        return ir::with_readonly_paths(result,paths);
    }
    VOID_SEMANTIC_HANDLER Type slice_expression(Expr& e) {
        auto array=expression(*e.parts[0]);if(array==error_type)return error_type;
        if(!ir::array_type(array))error(source,e.token,Code::unsupported_type,"Slicing requires an array.");
        for(unsigned n=1;n<3;++n)if(!numeric::integer(expression(*e.parts[n])))error(source,e.parts[n]->token,Code::type_mismatch,"Slice endpoint requires an integer, excluding bool and floats.");
        if(!current)error(source,e.token,Code::constant_call,"Slicing is a runtime array operation.");
        return e.type=slice_type(array);
    }
    VOID_SEMANTIC_HANDLER Type index_expression(Expr& e) {
        auto array=expression(*e.parts[0]);
        if(array==error_type) return error_type;
        if(array.kind==Type::string)return text_index_expression(e);
        if(array.kind==Type::dictionary){if(!current)error(source,e.token,Code::constant_call,"Dictionary lookup requires runtime evaluation.");expect(expression(*e.parts[1],ir::readonly_view(ir::dictionary_key(array))),ir::readonly_view(ir::dictionary_key(array)),e.parts[1]->token);return e.type=ir::dictionary_value(array);}
        if(!ir::array_type(array)) error(source,e.token,Code::unsupported_type,"Indexing requires an array or dictionary.");
        auto index=expression(*e.parts[1]);
        if(index==error_type) return error_type;
        if(!numeric::integer(index)) error(source,e.parts[1]->token,Code::type_mismatch,"Array index requires an integer, excluding bool and floating types.");
        return e.type=ir::array_element(array);
    }
    VOID_SEMANTIC_HANDLER Type has_expression(Expr& e) {
        auto left=resolve_type(e.type_arguments.at(0),unit,root,units,e.token);
        auto right=resolve_type(e.type_arguments.at(1),unit,root,units,e.token);
        auto supported=[](Type t){return !t.nullable && !t.construction && t.definition && (t.kind==Type::managed_ref || t.kind==Type::aggregate || t.kind==Type::interface_ref) && t.definition->cases.empty() && !t.definition->callable;};
        if(!supported(left) || !supported(right))error(source,e.token,Code::unsupported_type,"has requires nonnullable struct or interface type operands.");
        // Default type inference needs only bool. Its value is checked again
        // after all method headers are resolved; never freeze a partial catalog.
        if(inferring_default)return e.type=Type::boolean;
        root.prepare_relationships(left,right,source,e.token);
        bool found=false;
        if(right.kind==Type::interface_ref){try{ir::satisfy_constraint(*root.member_catalog,left,right,unit.identity);found=true;}catch(const std::invalid_argument&) {}catch(const std::length_error& failure){error(source,e.token,Code::limit,failure.what());}}
        else if(left.kind!=Type::interface_ref)for(const auto& field:left.definition->fields)if(field.using_field && Type(field.type).definition==right.definition)found=true;
        e.kind=Expr::Kind::literal;e.value=found;return e.type=Type::boolean;
    }
    VOID_SEMANTIC_HANDLER Type choice_expression(Expr& e,const std::optional<Type>& expected) {
        scopes.emplace_back();
        Type result=Type::invalid;
        try {condition(*e.parts[0]);result=expression(*e.parts[1],expected);scopes.pop_back();}
        catch(...){scopes.pop_back();pattern_bindings=false;throw;}
        if(expected){expect(result,*expected,e.parts[1]->token);result=*expected;}
        if(result==Type::none)error(source,e.token,Code::invalid_value,"An if-expression requires value-producing branches.");
        expect(expression(*e.parts[2],result),result,e.parts[2]->token);
        return e.type=result;
    }
    VOID_SEMANTIC_HANDLER Type literal_expression(Expr& e, const std::optional<Type>& expected) {
        if (e.token.text == "null") {
            if(expected && ir::nullable_value(*expected)){e.kind=Expr::Kind::sum;e.slot=0;return e.type=*expected;}
            if(expected && expected->definition && expected->definition->callable && expected->nullable){e.kind=Expr::Kind::function_null;return e.type=*expected;}
            if (!expected || !ir::reference_value(*expected) || !expected->nullable) error(source,e.token,Code::type_mismatch,"null requires a nullable type context.","Use an explicit T? type annotation.");
            return e.type = *expected;
        }
        if (e.literal_text.empty()) return e.type;
        Type target = anchor(e).value_or(expected && numeric::number(*expected) && (!decimal(e) || numeric::floating(*expected)) ? *expected : decimal(e) ? Type::f32 : Type::i32);
        try { e.value = numeric::literal(e.literal_text,target); }
        catch (const std::invalid_argument& failure) { error(source,e.token,numeric::floating(target) ? Code::numeric_representability : Code::integer_range,failure.what()); }
        return e.type = target;
    }
    VOID_SEMANTIC_HANDLER Type name_expression(Expr& e) {
        if (!(e.qualifier.empty() && lookup(e.token.text))) if (auto [owner, value] = global(e); value) {
            if (!current) error(source, e.token, Code::constant_call, "A compile-time expression cannot read a runtime global.");
            if (value->type == Type::none) owner->resolve_global(*value);
            e.kind = Expr::Kind::global; e.target = value->identity;
            return e.type = value->type;
        }
        if(!(e.qualifier.empty() && lookup(e.token.text))) {
            bool candidate=false;
            if(e.qualifier.empty() && !unit.constant_names.contains(e.token.text) && !(same_library(unit.identity,root.identity) && root.constant_names.contains(e.token.text) && (!root.constant_names.at(e.token.text)->file_private || &root==&unit))){for(const auto& [id,u]:units)candidate|=u->declarations.contains(e.token.text);}
            else if(auto found=unit.qualifiers.find(e.qualifier);found!=unit.qualifiers.end())for(auto* u:found->second)candidate|=u->declarations.contains(e.token.text);
            if(candidate) {
                auto* function=&resolve(e);
                if(!function->generics.empty()){
                    if(e.type_arguments.empty())error(source,e.token,Code::unsupported_type,"A generic function value requires explicit concrete type arguments.");
                    std::vector<Type> arguments;for(auto type:e.type_arguments)arguments.push_back(resolve_type(type,unit,root,units,e.token));auto& owner=generic_owner(*function);function=&unit.specialize_function(unit,owner,const_cast<Function&>(*function),arguments,e.token,current?current->specialization_depth+1:0,Type::none);
                }else if(!e.type_arguments.empty())error(source,e.token,Code::unsupported_type,"This function value is not generic.");
                if(function->c_primitive)error(source,e.token,Code::unsupported_type,"C primitive bindings require a direct call.");
                if(!function->returns.empty())error(source,e.token,Code::unsupported_type,"A multiple-return function cannot be used as a function value.");
                if(function->asynchronous)error(source,e.token,Code::unsupported_type,"Async function values require a reviewed async callable contract; call the function directly.");
                std::vector<Type> parameters;for(const auto& parameter:function->parameters)parameters.push_back(parameter.type);
                e.target=function->symbol;e.kind=Expr::Kind::function_reference;return e.type=unit.make_callable(parameters,function->result);
            }
        }
        if(!e.type_arguments.empty())error(source,e.token,Code::unsupported_type,"Explicit value type arguments require a named generic function.");
        using K = Expr::Kind;
        auto* v = e.qualifier.empty() ? lookup(e.token.text) : nullptr;
        if (!v) {
            auto [owner, c] = constant(e);
            if (c) {
                evaluate(*owner, *c); e.target = owner->identity + "::" + c->declaration.name; e.kind = K::literal; e.value = c->value;
                e.aggregate_value = c->aggregate_value;
                // Fresh list objects in pure templates retain readonly authority.
                // Existing null reference templates remain ordinary value defaults.
                auto paths=ir::readonly_paths(c->type);
                for(const auto& path:ir::reference_paths(c->type)) {
                    auto leaf=c->type;for(auto field:path)leaf=ir::field_type(leaf,field);
                    if(leaf.kind==Type::growable_array || leaf.kind==Type::dictionary)paths.insert(path);
                }
                return e.type = ir::with_readonly_paths(c->type,paths);
            }
        }
        if (!v && unit.failed_import) throw Poison{};
        if (!v) error(source, e.token, Code::unknown_identifier, "Unknown identifier '" + e.token.text + "'.", "Declare it with var before use.");
        if(v->task_group)error(source,e.token,Code::unsupported_type,"Task group aliases are lexical scopes, not first-class values; jobs.spawn is deferred.");
        e.slot = v->slot; return e.type = v->type;
    }
    VOID_SEMANTIC_HANDLER Type field_expression(Expr& e, const std::optional<Type>& expected) {
        using K = Expr::Kind;
        auto& base = *e.parts[0];
        if(text_type_base(base)){if(editor_cursor(e))editor_field_candidates(e,Type::string);error(source,e.token,Code::unknown_method,"String type methods require a call.");}
        if(auto type=primitive_base(base);type!=Type::none) {
            if(editor_cursor(e))editor_field_candidates(e,type);
            auto api=numeric::member(e.token.text);
            if(!api || api->callable)error(source,e.token,Code::unknown_method,"Numeric type properties are min_value and max_value; operations require a call.");
            e.kind=K::literal;e.type=type;e.value=numeric::limit(type,api->name=="max_value");e.target=numeric::symbol(type,api->name);e.parts.clear();return type;
        }
        if (base.kind == K::name && !lookup(base.token.text) && unit.qualifiers.contains(base.token.text)) {
            const bool resolved=unit.qualifiers.at(base.token.text).size()==1;
            e.qualifier = base.token.text; e.qualifier_token = base.token; e.kind = K::name; e.parts.clear();
            if(resolved && e.token.text.empty() && member_recovery_available())return recover_member(e);
            try {return expression_impl(e,expected);}
            catch(const CompilationError& failure) {
                if(resolved)return recover_or_report_member(e,failure,Code::visibility);
                throw;
            }
        }
        if(auto result=static_expression(e,false))return *result;
        if(auto variant=enum_expression(e,false))return *variant;
        auto t = expression(base);
        if (t == error_type) return error_type;
        if(auto result=enum_value_api(e,t,false))return *result;
        if(editor_cursor(e)) editor_field_candidates(e,t);
        if(e.token.text.empty() && member_recovery_available())return recover_member(e);
        if(t.definition && t.definition->task_result)error(source,e.token,Code::unsupported_type,"Inferred task handles support await; task internals are not source fields.");
        if(base.event)error(source,e.token,Code::invalid_value,"Event subscription storage is private.");
        if(const auto* api=core_member(t,e.token.text);api && api->property) {
            if(!current && (t.kind==Type::string || t.kind==Type::dictionary))error(source,e.token,Code::constant_call,"Collection inspection requires runtime evaluation.");
            auto target=core_signature(*api,t,e.token);core_record(*api,target,t);
            e.source_target=target.symbol;e.target=target.symbol.substr(0,target.symbol.find("::"));return e.type=target.result;
        }
        if (ir::nullable_type(t)) error(source,e.token,Code::type_mismatch,"Unwrap the nullable value before field access.");
        if (t.kind != Type::aggregate && t.kind != Type::managed_ref && t.kind!=Type::interface_ref) error(source,e.token,Code::type_mismatch,"Field access requires a struct or interface.");
        if(t.kind==Type::interface_ref && !direct_method(t,e.token))error(source,e.token,Code::type_mismatch,"Field access requires a struct; interface views expose only declared methods.");
        if(t.definition->callable)error(source,e.token,Code::type_mismatch,"Function values are invoked directly; their representation is private.");
        if(!t.definition->cases.empty())error(source,e.token,Code::type_mismatch,"Enum payloads are accessed through patterns.");
        Member selected;
        if(!e.type_arguments.empty()){auto* method=concrete_method(e,t,true);if(!method)error(source,e.token,Code::unsupported_type,"This member is not a generic method.");selected={{},method};}
        else if(expected && expected->definition && expected->definition->callable && t.kind!=Type::interface_ref && ir::select_member(*root.member_catalog,t,e.token.text,unit.identity,nullptr,lexical_owner()).status==ir::MemberSelection::missing){auto receiver=std::move(e.parts[0]);auto token=e.token;e=std::move(*receiver);return bind_method(e,token,*expected);}
        else {
            try {selected=member(t,e.token);}
            catch(const CompilationError& failure) {return recover_or_report_member(e,failure,Code::unknown_identifier);}
        }
        if(selected.function && selected.function->accessor==1) {
            for(auto index:selected.path)embedded_receiver(e.parts[0],index,e.token);
            e.kind=K::method_call;e.property=true;
            auto result=resolved_method_expression(e,e.parts[0]->type);
            const auto setter=selected.function->symbol.substr(0,selected.function->symbol.rfind("::method::"))+"::method::$set$"+e.token.text;
            for(const auto& [id,u]:units)if(u->methods.contains(setter))e.setter=setter;
            return result;
        }
        if(selected.function){
            const auto& target=*selected.function;std::vector<Type> parameters;for(const auto& parameter:target.parameters)parameters.push_back(parameter.type);
            if(!target.returns.empty())error(source,e.token,Code::unsupported_type,"A multiple-return method cannot be used as a function value.");
            if(target.asynchronous)error(source,e.token,Code::unsupported_type,"Async method values require a reviewed async callable contract.");auto signature=unit.make_callable(parameters,target.result);auto arguments=e.type_arguments;auto receiver=std::move(e.parts[0]);auto token=e.token;e=std::move(*receiver);return bind_method(e,token,signature,std::move(arguments));
        }
        for (unsigned n=0;n+1<selected.path.size();++n) embedded_receiver(e.parts[0],selected.path[n],e.token);
        auto owner=e.parts[0]->type;
        if(owner.definition && owner.definition->native_resource && !(console::session_type(owner) && unit.identity==console::source_module))error(source,e.token,Code::unsupported_type,"Native resource payload is binding-private.");
        e.slot=selected.path.back(); e.target=owner.definition->fields[e.slot].owner+"::field::"+e.token.text;
        e.event=owner.definition->fields[e.slot].event;e.event_owner=owner.definition->fields[e.slot].owner;if(e.event && !allow_event)error(source,e.token,Code::invalid_value,"Events support subscriptions and owner emission; subscriber storage cannot escape.");
        return e.type=ir::field_type(owner,e.slot);
    }
    bool member_recovery_available() const {
        return current && diagnostics.recovery && !diagnostics.editor &&
            diagnostics.recovery->module==unit.identity && diagnostics.recovery->available() &&
            !invalid_function && !unit.failed_import;
    }
    Type recover_member(Expr& e) {
        diagnostics.recovery->record();
        e.kind=Expr::Kind::editor_hole;
        return e.type=error_type;
    }
    VOID_SEMANTIC_HANDLER Type recover_or_report_member(Expr& e,const CompilationError& failure,Code missing) {
        if(current && !invalid_function && !unit.failed_import && failure.diagnostics().size()==1) {
            auto diagnostic=failure.diagnostics().front();
            if(diagnostic.code==missing && diagnostic.primary.begin==e.token.offset &&
               diagnostic.primary.source && diagnostic.primary.source->path()==source.path()) {
                if(member_recovery_available())return recover_member(e);
                diagnostic.editor_member_recovery=true;
                throw CompilationError({std::move(diagnostic)});
            }
        }
        throw failure;
    }
#include "collection_display.inc"
#include "display_semantics.inc"
#include "json.inc"
    VOID_SEMANTIC_HANDLER void editor_format(const Expr& spec,Type type) {
        if(!diagnostics.editor || diagnostics.editor->module!=unit.identity || spec.kind!=Expr::Kind::literal || spec.aggregate_value.text!="_void_editor_cursor_")return;
        auto& out=diagnostics.editor->result;
        if(invalid_function || unit.failed_import || type==error_type)throw EditorProbeFound{};
        std::vector<std::string> patterns={"<12",">12","^12"};
        if(numeric::number(type)) {
            patterns.insert(patterns.end(),{"000","0.00","0.00#","#,##0.00","+0;-0;0","0%","D","F2","N2","P2","E3","e3"});
            if(numeric::integer(type))patterns.insert(patterns.end(),{"X","x","B","b"});
        } else if(type.definition && type.definition->module_identity=="/@stdlib/time.void") {
            patterns.insert(patterns.end(),{"yyyy-MM-dd","HH:mm:ss","HH:mm:ss.fffffffff","yyyy-MM-dd'T'HH:mm:ss","yyyy-MM-dd'T'HH:mm:sszzz","yyyy-MM-dd HH:mm:ss VV","dd:hh:mm:ss.fffffffff","MMMM dd, yyyy","hh:mm:ss tt","zzzz","VV"});
        }
        for(const auto& pattern:patterns)try {
            if(numeric::number(type))formatting::validate(numeric::display_number(0,type).kind,formatting::parse_numeric(pattern));
            else if(type.definition && type.definition->module_identity=="/@stdlib/time.void") {
                const std::map<std::string,temporal::Kind> kinds={{"date",temporal::Kind::date},{"time_of_day",temporal::Kind::time},{"local_datetime",temporal::Kind::local},{"instant",temporal::Kind::instant},{"offset_datetime",temporal::Kind::offset},{"zoned_datetime",temporal::Kind::zoned},{"duration",temporal::Kind::duration}};
                auto kind=kinds.find(type.definition->name);if(kind==kinds.end())continue;temporal::validate(kind->second,pattern);
            } else formatting::parse_text(pattern);
            out.candidates.push_back({pattern,ir::type_name(type)+" format specification",12,{},"Invariant shared formatting pattern for "+ir::type_name(type)+"."});
        }catch(const formatting::Error&){}
        throw EditorProbeFound{};
    }
    VOID_SEMANTIC_HANDLER Type interpolation_call(Expr& e) {
        if(e.target=="$interpolation" || e.target=="$hole")return e.type;
        if(e.token.text=="$interpolation") {
            for(auto& part:e.parts)expect(expression(*part),Type::string,part->token);
            e.target="$interpolation";return e.type=Type::string;
        }
        auto& operand=*e.parts.at(0);if(operand.kind==Expr::Kind::literal && operand.token.text=="null"){operand.type=Type::string;operand.token.source_width=4;operand.token.text="\"null\"";operand.aggregate_value.text="null";}
        auto t=expression(operand);auto& spec=*e.parts.at(1);expect(expression(spec),Type::string,spec.token);
        if(diagnostics.editor)editor_format(spec,t);
        e.format_specification=format_specification_syntax(spec);
        if(t==error_type)return e.type=error_type;
        if(!numeric::number(t) && t!=Type::boolean && t!=Type::string) {
            if(!t.definition || (t.kind!=Type::aggregate && t.kind!=Type::managed_ref && t.kind!=Type::interface_ref && !ir::collection_type(t)))error(source,e.parts[0]->token,Code::unsupported_type,"This value does not yet have a display implementation.");
            if(t.definition->module_identity=="/@stdlib/time.void") {
                const std::map<std::string,temporal::Kind> kinds={{"date",temporal::Kind::date},{"time_of_day",temporal::Kind::time},{"local_datetime",temporal::Kind::local},{"instant",temporal::Kind::instant},{"offset_datetime",temporal::Kind::offset},{"zoned_datetime",temporal::Kind::zoned},{"duration",temporal::Kind::duration}};
                if(auto kind=kinds.find(t.definition->name);kind!=kinds.end())try{temporal::validate(kind->second,spec.aggregate_value.text);}catch(const formatting::Error& failure){auto site=spec.token;site.offset+=failure.offset;site.source_width=1;error(source,site,Code::invalid_value,failure.what());}
            }
            auto& adapter=nominal_display_adapter(t,true,e.parts[0]->token);e.display_adapter=adapter.symbol;
            try{auto plan=formatting::split_layout(spec.aggregate_value.text);if(!adapter.display_has_format && !plan.pattern.empty())throw formatting::Error(plan.pattern_offset,"Type has no custom format hook");}catch(const formatting::Error& failure){auto site=spec.token;site.offset+=failure.offset;site.source_width=1;error(source,site,Code::invalid_value,failure.what());}
            e.type_arguments.push_back(unit.make_result(Type::string,Type::string));e.target="$hole";return e.type=Type::string;
        }
        try {
            if(numeric::number(t))formatting::validate(numeric::display_number(0,t).kind,formatting::parse_numeric(spec.aggregate_value.text));
            else formatting::parse_text(spec.aggregate_value.text);
        }catch(const formatting::Error& failure){auto site=spec.token;site.offset+=failure.offset;site.source_width=1;error(source,site,Code::invalid_value,failure.what());}
        e.type_arguments.push_back(unit.make_result(Type::string,Type::string));e.target="$hole";return e.type=Type::string;
    }
    bool display_prelude(const Expr& e) {
        if(!e.qualifier.empty() || (e.token.text!="string" && e.token.text!="format" && e.token.text!="print") || lookup(e.token.text) || unit.declarations.contains(e.token.text) || unit.constant_names.contains(e.token.text) || unit.invalid_declarations.contains(e.token.text))return false;
        for(const auto& import:unit.imports)if(!import.explicit_alias && !import.target.empty()) {
            const auto& owner=*units.at(import.target);
            if(owner.declarations.contains(e.token.text) || owner.constant_names.contains(e.token.text) || owner.structure_names.contains(e.token.text) || owner.invalid_declarations.contains(e.token.text))return false;
        }
        return !unit.structure_names.contains(e.token.text);
    }
    VOID_SEMANTIC_HANDLER Type display_call(Expr& e) {
        if(e.target=="$display.string" || e.target=="$display.format")return e.type;
        if(!e.type_arguments.empty())error(source,e.token,Code::argument_count,"Display calls do not take explicit type arguments.");
        const bool formatted=e.token.text=="format";
        Function target;target.token=e.token;target.result=formatted?unit.make_result(Type::string,Type::string):Type(Type::string);
        target.parameters.push_back({Token{TokenKind::name,"value",e.token.offset},Type::i32});
        if(formatted)target.parameters.push_back({Token{TokenKind::name,"spec",e.token.offset},Type::string});
        auto mapping=argument_mapping(e,target,0,false);
        auto value=std::find(mapping.begin(),mapping.end(),0);
        if(value==mapping.end())error(source,e.token,Code::argument_count,"Display requires a value argument.");
        auto& operand=*e.parts[value-mapping.begin()];
        if(operand.kind==Expr::Kind::literal && operand.token.text=="null"){operand.type=Type::string;operand.token.source_width=4;operand.token.text="\"null\"";operand.aggregate_value.text="null";}
        auto t=expression(operand);
        if(t==error_type)return e.type=error_type;
        if(diagnostics.editor && formatted)for(unsigned n=0;n<mapping.size();++n)if(mapping[n]==1)editor_format(*e.parts[n],t);
        Function* adapter=nullptr;
        if(!numeric::number(t) && t!=Type::boolean && t!=Type::string) {
            if(!t.definition || (t.kind!=Type::aggregate && t.kind!=Type::managed_ref && t.kind!=Type::interface_ref && !ir::collection_type(t)))error(source,e.token,Code::unsupported_type,"This value does not yet have a display implementation.");
            adapter=&nominal_display_adapter(t,formatted,e.token);
        }
        target.parameters[0].type=ir::readonly_view(t);
        target.symbol=std::string("$display.")+(formatted?"format":"string")+"::"+std::to_string(t.kind)+":"+std::to_string(Type::key(t).size())+":"+Type::key(t)+(t.nullable?"?":"")+"|"+ir::capability_key(t);
        builtin_record(target,formatted?"Format a value using invariant patterns; invalid specifications return Err with a byte offset.":"Display a value using invariant type-aware text.");
        auto& docs=unit.builtins.at(target.symbol).documentation;
        docs.parameters.push_back({"value","The value to display.",{}});
        if(formatted)docs.parameters.push_back({"spec","The invariant format specification.",{}});
        e.source_target=target.symbol;e.target=adapter?adapter->symbol:formatted?"$display.format":"$display.string";
        e.type=call_arguments(e,target,0);
        if(formatted)for(unsigned n=0;n<e.parts.size();++n)if(e.argument_indexes[n]==1){e.format_specification=format_specification_syntax(*e.parts[n]);if((ir::collection_type(t) || (t.definition && !t.definition->cases.empty())) && e.parts[n]->kind==Expr::Kind::literal && e.parts[n]->type==Type::string)try{formatting::parse_text(e.parts[n]->aggregate_value.text);}catch(const formatting::Error& failure){auto site=e.parts[n]->token;site.offset+=failure.offset;site.source_width=1;error(source,site,Code::invalid_value,failure.what());}}
        return e.type;
    }
    VOID_SEMANTIC_HANDLER Type print_call(Expr& e) {
        if(e.target=="$console.print")return e.type;
        if(!current)error(source,e.token,Code::constant_call,"print requires runtime evaluation.");
        if(!e.type_arguments.empty())error(source,e.token,Code::argument_count,"print does not take type arguments.");
        Function target;target.token=e.token;target.symbol="$console.print";target.result=Type::none;
        unsigned count=0;for(const auto& name:e.names)if(name.text.empty())++count;
        for(unsigned n=0;n<count;++n)target.parameters.push_back({Token{TokenKind::name,"$value"+std::to_string(n),e.token.offset},Type::i32});
        for(const auto& name:{"sep","end","file","flush"})target.parameters.push_back({Token{TokenKind::name,name,e.token.offset},std::string(name)=="flush"?Type(Type::boolean):std::string(name)=="file"?Type(Type::none):Type(Type::string)});
        auto mapping=argument_mapping(e,target,0,false);
        const std::string public_signature="print(*values, sep: string = space, end: string = newline, file: console.output = console.stdout, flush: bool = false) -> no result";
        if(diagnostics.editor) {
            editor_call(e,target,0);
            auto& out=diagnostics.editor->result;
            if(!out.signature.empty() && out.signature.starts_with("print(")) {
                out.signature=public_signature;out.parameters={"*values","sep: string = space","end: string = newline","file: console.output = console.stdout","flush: bool = false"};
                out.parameter_documentation={"Values to display from left to right.","Text written between values.","Text written after the record.","The console output receiving the record.","Whether to flush after writing the record."};out.documentation="Capture arguments once, display left to right and write one complete record. Checked I/O failure; prohibited on workers.";
                out.active_parameter=out.active_parameter<count?0:out.active_parameter-count+1;
            }
        }
        for(unsigned n=0;n<e.parts.size();++n){
            auto index=mapping[n];
            if(index<count){
                auto value=std::move(e.parts[n]);auto wrapper=std::make_unique<Expr>(Expr::Kind::call,value->token);wrapper->token.text="string";wrapper->implicit=true;wrapper->parts.push_back(std::move(value));wrapper->names.push_back(Token{TokenKind::name,"",e.token.offset});
                const bool null_literal=wrapper->parts[0]->kind==Expr::Kind::literal && wrapper->parts[0]->token.text=="null";
                display_call(*wrapper);if(null_literal)wrapper->op="$print.null";
                target.parameters[index].type=ir::readonly_view(wrapper->parts[0]->type);e.parts[n]=std::move(wrapper);
            }else {
                auto t=expression(*e.parts[n]);
                if(index==count+2){if(!console::endpoint_type(t) || t.definition->name!="output")error(source,e.parts[n]->token,Code::type_mismatch,"print file requires console.output.");target.parameters[index].type=t;}
                else expect(t,index==count+3?Type(Type::boolean):Type(Type::string),e.parts[n]->token);
            }
        }
        builtin_record(target,"print(*values, sep: string = space, end: string = newline, file: console.output = console.stdout, flush: bool = false). Capture written arguments once, render left to right, write one record. Checked I/O failure; prohibited on workers.");
        unit.builtins.at(target.symbol).signature=public_signature;
        e.slot=count;e.argument_indexes=std::move(mapping);e.target=e.source_target=target.symbol;return e.type=Type::none;
    }
    VOID_SEMANTIC_HANDLER Type console_call(Expr& e,unsigned operation) {
        const auto& contract=console::source_operations.at(operation);Function target;target.token=e.token;target.symbol="$console."+std::to_string(operation);target.result=Type(unit.structure_names.at("_reply")->definition);
        for(unsigned n=0;n<contract.arguments.size();++n){
            Type type=Type::i64;
            if(contract.arguments[n]=='e' || contract.arguments[n]=='r')type=expression(*e.parts.at(n));
            else if(contract.arguments[n]=='t')type=Type::string;
            else if(contract.arguments[n]=='b'){type=expression(*e.parts.at(n));if(type.kind!=Type::growable_array || ir::array_element(type)!=Type::u8)error(source,e.token,Code::type_mismatch,"Console bytes require [u8].");}
            target.parameters.push_back({Token{TokenKind::name,"arg"+std::to_string(n),e.token.offset},ir::readonly_view(type)});
        }
        builtin_record(target,"Private checked standard-stream Runtime operation.");e.target=e.source_target=target.symbol;return e.type=call_arguments(e,target,0);
    }
    VOID_SEMANTIC_HANDLER Type temporal_call(Expr& e,unsigned operation) {
        if(!current)error(source,e.token,Code::constant_call,"Temporal Runtime operations require runtime evaluation.");
        const auto& contract=temporal::operations.at(operation);Function target;target.token=e.token;
        Type result=contract.result_words==0?Type(Type::string):Type(Type::i64);
        if(contract.result_words==3)result=Type(unit.structure_names.at("_clock_sample")->definition);
        target.result=unit.make_result(result,Type::string);target.symbol="$time."+std::to_string(operation);
        for(unsigned n=0;n<contract.arguments.size();++n)target.parameters.push_back({Token{TokenKind::name,"arg"+std::to_string(n),e.token.offset},contract.arguments[n]=='t'?Type(Type::string):Type(Type::i64)});
        builtin_record(target,"Validated temporal Runtime kernel operation.");e.source_target=target.symbol;e.target=target.symbol;
        return e.type=call_arguments(e,target,0);
    }
#include "filesystem.inc"
#include "url.inc"
#include "http.inc"
#include "os.inc"
#include "host.inc"
#include "binary_codec.inc"
#include "stdlib_conversion.inc"
#include "math.inc"

    VOID_SEMANTIC_HANDLER Type text_case_call(Expr& e) {
        Function target;
        target.token = e.token;
        target.symbol = "$text.case";
        target.result = Type::string;
        target.parameters.push_back({Token{TokenKind::name, "value", e.token.offset}, Type::string});
        target.parameters.push_back({Token{TokenKind::name, "upper", e.token.offset}, Type::boolean});
        e.target = target.symbol;
        return e.type = call_arguments(e, target, 0);
    }

    // Builtin dispatch returns before ordinary argument recursion, keeping
    // unrelated adapter temporaries out of every nested user-call frame.
    VOID_SEMANTIC_HANDLER std::optional<Type> builtin_call_expression(Expr& e) {
        if (e.target == "$collections.version" || e.target == "$collections.check") return e.type;
        if (unit.identity == "/@stdlib/collections.void" && e.qualifier.empty() &&
            (e.token.text == "_collections_version" || e.token.text == "_collections_check")) {
            const bool check = e.token.text == "_collections_check";
            if (!current || !e.type_arguments.empty() || e.parts.size() != (check ? 2u : 1u))
                error(source, e.token, Code::argument_count, "Private collection guard has invalid arguments.");
            const auto owner = expression(*e.parts[0]);
            if (owner.kind != Type::growable_array)
                error(source, e.token, Code::type_mismatch, "Private collection guard requires a growable list.");
            Function target;
            target.token = e.token;
            target.symbol = check ? "$collections.check" : "$collections.version";
            target.result = check ? Type::none : Type::u64;
            target.parameters.push_back({Token{TokenKind::name, "items", e.token.offset}, ir::readonly_view(owner)});
            if (check) target.parameters.push_back({Token{TokenKind::name, "version", e.token.offset}, Type::u64});
            e.target = target.symbol;
            return e.type = call_arguments(e, target, 0);
        }
        if(unit.identity=="/@stdlib/json.void" &&
           (e.token.text=="_json_to_value" || e.token.text=="_json_from_value") && math_private_selected(e))
            return json_call(e);
        if (e.target.starts_with("$math.")) return e.type;
        if (unit.identity == math::source_module && e.qualifier.empty()) {
            if (const auto* operation = math::operation(e.token.text); operation && math_private_selected(e))
                return math_private_call(e, *operation);
        }
        if(e.qualifier.empty())for(unsigned n=0;n<host::operations.size();++n)
            if(unit.identity==host::operations[n].module && e.token.text==host::operations[n].name)return host_call(e,n);
        if (unit.identity == os::source_module && e.qualifier.empty()) {
            for (unsigned n = 0; n < os::operations.size(); ++n)
                if (e.token.text == os::operations[n].name) return os_call(e, n);
        }
        if (unit.identity == filesystem::source_module && e.qualifier.empty() && e.token.text=="_io_write_bytes_checked")return filesystem_call(e,static_cast<unsigned>(filesystem::Operation::write_bytes),false,true);
        if (unit.identity == filesystem::source_module && e.qualifier.empty()) {
            for (unsigned n = 0; n < filesystem::source_operations.size(); ++n) {
                auto name = filesystem::source_operations[n].name;
                if (e.token.text == name) return filesystem_call(e, n, false);
                if (n < filesystem::claim_operation && e.token.text == std::string(name) + "_async") {
                    return filesystem_call(e, n, true);
                }
            }
        }
        if (unit.identity == http::source_module && e.qualifier.empty()) {
            for (unsigned n = 0; n < http::source_operations.size(); ++n) {
                const auto& contract = http::source_operations[n];
                if (e.token.text == contract.name) return http_call(e, n, false);
                if (contract.asynchronous && e.token.text == std::string(contract.name) + "_async") return http_call(e, n, true);
            }
        }
        if (unit.identity == url::source_module && e.qualifier.empty()) {
            for (unsigned n = 0; n < url::source_operations.size(); ++n)
                if (e.token.text == url::source_operations[n].name) return url_call(e, n);
        }

        if (unit.identity == "/@stdlib/convert.void" && e.qualifier.empty() &&
            (e.token.text == "_convert_parse" || e.token.text == "_convert_unwrap")) {
            return conversion_call(e);
        }

        if (unit.identity == "/@stdlib/json.void" && e.token.text == "_json_integer" && math_private_selected(e)) {
            if (!current || e.type_arguments.size() != 1 || !e.parts.empty())
                error(source,e.token,Code::argument_count,"Private JSON numeric selection requires one concrete type and no value arguments.");
            const auto type=resolve_type(e.type_arguments[0],unit,root,units,e.token);
            if (!numeric::number(type))
                error(source,e.token,Code::unsupported_type,"json.as_number requires one of the ten numeric primitive types.");
            e.kind=Expr::Kind::literal;e.type=Type::boolean;e.value=e.aggregate_value.bits=numeric::integer(type);
            return e.type;
        }

        if (e.target == "$text.case" || e.target == "$text.translate") {
            return e.type;
        }

        if (unit.identity == "/@stdlib/text.void" && e.qualifier.empty() &&
            e.token.text == "_text_case") {
            return text_case_call(e);
        }

        if (unit.identity == "/@stdlib/text.void" && e.qualifier.empty() &&
            e.token.text == "_translate_scalars") {
            if (!current || !e.type_arguments.empty())
                error(source,e.token,Code::unsupported_type,"Private text translation requires a runtime call.");
            Function target;
            target.token = e.token;
            target.symbol = "$text.translate";
            target.result = unit.make_result(Type::string, Type::i32);
            target.parameters.push_back({Token{TokenKind::name, "value", e.token.offset}, Type::string});
            target.parameters.push_back({Token{TokenKind::name, "keys", e.token.offset}, ir::readonly_view(list_type(Type::i32))});
            target.parameters.push_back({Token{TokenKind::name, "replacements", e.token.offset}, ir::readonly_view(list_type(Type::string))});
            target.parameters.push_back({Token{TokenKind::name, "max_bytes", e.token.offset}, Type::i64});
            e.target = target.symbol;
            return e.type = call_arguments(e, target, 0);
        }

        if (e.target == "$memory.write" || e.target == "$temporal.result") {
            return e.type;
        }

        if (unit.identity == "/@stdlib/io/memory.void" && e.qualifier.empty() &&
            e.token.text == "_memory_write") {
            Function target;
            target.token = e.token;
            target.symbol = "$memory.write";
            target.result = Type::none;
            auto bytes = byte_list();
            target.parameters.push_back({Token{TokenKind::name, "buffer", e.token.offset}, bytes});
            target.parameters.push_back({Token{TokenKind::name, "bytes", e.token.offset}, ir::readonly_view(bytes)});
            target.parameters.push_back({Token{TokenKind::name, "position", e.token.offset}, Type::i64});
            e.target = target.symbol;
            return e.type = call_arguments(e, target, 0);
        }

        if ((unit.identity == "/@stdlib/time.void" || (current && current->display_adapter)) &&
            (e.token.text == "$temporal_result" || e.token.text == "_temporal_result")) {
            if (e.parts.size() != 1) {
                error(source, e.token, Code::argument_count, "Temporal display needs one result.");
            }
            auto value = expression(*e.parts[0]);
            if (!value.definition || !value.definition->result_type ||
                ir::field_type(value, 1) != Type::string ||
                Type::key(ir::field_type(value, 2)) != "/@stdlib/time.void::error") {
                error(source, e.token, Code::type_mismatch, "Invalid temporal display result.");
            }
            e.target = "$temporal.result";
            return e.type = unit.make_result(Type::string, Type::string);
        }

        if (e.target.starts_with("$random.") || (unit.identity == "/@stdlib/random.void" && e.qualifier.empty() &&
            (e.token.text == "_random_xor" || e.token.text == "_random_domain_max" || e.token.text == "_random_seed"))) return random_private_call(e);

        if (unit.identity == "/@stdlib/encoding/binary.void" && e.qualifier.empty() &&
            (e.token.text == "_binary_width" || e.token.text == "_binary_bits" ||
             e.token.text == "_binary_value")) {
            return binary_codec_call(e);
        }

        if (!(e.qualifier.empty() && (lookup(e.token.text) || (current && current->substitutions.contains(e.token.text))))) if (auto [owner, value] = global(e); value) {
            auto target = std::make_unique<Expr>(Expr::Kind::name, e.token);
            target->qualifier = e.qualifier; target->qualifier_token = e.qualifier_token;
            auto type = expression(*target);
            e.parts.insert(e.parts.begin(), std::move(target));
            if (type.definition && type.definition->callable) { e.kind = Expr::Kind::function_call; return function_invocation(e); }
            e.kind = Expr::Kind::method_call; e.token.source_width = e.token.text.size(); e.token.text = "call";
            return resolved_method_expression(e, type);
        }
        if(e.target=="$console.print" || (e.target.starts_with("$console.") && !e.target.empty()))return e.type;
        if(unit.identity==console::source_module && e.qualifier.empty())for(unsigned n=0;n<console::source_operations.size();++n)if(e.token.text==console::source_operations[n].name)return console_call(e,n);
        if(e.token.text=="print" && display_prelude(e))return print_call(e);
        if(e.target=="$layout.unwrap")return e.type;
        if(unit.identity=="/@stdlib/time.void" && e.qualifier.empty() && e.token.text=="_temporal_unwrap") {
            Function target;target.token=e.token;target.symbol="$layout.unwrap";target.result=Type::string;target.parameters.push_back({Token{TokenKind::name,"value",e.token.offset},unit.make_result(Type::string,Type::string)});
            builtin_record(target,"Checked temporal default display.");e.source_target=target.symbol;e.target=target.symbol;return e.type=call_arguments(e,target,0);
        }
        if(e.target.starts_with("$time.") || e.target=="$async.sleep")return e.type;
        if(unit.identity=="/@stdlib/time.void" && e.qualifier.empty() && e.token.text=="_sleep") {
            if(!current || !current->asynchronous)
                error(source,e.token,Code::unsupported_type,"Sleep requires an owning async activation.");
            Function target;
            target.token=e.token;
            target.symbol="$async.sleep";
            target.result=unit.make_task(Type::i32);
            target.parameters.push_back({Token{TokenKind::name,"seconds",e.token.offset},Type::i64});
            target.parameters.push_back({Token{TokenKind::name,"nanoseconds",e.token.offset},Type::i64});
            builtin_record(target,"Private monotonic timer admission.");
            e.target=e.source_target=target.symbol;
            return e.type=call_arguments(e,target,0);
        }
        if(unit.identity=="/@stdlib/time.void" && e.qualifier.empty())for(unsigned n=0;n<temporal::operations.size();++n)if(temporal::operations[n].name==e.token.text)return temporal_call(e,n);

        if(current && current->collection_adapter && e.token.text.starts_with("$collection_")) {
            if(!e.target.empty())return e.type;
            const auto operation=e.token.text.substr(12);std::vector<Type> args;for(auto& part:e.parts)args.push_back(expression(*part));
            if(operation=="finish")e.type=Type::string;
            else if(operation=="tag")e.type=Type::i32;
            else if(operation=="payload")e.type=ir::field_type(args.at(0),e.parts.at(1)->value);
            else if(operation=="invalid")e.type=Type::none;
            else if(operation=="append"){expect(args.at(0),Type::string,e.token);e.type=Type::none;}
            else if(operation=="version")e.type=Type::u64;
            else if(operation=="check" || operation=="leave")e.type=Type::none;
            else if(operation=="enter")e.type=Type::boolean;
            else if(operation=="entry")e.type=ir::array_element(args.at(0));
            else if(operation=="key" || operation=="value")e.type=ir::field_type(args.at(0),operation=="value"?1:0);
            else error(source,e.token,Code::invalid_value,"Invalid private collection operation.");
            e.target="$collection."+operation;return e.type;
        }
        if(e.token.text=="$display_format"){e.token.text="format";return display_call(e);}
        if(e.token.text=="$display_pattern" || e.token.text=="$display_alignment") {
            if(!e.target.empty())return e.type;
            expect(expression(*e.parts.at(0)),Type::string,e.token);e.argument_indexes={0};e.target=e.token.text=="$display_pattern"?"$layout.pattern":"$layout.alignment";return e.type=unit.make_result(Type::string,Type::string);
        }
        if(e.token.text=="$interpolation" || e.token.text=="$hole")return interpolation_call(e);
        if(e.target.starts_with("$display.") || display_prelude(e))return display_call(e);
        if(e.target.starts_with("$numeric::") || !e.argument_indexes.empty())return e.type;
        return std::nullopt;
    }
    VOID_SEMANTIC_HANDLER Type call_expression(Expr& e) {
        if(e.op=="$static_call")return e.type; // Already bound through its nominal owner.
        if (auto builtin = builtin_call_expression(e)) return *builtin;
        if(current && e.qualifier.empty() && !lookup(e.token.text) && current->substitutions.contains(e.token.text)) {
            if(!e.type_arguments.empty())error(source,e.token,Code::unsupported_type,"Type parameters are not generic constructors.");
            if(current->substitution_sources.contains(e.token.text))e.source_target=current->substitution_sources.at(e.token.text);
            const auto type=current->substitutions.at(e.token.text);
            if(type.definition && type.definition->task_result)error(source,e.token,Code::not_callable,"Inferred task handles have no source constructor; call an async function.");
            if(!type.definition || ir::nullable_type(type) || (type.kind!=Type::aggregate && type.kind!=Type::managed_ref) || type.definition->enum_backing || !type.definition->cases.empty() || type.definition->callable)error(source,e.token,Code::not_callable,"The specialized type has no source constructor.");
            auto [owner,structure]=owned_structure(type);construction(e,*owner,*structure,true,unit);return e.type;
        }
        auto [owner, structure] = structure_lookup(unit,root,units,e.qualifier.empty() ? e.token.text : e.qualifier + "." + e.token.text,e.qualifier.empty() ? e.token : e.qualifier_token);
        if (structure && !(e.qualifier.empty() && lookup(e.token.text))) {
            if(structure->definition->enum_backing || !structure->definition->cases.empty())error(source,e.token,Code::not_callable,"Construct enum values with type.variant and its payload arguments.");
            auto& concrete=concrete_structure(e,*owner,*structure);
            construction(e,*owner,concrete,current != nullptr,unit);
            return e.type;
        }
        if (auto* value = e.qualifier.empty() ? lookup(e.token.text) : nullptr) {
            if (value->type == error_type) throw Poison{};
            if(value->type.kind==Type::aggregate || value->type.kind==Type::managed_ref || value->type.kind==Type::interface_ref) {
                auto base=std::make_unique<Expr>(Expr::Kind::name,e.token);expression(*base);
                e.parts.insert(e.parts.begin(),std::move(base));e.kind=Expr::Kind::method_call;
                e.token.source_width=e.token.text.size();e.token.text="call";
                return resolved_method_expression(e,value->type);
            }
            error(source, e.token, Code::not_callable, "A local value is not callable.");
        }
        const auto& target = [&]() -> const Function& {
            auto check_arguments = [&] { for (auto& argument : e.parts) expression(*argument); };
            try { return random_arity(e,concrete_function(e,resolve(e)),0); }
            catch (const CompilationError&) { check_arguments(); throw; }
            catch (const Poison&) { check_arguments(); throw; }
        }();
        if (e.parts.empty() && e.type_arguments.empty()) if (const auto* location = os::location_adapter(target.symbol,true)) {
            const auto& declarations = units.at(std::string(os::source_module))->declarations;
            auto adapter = declarations.find(std::string(location->implementation));
            if (adapter == declarations.end() || target.visibility != "public" || target.asynchronous ||
                target.parameters.size() != 1 || target.parameters.front().type != Type::string ||
                adapter->second->visibility != "private" || adapter->second->asynchronous ||
                !adapter->second->parameters.empty() || adapter->second->result != target.result)
                error(source,e.token,Code::invalid_value,"Invalid trusted OS application-location adapter.");
            e.source_target = target.symbol;
            e.target = adapter->second->symbol;
            e.op = "$os.location";
            return e.type = call_arguments(e,*adapter->second,0);
        }
        e.target = target.symbol;
        return e.type = call_arguments(e,target,0);
    }
    VOID_SEMANTIC_HANDLER std::optional<Type> fallible_anchor(const Expr& e) {
        auto target=resolve_type(e.type,unit,root,units,e.token);
        return unit.make_option(target);
    }
    VOID_SEMANTIC_HANDLER Type fallible_conversion(Expr& e) {
        if(!e.type_arguments.empty())return e.type;
        auto destination=resolve_type(e.type,unit,root,units,e.token);
        auto& operand=*e.parts[0];
        Type from=Type::none;
        if(operand.kind==Expr::Kind::literal && !operand.literal_text.empty() && !decimal(operand)) {
            for(auto candidate:{Type::i32,Type::i64,Type::u64})try {
                operand.value=numeric::literal(operand.literal_text,candidate);
                operand.type=from=candidate;operand.literal_text.clear();break;
            } catch(const std::invalid_argument&) {}
            if(from==Type::none)error(source,operand.token,Code::integer_range,"Fallible conversion literal is outside the fixed-width integer domain.");
        } else from=expression(operand);
        if(from==error_type)return error_type;
        if(ir::nullable_type(from)) {
            auto payload=ir::nullable_payload(from);expect(payload,destination,e.token);
            e.op="$nullable_try";e.type_arguments.push_back(destination);
            return e.type=ir::with_field_type(unit.make_option(destination),1,payload);
        }
        if(from.kind==Type::numeric_enum && numeric::integer(destination)) {
            auto original=std::move(e.parts[0]);auto extraction=std::make_unique<Expr>(Expr::Kind::conversion,e.token);
            extraction->token.text="as";extraction->type=*from.definition->enum_backing;extraction->op="$enum_value";
            extraction->parts.push_back(std::move(original));e.parts[0]=std::move(extraction);from=*from.definition->enum_backing;
        }
        if(!numeric::integer(from) || !(numeric::integer(destination) || destination.kind==Type::numeric_enum))
            error(source,e.token,Code::numeric_conversion,"as? requires an integer operand and an integer or numeric enum destination.");
        e.type_arguments.push_back(destination);
        return e.type=unit.make_option(destination);
    }
    VOID_SEMANTIC_HANDLER Type conversion_expression(Expr& e) {
        if(e.token.text=="as?")return fallible_conversion(e);
        using K = Expr::Kind;
        Type target = e.type = resolve_type(e.type,unit,root,units,e.token);
        auto& operand = *e.parts[0];
        if(ir::nullable_value(target)) {
            auto from=expression(operand,target);expect(from,target,e.token);e.op="identity";return e.type=from;
        }
        std::optional<Type> analyzed;
        if(operand.kind!=K::literal || operand.literal_text.empty()) {
            auto from=expression(operand,operand.token.text=="null" && target.nullable?std::optional<Type>(target):std::nullopt);
            if(from==error_type)return error_type;
            if(ir::nullable_value(from)){auto payload=ir::nullable_payload(from);expect(payload,target,e.token);e.op="$nullable_unwrap";return e.type=payload;}
            analyzed=from;
        }
        if(target.kind==Type::numeric_enum) {
            if(operand.kind==K::literal && !operand.literal_text.empty() && !decimal(operand)) {
                Type selected=Type::none;
                for(auto candidate:{Type::i32,Type::i64,Type::u64})try {operand.value=numeric::literal(operand.literal_text,candidate);operand.type=selected=candidate;operand.literal_text.clear();break;}catch(const std::invalid_argument&){}
                if(selected==Type::none)error(source,operand.token,Code::integer_range,"Enum conversion literal is outside the integer domain.");
            }
            if(!numeric::integer(analyzed?*analyzed:expression(operand)))error(source,e.token,Code::numeric_conversion,"A numeric enum cast requires an integer source; distinct enums do not convert implicitly.");
            return target;
        }
        if (operand.kind == K::literal && !operand.literal_text.empty() && !anchor(operand)) {
            if (!numeric::number(target)) error(source,e.token,Code::numeric_conversion,"Numeric as requires numeric source and destination types.");
            if (!decimal(operand) || numeric::floating(target)) {
                try { operand.value = numeric::literal(operand.literal_text,target,true); }
                catch (const std::invalid_argument& failure) {
                    // A checked operation in an unevaluated constant branch must
                    // remain typed, without executing its range check eagerly.
                    if ((!current || suppress_zero) && numeric::integer(target)) {
                        for (auto candidate : {Type::i32,Type::i64,Type::u64}) {
                            try { operand.value = numeric::literal(operand.literal_text,candidate); operand.type = candidate; operand.literal_text.clear(); return target; }
                            catch (const std::invalid_argument&) {}
                        }
                    }
                    error(source,e.token,Code::numeric_conversion,failure.what());
                }
                operand.type = target; operand.literal_text.clear(); return target;
            }
        }
        auto from = analyzed?*analyzed:expression(operand,operand.token.text=="null" && (target.kind==Type::interface_ref || (target.definition && target.definition->callable)) && target.nullable ? std::optional<Type>(target) : std::nullopt);
        if (from == error_type) return error_type;
        if(from.definition && target.definition && from.definition->callable && target.definition->callable && Type::key(from)==Type::key(target))return e.type=target;
        if(target.kind==Type::interface_ref && from.kind!=Type::interface_ref) {
            auto converted=interface_conversion(operand,from,target);e.op="identity";return e.type=converted;
        }
        if(from.kind==Type::interface_ref && target.kind==Type::interface_ref && Type::key(from)==Type::key(target)) {
            if(!from.nullable || target.nullable) e.op="upcast";
            target.readonly=from.readonly;return e.type=target;
        }
        if (from.kind==Type::managed_ref && target.kind==Type::managed_ref && Type::key(from)==Type::key(target) && !target.nullable) {
            target.readonly=from.readonly;return e.type=target;
        }
        if (ir::subtype(from,target) && (!from.nullable || target.nullable)) {
            e.op="upcast"; target.readonly=from.readonly; return e.type=target;
        }
        if (ir::reference_value(from) || ir::reference_value(target)) error(source,e.token,Code::numeric_conversion,"Checked reference conversion requires the same nominal type or an ancestor without nullability strengthening.","Unwrap nullable references before upcasting; source downcasts and unrelated reference casts are not supported.");
        if(from.kind==Type::numeric_enum && numeric::integer(target))return e.type=target;
        if (!numeric::number(from) || !numeric::number(target)) error(source,e.token,Code::numeric_conversion,"Numeric as requires numeric source and destination types.");
        return target;
    }
    std::vector<Type> operator_generic_arguments() const {
        std::vector<Type> arguments;
        if(current)for(const auto& [name,type]:current->substitutions)arguments.push_back(type);
        return arguments;
    }
    bool operator_expression(Expr& e,const std::vector<Type>& operands) {
        if(e.implicit)return false; // Compiler-generated identity guards are not source operators.
        const auto glyph=e.op=="!="?std::string("=="):e.op;
        ir::MemberSelection selected;
        if(diagnostics.editor && diagnostics.editor->operator_context && !(current && current->enum_adapter) && diagnostics.editor->module==unit.identity && diagnostics.editor->offset==e.token.offset) {
            auto& out=diagnostics.editor->result;++diagnostics.editor->operator_occurrences;
            if(!invalid_function && !unit.failed_import)for(const auto& name:{"+","-","*","/","%","==","!=","<",">","<=",">="}) {
                auto match=ir::select_operator(*root.member_catalog,name==std::string("!=")?"==":name,operands,unit.identity,lexical_owner(),operator_generic_arguments());
                if(match.status!=ir::MemberSelection::found)continue;
                for(const auto& [id,u]:units)for(const auto& f:u->functions)if(f.symbol==match.method){auto spelling=std::string(name);if(e.token.text.size()==2 && e.token.text[1]=='=' && e.token.text!="==" && e.token.text!="!=" && e.token.text!="<=" && e.token.text!=">="){if(spelling.size()!=1 || spelling=="<" || spelling==">")continue;spelling+="=";}out.candidates.push_back({spelling,editor_signature(f),24,{},source_documentation(u->source,f.token.offset).text(f.result!=Type::none)});diagnostics.editor->candidate_targets[spelling].insert(f.symbol);++diagnostics.editor->candidate_occurrences[spelling];}
            }
            // Continue the ordinary checker so effects and capability contracts
            // are finalized before presenting these candidates.
        }
        try{selected=ir::select_operator(*root.member_catalog,glyph,operands,unit.identity,lexical_owner(),operator_generic_arguments());}
        catch(const std::invalid_argument& failure){error(source,e.token,Code::readonly,failure.what());}
        if(selected.status==ir::MemberSelection::missing)return false;
        std::vector<RelatedLocation> related;
        const Function* target=nullptr;
        for(const auto& [id,owner]:units)for(const auto& f:owner->functions) {
            if(f.symbol==selected.method)target=&f;
            for(const auto& candidate:selected.candidates)if(f.symbol==candidate.second)related.push_back({span(owner->source,f.token),"Operator declared by "+Type::key(f.operator_owner)+"."});
        }
        if(selected.status==ir::MemberSelection::ambiguous)error(source,e.token,Code::ambiguous,"Ambiguous exact operator '"+e.op+"'.",{},std::move(related));
        if(selected.status==ir::MemberSelection::inaccessible)error(source,e.token,Code::visibility,"Exact operator '"+e.op+"' is not accessible; import its direct declaring owner and respect its visibility.",{},std::move(related));
        if(!target)throw std::logic_error("Missing selected operator function");
        if(!current)error(source,e.token,Code::constant_call,"A compile-time constant cannot call a custom operator.");
        const bool negate=e.op=="!=";
        e.kind=Expr::Kind::call;e.op="$operator";e.target=target->symbol;e.type=target->result;
        for(unsigned n=0;n<operands.size();++n)e.argument_indexes.push_back(n);
        if(negate){auto call=std::make_unique<Expr>(std::move(e));e=Expr(Expr::Kind::unary,call->token);e.op="not";e.type=Type::boolean;e.parts.push_back(std::move(call));}
        return true;
    }
#include "enum_equality.inc"
#include "collection_equality.inc"
    void assignment_value(Statement& s,Type type) {
        if(s.op=="<<=" || s.op==">>=") {
            auto count=expression(*s.expression);
            if(type!=error_type && count!=error_type && (!numeric::integer(type) || !numeric::integer(count)))
                error(source,s.compound_token,Code::numeric_operator,"Shifts require integer values and integer counts; enums are not shift operands.");
            return;
        }
        if((s.op=="&=" || s.op=="|=" || s.op=="^=") && type!=error_type && !ir::bitwise_type(type))
            error(source,s.compound_token,Code::numeric_operator,"Bitwise assignment requires an integer or flags enum.");
        if(s.op!="=" && s.op.size()==2 && s.op[1]=='=') {
            std::optional<Type> context=numeric::number(type)?std::optional<Type>(type):std::nullopt;
            if(s.expression->token.text=="null" && (ir::reference_value(type) || (type.definition && type.definition->callable))){context=type;context->nullable=true;}
            auto right=expression(*s.expression,context);
            Expr operation(Expr::Kind::binary,s.compound_token);operation.op=s.op.substr(0,1);
            if(right!=error_type && operator_expression(operation,{type,right})) {
                s.compound_target=operation.target;
                if(operation.type!=type) {
                    auto value=std::make_unique<Expr>(Expr::Kind::name,s.compound_token);value->type=operation.type;
                    value->slot=s.compound_slot=static_cast<unsigned>(current->locals.size());current->locals.push_back(operation.type);
                    expect(contextual_conversion(*value,operation.type,type),type,s.compound_token);s.compound_conversion=std::move(value);
                }
                return;
            }
            // Only builtin fallback supplies ordinary assignment conversions.
            expect(contextual_conversion(*s.expression,right,type),type,s.expression->token);
        }else expect(expression(*s.expression,type),type,s.expression->token);
        if(s.op!="=" && type!=error_type && !numeric::number(type) &&
           !((s.op=="&=" || s.op=="|=" || s.op=="^=") && ir::bitwise_type(type)) && !(type==Type::string && s.op=="+="))
            error(source,s.compound_token,Code::type_mismatch,"Unsupported compound assignment operand types.");
        if(s.op=="%=" && numeric::floating(type))error(source,s.compound_token,Code::numeric_operator,"Illegal compound floating remainder operator.");
        if(numeric::integer(type) && (s.op=="/=" || s.op=="%=") && s.expression->kind==Expr::Kind::literal && s.expression->value==0)
            error(source,s.expression->token,Code::zero_divisor,"Division or remainder by zero.");
    }
    VOID_SEMANTIC_HANDLER Type unary_expression(Expr& e, const std::optional<Type>& expected) {
        auto left = expression(*e.parts[0], e.op == "not" ? std::optional<Type>(Type::boolean) : expected);
        if (left == error_type) return error_type;
        if(e.op=="~") {
            if(!ir::bitwise_type(left))error(source,e.token,Code::numeric_operator,"Bitwise complement requires an integer or flags enum.");
            return e.type=left;
        }
        if(e.op!="not" && operator_expression(e,{left}))return e.type;
        if (e.op == "not") expect(left,Type::boolean,e.token);
        else if (!numeric::number(left)) error(source,e.token,Code::numeric_operator,"Type mismatch: unary arithmetic requires a numeric operand.");
        return e.type = left;
    }
    VOID_SEMANTIC_HANDLER Type binary_expression(Expr& e, const std::optional<Type>& expected) {
        using K = Expr::Kind;
        // Overload/display binding may revisit an already checked null test.
        if(e.op=="$nullable_null_equal" || e.op=="$nullable_null_unequal")return e.type;
        if(e.op=="<<" || e.op==">>") {
            auto left=expression(*e.parts[0],expected);
            auto count=expression(*e.parts[1]);
            if(left==error_type || count==error_type)return error_type;
            if(!numeric::integer(left) || !numeric::integer(count))
                error(source,e.token,Code::numeric_operator,"Shifts require integer values and integer counts; enums are not shift operands.");
            return e.type=left;
        }
        bool logical = e.op == "and" || e.op == "or";
        auto left_hint = anchor(*e.parts[0]), right_hint = anchor(*e.parts[1]);
        // Result context cannot choose a custom signature. Numeric anchors and
        // ordinary defaults type operands before selection; nominal conversions
        // are reserved for builtin fallback after exact custom lookup fails.
        auto context=logical?std::optional<Type>(Type::boolean):left_hint && numeric::number(*left_hint)?left_hint:right_hint && numeric::number(*right_hint)?right_hint:!left_hint && !right_hint && expected && numeric::number(*expected)?expected:std::optional<Type>(decimal(e)?Type::f32:Type::i32);
        if(left_hint && ir::nullable_value(*left_hint) && numeric::number(ir::nullable_payload(*left_hint)))context=ir::nullable_payload(*left_hint);
        else if(right_hint && ir::nullable_value(*right_hint) && numeric::number(ir::nullable_payload(*right_hint)))context=ir::nullable_payload(*right_hint);
        auto null_context=[&](const Expr& part,const std::optional<Type>& opposite)->std::optional<Type>{
            if((e.op=="==" || e.op=="!=") && (part.kind==K::array_literal || part.kind==K::dictionary_literal)) {
                if(opposite){auto type=ir::nullable_type(*opposite)?ir::nullable_payload(*opposite):*opposite;if(ir::collection_type(type))return type;}
                return std::nullopt;
            }
            if(part.token.text=="null" && opposite && (ir::nullable_type(*opposite) || ir::reference_value(*opposite) || (opposite->definition && opposite->definition->callable))){auto type=*opposite;if(!ir::nullable_value(type))type.nullable=true;return type;}return context;};
        auto left=expression(*e.parts[0],null_context(*e.parts[0],right_hint));
        bool previous=suppress_zero;
        if(e.parts[0]->kind==K::literal && ((e.op=="and" && !e.parts[0]->value) || (e.op=="or" && e.parts[0]->value)))suppress_zero=true;
        auto right=expression(*e.parts[1],null_context(*e.parts[1],left==error_type?right_hint:std::optional<Type>(left)));suppress_zero=previous;
        if(left==error_type || right==error_type)return error_type;
        if(logical){expect(left,Type::boolean,e.parts[0]->token);expect(right,Type::boolean,e.parts[1]->token);return e.type=Type::boolean;}
        if(e.op=="&" || e.op=="|" || e.op=="^") {
            expect(right,left,e.token);
            if(!ir::bitwise_type(left))error(source,e.token,Code::numeric_operator,"Bitwise operators require equal integer types or the same flags enum.");
            return e.type=left;
        }
        if(!left_hint && !right_hint && expected && numeric::number(*expected) && ((left.definition && (left.kind==Type::aggregate || left.kind==Type::managed_ref)) || (right.definition && (right.kind==Type::aggregate || right.kind==Type::managed_ref)))) {
            auto defaults=std::optional<Type>(decimal(e)?Type::f32:Type::i32);
            if(numeric::number(left))left=expression(*e.parts[0],defaults);
            if(numeric::number(right))right=expression(*e.parts[1],defaults);
            if(left==error_type || right==error_type)return error_type;
        }
        if(operator_expression(e,{left,right}))return e.type;
        if(ir::nullable_value(left) || ir::nullable_value(right)) {
            if(e.op!="==" && e.op!="!=")error(source,e.token,Code::numeric_operator,"Unwrap nullable values before applying this operator.");
            auto nullable=ir::nullable_value(left)?left:right;
            left=contextual_conversion(*e.parts[0],left,nullable);right=contextual_conversion(*e.parts[1],right,nullable);expect(right,left,e.token);
            if((e.parts[0]->kind==K::sum && e.parts[0]->slot==0) || (e.parts[1]->kind==K::sum && e.parts[1]->slot==0)) {
                e.op=e.op=="=="?"$nullable_null_equal":"$nullable_null_unequal";return e.type=Type::boolean;
            }
            return enum_equality(e,left);
        }
        if((e.op=="==" || e.op=="!=") && (ir::collection_type(left) || ir::collection_type(right)) &&
           (left.kind!=right.kind || Type::key(left)!=Type::key(right)))
            error(source,e.token,Code::type_mismatch,"Collection equality requires matching collection and element types.");
        auto builtin_context=left_hint?left_hint:right_hint?right_hint:expected && numeric::number(*expected)?expected:context;
        if(builtin_context && (ir::reference_value(*builtin_context) || (builtin_context->definition && builtin_context->definition->callable)) && (e.op=="==" || e.op=="!="))builtin_context->nullable=true;
        left=contextual_conversion(*e.parts[0],left,builtin_context);right=contextual_conversion(*e.parts[1],right,builtin_context);
        if (left==Type::string || right==Type::string) {
            expect(right,left,e.token);
            if (left!=Type::string || (e.op!="+" && e.op!="==" && e.op!="!=")) error(source,e.token,Code::numeric_operator,"Strings support only +, == and !=.");
            return e.type=e.op=="+" ? Type::string : Type::boolean;
        }
        if (e.op != "==" && e.op != "!=" && !numeric::number(left)) expect(left,Type::i32,e.parts[0]->token);
        if ((e.op=="==" || e.op=="!=") && left.kind==Type::managed_ref && right.kind==Type::managed_ref) {
            auto normalize=[&](std::unique_ptr<Expr>& operand,Type target) {
                auto original=std::move(operand); target.nullable=original->type.nullable; target.readonly=original->type.readonly;
                operand=std::make_unique<Expr>(K::conversion,original->token); operand->op="upcast"; operand->type=target; operand->parts.push_back(std::move(original));
            };
            if (Type::key(left)!=Type::key(right)) {
                if (ir::subtype(right,left)) { normalize(e.parts[1],left); right=e.parts[1]->type; }
                else if (ir::subtype(left,right)) { normalize(e.parts[0],right); left=e.parts[0]->type; }
                else error(source,e.token,Code::type_mismatch,"Reference equality requires related nominal types or explicit common-base views.");
            }
        } else if(!(left.definition && right.definition && left.definition->callable && right.definition->callable && Type::key(left)==Type::key(right)))expect(right,left,e.token);
        if (e.op == "==" || e.op == "!=") {
            if(left.definition && right.definition && left.definition->callable && right.definition->callable && Type::key(left)==Type::key(right))return e.type=Type::boolean;
            if(ir::collection_type(left)) {
                auto result=collection_equality(e,left);
                if(diagnostics.editor && diagnostics.editor->operator_context && !(current && current->enum_adapter) && !invalid_function && !unit.failed_import && diagnostics.editor->module==unit.identity && diagnostics.editor->offset==e.token.offset)
                    for(const auto& name:{"==","!="}) {
                        diagnostics.editor->result.candidates.push_back({name,"Collection content equality -> bool",24});
                        diagnostics.editor->candidate_targets[name].insert("$collection_equal");++diagnostics.editor->candidate_occurrences[name];
                    }
                return result;
            }
            if (left.kind == Type::aggregate) {
                const auto& definition = *left.definition;
                if (!definition.cases.empty() && !definition.result_type && !definition.option_type) {
                    return enum_equality(e,left);
                }
                error(source,e.token,Code::numeric_operator,"Value struct equality is not implemented.");
            }
            if (left == Type::none) error(source,e.token,Code::invalid_value,"Cannot compare a no-result call.");
            return e.type = Type::boolean;
        }
        if (!numeric::number(left)) expect(left,Type::i32,e.parts[0]->token);
        if (!numeric::supports(operation(e.op),left)) error(source,e.token,Code::numeric_operator,"Illegal numeric operator % for " + std::string(ir::type_name(left)) + ".");
        if (current && !suppress_zero && numeric::integer(left) && (e.op == "/" || e.op == "%") && e.parts[1]->kind == K::literal && e.parts[1]->value == 0)
            error(source,e.parts[1]->token,Code::zero_divisor,"Division or remainder by zero.","Use a nonzero divisor.");
        return e.type = (e.op == "<" || e.op == "<=" || e.op == ">" || e.op == ">=") ? Type::boolean : left;
    }
    VOID_SEMANTIC_HANDLER void variable_statement(Statement& s) {
            if (s.invalid) { s.slot = declare(s.token, error_type); invalid_function = true; return; }
            if (s.annotation) s.annotation = resolve_type(*s.annotation,unit,root,units,s.token);
            Type t = s.expression ? expression(*s.expression, s.annotation) : s.annotation.value_or(Type::none);
            if (t == Type::none) {
                s.slot = declare(s.token, error_type, s.readonly);
                error(source, s.token, Code::invalid_value, "Variable requires a value type.", "Supply a numeric/bool annotation or a value-producing initializer.");
            }
            if (s.annotation) {
                try { expect(t, *s.annotation, s.expression ? s.expression->token : s.token); if (t!=error_type) t=*s.annotation; }
                catch (const CompilationError& failure) { diagnostics.add(failure); t = error_type; }
            }
            if(!s.expression && ir::nullable_value(t)){s.expression=std::make_unique<Expr>(Expr::Kind::sum,s.token);s.expression->type=t;s.expression->slot=0;}
            if(!s.expression && t.definition && t.definition->callable) {
                if(!t.nullable)error(source,s.token,Code::initializer,"A non-nullable function value requires an initializer.");
                s.expression=std::make_unique<Expr>(Expr::Kind::function_null,s.token);s.expression->type=t;
            }
            if (!s.expression && ir::reference_value(t) && !t.nullable) error(source,s.token,Code::initializer,"A non-nullable reference requires an initializer.","Construct the object explicitly using its type.");
            if (!s.expression && ir::collection_type(t)) {
                if(needs_array_recipe(t))s.expression=array_recipe(t,s.token,true,unit);
                else {s.expression=std::make_unique<Expr>(Expr::Kind::literal,s.token);s.expression->type=t;s.expression->aggregate_value=array_default(t,s.token);}
            }
            if(!s.expression && t.kind==Type::numeric_enum)error(source,s.token,Code::initializer,"Numeric enum values require an explicit initializer.");
            if(!s.expression && t.definition && !t.definition->cases.empty())error(source,s.token,Code::initializer,"Enum/result values require an explicit variant initializer.");
            if (!s.expression && t.kind == Type::aggregate) {
                auto [owner,structure] = owned_structure(t);
                s.expression = std::make_unique<Expr>(Expr::Kind::construction,s.token);
                construction(*s.expression,*owner,*structure,true,unit);
                if(s.expression->constructed_type!=Type::none)error(source,s.token,Code::initializer,"Fallible initialization requires an explicit constructor expression and result handling.");

            }
            if (!ir::reference_value(t) && t.kind!=Type::growable_array && t.kind!=Type::dictionary) t.readonly=false;
            s.slot = declare(s.token, t, s.readonly); return;
    }
    VOID_SEMANTIC_HANDLER void destructure_statement(Statement& s) {
        auto result=expression(*s.expression,{},true);
        if(result==error_type)return;
        if(!ir::multiple_return_type(result))
            error(source,s.expression->token,Code::invalid_value,"Destructuring requires a multiple-return function call.");
        const auto count=result.definition->fields.size();
        if(s.bindings.size()!=count) {
            const auto& site=s.bindings.size()>count?s.bindings[count].token:s.expression->token;
            error(source,site,Code::argument_count,"Destructuring requires "+std::to_string(count)+
                " bindings, received "+std::to_string(s.bindings.size())+".");
        }
        std::map<std::string,Token> names;
        for(const auto& binding:s.bindings) {
            auto [previous,added]=names.emplace(binding.token.text,binding.token);
            if(!added)
                error(source,binding.token,Code::duplicate,"Duplicate destructuring binding '"+binding.token.text+"'.",{},
                      {{span(source,previous->second),"Previously declared"}});
            check_local_name(binding.token,false);
        }
        for(unsigned n=0;n<count;++n)
            s.bindings[n].slot=declare(s.bindings[n].token,ir::field_type(result,n),s.readonly);
    }
    VOID_SEMANTIC_HANDLER void return_statement(Statement& s) {
        if(current->deferred)error(source,s.token,Code::invalid_value,"Deferred code cannot return directly.");
        const bool packed=s.expression && s.expression->kind==Expr::Kind::return_pack;
        if(packed || !current->returns.empty()) {
            const auto count=current->returns.empty()?std::size_t(current->result!=Type::none):current->returns.size();
            const auto received=packed?s.expression->parts.size():std::size_t(bool(s.expression));
            const bool first_enum_field=!packed && received==1 && count>1 && s.expression->kind==Expr::Kind::field &&
                current->returns.front().type.definition && !current->returns.front().type.definition->result_type &&
                !current->returns.front().type.definition->option_type &&
                (!current->returns.front().type.definition->cases.empty() || current->returns.front().type.definition->enum_backing);
            if(first_enum_field && (editor_cursor(*s.expression) || enum_recovery_available())) {
                const auto& expected=current->returns.front().type;
                if(enum_expression(*s.expression,false,&expected)) {
                    if(s.expression->kind!=Expr::Kind::editor_hole)diagnostics.recovery->record();
                    return;
                }
            }
            if(received!=count) {
                Diagnostic diagnostic(Code::argument_count);
                diagnostic.primary=span(source,s.token);
                diagnostic.description="Return requires "+std::to_string(count)+
                    (count==1?" value, received ":" values, received ")+std::to_string(received)+".";
                diagnostic.editor_enum_recovery=first_enum_field;
                throw CompilationError({std::move(diagnostic)});
            }
            s.expression->type=current->result;
            for(unsigned n=0;n<count;++n) {
                auto& part=*s.expression->parts[n];
                const auto expected=current->returns[n].type;
                try { expect(expression(part,expected),expected,part.token); }
                catch(const CompilationError& failure) { diagnostics.add(failure);invalid_function=true; }
            }
            return;
        }
        if(s.expression) {
            auto result=expression(*s.expression,current->result);
            if(result==error_type)return;
            expect(result,current->result,s.token);
            // A no-result call is a statement, not a return value.
            if(current->result==Type::none)
                error(source,s.token,Code::invalid_value,"A no-result function cannot return an expression.",
                      "Call the function, then use an empty return.");
        } else expect(Type::none,current->result,s.token);
    }
    unsigned sequence(std::vector<Statement>& body, bool nested = true) {
        if (nested) scopes.emplace_back();
        unsigned flow = next_flow;
        for (auto& s : body) {
            struct RestoreStatement {
                Statement*& current;
                Statement* previous;
                ~RestoreStatement(){current=previous;}
            } restore_statement{editor_statement,editor_statement};
            editor_statement=&s;
            if (diagnostics.full()) break;
            unsigned result = next_flow;
            auto old_zero = suppress_zero;
            try { result = statement(s); s.editor_checked = true; }
            catch (const CompilationError& failure) { diagnostics.add(failure); }
            catch (const Poison&) {}
            suppress_zero = old_zero;
            // An invalid return remains a return for flow recovery, never valid IR.
            if (s.kind == Statement::Kind::returned) result = return_flow;
            if (flow & next_flow) flow = (flow & ~next_flow) | result;
        }
        if (nested) scopes.pop_back();
        return flow;
    }
    unsigned statement(Statement& s) {
        // Deferred lambdas recursively analyze their bodies. Other statement
        // cases must not reserve their temporaries at every deferred level.
        if(s.kind==Statement::Kind::deferred) {
            if(!current)error(source,s.token,Code::invalid_value,"defer requires a function activation.");
            if(s.expression->kind==Expr::Kind::lambda)expression(*s.expression);else deferred_call(s);
            current->cleanup_type=unit.make_cleanup();
            return s.flow = next_flow;
        }
        // Expanded loops add scopes; retain only their own handler frames so
        // recursive checking still fits the supported 1 MiB native stack.
        if(s.kind==Statement::Kind::loop)return loop_statement(s);
        if(s.kind==Statement::Kind::scoped)return scoped_statement(s);
        if(s.kind==Statement::Kind::conditional)return conditional_statement(s);
        return ordinary_statement(s);
    }
    #include "iteration.inc"
    VOID_SEMANTIC_HANDLER unsigned ordinary_statement(Statement& s) {
        using K = Statement::Kind;
        unsigned flow = next_flow;
        switch (s.kind) {
        case K::no_op: break;
        case K::variable: variable_statement(s); break;
        case K::destructure: destructure_statement(s); break;
        case K::deferred: case K::loop: case K::scoped: case K::conditional:
            break; // Dispatched before reserving the ordinary statement frame.
        case K::assignment: {
            if (diagnostics.editor && diagnostics.editor->module == unit.identity) {
                auto cursor = [&](const Token& token) { return token.text == "_void_editor_cursor_" && token.offset == diagnostics.editor->offset; };
                if (cursor(s.token) || std::any_of(s.fields.begin(), s.fields.end(), cursor)) {
                    auto target = std::make_unique<Expr>(Expr::Kind::name, s.token);
                    for (const auto& field : s.fields) { auto child = std::make_unique<Expr>(Expr::Kind::field, field); child->parts.push_back(std::move(target)); target = std::move(child); }
                    expression(*target); // Use the ordinary binding/member completion path on LHS too.
                }
            }
            auto* variable = lookup(s.name);
            bool global_selection = false;
            if (!variable) {
                Expr probe(Expr::Kind::name, s.token);
                if (!s.fields.empty() && unit.qualifiers.contains(s.name)) {
                    probe.qualifier = s.name; probe.qualifier_token = s.token; probe.token = s.fields.front();
                }
                const bool static_selection=!s.fields.empty() && structure_lookup(unit,root,units,unit.qualifiers.contains(s.name)?s.name+"."+s.fields.front().text:s.name,s.token).second;
                if (global(probe).second || static_selection || s.selection) {
                    if (!s.selection) {
                        auto target = std::make_unique<Expr>(Expr::Kind::name, s.token);
                        for (const auto& field : s.fields) { auto child = std::make_unique<Expr>(Expr::Kind::field, field); child->parts.push_back(std::move(target)); target = std::move(child); }
                        if (s.index) { auto child = std::make_unique<Expr>(Expr::Kind::array_index, s.index->token); child->parts.push_back(std::move(target)); child->parts.push_back(std::move(s.index)); target = std::move(child); }
                        s.selection = std::move(target); s.fields.clear();
                    }
                    global_selection = true;
                }
            }
            if ((variable && variable->readonly) || (!variable && !global_selection && visible_name(s.name)))
                error(source, s.token, Code::readonly, "Cannot mutate readonly const or declaration '" + s.name + "'.", "Use var for a mutable local binding.");
            if (!variable && !global_selection) error(source, s.token, Code::unknown_identifier, "Unknown identifier '" + s.name + "'.", "Declare it with var before assignment.");
            auto t = variable ? variable->type : Type(Type::none); if (variable) s.slot = variable->slot;
            if(!s.selection && !s.fields.empty()) {
                auto probe=t;
                for(const auto& field:s.fields) {
                    auto selected=member(probe,field);
                    if(selected.function) {
                        if(selected.function->accessor==1) {
                            auto target=std::make_unique<Expr>(Expr::Kind::name,s.token);
                            for(const auto& name:s.fields) {auto child=std::make_unique<Expr>(Expr::Kind::field,name);child->parts.push_back(std::move(target));target=std::move(child);}
                            if(s.index) {auto child=std::make_unique<Expr>(Expr::Kind::array_index,s.index->token);child->parts.push_back(std::move(target));child->parts.push_back(std::move(s.index));target=std::move(child);}
                            s.selection=std::move(target);s.fields.clear();
                        }
                        break;
                    }
                    for(auto index:selected.path)probe=ir::field_type(probe,index);
                }
            }
            if(s.selection) {
                auto previous=allow_event;allow_event=true;t=expression(*s.selection);allow_event=previous;
                std::function<void(const Expr&,bool)> writable=[&](const Expr& e,bool writeback){
                    if(e.kind==Expr::Kind::global && writeback)for(const auto& [id,owner]:units)for(const auto& value:owner->globals)if(value.identity==e.target && value.readonly)error(source,e.token,Code::readonly,"Cannot mutate a static const field.");
                    if(e.kind==Expr::Kind::field && !e.property && (e.parts[0]->type.kind==Type::aggregate || e.parts[0]->type.kind==Type::managed_ref) && e.parts[0]->type.definition && e.parts[0]->type.definition->fields.at(e.slot).readonly)
                        const_field_error(e.parts[0]->type,e.slot,e.token);
                    if(e.property && writeback && e.setter.empty())error(source,e.token,Code::readonly,"Property has no setter.");
                    if(e.kind==Expr::Kind::field || e.kind==Expr::Kind::array_index || e.property) {
                        auto owner=e.parts[0]->type;
                        writable(*e.parts[0],writeback && (owner.kind==Type::aggregate || owner.kind==Type::fixed_array));
                    }
                };writable(*s.selection,true);
                if(s.selection->target.starts_with("$text."))error(source,s.selection->token,Code::readonly,"String contents and inspection properties are immutable.");
                if(s.selection->target=="$array.count" || s.selection->target=="$dict.count")error(source,s.selection->token,Code::readonly,"Array count is readonly.");
                s.event=s.selection->event;s.assigned_type=t;
                if(s.event){if(s.op!="+=" && s.op!="-=")error(source,s.token,Code::invalid_value,"Events support only += and -= subscription changes.");auto signature=ir::array_element(t);expect(expression(*s.expression,signature),signature,s.expression->token);break;}
                assignment_value(s,t);
                break;
            }
            std::vector<Token> expanded;
            for (const auto& field : s.fields) {
                auto selected=member(t,field);
                if (selected.function) error(source,field,Code::invalid_value,"Cannot assign to method '"+field.text+"'.");
                for (unsigned n=0;n<selected.path.size();++n) {
                    auto index=selected.path[n]; auto token=field;
                    const auto& declaration=t.definition->fields[index];
                    bool grant=current && current->initializer && current->receiver && variable->slot==current->receiver->slot &&
                        s.field_indexes.empty() && selected.path.size()==1 && s.fields.size()==1 && !s.index && declaration.owner==Type::key(current->receiver->type);
                    if(declaration.readonly && !grant)const_field_error(t,index,field);
                    token.text=t.definition->fields[index].name;
                    expanded.push_back(token); s.implicit_fields.push_back(n+1<selected.path.size());
                    s.event=t.definition->fields[index].event;s.field_indexes.push_back(index); t=ir::field_type(t,index);
                }
            }
            s.fields=std::move(expanded);
            if(s.index) {
                if(!ir::collection_type(t)) error(source,s.token,Code::unsupported_type,"Selected assignment requires an array or dictionary.");
                auto index=expression(*s.index,t.kind==Type::dictionary?std::optional(ir::dictionary_key(t)):std::nullopt);
                if(t.kind==Type::dictionary){expect(index,ir::dictionary_key(t),s.index->token);t=ir::dictionary_value(t);}
                else {if(index!=error_type && !numeric::integer(index))error(source,s.index->token,Code::type_mismatch,"Array index requires an integer.");t=ir::array_element(t);}
            }
            s.assigned_type = t;
            if(s.event){if((s.op!="+=" && s.op!="-=") || s.index)error(source,s.token,Code::invalid_value,"Events support only += and -= subscription changes.");auto signature=ir::array_element(t);expect(expression(*s.expression,signature),signature,s.expression->token);break;}
            assignment_value(s,t);
            break;
        }
        case K::expression:
        {
            const bool member_selection=s.expression->kind==Expr::Kind::field;
            if (expression(*s.expression) == error_type) break;
            if (s.expression->kind != Expr::Kind::spawned && s.expression->kind != Expr::Kind::task_cancel && s.expression->kind != Expr::Kind::awaited &&
                s.expression->kind != Expr::Kind::call && s.expression->kind != Expr::Kind::method_call && s.expression->kind!=Expr::Kind::function_call) {
                if(member_selection && member_recovery_available()){diagnostics.recovery->record();break;}
                Diagnostic diagnostic(Code::invalid_value);
                diagnostic.primary=span(source,s.token);
                diagnostic.description="Only function calls can be standalone expressions.";
                diagnostic.hint="Assign this value with var or return it.";
                diagnostic.editor_member_recovery=member_selection;
                throw CompilationError({std::move(diagnostic)});
            }
            break;
        }
        case K::returned:
            return_statement(s);
            flow = return_flow; break;
        case K::match: {
            auto subject=expression(*s.expression);flow=0;
            for(auto& arm:s.body){scopes.emplace_back();check_pattern(*arm.pattern,subject);flow|=sequence(arm.body);scopes.pop_back();}
            s.exhaustive=warn_match(s.token,subject,[&](auto visit){for(const auto& arm:s.body)visit(*arm.pattern);});
            if(!s.exhaustive)flow|=next_flow;break;
        }
        case K::broken: case K::continued:
            if (!loops) error(source, s.token, Code::loop_control, s.token.text + " is only valid inside a for loop.");
            flow = s.kind == K::broken ? break_flow : continue_flow; break;
        }
        return s.flow = flow;
    }
    VOID_SEMANTIC_HANDLER unsigned scoped_statement(Statement& s) {
        unsigned flow = next_flow;

        auto& call=*s.expression;
        if(call.kind==Expr::Kind::method_call && call.token.text=="group" && call.parts.size()==1 && call.parts[0]->kind==Expr::Kind::name && call.parts[0]->token.text=="tasks" && !lookup("tasks") && !visible_name("tasks")){
            if(!current || !current->asynchronous || current->deferred || !call.type_arguments.empty())error(source,s.token,Code::unsupported_type,"tasks.group() requires an async scope.");
            call.kind=Expr::Kind::literal;call.type=Type::u64;call.parts.clear();s.task_group=true;scopes.emplace_back();s.slot=declare(s.binder,Type::u64,true);lookup(s.binder.text)->task_group=true;
            flow=sequence(s.body,false);scopes.pop_back();return s.flow=flow;
        }
        auto type=expression(*s.expression);if(type.kind!=Type::managed_ref || type.nullable)error(source,s.token,Code::type_mismatch,"with requires a non-nullable managed resource with deinit.");
        scopes.emplace_back();s.slot=declare(s.binder,type,false);
        auto cell=unit.make_capture_record(current->symbol+"::$cell$"+std::to_string(s.slot),type,true);auto initial=type;initial.nullable=true;std::const_pointer_cast<ir::ValueType>(cell.definition)->fields[0].type=initial;current->captured_locals.emplace(s.slot,cell);
        Function lambda;lambda.anonymous=true;lambda.token=s.token;lambda.token.text="$with$"+std::to_string(current->lambdas.size());lambda.visibility="private";
        Statement invoke(Statement::Kind::expression,s.token);invoke.expression=std::make_unique<Expr>(Expr::Kind::method_call,Token{TokenKind::name,"deinit",s.token.offset});invoke.expression->parts.push_back(std::make_unique<Expr>(Expr::Kind::name,s.binder));lambda.body.push_back(std::move(invoke));
        s.selection=std::make_unique<Expr>(Expr::Kind::lambda,s.token);s.selection->slot=current->lambdas.size();current->lambdas.push_back(std::move(lambda));expression(*s.selection);
        for(const auto& selected:unit.functions)if(selected.symbol==s.selection->target && selected.body.size()==1 && selected.body[0].expression){
            const auto& dispose=*selected.body[0].expression;
            if(dispose.kind==Expr::Kind::method_call && dispose.token.text=="deinit" && dispose.type==Type::none && dispose.virtual_slot.empty() && !dispose.interface_call && dispose.native_symbol.empty() && dispose.parts.size()==1 && dispose.parts[0]->type==current->locals[s.slot])s.cleanup_target=dispose.target;
        }
        current->cleanup_type=unit.make_cleanup();flow=sequence(s.body,false);scopes.pop_back();return s.flow = flow;
    }

    VOID_SEMANTIC_HANDLER unsigned conditional_statement(Statement& s) {
        unsigned flow = next_flow;

        scopes.emplace_back();
        try {
            if(s.selection)expression(*s.selection); // Authored receiver of an editor-only member hole.
            condition(*s.expression);
        }
        catch (const CompilationError& failure) { diagnostics.add(failure); }
        pattern_bindings=false;
        auto yes=sequence(s.body);scopes.pop_back();
        flow=yes | (s.otherwise.empty()?next_flow:sequence(s.otherwise));return s.flow = flow;
    }

    VOID_SEMANTIC_HANDLER unsigned loop_statement(Statement& s) {
        unsigned flow = next_flow;

        if(s.asynchronous_iteration && (!current || !current->asynchronous || current->deferred))
            error(source,s.token,Code::unsupported_iteration,"for await requires an async function outside deferred cleanup.");
        if(!s.binder.text.empty()) {
            // Editor recovery may retain an unfinished header. Its binder
            // slots are valid only after the iterable has been checked.
            s.invalid=true;
            Type item;
            if(s.range_end) {
                if(s.asynchronous_iteration)error(source,s.token,Code::unsupported_iteration,"for await requires a custom async iterable, not a range.");
                if(!s.index_binder.text.empty())error(source,s.index_binder,Code::unsupported_iteration,"Ranges have one item binding.");
                auto hint=anchor(*s.expression);if(!hint)hint=anchor(*s.range_end);
                auto first=expression(*s.expression,hint);auto last=expression(*s.range_end,first);expect(last,first,s.range_end->token);
                if(first!=Type::i32 && first!=Type::i64)error(source,s.token,Code::type_mismatch,"Range endpoints require matching i32 or i64 values.");
                item=first;
            } else {
                auto array=expression(*s.expression);
                if(array==error_type)throw Poison{};
                if(s.asynchronous_iteration && ir::collection_type(array))
                    error(source,s.token,Code::unsupported_iteration,"for await requires a custom async iterable; built-in collections use ordinary for.");
                if(s.asynchronous_iteration || !ir::collection_type(array)){custom_iteration(s);return statement(s);}
                item=array.kind==Type::dictionary?ir::dictionary_value(array):ir::array_element(array);
            }
            scopes.emplace_back();s.slot=declare(s.binder,item);
            if(!s.index_binder.text.empty())s.index_slot=declare(s.index_binder,s.expression->type.kind==Type::dictionary?ir::dictionary_key(s.expression->type):Type(Type::i64));
            s.invalid=false;
        } else if (s.expression) {
            scopes.emplace_back();
            try {
                if(s.selection)expression(*s.selection);
                condition(*s.expression);
            }
            catch (const CompilationError& failure) { diagnostics.add(failure); }
        }
        ++loops; auto body = sequence(s.body); --loops;
        if(!s.binder.text.empty() || s.expression)scopes.pop_back();
        flow = body & return_flow;
        if (!s.binder.text.empty() || !always(s.expression.get()) || (body & break_flow)) flow |= next_flow;
        return s.flow = flow;
    }

public:
    Type infer_default(Expr& e,const Function& context) {
        inferring_default=true;return pure_expression(e,&context);
    }
    Type pure_expression(Expr& e,const Function* context=nullptr,std::optional<Type> expected={}) {
        auto previous=default_context;default_context=context;
        try {auto result=expression(e,expected);default_context=previous;return result;}
        catch(...) {default_context=previous;throw;}
    }
    Semantics(Unit& u, const std::map<std::string, std::unique_ptr<Unit>>& graph, Unit& r,
        std::function<void(Unit&, Constant&)> eval, Diagnostics& d, std::function<void(Unit&,Structure&)> defs)
        : source(u.source), unit(u), units(graph), root(r), evaluate(std::move(eval)), defaults(std::move(defs)), diagnostics(d) {}
    void validate_generic_receiver(Type receiver,const Token& token) {
        auto selected=ir::select_member(*root.member_catalog,receiver,token.text,unit.identity,nullptr,lexical_owner());
        if(selected.path.empty() && !selected.method.empty())
            error(source,token,Code::receiver,"Inherited methods cannot be hidden by a method-specific generic declaration.");
    }
    void validate_header_names(Function& f) {
        current=&f;scopes={{}};
        if(f.receiver && !f.interface_requirement)declare(f.receiver->token,f.receiver->type);
        for(const auto& p:f.parameters)declare(p.token,p.type,p.type.readonly);
        f.locals.clear();f.editor_bindings.clear();current=nullptr;scopes.clear();
    }
    void analyze_constant(Constant& c) {
        if (c.declaration.annotation) c.declaration.annotation = resolve_type(*c.declaration.annotation,unit,root,units,c.declaration.token);
        c.type = expression(*c.declaration.expression, c.declaration.annotation);
        if (c.type == error_type || invalid_function) throw Poison{};
        if (c.declaration.annotation) expect(c.type, *c.declaration.annotation, c.declaration.token);
    }
    void validate_parameter_structure(const Function& f) {
        bool optional=false;
        for(const auto& p:f.parameters) {
            if(p.default_expression) {
                optional=true;
            } else if(optional)error(source,p.token,Code::argument_count,"Required parameters must precede defaulted parameters.");
        }
    }
    void parameter_defaults(Function& f) {
        if(f.defaults_prepared)return;f.defaults_prepared=true;
        validate_parameter_structure(f);
        for(auto& p:f.parameters)if(p.default_expression) {
            expect(pure_expression(*p.default_expression,&f,p.type),p.type,p.token);
            if(p.default_expression->type==error_type)throw Poison{};
            p.default_value=constant_value(source,*p.default_expression,root.compile_evaluation);
        }
    }
    void initializer_fields(Function& f) {
        if (!f.initializer || f.initializer_prepared) return;f.initializer_prepared=true;
        auto [owner,structure] = owned_structure(f.receiver->type);
        // A rejected/recovered field default leaves a partial template. Reuse
        // the default resolver's poison state before reading any template field.
        defaults(*owner,*structure);
        if (f.receiver->type.kind==Type::aggregate) for (const auto& field:structure->fields) {
            auto type=*field.annotation;
            const bool complete_default=field.expression && type.definition && (type.kind==Type::numeric_enum || type.definition->callable || !type.definition->cases.empty() || console::endpoint_type(type));
            if (ir::requires_initialization(type) && !complete_default)
                error(source,f.token,Code::initializer,"Value init requires every field to be valid before its body; a non-nullable reference has no safe default.","Use named-field construction without init for a value with required reference fields.",{{span(owner->source,field.token),"Required reference field declared here."}});
        }
        Expr plan(Expr::Kind::construction,f.token); plan.initializer=f.symbol;
        construction_fields(plan,*owner,*structure,true,true);
        f.initialization_defaults=std::move(plan.omitted); f.initialization_value=structure->defaults;
    }
    bool needs_array_recipe(Type t) {
        if(t.kind==Type::growable_array || t.kind==Type::dictionary)return false;
        if(t.kind==Type::fixed_array)return *t.definition->extent && needs_array_recipe(ir::array_element(t));
        if(t.kind!=Type::aggregate || t.definition->callable)return false;
        if(t.definition->result_type || t.definition->option_type || t.definition->nullable_value)return false;
        auto [owner,structure]=owned_structure(t);if(structure->initializer)return true;
        for(const auto& field:t.definition->fields)if(needs_array_recipe(field.type))return true;
        return false;
    }
    std::unique_ptr<Expr> array_recipe(Type t,const Token& token,bool runtime,Unit& access) {
        auto e=std::make_unique<Expr>(Expr::Kind::literal,token);e->type=t;
        if(ir::nullable_value(t)){e->kind=Expr::Kind::sum;e->slot=0;return e;}
        if(t.kind==Type::growable_array || t.kind==Type::dictionary)return e;
        if(t.kind==Type::fixed_array) {
            if(!*t.definition->extent){e->aggregate_value.repeat=0;return e;}
            e->kind=Expr::Kind::array_default;e->parts.push_back(array_recipe(ir::array_element(t),token,runtime,access));return e;
        }
        if(t.definition && t.definition->callable){if(!t.nullable)error(source,token,Code::initializer,"An array of functions requires complete values.");e->kind=Expr::Kind::function_null;return e;}
        if(t.kind==Type::numeric_enum || (t.definition && !t.definition->cases.empty()))error(source,token,Code::initializer,"Enum/result elements require explicit variants.");
        if(t.kind==Type::aggregate){auto [owner,structure]=owned_structure(t);construction(*e,*owner,*structure,runtime,access);if(e->constructed_type!=Type::none)error(source,token,Code::initializer,"A fallible array element requires explicit constructor result handling.");}
        else if(ir::reference_value(t) && !t.nullable)error(source,token,Code::initializer,"A required array reference needs an explicit initializer.");
        return e;
    }
    ir::ConstantValue array_default(Type t,const Token& token) {
        ir::ConstantValue value;if(t.kind==Type::growable_array || t.kind==Type::dictionary)return value;value.repeat=*t.definition->extent;
        if(!*value.repeat) return value;
        auto element=ir::array_element(t);
        if(element.kind==Type::numeric_enum)error(source,token,Code::initializer,"Numeric enum elements require explicit values.");
        if(console::endpoint_type(element))error(source,token,Code::initializer,"Console endpoint elements require explicit borrowed values.");
        if(ir::reference_value(element) && !element.nullable)error(source,token,Code::initializer,"A nonempty array of required references needs a complete literal.");
        if(ir::collection_type(element)) value.fields.push_back(array_default(element,token));
        else if(element.kind==Type::aggregate) {
            if(ir::nullable_value(element)){ir::ConstantValue absent;absent.fields.resize(2);value.fields.push_back(std::move(absent));return value;}
            if(element.definition->callable){if(!element.nullable)error(source,token,Code::initializer,"A nonempty array of functions requires complete values.");value.fields.emplace_back();return value;}
            if(!element.definition->cases.empty())error(source,token,Code::initializer,"Enum/result elements require explicit variants.");
            auto [owner,structure]=owned_structure(element);defaults(*owner,*structure);
            value.fields.push_back(structure->defaults);
        } else value.fields.emplace_back();
        return value;
    }
    ir::ConstantValue field_default(Statement& field) {
        auto t = *field.annotation;
        if (field.expression) {
            expect(expression(*field.expression,t),t,field.token);
            if (field.expression->type == error_type) throw Poison{};
            return constant_value(source,*field.expression,root.compile_evaluation);
        }
        if(ir::collection_type(t)) {
            return array_default(t,field.token);
        }
        if(ir::nullable_value(t)){ir::ConstantValue absent;absent.fields.resize(2);return absent;}
        if (t.kind != Type::aggregate) return {};
        if(t.definition->callable)return {};
        if(!t.definition->cases.empty())return {};
        for (const auto& [id,owner] : units) for (auto& nested : owner->structures) if (nested.definition == t.definition) {
            defaults(*owner,nested); return nested.defaults;
        }
        throw std::logic_error("Missing owned value type");
    }
    void analyze_global(Global& value) {
        auto& f = *value.initializer;
        current = &f; scopes = {{}}; invalid_function = false;
        variable_statement(f.body.front());
        value.type = f.locals.at(f.body.front().slot);if(value.readonly)value.type=ir::readonly_view(value.type); f.result = value.type;
        Statement returned(Statement::Kind::returned, value.token);
        returned.expression = std::make_unique<Expr>(Expr::Kind::name, f.body.front().token);
        statement(returned); f.body.push_back(std::move(returned)); f.analyzed = true;
    }
    VOID_SEMANTIC_HANDLER void retain_editor_receiver(Function& function,EditorReceiverFound& found) {
        auto& probe=*diagnostics.editor;
        auto receiver=std::move(found.receiver);
        std::function<bool(std::vector<Statement>&)> retain=[&](auto& body) {
            for(std::size_t n=0;n<body.size();++n) {
                auto& statement=body[n];
                if(&statement==found.statement) {
                    Statement evaluation(Statement::Kind::expression,found.site);
                    probe.receiver=receiver.get();evaluation.expression=std::move(receiver);
                    body.erase(body.begin()+n,body.end());body.push_back(std::move(evaluation));return true;
                }
                if(retain(statement.body)){statement.otherwise.clear();body.erase(body.begin()+n+1,body.end());return true;}
                if(retain(statement.otherwise)){body.erase(body.begin()+n+1,body.end());return true;}
            }
            return false;
        };
        if(!retain(function.body))throw EditorProbeFound{};
        probe.function=&function;probe.owner=&unit;probe.readonly_storage=found.readonly_storage;
        probe.receiver_addressable=found.addressable;
        // The missing suffix may mutate, retain or return any formal reference.
        // Never infer a pure recursive callee from the truncated activation.
        std::vector<ir::ReferenceOrigin> origins;
        unsigned index=0;
        const bool nonmutating_receiver=function.accessor==1 || (function.receiver &&
            (function.token.text=="string" || function.token.text=="format"));
        auto opaque=[&](Type type) {
            const auto readonly=ir::readonly_paths(type);
            for(const auto& path:ir::reference_paths(type)) {
                ir::ReferenceOrigin origin{index,path};origins.push_back(origin);
                // An unknown suffix cannot grant authority forbidden by its
                // formal contract. Such leaves may still be returned as aliases.
                if(readonly.contains(path))continue;
                if(index!=0 || !function.receiver || !nonmutating_receiver) {
                    function.reference_effects.mutations.insert(origin);function.reference_effects.required_mutable.insert(origin);
                }
                function.reference_effects.retained.insert(origin);
            }
            ++index;
        };
        if(function.receiver){opaque(function.receiver->type);function.receiver_mutates=!nonmutating_receiver;}
        for(const auto& parameter:function.parameters)opaque(parameter.type);
        for(const auto& path:ir::reference_paths(function.result)) {
            auto& fact=function.reference_effects.returned[path];fact.readonly=true;
            fact.origins.insert(origins.begin(),origins.end());
        }
        if(function.receiver && function.receiver->type.kind==Type::aggregate && !nonmutating_receiver)
            for(const auto& path:ir::reference_paths(function.receiver->type)) {
                auto& fact=function.reference_effects.receiver_after[path];fact.readonly=true;
                fact.origins.insert(origins.begin(),origins.end());
            }
    }
    void finish_editor_receiver(Function& function) {
        current=&function;invalid_function=false;
        auto& probe=*diagnostics.editor;probe.finalizing=true;
        Expr selection(Expr::Kind::field,Token{TokenKind::name,"_void_editor_cursor_",probe.offset});
        selection.parts.push_back(std::make_unique<Expr>(std::move(*probe.receiver)));
        auto type=selection.parts.front()->type;type.readonly|=probe.readonly_storage;
        editor_field_candidates(selection,type);
    }
    void analyze_one(Function& f) {
            if(f.interface_requirement) {
                f.analyzed=true;f.locals={f.receiver->type};f.receiver->slot=0;
                for(auto& p:f.parameters) {p.slot=static_cast<unsigned>(f.locals.size());f.locals.push_back(p.type);}
                return;
            }
            if (diagnostics.full()) return;
            if(f.analyzed)return;f.analyzed=true;f.analyzing=true;
            struct FinishAnalysis {Function& f;~FinishAnalysis(){f.analyzing=false;}} finish_analysis{f};
            Diagnostics::Context generic_context(diagnostics,f.instantiation_context,f.specialization_depth);
            current = &f; scopes = {{}}; invalid_function = false;
            auto before = diagnostics.size();
            if (f.receiver) {
                try { f.receiver->slot = declare(f.receiver->token,f.receiver->type); }
                catch (const CompilationError& failure) { diagnostics.add(failure); }
            }
            for (auto& p : f.parameters) {
                try {if(f.anonymous && &p==&f.parameters.front()){p.slot=static_cast<unsigned>(f.locals.size());f.locals.push_back(p.type);}else p.slot = declare(p.token, p.type, p.type.readonly); }
                catch (const CompilationError& failure) { diagnostics.add(failure); }
            }
            if (f.base_prelude) {
                try {
                    auto [base_owner,base]=owned_structure(*owned_structure(f.receiver->type).second->base_type);
                    construction(*f.base_prelude,*base_owner,*base,true,unit);
                    const bool fallible=f.base_prelude->constructed_type!=Type::none;
                    if(fallible) {
                        if(!f.base_propagates || !ir::fallible_initializer_result(f.result))
                            error(source,f.base_prelude->token,Code::initializer,"A fallible base initializer requires super.init(...)? in a fallible derived initializer.");
                        if(ir::field_type(f.base_prelude->initializer_result,2)!=ir::field_type(f.result,2))
                            error(source,f.base_prelude->token,Code::type_mismatch,"Base initializer propagation requires exactly matching error types.");
                        f.base_prelude->type=f.base_prelude->constructed_type;
                    } else if(f.base_propagates)error(source,f.base_prelude->token,Code::initializer,"An infallible super.init does not produce a result to propagate.");
                    f.base_prelude->kind=Expr::Kind::base_initialization;
                } catch (const CompilationError& failure) { diagnostics.add(failure); }
                  catch (const Poison&) {}
            }
            if(!f.native_symbol.empty() || f.c_primitive) {
                if(f.compile_time)try {validate_compile_function(source,f);}
                    catch(const CompilationError& failure){diagnostics.add(failure);f.compile_failed=true;}
                return;
            }
            unsigned flow;
            try {flow=sequence(f.body,false);}
            catch(EditorReceiverFound& found) {retain_editor_receiver(f,found);return;}
            if (f.result != Type::none && (flow & next_flow) && diagnostics.size() == before && !f.parse_failed && !invalid_function)
                try { error(source, f.token, Code::missing_return, "Missing return path in '" + f.token.text + "'.", "Return " + editor_return_type(f) + " on every path that reaches the end."); }
                catch (const CompilationError& failure) { diagnostics.add(failure); }
            if(f.compile_time) {
                f.compile_failed=f.parse_failed || invalid_function || diagnostics.size()!=before;
                if(!f.compile_failed)try {validate_compile_function(source,f);f.compile_checked=true;}
                catch(const CompilationError& failure){diagnostics.add(failure);f.compile_failed=true;}
            }
    }
    void analyze(){for(auto& f:unit.functions)analyze_one(f);}

};

// VFS paths are normalized before touching the host filesystem. Physical root
// spelling never participates in language identity or native symbol identity.
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
        Semantics semantics(*f.compile_owner,units,root,evaluate,diagnostics,defaults);
        semantics.parameter_defaults(f);
        if(!body)return;
        if(f.analyzing)error(source,site,Code::constant_cycle,"Circular dependency while analyzing compile func '"+f.token.text+"'.");
        if(!f.analyzed) {
            Depth guard(diagnostics.semantic_depth(),source,site,64);
            semantics.analyze_one(f);
        }
        if(f.compile_failed || !f.compile_checked)throw Poison{};
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
                    Semantics semantics(*owner,units,root,evaluate,diagnostics,defaults);
                    auto type=semantics.pure_expression(*expression,nullptr,backing);
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
            Semantics(owner, units, root, evaluate, diagnostics, defaults).analyze_constant(c);
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
            Semantics semantics(owner,units,root,evaluate,diagnostics,defaults);
            if (auto base=structure.definition->base.lock()) for (auto& [id,u]:units) for (auto& parent:u->structures) if (parent.definition==base) {
                defaults(*u,parent); structure.defaults.fields=parent.defaults.fields;
            }
            for (auto& field : structure.fields) structure.defaults.fields.push_back(semantics.field_default(field));
            structure.default_state = 2;
        } catch (...) { structure.default_state = 3; throw; }
    };
    auto array_extent=[&](Unit& owner,ArraySyntax& syntax) {
        std::optional<std::uint64_t> extent;
            if(syntax.extent) {
                Semantics semantics(owner,units,root,evaluate,diagnostics,defaults);
                auto type=semantics.pure_expression(*syntax.extent);
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
                    parameter.type=Semantics(owner,units,root,evaluate,diagnostics,defaults).infer_default(*parameter.default_expression,f);
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
                Semantics(*u, units, root, evaluate, diagnostics, defaults).analyze_global(value);
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
        try { Semantics(*unit,units,root,evaluate,diagnostics,defaults).parameter_defaults(f); }
        catch (const CompilationError& failure) { diagnostics.add(failure); }
        catch (const Poison&) {}
    }
    for (auto& [id,unit] : units) for (auto& f : unit->functions) {
        if (f.parse_failed) continue;
        try { Semantics(*unit,units,root,evaluate,diagnostics,defaults).initializer_fields(f); }
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
            Semantics(*u,units,root,evaluate,diagnostics,defaults).analyze();
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
        Semantics(*diagnostics.editor->owner,units,root,evaluate,diagnostics,defaults).finish_editor_receiver(*diagnostics.editor->function);
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
