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
        if(ir::collection_type(raw)) {
            auto a=std::find_if(u->arrays.begin(),u->arrays.end(),[&](const auto& a){return a.definition==raw.definition;});
            if(a!=u->arrays.end()){if(a->key)type(*a->key,token,names);else (void)array_extent(*u,*a);type(a->element,token,names);}
            return;
        }
        auto application=std::find_if(u->applications.begin(),u->applications.end(),[&](const auto& a){return a.definition==raw.definition;});
        if(application!=u->applications.end()) {
            auto [owner,declaration]=structure_lookup(*u,root,units,application->name,application->token);(void)owner;
            if(!declaration || declaration->generics.empty())error(u->source,application->token,Code::unsupported_type,"Type '"+application->name+"' is not a generic struct.");
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
