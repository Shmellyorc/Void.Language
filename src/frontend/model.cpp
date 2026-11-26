#include "model.hpp"
#include "void/compiler_observation.hpp"
#include "void/console_ops.hpp"
#include "diagnostics.hpp"
#include "void/path.hpp"
#include <algorithm>
#include <functional>

namespace vd::frontend {
std::string format_specification_syntax(const Expr& root) {
    std::string out;auto field=[&](const std::string& value){out+=std::to_string(value.size())+":"+value;};
    std::vector<const Expr*> pending{&root};
    while(!pending.empty()) {
        const auto& e=*pending.back();pending.pop_back();
        field(std::to_string(static_cast<unsigned>(e.kind)));field(e.token.text);field(e.op);field(e.target);field(e.qualifier);field(std::to_string(e.parts.size()));
        field(std::to_string(e.names.size()));field(std::to_string(e.omitted.size()));field(std::to_string(e.patterns.size()));
        for(const auto& name:e.names)field(name.text);
        for(auto i=e.parts.rbegin();i!=e.parts.rend();++i)pending.push_back(i->get());
        for(const auto& omitted:e.omitted)pending.push_back(omitted.get());
        std::vector<const Pattern*> patterns;for(const auto& pattern:e.patterns)patterns.push_back(&pattern);
        for(std::size_t n=0;n<patterns.size();++n) {
            const auto& pattern=*patterns[n];field(std::to_string(static_cast<unsigned>(pattern.kind)));field(pattern.spelling);field(pattern.exclusive?"exclusive":"inclusive");field(pattern.rest?"rest":"exact");
            field(std::to_string(pattern.fields.size()));field(std::to_string(pattern.values.size()));field(std::to_string(pattern.children.size()));
            for(const auto& name:pattern.fields)field(name.text);
            for(const auto& value:pattern.values)pending.push_back(value.get());
            for(const auto& child:pattern.children)patterns.push_back(&child);
        }
    }
    return out;
}

std::set<unsigned> managed_construction_parameters(const Function& f,const std::set<unsigned>& mask) {
    std::set<unsigned> result;const unsigned first=f.receiver?1u:0u;
    for(auto index:mask) {
        auto type=index<first?f.receiver->type:f.parameters.at(index-first).type;
        if(type.kind==Type::managed_ref)result.insert(index);
    }
    return result;
}
// Presentation uses the selected compiler function, including concrete generic
// types. Source identities remain shared for navigation and references.
std::string random_public_name(const Function& f) {
    auto name=f.token.text;
    if (f.symbol.starts_with("/@stdlib/random.void::") && name.starts_with("_arity_")) {
        name=name.substr(7); name.resize(name.rfind('_'));
    }
    return name;
}
bool random_numeric_function(const Function& f) {
    if(!f.symbol.starts_with("/@stdlib/random.void::"))return false;
    auto name=random_public_name(f);
    auto suffix=name.starts_with("try_rand_")?name.substr(9):name.starts_with("rand_")?name.substr(5):"";
    return numeric::number(numeric::primitive(suffix));
}
// Uninstantiated generic declarations retain source type spellings, including
// collection/result/callable patterns whose concrete layouts do not exist yet.
Type editor_pattern_type(Type raw) {
    if(raw.kind!=Type::unresolved && !ir::collection_type(raw) && !(raw.definition && (raw.definition->result_type || raw.definition->callable)))return raw;
    auto name=std::make_shared<ir::ValueType>();name->name=raw.definition->name;name->identity=name->name;
    Type type(name,Type::unresolved);type.nullable=raw.nullable;return type;
}
std::string editor_return_type(const Function& f,bool symbolic) {
    const bool pattern=symbolic || !f.generics.empty();
    if(f.returns.empty())return ir::type_name(pattern?editor_pattern_type(f.result):f.result);
    std::string label="(";
    for(const auto& component:f.returns) {
        if(label.size()>1)label+=", ";
        if(component.name)label+=component.name->text+": ";
        label+=ir::type_name(pattern?editor_pattern_type(component.type):component.type);
    }
    return label+")";
}
std::string editor_signature(const Function& f) {
    observation::Scope timing(observation::EditorDetail::formatting);
    std::string out=(random_numeric_function(f) && f.token.text.starts_with("_arity_") ? "public" : f.visibility)+std::string(" ");
    if(f.compile_time)out+="compile func ";
    if(f.static_owner)out+="static "+ir::type_name(f.static_owner->type)+".";
    if(!f.operator_glyph.empty())out+="operator "+f.operator_glyph;
    else out+=random_public_name(f);
    out+="(";
    for(unsigned n=0;n<f.parameters.size();++n){if(n)out+=", ";const auto& p=f.parameters[n];out+=p.token.text+": "+ir::type_name(f.generics.empty()?p.type:editor_pattern_type(p.type));if(p.default_expression)out+=" = …";}
    out+=") -> "+editor_return_type(f);
    if (f.symbol.starts_with("/@stdlib/random.void::") && !f.token.text.starts_with("_arity_")) {
        auto name=random_public_name(f);
        auto suffix=name.starts_with("try_rand_")?name.substr(9):name.starts_with("rand_")?name.substr(5):"";
        if (numeric::number(numeric::primitive(suffix))) {
            if(f.parameters.empty())out+="\n"+name+"(maximum: "+suffix+") -> "+ir::type_name(f.result);
            out+="\n"+name+"(lower: "+suffix+", upper: "+suffix+") -> "+ir::type_name(f.result);
            out+="\nBounded forms are direct calls; function values retain the declared signature.";
        }
    }
    if(!f.operator_glyph.empty())out+="\nDeclared by "+ir::type_name(f.operator_owner);
    else if(f.receiver)out+="\nDeclared by "+ir::type_name(f.receiver->type);
    if(f.capability_contract.empty())return out+"\nEffects: pending analysis";
    out+=f.asynchronous?"\nEffects: asynchronous":"\nEffects: synchronous";
    auto effects=[&](const auto& origins,const std::string& label){for(const auto& origin:origins){out+="; "+label+" ";if(f.receiver && origin.parameter==0)out+="self";else {auto n=origin.parameter-(f.receiver?1u:0u);out+=n<f.parameters.size()?f.parameters[n].token.text:"?";}for(auto field:origin.path)out+="["+std::to_string(field)+"]";}};
    effects(f.reference_effects.mutations,"mutates");effects(f.reference_effects.retained,"retains");effects(f.reference_effects.required_mutable,"requires mutable");
    for(const auto& [path,fact]:f.reference_effects.returned)if(fact.readonly)out+="; returns readonly reference";
    return out;
}

Documentation source_documentation(const SourceFile& source,std::size_t offset) {
    const auto& text=source.text();auto line=offset?text.find_last_of("\r\n",offset-1):std::string::npos;
    if(line==std::string::npos)return {};
    auto end=line;std::vector<std::pair<std::size_t,std::size_t>> lines;
    while(true) {
        if(end && text[end]=='\n' && text[end-1]=='\r')--end;
        auto begin=end?text.find_last_of("\r\n",end-1):std::string::npos;begin=begin==std::string::npos?0:begin+1;
        auto first=text.find_first_not_of(' ',begin);
        if(first>=end || text.compare(first,2,"##"))break;
        first+=2;if(first<end && text[first]==' ')++first;
        lines.emplace_back(first,end);
        if(!begin)break;end=begin-1;
    }
    Documentation result;bool prose=false;auto snapshot=std::make_shared<const SourceFile>(source);
    for(auto it=lines.rbegin();it!=lines.rend();++it) {
        const auto [begin,end]=*it;auto first=text.find_first_not_of(" \t",begin);
        auto content=std::string_view(text).substr(begin,end-begin);
        auto tag=first<end?std::string_view(text).substr(first,end-first):std::string_view{};
        auto recognized=[&](std::string_view name){return tag==name || (tag.starts_with(name) && tag.size()>name.size() && (tag[name.size()]==' ' || tag[name.size()]=='\t'));};
        auto description=[&](std::size_t from){auto a=tag.find_first_not_of(" \t",from);if(a==std::string_view::npos)return std::string{};auto b=tag.find_last_not_of(" \t");return std::string(tag.substr(a,b-a+1));};
        if(recognized("@param")) {
            auto a=tag.find_first_not_of(" \t",6);if(a==std::string_view::npos)a=tag.size();
            auto b=tag.find_first_of(" \t",a);if(b==std::string_view::npos)b=tag.size();
            result.parameters.push_back({std::string(tag.substr(a,b-a)),description(b),{snapshot,{},first,first+b}});
        } else if(recognized("@return")) {
            auto value=description(7);if(!value.empty() && !result.returns)result.returns=DocumentationText{std::move(value),{snapshot,{},first,first+7}};
        } else {
            if(prose)result.description+='\n';result.description+=content;prose=true;
        }
    }
    return result;
}
Documentation numeric_documentation(const numeric::Member& api) {
    Documentation docs;docs.description=std::string(api.documentation);
    for(unsigned n=0;n<api.parameter_documentation.size();++n)docs.parameters.push_back({std::string(api.parameters[n]),std::string(api.parameter_documentation[n]),{}});
    return docs;
}
void validate_documentation(const SourceFile& source,const Function& f,Diagnostics& diagnostics) {
    auto docs=source_documentation(source,f.token.offset);
    std::map<std::string,SourceSpan> first;
    bool documented=false;
    for(const auto& p:docs.parameters) {
        auto formal=std::find_if(f.parameters.begin(),f.parameters.end(),[&](const auto& formal){return formal.token.text==p.name;});
        if(formal==f.parameters.end())diagnostics.warn(p.source,Code::documentation_unknown_parameter,"Unknown documented parameter '"+p.name+"'.");
        else documented|=!p.description.empty();
        auto [original,inserted]=first.emplace(p.name,p.source);
        if(!inserted)diagnostics.warn(p.source,Code::documentation_duplicate_parameter,"Duplicate documentation for parameter '"+p.name+"'.",{{original->second,"First documentation tag"}});
    }
    if(documented)for(const auto& p:f.parameters)if(docs.parameter(p.token.text).empty())
        diagnostics.warn(source,p.token,Code::documentation_missing_parameter,"Missing documentation for parameter '"+p.token.text+"'.");
}
bool standard_module(std::string_view id) { return id.starts_with("/@stdlib/"); }
bool same_library(std::string_view a, std::string_view b) { return source_domain(a)==source_domain(b); }
bool source_visible(std::string_view visibility, std::string_view owner, std::string_view scope) {
    return owner==scope || (visibility!="private" && (visibility!="internal" || same_library(owner,scope)));
}

std::string import_alias(const Import& import) {
    if (import.explicit_alias) return import.alias.text;
    if (import.path.kind != TokenKind::path && import.path.text.starts_with("pkg/"))
        return import.path.text.substr(4);
    return path_utf8(utf8_path(import.target.empty() ? import.path.text : import.target).stem());
}

// Entry selection is shared by validated metadata and typed IR lowering.
const Function* application_entry(const Unit& unit) {
    if(unit.application_body)return unit.declarations.at("$application");
    if(unit.declarations.contains("entry"))return nullptr;
    auto found=unit.declarations.find("main");if(found==unit.declarations.end())return nullptr;
    const auto* f=found->second;
    return !f->receiver && f->parameters.empty() && f->generics.empty() && !f->asynchronous && (f->result==Type::none || f->result==Type::i32) ? f : nullptr;
}

ir::Op operation(const std::string& op) {
    static const std::map<std::string, ir::Op> ops = {{"+", ir::Op::add}, {"-", ir::Op::subtract}, {"*", ir::Op::multiply},
        {"<<",ir::Op::shift_left},{">>",ir::Op::shift_right},{"&",ir::Op::bit_and},{"|",ir::Op::bit_or},{"^",ir::Op::bit_xor}, {"/", ir::Op::divide}, {"%", ir::Op::remainder}, {"<", ir::Op::less}, {"<=", ir::Op::less_equal},
        {">", ir::Op::greater}, {">=", ir::Op::greater_equal}, {"==", ir::Op::equal}, {"!=", ir::Op::not_equal}};
    return ops.at(op);
}

bool checked_initializer_default(Type t) {
    // These required-value fields can have a checked pure declaration default.
    // Prepare that value before init, just as for numeric enums and options.
    return t.kind==Type::numeric_enum ||
        (t.definition && (t.definition->callable || !t.definition->cases.empty())) || console::endpoint_type(t);
}
}
