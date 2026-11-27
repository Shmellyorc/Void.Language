#include "semantics.hpp"
#include "constant_evaluation.hpp"
#include "diagnostics.hpp"
#include "parser.hpp"
#include "stack_guard.hpp"
#include "type_resolution.hpp"
#include "void/compiler_observation.hpp"
#include "void/console_ops.hpp"
#include "void/filesystem_ops.hpp"
#include "void/host_ops.hpp"
#include "void/http_ops.hpp"
#include "void/math_ops.hpp"
#include "void/os.hpp"
#include "void/path.hpp"
#include "void/temporal.hpp"
#include "void/temporal_ops.hpp"
#include "void/url_ops.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <limits>

namespace vd::frontend {
namespace {
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
    unsigned loops = 0, constant_pattern_depth = 0, constant_pattern_slots = 0;
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
#include "weak_references.inc"
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

        if(!numeric::integer(expression(*e.parts[1])))error(source,e.parts[1]->token,Code::type_mismatch,"String index requires an integer, excluding bool and floats.");e.target="$text.index";return e.type=Type::string;
    }
    VOID_SEMANTIC_HANDLER Type core_call(Expr& e,const Type& receiver,bool type_level=false) {

        if(!e.type_arguments.empty())error(source,e.token,Code::argument_count,"Core members do not take explicit generic arguments.");
        const auto* api=core_member(receiver,e.token.text,type_level);
        if(!api || api->property)error(source,e.token,Code::unknown_method,"Unknown callable member '"+e.token.text+"' in "+ir::type_name(receiver)+".");
        auto target=core_signature(*api,receiver,e.token);
        const auto first=type_level?0u:1u;
        if(type_level)e.parts.erase(e.parts.begin());
        if(api->positional && std::any_of(e.names.begin(),e.names.end(),[](const auto& n){return !n.text.empty();}))
            error(source,e.token,Code::argument_count,"Incorrect positional arguments for collection operation.");
        if(!current && !constant_core_operation(target.symbol.substr(0,target.symbol.find("::"))))
            error(source,e.token,Code::constant_call,"This text or collection operation requires runtime evaluation.");
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
        if(!e.argument_indexes.empty() || e.target=="$weak.lock")return e.type;
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
        if(receiver.definition && receiver.definition->weak_target && e.token.text=="lock")return weak_promotion(e,receiver);
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
        const auto slot = current ? static_cast<unsigned>(current->locals.size()) : constant_pattern_slots++;
        if(current) {
            current->locals.push_back(type);
            current->editor_bindings.push_back({token,slot,readonly});
        }
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
                if(!candidates.contains("weak")) {
                    Expr probe(Expr::Kind::name,e.token);
                    probe.token.text="weak";
                    if(prelude_available(probe))candidates.emplace("weak",EditorCandidate{
                        "weak","weak(target: T) -> weak<T>",3,{},weak_creation_doc});
                }
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
        if(type.definition && type.definition->weak_target && !invalid_function && !unit.failed_import)
            weak_member_candidates(e,type);
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
    struct ConstantPatternFrame {
        unsigned& depth;
        unsigned& slots;
        Expr& expression;
        bool active;
        ConstantPatternFrame(Semantics& context, Expr& e)
            : depth(context.constant_pattern_depth), slots(context.constant_pattern_slots),
              expression(e), active(!context.current) {
            if(active)++depth;
        }
        ~ConstantPatternFrame() {
            if(active && --depth==0) {
                expression.constant_pattern_frame=slots;
                slots=0;
            }
        }
    };
    VOID_SEMANTIC_HANDLER Type test_expression(Expr& e){
        ConstantPatternFrame frame(*this,e);
        auto subject=expression(*e.parts[0]);
        const auto scope_depth=scopes.size();
        if(!pattern_bindings)scopes.emplace_back();
        struct Leave {decltype(scopes)& stack;std::size_t depth;~Leave(){stack.resize(depth);}} leave{scopes,scope_depth};
        check_pattern(e.patterns[0],subject);
        return e.type=Type::boolean;
    }
    VOID_SEMANTIC_HANDLER Type match_expression(Expr& e,const std::optional<Type>& expected){
        ConstantPatternFrame frame(*this,e);
        return check_match(e,expected);
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
        if(array.kind==Type::dictionary){expect(expression(*e.parts[1],ir::readonly_view(ir::dictionary_key(array))),ir::readonly_view(ir::dictionary_key(array)),e.parts[1]->token);return e.type=ir::dictionary_value(array);}
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
        if(!v->owner)e.source_target=constant_pattern_identity(unit.identity,v->declaration);
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
    bool prelude_available(const Expr& e) {
        if(!e.qualifier.empty() || lookup(e.token.text) || unit.declarations.contains(e.token.text) || unit.constant_names.contains(e.token.text) || unit.invalid_declarations.contains(e.token.text))return false;
        for(const auto& import:unit.imports)if(!import.explicit_alias && !import.target.empty()) {
            const auto& owner=*units.at(import.target);
            if(owner.declarations.contains(e.token.text) || owner.constant_names.contains(e.token.text) || owner.structure_names.contains(e.token.text) || owner.invalid_declarations.contains(e.token.text))return false;
        }
        return !unit.structure_names.contains(e.token.text);
    }
    bool display_prelude(const Expr& e) {
        return (e.token.text=="string" || e.token.text=="format" || e.token.text=="print") && prelude_available(e);
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
        if(e.target=="$weak.create")return e.type;
        if(e.token.text=="weak" && prelude_available(e) && !(current && current->substitutions.contains("weak")))
            return weak_creation(e);
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
            if(!type.definition || ir::nullable_type(type) || (type.kind!=Type::aggregate && type.kind!=Type::managed_ref) || type.definition->enum_backing || !type.definition->cases.empty() || type.definition->callable || type.definition->weak_target)error(source,e.token,Code::not_callable,"The specialized type has no source constructor.");
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

Semantics checker(const SemanticContext& context, Unit& unit) {
    return Semantics(unit, context.units, context.root, context.evaluate, context.diagnostics, context.defaults);
}
}

void SemanticContext::prepare_compile_body(Unit& owner, Function& function, const SourceFile& source,
                                           const Token& site, bool body) const {
    auto semantics = checker(*this, owner);
    semantics.parameter_defaults(function);
    if (!body) return;
    if (function.analyzing)
        error(source, site, Code::constant_cycle,
              "Circular dependency while analyzing compile func '" + function.token.text + "'.");
    if (!function.analyzed) {
        Depth guard(diagnostics.semantic_depth(), source, site, 64);
        semantics.analyze_one(function);
    }
    if (function.compile_failed || !function.compile_checked) throw Poison{};
}

Type SemanticContext::pure_expression(Unit& owner, Expr& expression, std::optional<Type> expected) const {
    return checker(*this, owner).pure_expression(expression, nullptr, expected);
}

Type SemanticContext::infer_default(Unit& owner, Expr& expression, const Function& function) const {
    return checker(*this, owner).infer_default(expression, function);
}

void SemanticContext::analyze_constant(Unit& owner, Constant& value) const {
    checker(*this, owner).analyze_constant(value);
}

void SemanticContext::field_defaults(Unit& owner, Structure& structure) const {
    auto semantics = checker(*this, owner);
    for (auto& field : structure.fields) structure.defaults.fields.push_back(semantics.field_default(field));
}

void SemanticContext::analyze_global(Unit& owner, Global& value) const {
    checker(*this, owner).analyze_global(value);
}

void SemanticContext::parameter_defaults(Unit& owner, Function& function) const {
    checker(*this, owner).parameter_defaults(function);
}

void SemanticContext::initializer_fields(Unit& owner, Function& function) const {
    checker(*this, owner).initializer_fields(function);
}

void SemanticContext::prepare_specialized_method(Unit& owner, Function& function) const {
    auto semantics = checker(*this, owner);
    semantics.parameter_defaults(function);
    semantics.initializer_fields(function);
}

void SemanticContext::validate_header(Unit& owner, Function& function) const {
    auto semantics = checker(*this, owner);
    semantics.validate_header_names(function);
    semantics.validate_parameter_structure(function);
}

void SemanticContext::validate_generic_receiver(Unit& owner, Type receiver, const Token& site) const {
    checker(*this, owner).validate_generic_receiver(receiver, site);
}

void SemanticContext::analyze_unit(Unit& owner) const {
    checker(*this, owner).analyze();
}

void SemanticContext::finish_editor_receiver(Unit& owner, Function& function) const {
    checker(*this, owner).finish_editor_receiver(function);
}
}
