#include "semantic_passes.hpp"
#include "analysis.hpp"
#include "source_graph.hpp"
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
            for(const auto& f:u->functions)if(application_main(f))error(u->source,f.token,Code::entry,"Executable top-level statements compete with main().");
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
    if (auto* app = application_entry(root); app && app->asynchronous)
        root.async_application_task = root.make_task(Type::i32);
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
