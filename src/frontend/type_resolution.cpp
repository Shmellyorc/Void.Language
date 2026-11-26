#include "type_resolution.hpp"
#include "diagnostics.hpp"
#include "stack_guard.hpp"
#include <algorithm>

namespace vd::frontend {

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
namespace {
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
}
