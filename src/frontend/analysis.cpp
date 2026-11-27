#include "analysis.hpp"
#include "lowering.hpp"
#include "stack_guard.hpp"
#include "type_resolution.hpp"
#include "void/compiler_observation.hpp"
#include "void/format.hpp"
#include "void/path.hpp"
#include "void/temporal.hpp"
#include <algorithm>
#include <tuple>

namespace vd {
using namespace frontend;

std::shared_ptr<const ValidatedAnalysis> AnalysisAccess::build(std::map<std::string, std::unique_ptr<Unit>> units, const std::string& entry,std::vector<Diagnostic> warnings,const std::string& context_key,std::shared_ptr<Unit> library_scope,ApplicationMetadata::Owner metadata,bool native_authorized,std::shared_ptr<const project::LockedDependencies> dependencies,bool editor_only,bool presentation) {
    observation::Scope timing(observation::Phase::analysis_assembly);
    auto result = std::shared_ptr<ValidatedAnalysis>(new ValidatedAnalysis);result->warnings_=std::move(warnings);
    result->entry_ = entry;
    result->application_metadata_ = std::move(metadata);
    result->locked_dependencies_ = dependencies;
    const auto& selected=library_scope?*library_scope:*units.at(entry);
    result->executable_=!library_scope && !editor_only;
    const auto* app=application_entry(selected);
    result->async_application_ = app && app->asynchronous;
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
    if(result->async_application_)result->contract_+="|async-application-v1";
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
    std::set<std::string> instantiated, authored_generic_functions;
    for(const auto& [id,u]:*graph) {
        for(const auto& f:u->generic_functions)authored_generic_functions.insert(f.symbol);
        for(const auto& f:u->functions)if(!f.generic_origin.empty()) {
            source_identities.emplace(f.symbol,f.generic_origin);
            instantiated.insert(f.generic_origin);
        }
        for(const auto& s:u->structures)if(s.generic_origin) {
            source_identities.emplace(s.definition->identity,s.generic_origin->definition->identity);
            instantiated.insert(s.generic_origin->definition->identity);
        }
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
    auto rename_relationship=[&](const std::string& left,const std::string& right) {
        auto a=source_identity(left),b=source_identity(right);
        if(a==b)return;
        if(b<a)std::swap(a,b);
        result->rename_relationships_.emplace(std::move(a),std::move(b));
    };
    auto declaration = [&](AnalyzedDeclaration d,bool native_contract=true,bool visible=true) {
        using K=AnalyzedDeclaration::Kind;
        switch(d.kind) {
        case K::value_structure: case K::managed_structure: case K::interface_type:
        case K::managed_field: case K::value_field: case K::type_parameter: case K::interface_requirement:
            d.rename_safe=true;
            break;
        default:
            d.rename_safe|=d.editor_symbol_kind==22;
        }
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
        auto unresolved_template=[&](const std::string& identity,const std::vector<Token>& syntax) {
            if(instantiated.contains(identity))return;
            for(const auto& token:syntax)
                if(token.kind==TokenKind::name && valid_editor_identifier(token.text))
                    result->unresolved_template_names_[token.text].push_back(location(token));
        };
        for(const auto& f:unit->generic_functions)unresolved_template(f.symbol,f.syntax);
        for(const auto& s:unit->generic_structures)unresolved_template(s.definition->identity,s.syntax);
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
        for(const auto& import:unit->imports)if(import.explicit_alias && aliases.insert(import.alias.text).second) {
            AnalyzedDeclaration record{AnalyzedDeclaration::Kind::import_alias,id+"::import::"+import.alias.text,id,"private",location(import.alias),{},Type::none,{},{},id};
            record.rename_safe=true;
            result->editor_declarations_.push_back(std::move(record));
        }
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
                if(p->constant_binding && !implicit) {
                    const auto identity=constant_pattern_identity(id,p->token);
                    AnalyzedDeclaration record{AnalyzedDeclaration::Kind::local_variable,identity,id,"local",
                        location(p->token),{},p->type,{},{},owner};
                    record.rename_safe=true;
                    declaration(std::move(record),false);
                }
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
                if(p->kind==Pattern::Kind::constructor && p->narrowed!=Type::none && p->narrowed.definition) {
                    if(p->token.offset!=p->variant_token.offset)namespace_reference(owner,p->token,implicit);
                    Expr use(Expr::Kind::name,p->variant_token);
                    reference(owner,p->narrowed.definition->identity,use,*unit,implicit,false);
                    for(unsigned n=0;n<p->indexes.size();++n) {
                        const auto& member=p->narrowed.definition->fields.at(p->indexes[n]);
                        Expr field(Expr::Kind::name,p->fields.at(n));
                        reference(owner,member.owner+"::field::"+member.name,field,*unit,implicit,false);
                    }
                }
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
                reference(owner,e.source_target,e,*unit,implicit_site,
                          !e.source_target.starts_with("$result::") &&
                          !(e.kind==Expr::Kind::name && e.source_target.starts_with(id+"::$constant_pattern::")));
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
            if (!e.initializer.empty())
                reference(owner,e.initializer,e,*unit,implicit,true,/*navigation_only=*/true);
            if(!e.setter.empty())reference(owner,e.setter,e,*unit,true);
            if(!e.display_adapter.empty())reference(owner,e.display_adapter,e,*unit,true);
            const auto& callee=e.initializer.empty()?e.target:e.initializer;
            if(!e.argument_indexes.empty() && !callee.starts_with("$") &&
               std::any_of(e.names.begin(),e.names.end(),[](const auto& name){return !name.text.empty();}))
              for(const auto& [target_id,target_unit]:*graph)for(const auto& target:target_unit->functions)if(target.symbol==callee &&
                  (target.generic_origin.empty() || authored_generic_functions.contains(target.generic_origin))) {
                for(unsigned n=0;n<e.names.size();++n)if(!e.names[n].text.empty()) {
                    const auto& parameter=target.parameters.at(e.argument_indexes.at(n));
                    auto identity=target.generic_origin.empty()?target.symbol+"::local::"+std::to_string(parameter.slot):
                        target.generic_origin+"::parameter::"+parameter.token.text;
                    Expr use(Expr::Kind::name,e.names[n]);reference(owner,identity,use,*unit,implicit_site,false);
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
            method_record.rename_safe=f.operator_glyph.empty() && !f.anonymous && !f.initializer &&
                !method_record.declaration_only && !f.global_initializer && f.token.text!="entry" &&
                f.token.text!="main" && f.token.text!="deinit" && f.token.text!="call" &&
                (f.accessor || valid_editor_identifier(f.token.text));
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
            if(f.overriding) {
                Expr use(Expr::Kind::name,f.token);reference(f.symbol,f.virtual_slot,use,*unit,true);
                rename_relationship(f.symbol,f.virtual_slot);
            }
            for (const auto& p : f.parameters) if (p.default_expression) expression(*p.default_expression,f.symbol,{});
            std::map<unsigned,std::string> locals;
            for(const auto& binding:f.editor_bindings) {
                if(presentation && f.locals.at(binding.slot)==error_type)continue;
                if(f.display_adapter || f.json_adapter || f.enum_adapter || binding.token.text.starts_with("$iterator$") || (f.global_initializer && !valid_editor_identifier(binding.token.text)))continue;
                const bool parameter=(f.receiver && f.receiver->slot==binding.slot) ||
                    std::any_of(f.parameters.begin(),f.parameters.end(),[&](const auto& p){return p.slot==binding.slot;});
                if(!f.generic_origin.empty()) {
                    if(parameter && authored_generic_functions.contains(f.generic_origin))
                        locals.emplace(binding.slot,f.generic_origin+"::parameter::"+binding.token.text);
                    continue;
                }
                if(binding.readonly && !parameter) continue;
                auto key=source_identity(f.symbol)+"::local::"+std::to_string(binding.slot);
                auto kind=parameter ? AnalyzedDeclaration::Kind::parameter : AnalyzedDeclaration::Kind::local_variable;
                AnalyzedDeclaration record{kind,key,id,"local",location(binding.token),{},f.locals.at(binding.slot),{},{},source_identity(f.symbol)};
                record.readonly=binding.readonly;
                record.rename_safe=valid_editor_identifier(binding.token.text);
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
                        record.rename_safe=f.generic_origin.empty() && valid_editor_identifier(s.token.text);
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
            rename_relationship(requirement,route.second);
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

ir::Module ValidatedAnalysis::lower(ir::Emission emission) const {
    observation::Scope timing(observation::Phase::typed_ir);
    auto module = lower_(); module.constant_contract = constants_; module.analysis = shared_from_this();
    module.application_metadata = application_metadata_;
    // The complete graph has already passed checking and typed-IR validation.
    if(emission==ir::Emission::entry)ir::retain_entry(module);
    return module;
}
}
