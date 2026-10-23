// Only dependent type/body checks wait for instantiation. Names, declaration
// structure and constraint declarations are checked even for unused templates.
for(auto& [id,u]:units) {
    auto parameters=[&](const auto& declaration) {
        std::set<std::string> names;
        for(const auto& p:declaration.generics) {
            names.insert(p.token.text);
            if(p.constraint) {
                auto type=resolve_type(*p.constraint,*u,root,units,p.token);
                if(type.kind!=Type::interface_ref || type.nullable)error(u->source,p.token,Code::unsupported_type,"A generic constraint must name a non-nullable accessible interface.");
            }
        }
        return names;
    };
    std::function<void(Type,const Token&,const std::set<std::string>&)> type=[&](Type raw,const Token& token,const auto& names) {
        if(raw.kind==Type::none)return;
        if(raw.kind==Type::unresolved && names.contains(raw.definition->name)) {
            if(raw.nullable)error(u->source,token,Code::unsupported_type,"Nullable generic parameters are not implemented.");
            return;
        }
        if(raw.definition && raw.definition->callable) {
            auto callable=std::find_if(u->function_types.begin(),u->function_types.end(),[&](const auto& syntax){return syntax.definition==raw.definition;});
            if(callable!=u->function_types.end()){for(auto parameter:callable->parameters)type(parameter,token,names);type(callable->result,token,names);}return;
        }
        if(raw.definition && raw.definition->result_type) {
            auto result=std::find_if(u->results.begin(),u->results.end(),[&](const auto& syntax){return syntax.definition==raw.definition;});
            if(result!=u->results.end()){type(result->success,token,names);type(result->failure,token,names);}return;
        }
        if(ir::collection_type(raw)) {
            auto a=std::find_if(u->arrays.begin(),u->arrays.end(),[&](const auto& a){return a.definition==raw.definition;});
            if(a!=u->arrays.end()){if(a->key)type(*a->key,token,names);else (void)array_extent(*u,*a);type(a->element,token,names);}
            return;
        }
        auto application=std::find_if(u->applications.begin(),u->applications.end(),[&](const auto& a){return a.definition==raw.definition;});
        if(application!=u->applications.end()) {
            auto [owner,declaration]=structure_lookup(*u,root,units,application->name,application->token);(void)owner;
            if(!declaration || declaration->generics.empty())error(u->source,application->token,Code::unsupported_type,"Type '"+application->name+"' is not a generic struct.");
            if(raw.nullable && !declaration->definition->managed)error(u->source,token,Code::unsupported_type,"Only managed references can be nullable.");
            if(application->arguments.size()!=declaration->generics.size())error(u->source,application->token,Code::argument_count,"Incorrect number of generic type arguments.");
            for(auto argument:application->arguments)type(argument,application->token,names);
        } else (void)resolve_type(raw,*u,root,units,token);
    };
    for(auto& s:u->generic_structures)try {
        auto names=parameters(s);std::set<std::string> fields;
        if(s.base_type)type(*s.base_type,s.base_token,names);
        for(const auto& f:s.fields) {
            if(!fields.insert(f.name).second)error(u->source,f.token,Code::duplicate,"Duplicate generic field '"+f.name+"'.");
            type(*f.annotation,f.token,names);
        }
        std::function<std::string(Type)> operand_key=[&](Type raw) {
            Depth guard(diagnostics.semantic_depth(),u->source,s.token,128);
            std::string key=std::to_string(raw.kind)+":"+Type::key(raw)+(raw.nullable?"?":"");
            auto add=[&](const std::string& part){key+=":"+std::to_string(part.size())+":"+part;};
            for(const auto& a:u->applications)if(a.definition==raw.definition){auto [module,owner]=structure_lookup(*u,root,units,a.name,a.token);(void)module;key=owner->definition->identity+(raw.nullable?"?":"");for(auto argument:a.arguments)add(operand_key(argument));return key;}
            for(auto& a:u->arrays)if(a.definition==raw.definition){key=a.key?"dictionary":a.extent?"fixed":"list";if(a.key)add(operand_key(*a.key));else if(a.extent)add(std::to_string(*array_extent(*u,a)));add(operand_key(a.element));return key;}
            for(const auto& r:u->results)if(r.definition==raw.definition){key="result";add(operand_key(r.success));add(operand_key(r.failure));return key;}
            for(const auto& f:u->function_types)if(f.definition==raw.definition){key=raw.nullable?"func?":"func";for(auto p:f.parameters)add(operand_key(p));add(operand_key(f.result));return key;}
            if(raw.kind==Type::unresolved && !names.contains(raw.definition->name)){auto concrete=resolve_type(raw,*u,root,units,s.token);key=std::to_string(concrete.kind)+":"+Type::key(concrete)+(concrete.nullable?"?":"");}
            return key;
        };
        std::set<std::string> operators;
        for(auto& f:s.operators) {
            bool owned=false;
            for(const auto& p:f.parameters)for(const auto& a:u->applications)if(a.definition==p.type.definition) {
                auto [module,owner]=structure_lookup(*u,root,units,a.name,a.token);
                if(module!=u.get() || owner!=&s || a.arguments.size()!=s.generics.size())continue;
                bool matching=true;for(unsigned n=0;n<a.arguments.size();++n)matching &= a.arguments[n].kind==Type::unresolved && !a.arguments[n].nullable && a.arguments[n].definition->name==s.generics[n].token.text;
                owned|=matching;
            }
            if(!owned)error(u->source,f.token,Code::receiver,"A generic operator requires an operand of its matching instantiated owner type.");
            std::string key=f.operator_glyph+":"+std::to_string(f.parameters.size());
            for(const auto& p:f.parameters){type(p.type,p.token,names);const auto identity=operand_key(p.type);key+=":"+std::to_string(identity.size())+":"+identity;}
            type(f.result,f.token,names);
            if(!operators.insert(key).second)error(u->source,f.token,Code::duplicate,"Duplicate generic operator operand tuple.");
            Semantics semantics(*u,units,root,evaluate,diagnostics,defaults);semantics.validate_header_names(f);semantics.validate_parameter_structure(f);
        }

    }catch(const CompilationError& failure){diagnostics.add(failure);}catch(const Poison&){}
    std::set<std::string> methods;
    for(auto& f:u->generic_functions)try {
        auto names=parameters(f);Semantics semantics(*u,units,root,evaluate,diagnostics,defaults);
        if(f.receiver) {
            if(f.receiver->type.nullable)error(u->source,f.receiver->token,Code::receiver,"A receiver must be non-nullable.");
            Structure* owner=nullptr;
            auto application=std::find_if(u->applications.begin(),u->applications.end(),[&](const auto& a){return a.definition==f.receiver->type.definition;});
            if(application!=u->applications.end()) {
                auto found=structure_lookup(*u,root,units,application->name,application->token);
                if(found.first!=u.get() || !found.second || found.second->generics.empty())error(u->source,f.receiver->token,Code::receiver,"A generic receiver must name an owning-module generic struct.");
                owner=found.second;
                if(application->arguments.size()!=owner->generics.size())error(u->source,f.receiver->token,Code::argument_count,"Generic receiver parameter count mismatch.");
                for(unsigned n=0;n<owner->generics.size();++n) {
                    auto argument=application->arguments[n];const auto& parameter=owner->generics[n];
                    if(argument.kind!=Type::unresolved || argument.nullable || argument.definition->name!=parameter.token.text)
                        error(u->source,f.receiver->token,Code::receiver,"A generic receiver must bind its owner's declared type parameter names.");
                    if(!names.insert(parameter.token.text).second)error(u->source,parameter.token,Code::duplicate,"Method type parameters cannot shadow owner type parameters.");
                }
            } else {
                auto receiver=resolve_type(f.receiver->type,*u,root,units,f.receiver->token);
                for(auto& s:u->structures)if(s.definition==receiver.definition)owner=&s;
                if(!owner)error(u->source,f.receiver->token,Code::receiver,"Methods must be declared in their receiver's owning module.");
            }
            if(!f.generics.empty() && owner->generics.empty())semantics.validate_generic_receiver(Type(owner->definition,owner->definition->managed?Type::managed_ref:Type::aggregate),f.token);
            f.symbol=owner->definition->identity+"::method::"+f.token.text;
            if(!methods.insert(f.symbol).second || u->methods.contains(f.symbol))error(u->source,f.token,Code::duplicate,"Duplicate generic receiver method.");
            for(const auto& field:owner->fields)if(field.name==f.token.text)error(u->source,f.token,Code::duplicate,"Method conflicts with a stored field.");
            if(!f.generics.empty() && (f.token.text=="init" || f.virtual_method || f.overriding))error(u->source,f.token,Code::unsupported_type,"Initializers and virtual methods cannot introduce method-specific type parameters.");
        }
        for(const auto& p:f.parameters) {
            type(p.type,p.token,names);
        }
        type(f.result,f.token,names);
        semantics.validate_header_names(f);semantics.validate_parameter_structure(f);
    }catch(const CompilationError& failure){diagnostics.add(failure);}catch(const Poison&){}
}
