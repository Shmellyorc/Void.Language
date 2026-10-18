// Inference uses declaration shapes and the existing typed-anchor resolver. It
// never evaluates, lowers or rewrites argument expressions ahead of source order.
Unit& generic_owner(const Function& function) {
    for(const auto& [id,owner]:units)for(const auto& candidate:owner->generic_functions)if(&candidate==&function)return *owner;
    throw std::logic_error("Missing generic declaration owner");
}
Type inference_type(Expr& e) {
    if(auto hinted=anchor(e);hinted && hinted->kind!=Type::unresolved && *hinted!=Type::none)return *hinted;
    if(e.kind==Expr::Kind::dictionary_literal && !e.parts.empty()){auto key=inference_type(*e.parts[0]),value=inference_type(*e.parts[1]);if(key!=error_type && value!=error_type)return unit.make_dictionary(key,value);}
    if(e.kind==Expr::Kind::array_literal) {
        if(e.parts.empty())return error_type;
        std::optional<Type> element;
        for(auto& part:e.parts)if(auto hint=anchor(*part)){element=*hint;break;}
        if(!element)element=inference_type(*e.parts.front());
        if(*element==error_type)return error_type;
        auto definition=std::make_shared<ir::ValueType>();definition->name=definition->identity=ir::array_identity(*element,{});definition->element=ir::FieldType(*element);
        return Type(definition,Type::growable_array);
    }
    if(e.token.text=="null")return error_type;
    if(e.kind==Expr::Kind::literal && !e.literal_text.empty())return decimal(e)?Type::f32:Type::i32;
    if(e.kind==Expr::Kind::unary || e.kind==Expr::Kind::binary) {
        if(auto type=anchor(e))return *type;
        return decimal(e)?Type::f32:Type::i32;
    }
    return error_type;
}
std::vector<Type> infer_arguments(Unit& owner,const std::vector<GenericParameter>& parameters,const std::vector<Type>& explicit_arguments,
                                const std::vector<std::pair<Type,Expr*>>& supplied,const Token& site) {
    if(!explicit_arguments.empty()) {
        if(explicit_arguments.size()!=parameters.size())error(source,site,Code::argument_count,"Incorrect number of generic type arguments.");
        std::vector<Type> result;for(auto t:explicit_arguments)result.push_back(resolve_type(t,unit,root,units,site));return result;
    }
    std::map<std::string,Type> inferred;
    auto parameter=[&](Type pattern){return pattern.kind==Type::unresolved && std::any_of(parameters.begin(),parameters.end(),[&](const auto& p){return p.token.text==pattern.definition->name;});};
    std::function<void(Type,Type)> match=[&](Type pattern,Type actual) {
        if(actual==error_type || actual==Type::null_value)return;
        if(parameter(pattern)) {
            actual=ir::with_readonly_paths(actual,{});actual.readonly=false;
            auto [found,inserted]=inferred.emplace(pattern.definition->name,actual);
            if(!inserted && found->second!=actual)error(source,site,Code::type_mismatch,"Conflicting inference for generic parameter '"+pattern.definition->name+"': "+ir::type_name(found->second)+" and "+ir::type_name(actual)+".");
        } else if(pattern.definition && pattern.definition->callable && actual.definition && actual.definition->callable){
            auto found=std::find_if(owner.function_types.begin(),owner.function_types.end(),[&](const auto& syntax){return syntax.definition==pattern.definition;});
            if(found!=owner.function_types.end() && found->parameters.size()==actual.definition->call_parameters.size()){for(unsigned n=0;n<found->parameters.size();++n)match(found->parameters[n],actual.definition->call_parameters[n]);match(found->result,*actual.definition->call_result);}
        } else if(pattern.definition && pattern.definition->result_type && actual.definition && actual.definition->result_type) {
            auto found=std::find_if(owner.results.begin(),owner.results.end(),[&](const auto& syntax){return syntax.definition==pattern.definition;});
            if(found!=owner.results.end()){match(found->success,ir::field_type(actual,1));match(found->failure,ir::field_type(actual,2));}
        } else if(ir::collection_type(pattern)) {
            auto found=std::find_if(owner.arrays.begin(),owner.arrays.end(),[&](const auto& a){return a.definition==pattern.definition;});
            if(found!=owner.arrays.end() && ir::collection_type(actual) && actual.kind==pattern.kind){if(found->key){match(*found->key,ir::dictionary_key(actual));match(found->element,ir::dictionary_value(actual));}else match(found->element,ir::array_element(actual));}
        } else if(pattern.kind==Type::unresolved) {
            auto application=std::find_if(owner.applications.begin(),owner.applications.end(),[&](const auto& a){return a.definition==pattern.definition;});
            if(application==owner.applications.end())return;
            auto [declaration_owner,generic]=structure_lookup(owner,root,units,application->name,application->token);(void)declaration_owner;
            for(const auto& [id,u]:units)for(const auto& concrete:u->structures)if(concrete.definition==actual.definition && concrete.generic_origin==generic)
                for(unsigned n=0;n<application->arguments.size() && n<concrete.type_arguments.size();++n)match(application->arguments[n],concrete.type_arguments[n]);
        }
    };
    std::set<std::string> anchored;
    std::function<void(Type,Expr&,bool)> argument=[&](Type pattern,Expr& expression,bool fallback) {
        if(pattern.kind==Type::dictionary && expression.kind==Expr::Kind::dictionary_literal){auto found=std::find_if(owner.arrays.begin(),owner.arrays.end(),[&](const auto& a){return a.definition==pattern.definition;});if(found!=owner.arrays.end() && found->key){for(unsigned n=0;n<expression.parts.size();n+=2){argument(*found->key,*expression.parts[n],fallback);argument(found->element,*expression.parts[n+1],fallback);}return;}}
        if(ir::array_type(pattern) && expression.kind==Expr::Kind::array_literal) {
            auto found=std::find_if(owner.arrays.begin(),owner.arrays.end(),[&](const auto& a){return a.definition==pattern.definition;});
            if(found!=owner.arrays.end()){for(auto& item:expression.parts)argument(found->element,*item,fallback);return;}
        }
        if(auto hint=anchor(expression);hint && hint->kind!=Type::unresolved)match(pattern,*hint);
        else if(fallback && !(parameter(pattern) && anchored.contains(pattern.definition->name)))match(pattern,inference_type(expression));
    };
    for(auto [pattern,expression]:supplied)argument(pattern,*expression,false);
    for(const auto& [name,type]:inferred){(void)type;anchored.insert(name);}
    for(auto [pattern,expression]:supplied)argument(pattern,*expression,true);
    std::vector<Type> result;
    for(const auto& p:parameters) {
        auto found=inferred.find(p.token.text);
        if(found==inferred.end())error(source,site,Code::type_mismatch,"Cannot infer generic parameter '"+p.token.text+"'; supply explicit type arguments.");
        result.push_back(found->second);
    }
    return result;
}
const Function& concrete_function(Expr& e,const Function& declaration,unsigned first=0) {
    if(declaration.generics.empty()) {
        if(!e.type_arguments.empty())error(source,e.token,Code::unsupported_type,"Function '"+e.token.text+"' is not generic.");
        return declaration;
    }
    auto& generic=const_cast<Function&>(declaration);auto& owner=generic_owner(generic);
    if(e.parts.size()-first!=generic.parameters.size())error(source,e.token,Code::argument_count,"Incorrect argument count for generic function '"+e.token.text+"'.");
    std::vector<std::pair<Type,Expr*>> supplied;for(unsigned n=0;n<e.parts.size()-first;++n)supplied.emplace_back(generic.parameters[n].type,e.parts[n+first].get());
    auto arguments=infer_arguments(owner,generic.generics,e.type_arguments,supplied,e.token);
    return unit.specialize_function(unit,owner,generic,arguments,e.token,current?current->specialization_depth+1:0,Type::none);
}
Structure& concrete_structure(Expr& e,Unit& owner,Structure& declaration,unsigned first=0) {
    if(declaration.generics.empty()) {
        if(!e.type_arguments.empty())error(source,e.token,Code::unsupported_type,"Type '"+e.token.text+"' is not generic.");
        return declaration;
    }
    std::vector<std::pair<Type,Expr*>> supplied;
    const Function* initializer=nullptr;
    for(const auto& f:owner.generic_functions)if(f.receiver && f.token.text=="init")
        for(const auto& a:owner.applications)if(a.definition==f.receiver->type.definition && a.name==declaration.token.text)initializer=&f;
    for(unsigned n=0;n<e.parts.size()-first;++n) {
        const auto name=e.names[n].text;std::optional<Type> pattern;
        if(initializer) {
            if(name.empty() && n<initializer->parameters.size())pattern=initializer->parameters[n].type;
            else for(const auto& p:initializer->parameters)if(p.token.text==name)pattern=p.type;
        } else for(const auto& field:declaration.fields)if(field.name==name)pattern=*field.annotation;
        if(!pattern)error(source,name.empty()?e.token:e.names[n],Code::argument_count,"Unknown generic constructor argument; use its declared field or initializer parameter.");
        supplied.emplace_back(*pattern,e.parts[n+first].get());
    }
    auto arguments=infer_arguments(owner,declaration.generics,e.type_arguments,supplied,e.token);
    return unit.specialize_structure(unit,owner,declaration,arguments,e.token);
}

const Function* concrete_method(Expr& e,Type receiver,bool reference=false) {
    if(!receiver.definition)return nullptr;
    for(const auto& [id,u]:units)for(auto& generic:u->generic_functions) {
        if(!generic.receiver || generic.generics.empty() || generic.token.text!=e.token.text)continue;
        const Structure* actual=nullptr;for(const auto& s:u->structures)if(s.definition==receiver.definition)actual=&s;
        if(!actual)continue;
        const auto owner=actual->generic_origin?actual->generic_origin->definition:actual->definition;
        if(generic.symbol!=owner->identity+"::method::"+e.token.text)continue;
        if((u.get()!=&unit && generic.file_private) || (u.get()!=&unit && u.get()!=&root && !std::any_of(unit.imports.begin(),unit.imports.end(),[&](const auto& i){return i.target==id;})))
            error(source,e.token,Code::visibility,"Generic method is not accessible from this module.");
        validate_generic_receiver(receiver,e.token);
        if(!reference && e.parts.size()-1!=generic.parameters.size())error(source,e.token,Code::argument_count,"Incorrect generic method argument count.");
        std::vector<std::pair<Type,Expr*>> supplied;if(!reference)for(unsigned n=0;n<generic.parameters.size();++n)supplied.emplace_back(generic.parameters[n].type,e.parts[n+1].get());
        auto arguments=infer_arguments(*u,generic.generics,e.type_arguments,supplied,e.token);
        return &unit.specialize_function(unit,*u,generic,arguments,e.token,current?current->specialization_depth+1:0,receiver);
    }
    if(!e.type_arguments.empty())error(source,e.token,Code::unsupported_type,"Method '"+e.token.text+"' does not introduce generic type parameters.");
    return nullptr;
}
