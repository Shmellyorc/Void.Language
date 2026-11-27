// Only dependent type/body checks wait for instantiation. Names, declaration
// structure and constraint declarations are checked even for unused templates.
for(auto& [id,u]:units) {
    std::function<void(Type,const Token&,const std::set<std::string>&)> type=[&](Type raw,const Token& token,const auto& names) {
        if(raw.kind==Type::none)return;
        if(raw.kind==Type::unresolved && names.contains(raw.definition->name)) {
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
            if(!declaration && (application->name=="option" || application->name=="weak")) {
                if(application->arguments.size()!=1)
                    error(u->source,application->token,Code::argument_count,application->name+"<T> requires exactly one type argument.");
                type(application->arguments.front(),application->token,names);
                return;
            }
            if(!declaration || declaration->generics.empty())error(u->source,application->token,Code::unsupported_type,"Type '"+application->name+"' is not a generic type.");
            if(application->arguments.size()!=declaration->generics.size())error(u->source,application->token,Code::argument_count,"Incorrect number of generic type arguments.");
            for(auto argument:application->arguments)type(argument,application->token,names);
        } else (void)resolve_type(raw,*u,root,units,token);
    };
    auto parameter_constraints=[&](const auto& declaration,const std::set<std::string>& names) {
        for(const auto& p:declaration.generics)if(p.constraint) {
            type(*p.constraint,p.token,names);
            bool interface=false;
            auto application=std::find_if(u->applications.begin(),u->applications.end(),[&](const auto& a){return a.definition==p.constraint->definition;});
            if(application!=u->applications.end()) {
                auto selected=structure_lookup(*u,root,units,application->name,application->token);
                interface=selected.second && selected.second->definition->interface_type;
            } else interface=resolve_type(*p.constraint,*u,root,units,p.token).kind==Type::interface_ref;
            if(!interface || p.constraint->nullable)error(u->source,p.token,Code::unsupported_type,"A generic constraint must name a non-nullable accessible interface.");
        }
    };
    auto parameters=[](const auto& declaration) {
        std::set<std::string> names;for(const auto& p:declaration.generics)names.insert(p.token.text);return names;
    };
    for(auto& s:u->generic_structures)try {
        auto names=parameters(s);parameter_constraints(s,names);std::set<std::string> fields;
        if(s.base_type)type(*s.base_type,s.base_token,names);
        for(const auto& f:s.static_fields) {
            if(!fields.insert(f.name).second)error(u->source,f.token,Code::duplicate,"Duplicate generic static field.");
            if(f.annotation)type(*f.annotation,f.token,names);
        }
        for(const auto& f:s.fields) {
            if(!fields.insert(f.name).second)error(u->source,f.token,Code::duplicate,"Duplicate generic field '"+f.name+"'.");
            type(*f.annotation,f.token,names);
        }
        auto requirement_visibility=[&](Type raw,const Token& token) {
            if(raw.kind==Type::unresolved && names.contains(raw.definition->name))return;
            if(ir::nullable_value(raw))raw=ir::nullable_payload(raw);
            auto definition=raw.definition;
            if(raw.kind==Type::unresolved) {
                auto application=std::find_if(u->applications.begin(),u->applications.end(),[&](const auto& a){return a.definition==raw.definition;});
                if(application!=u->applications.end()) {
                    auto selected=structure_lookup(*u,root,units,application->name,application->token);
                    if(selected.second)definition=selected.second->definition;
                } else {auto resolved=resolve_type(raw,*u,root,units,token);if(ir::nullable_value(resolved))resolved=ir::nullable_payload(resolved);definition=resolved.definition;}
            }
            if(definition && ((s.visibility=="public" && definition->visibility!="public") ||
                              (s.visibility=="internal" && definition->visibility=="private")))
                error(u->source,token,Code::visibility,"An interface requirement cannot expose a less-visible type.");
        };
        std::set<std::string> requirements;
        for(auto& requirement:s.requirements) {
            if(!requirements.insert(requirement.token.text).second)error(u->source,requirement.token,Code::duplicate,"Duplicate interface requirement.");
            for(const auto& parameter:requirement.parameters){type(parameter.type,parameter.token,names);requirement_visibility(parameter.type,parameter.token);}
            type(requirement.result,requirement.token,names);
            if(requirement.returns.empty())requirement_visibility(requirement.result,requirement.token);
            for(const auto& component:requirement.returns){type(component.type,component.site,names);requirement_visibility(component.type,component.site);}
            semantics.validate_header(*u,requirement);
        }
        std::function<std::string(Type)> operand_key=[&](Type raw) {
            Depth guard(diagnostics.semantic_depth(),u->source,s.token,128);
            std::string key=std::to_string(raw.kind)+":"+Type::key(raw)+(raw.nullable?"?":"");
            auto add=[&](const std::string& part){key+=":"+std::to_string(part.size())+":"+part;};
            for(const auto& a:u->applications)if(a.definition==raw.definition){auto [module,owner]=structure_lookup(*u,root,units,a.name,a.token);(void)module;key=(owner?owner->definition->identity:"$"+a.name)+(raw.nullable?"?":"");for(auto argument:a.arguments)add(operand_key(argument));return key;}
            for(auto& a:u->arrays)if(a.definition==raw.definition){key=std::string(a.key?"dictionary":a.extent?"fixed":"list")+(raw.nullable?"?":"");if(a.key)add(operand_key(*a.key));else if(a.extent)add(std::to_string(*array_extent(*u,a)));add(operand_key(a.element));return key;}
            for(const auto& r:u->results)if(r.definition==raw.definition){key=raw.nullable?"result?":"result";add(operand_key(r.success));add(operand_key(r.failure));return key;}
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
                owned|=matching && (!p.type.nullable || s.definition->managed);
            }
            if(!owned)error(u->source,f.token,Code::receiver,"A generic operator requires an operand of its matching instantiated owner type; a nullable value operand is not an owner.");
            std::string key=f.operator_glyph+":"+std::to_string(f.parameters.size());
            for(const auto& p:f.parameters){type(p.type,p.token,names);const auto identity=operand_key(p.type);key+=":"+std::to_string(identity.size())+":"+identity;}
            type(f.result,f.token,names);
            if(!operators.insert(key).second)error(u->source,f.token,Code::duplicate,"Duplicate generic operator operand tuple.");
            semantics.validate_header(*u,f);
        }

    }catch(const CompilationError& failure){diagnostics.add(failure);}catch(const Poison&){}
    std::set<std::string> methods;
    for(auto& f:u->generic_functions)try {
        auto names=parameters(f);
        const auto* receiver=f.receiver?&*f.receiver:f.static_owner?&*f.static_owner:nullptr;
        if(receiver) {
            if(receiver->type.nullable)error(u->source,receiver->token,Code::receiver,"A receiver must be non-nullable.");
            Structure* owner=nullptr;
            auto application=std::find_if(u->applications.begin(),u->applications.end(),[&](const auto& a){return a.definition==receiver->type.definition;});
            if(application!=u->applications.end()) {
                auto found=structure_lookup(*u,root,units,application->name,application->token);
                if(found.first!=u.get() || !found.second || found.second->generics.empty())error(u->source,receiver->token,Code::receiver,"A generic receiver must name an owning-module generic struct.");
                owner=found.second;
                if(application->arguments.size()!=owner->generics.size())error(u->source,receiver->token,Code::argument_count,"Generic receiver parameter count mismatch.");
                for(unsigned n=0;n<owner->generics.size();++n) {
                    auto argument=application->arguments[n];const auto& parameter=owner->generics[n];
                    if(argument.kind!=Type::unresolved || argument.nullable || argument.definition->name!=parameter.token.text)
                        error(u->source,receiver->token,Code::receiver,"A generic receiver must bind its owner's declared type parameter names.");
                    if(!names.insert(parameter.token.text).second)error(u->source,parameter.token,Code::duplicate,"Method type parameters cannot shadow owner type parameters.");
                }
            } else {
                auto resolved_receiver=resolve_type(receiver->type,*u,root,units,receiver->token);
                for(auto& s:u->structures)if(s.definition==resolved_receiver.definition)owner=&s;
                if(!owner)error(u->source,receiver->token,Code::receiver,"Methods must be declared in their receiver's owning module.");
            }
            if(owner->definition->interface_type)error(u->source,receiver->token,Code::receiver,"Interfaces declare requirements, not receiver or static implementations.");
            if(f.receiver && !f.generics.empty() && owner->generics.empty())semantics.validate_generic_receiver(*u,Type(owner->definition,owner->definition->managed?Type::managed_ref:Type::aggregate),f.token);
            f.symbol=owner->definition->identity+(f.static_owner?"::static::":"::method::")+f.token.text;
            if(!methods.insert(owner->definition->identity+"::member::"+f.token.text).second || u->methods.contains(owner->definition->identity+"::method::"+f.token.text) || u->static_methods.contains(owner->definition->identity+"::static::"+f.token.text))error(u->source,f.token,Code::duplicate,"Static and instance methods cannot share a name.");
            if(!methods.insert(f.symbol).second || u->methods.contains(f.symbol))error(u->source,f.token,Code::duplicate,"Duplicate generic receiver method.");
            for(const auto& field:owner->static_fields)if(field.name==f.token.text)error(u->source,f.token,Code::duplicate,"Method conflicts with a static field.");
            for(const auto& field:owner->fields)if(field.name==f.token.text)error(u->source,f.token,Code::duplicate,"Method conflicts with a stored field.");
            if(!f.generics.empty() && (f.token.text=="init" || f.virtual_method || f.overriding))error(u->source,f.token,Code::unsupported_type,"Initializers and virtual methods cannot introduce method-specific type parameters.");
        }
        parameter_constraints(f,names);
        for(const auto& p:f.parameters) {
            type(p.type,p.token,names);
        }
        type(f.result,f.token,names);
        for(const auto& component:f.returns)type(component.type,component.site,names);
        semantics.validate_header(*u,f);
    }catch(const CompilationError& failure){diagnostics.add(failure);}catch(const Poison&){}
}
