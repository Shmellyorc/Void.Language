// Type selectors resolve nominal owners; storage and calls remain ordinary
// runtime globals/functions, with no fabricated instance or ABI receiver.
VOID_SEMANTIC_HANDLER std::pair<Unit*,Structure*> static_type(Expr& base) {
    std::string spelling;
    if(base.kind==Expr::Kind::name && !lookup(base.token.text))spelling=base.token.text;
    else if(base.kind==Expr::Kind::field && base.parts[0]->kind==Expr::Kind::name &&
            !lookup(base.parts[0]->token.text) && unit.qualifiers.contains(base.parts[0]->token.text))
        spelling=base.parts[0]->token.text+"."+base.token.text;
    if(spelling.empty())return {};
    auto [owner,structure]=structure_lookup(unit,root,units,spelling,base.token);
    if(!structure || structure->definition->interface_type || !structure->case_tokens.empty())return {};
    return {owner,&concrete_structure(base,*owner,*structure)};
}
void static_access(Type type,const std::string& visibility,const Token& token) {
    if((visibility=="private" && lexical_owner()!=Type::key(type)) ||
       (visibility=="internal" && !same_library(type.definition->module_identity,unit.identity)))
        error(source,token,Code::visibility,"Static member is not accessible from this scope.");
}
VOID_SEMANTIC_HANDLER void editor_static_candidates(Unit& owner,Structure& structure,const Token&) {
    auto& result=diagnostics.editor->result;const auto type=Type::nominal(structure.definition);
    std::set<std::string> added;
    auto add=[&](const Token& token,const std::string& visibility,Type,int kind,const std::string& detail) {
        if(!token.text.starts_with(diagnostics.editor->prefix))return;
        try {static_access(type,visibility,token);}catch(const CompilationError&){return;}
        if(!added.insert(token.text).second)return;
        EditorCandidate candidate;candidate.name=token.text;candidate.kind=kind;candidate.detail=detail;
        candidate.documentation=source_documentation(owner.source,token.offset).text();result.candidates.push_back(std::move(candidate));
    };
    const auto prefix=Type::key(type)+"::static::";
    for(const auto& [key,value]:owner.static_fields)if(key.starts_with(prefix)) {
        if(value->type==Type::none)owner.resolve_global(*value);
        add(value->token,value->visibility,value->type,5,"static "+value->token.text+": "+ir::type_name(value->type));
    }
    const auto generic_prefix=structure.generic_origin?structure.generic_origin->definition->identity+"::static::":prefix;
    for(const auto& method:owner.generic_functions)if(method.static_owner && !method.generics.empty() && method.symbol.starts_with(generic_prefix))add(method.token,method.visibility,method.result,2,editor_signature(method));
    for(const auto& [key,method]:owner.static_methods)if(key.starts_with(prefix))add(method->token,method->visibility,method->result,2,editor_signature(*method));
    throw EditorProbeFound{};
}
VOID_SEMANTIC_HANDLER std::optional<Type> static_expression(Expr& e,bool call,bool hint=false) {
    auto [owner,structure]=static_type(*e.parts[0]);if(!structure)return {};
    auto type=Type::nominal(structure->definition);const auto key=Type::key(type)+"::static::"+e.token.text;
    if(!hint && editor_cursor(e))editor_static_candidates(*owner,*structure,e.token);
    if(auto found=owner->static_fields.find(key);found!=owner->static_fields.end()) {
        auto& value=*found->second;static_access(type,value.visibility,e.token);
        if(!current)error(source,e.token,Code::constant_call,"A compile-time expression cannot read a static runtime field.");
        if(value.type==Type::none)owner->resolve_global(value);
        if(!e.type_arguments.empty())error(source,e.token,Code::unsupported_type,"Static fields do not accept type arguments.");
        if(call && !(value.type.definition && value.type.definition->callable))error(source,e.token,Code::not_callable,"A static field is not callable.");
        if(hint)return call?std::optional<Type>(Type(*value.type.definition->call_result)):std::optional<Type>(value.type);
        const auto& base=*e.parts[0];e.enum_type_use=std::make_unique<TypeUse>(TypeUse{base.token,type,base.kind==Expr::Kind::field?std::optional<Token>{base.parts[0]->token}:std::nullopt});
        if(call) {
            auto selected=std::make_unique<Expr>(Expr::Kind::global,e.token);selected->type=value.type;selected->target=value.identity;selected->enum_type_use=std::move(e.enum_type_use);
            e.parts[0]=std::move(selected);e.kind=Expr::Kind::function_call;return function_invocation(e);
        }
        e.kind=Expr::Kind::global;e.target=value.identity;e.parts.clear();return e.type=value.type;
    }
    const Function* target=nullptr;bool specialized_method=false;
    if(auto found=owner->static_methods.find(key);found!=owner->static_methods.end())target=found->second;
    if(!target)for(auto& method:owner->generic_functions)if(method.static_owner && (method.symbol==key || (structure->generic_origin && method.symbol==structure->generic_origin->definition->identity+"::static::"+e.token.text)) && !method.generics.empty()) {
        static_access(type,method.visibility,e.token);
        // Hints use the same concrete specialization as checked calls. A
        // template result cannot describe an argument to outer inference.
        std::vector<std::pair<Type,Expr*>> supplied;
        if(call){auto mapping=argument_mapping(e,method,1,false);for(unsigned n=0;n<mapping.size();++n)supplied.emplace_back(method.parameters[mapping[n]].type,e.parts[n+1].get());}
        auto arguments=infer_arguments(*owner,method.generics,e.type_arguments,supplied,e.token);
        target=&unit.specialize_function(unit,*owner,method,arguments,e.token,current?current->specialization_depth+1:0,type);specialized_method=true;break;
    }
    if(!target)error(source,e.token,Code::unknown_method,"Unknown static member '"+structure->token.text+"."+e.token.text+"'.");
    static_access(type,target->visibility,e.token);
    if(hint) {
        if(call)return call_result_anchor(*target);
        std::vector<Type> parameters;for(const auto& parameter:target->parameters)parameters.push_back(parameter.type);
        return unit.make_callable(parameters,target->result);
    }
    if(!specialized_method && !e.type_arguments.empty())error(source,e.token,Code::unsupported_type,"This static method is not generic.");
    const auto& base=*e.parts[0];e.enum_type_use=std::make_unique<TypeUse>(TypeUse{base.token,type,base.kind==Expr::Kind::field?std::optional<Token>{base.parts[0]->token}:std::nullopt});
    if(call) {
        auto docs=source_documentation(owner->source,target->token.offset);if(diagnostics.editor)editor_call(e,*target,1,&docs);
        const auto result=call_arguments(e,*target,1);e.parts.erase(e.parts.begin());e.kind=Expr::Kind::call;e.target=target->symbol;e.op="$static_call";
        return e.type=result;
    }
    if(target->asynchronous || !target->returns.empty())error(source,e.token,Code::unsupported_type,"Async or multiple-return static methods require a direct call.");
    std::vector<Type> parameters;for(const auto& parameter:target->parameters)parameters.push_back(parameter.type);
    e.parts.clear();e.kind=Expr::Kind::function_reference;e.target=target->symbol;return e.type=unit.make_callable(parameters,target->result);
}
