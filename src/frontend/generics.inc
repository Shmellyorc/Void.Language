// Specializations are ordinary declarations in their original owning Unit.
// Reparse their already syntax-checked tokens with concrete type bindings through
// the same Parser; all layout, initialization, effects and lowering stay shared.
std::size_t generic_key_work=0;
auto specialization_name=[&](const SourceFile& source,const Token& token,const std::string& name,const std::vector<Type>& arguments) {
    std::string result=name+"<";
    for(const auto& t:arguments) {
        const auto key=std::to_string(static_cast<unsigned>(t.kind))+":"+Type::key(t)+(t.nullable?"?":"");
        if(key.size()>8388608-generic_key_work)error(source,token,Code::limit,"Generic specialization identity work budget exceeded.");
        generic_key_work+=key.size();result+=std::to_string(key.size())+":"+key;
    }
    return result+">";
};
auto transfer_syntax=[](Unit& owner,Unit& parsed) {
    owner.function_types.splice(owner.function_types.end(),parsed.function_types);owner.results.splice(owner.results.end(),parsed.results);owner.arrays.splice(owner.arrays.end(),parsed.arrays);owner.applications.splice(owner.applications.end(),parsed.applications);
    owner.type_uses.insert(owner.type_uses.end(),std::make_move_iterator(parsed.type_uses.begin()),std::make_move_iterator(parsed.type_uses.end()));
};
std::vector<std::function<void()>> pending_generic_constraints;
auto check_constraints=[&](Unit& caller,Unit& owner,const std::vector<GenericParameter>& parameters,const std::vector<Type>& arguments,const Token& site) {
    auto check=[&,caller=&caller,owner=&owner,parameters,arguments,site] {
        for(unsigned n=0;n<parameters.size();++n)if(parameters[n].constraint) {
            auto constraint=resolve_type(*parameters[n].constraint,*owner,root,units,parameters[n].token);
            try {ir::satisfy_constraint(*root.member_catalog,arguments[n],constraint,owner->identity);}
            catch(const std::invalid_argument& reason) {error(caller->source,site,Code::type_mismatch,"Generic constraint for '"+parameters[n].token.text+"' is not satisfied: "+reason.what(),{},{{span(owner->source,parameters[n].token),"Constraint declared here."}});}
        }
    };
    if(headers_ready)check();else pending_generic_constraints.push_back(std::move(check));
};
for(auto& [id,u]:units) {
    u->specialize_function=[&](Unit& caller,Unit& owner,Function& generic,const std::vector<Type>& arguments,const Token& site,unsigned depth,Type receiver)->Function& {
        if(arguments.size()!=generic.generics.size())error(caller.source,site,Code::argument_count,"Incorrect number of generic type arguments.");
        const auto method_name=generic.receiver?specialization_name(caller.source,site,generic.token.text,arguments):std::string{};
        const auto name=generic.receiver?receiver.definition->identity+"::method::"+method_name:specialization_name(caller.source,site,generic.symbol,arguments);
        if(auto found=owner.specialized_functions.find(name);found!=owner.specialized_functions.end())return *found->second;
        if(depth>=128)error(caller.source,site,Code::nesting,"Generic specialization dependency depth limit exceeded.");
        std::vector<RelatedLocation> context{{span(caller.source,site),"Generic specialization requested here."},{span(owner.source,generic.token),"Generic declaration."}};
        Diagnostics::Context diagnostic_context(diagnostics,context,depth);
        check_constraints(caller,owner,generic.generics,arguments,site);
        std::map<std::string,Type> bindings;
        if(generic.receiver)for(const auto& s:owner.structures)if(s.definition==receiver.definition && s.generic_origin)
            for(unsigned n=0;n<s.type_arguments.size();++n)bindings.emplace(s.generic_origin->generics[n].token.text,s.type_arguments[n]);
        for(unsigned n=0;n<arguments.size();++n)bindings.emplace(generic.generics[n].token.text,arguments[n]);
        Unit parsed(owner.source,owner.identity);Parser(owner.source,diagnostics,generic.syntax,std::move(bindings)).parse(parsed);
        if(parsed.parse_failed || parsed.functions.size()!=1)throw Poison{};
        transfer_syntax(owner,parsed);owner.functions.splice(owner.functions.end(),parsed.functions);
        auto& concrete=owner.functions.back();concrete.accessor=generic.accessor;
        for(const auto& p:generic.generics)concrete.substitution_sources.emplace(p.token.text,generic.symbol+"::type::"+p.token.text);
        if(generic.receiver)for(const auto& s:owner.structures)if(s.definition==receiver.definition && s.generic_origin)
            for(const auto& p:s.generic_origin->generics)concrete.substitution_sources.emplace(p.token.text,s.generic_origin->definition->identity+"::type::"+p.token.text);
        concrete.symbol=name;concrete.generic_origin=generic.symbol;concrete.concrete_name=name.substr(owner.identity.size()+2);concrete.display_name=generic.token.text;concrete.concrete_method_name=method_name;concrete.specialization_depth=depth;concrete.instantiation_context=context;
        owner.specialized_functions.emplace(name,&concrete);
        prepare_function(owner,concrete);
        Semantics(owner,units,root,evaluate,diagnostics,defaults).parameter_defaults(concrete);
        root.member_catalog->functions.push_back(member_header(owner,concrete));
        return concrete;
    };
    u->specialize_structure=[&](Unit& caller,Unit& owner,Structure& generic,const std::vector<Type>& arguments,const Token& site)->Structure& {
        if(arguments.size()!=generic.generics.size())error(caller.source,site,Code::argument_count,"Incorrect number of generic type arguments.");
        const auto name=specialization_name(caller.source,site,generic.token.text,arguments);
        if(auto found=owner.specialized_structures.find(name);found!=owner.specialized_structures.end())return *found->second;
        const auto depth=diagnostics.current_specialization_depth()+1;
        if(depth>=128)error(caller.source,site,Code::nesting,"Generic specialization dependency depth limit exceeded.");
        Depth guard(diagnostics.semantic_depth(),caller.source,site,128);
        std::vector<RelatedLocation> context{{span(caller.source,site),"Generic specialization requested here."},{span(owner.source,generic.token),"Generic declaration."}};
        Diagnostics::Context diagnostic_context(diagnostics,context,depth);
        check_constraints(caller,owner,generic.generics,arguments,site);
        std::map<std::string,Type> bindings;for(unsigned n=0;n<arguments.size();++n)bindings.emplace(generic.generics[n].token.text,arguments[n]);
        Unit parsed(owner.source,owner.identity);Parser(owner.source,diagnostics,generic.syntax,bindings).parse(parsed);
        if(parsed.parse_failed || parsed.structures.size()!=1)throw Poison{};
        transfer_syntax(owner,parsed);owner.structures.splice(owner.structures.end(),parsed.structures);
        auto& concrete=owner.structures.back();concrete.generic_origin=&generic;concrete.type_arguments=arguments;concrete.specialization_depth=depth;
        concrete.definition->name=name;concrete.definition->identity=owner.identity+"::"+name;concrete.definition->module_identity=owner.identity;concrete.definition->visibility=generic.visibility;
        concrete.definition->display_name=owner.identity+"::"+generic.token.text+"<";
        for(unsigned n=0;n<arguments.size();++n){if(n)concrete.definition->display_name+=", ";concrete.definition->display_name+=ir::type_name(arguments[n]);}
        concrete.definition->display_name+=">";
        concrete.definition->source_offset=generic.token.offset;concrete.definition->self=concrete.definition;concrete.definition->arena=arena;arena->definitions.push_back(concrete.definition);
        owner.specialized_structures.emplace(name,&concrete);owner.structure_names.emplace(name,&concrete);
        std::vector<Function*> methods;
        for(auto& method:owner.generic_functions)if(method.receiver && method.generics.empty()) {
            const auto application=std::find_if(owner.applications.begin(),owner.applications.end(),[&](const auto& a){return a.definition==method.receiver->type.definition;});
            if(application==owner.applications.end() || application->name!=generic.token.text)continue;
            Unit body(owner.source,owner.identity);Parser(owner.source,diagnostics,method.syntax,bindings).parse(body);
            if(body.parse_failed || body.functions.size()!=1)throw Poison{};
            transfer_syntax(owner,body);owner.functions.splice(owner.functions.end(),body.functions);auto& f=owner.functions.back();f.accessor=method.accessor;
            f.generic_origin=generic.definition->identity+"::method::"+method.token.text;f.display_name=generic.token.text+"::method::"+method.token.text;f.instantiation_context=context;f.specialization_depth=depth;
            for(const auto& p:generic.generics)f.substitution_sources.emplace(p.token.text,generic.definition->identity+"::type::"+p.token.text);
            methods.push_back(&f);
            if(f.token.text=="init")concrete.initializer=&f;
        }
        layout(owner,concrete);
        if(headers_ready) {
            for(auto* f:methods)prepare_function(owner,*f);
            validate_inheritance(units,root,diagnostics);
            root.member_catalog->nominal_types.emplace_back(concrete.definition,concrete.definition->managed?Type::managed_ref:Type::aggregate);
            for(auto* f:methods)root.member_catalog->functions.push_back(member_header(owner,*f));
            defaults(owner,concrete);
            for(auto* f:methods) {Semantics semantics(owner,units,root,evaluate,diagnostics,defaults);semantics.parameter_defaults(*f);semantics.initializer_fields(*f);}
        }
        return concrete;
    };
    u->resolve_application=[&,owner=u.get()](Type raw,const Token& site)->Type {
        auto found=std::find_if(owner->applications.begin(),owner->applications.end(),[&](const auto& a){return a.definition==raw.definition;});
        if(found==owner->applications.end())return raw;
        if(found->resolved!=Type::none)return found->resolved;
        auto [declaration_owner,generic]=structure_lookup(*owner,root,units,found->name,found->token);
        if(!generic || generic->generics.empty())error(owner->source,found->token,Code::unsupported_type,"Type '"+found->name+"' is not a generic struct.");
        std::vector<Type> arguments;for(auto argument:found->arguments)arguments.push_back(resolve_type(argument,*owner,root,units,site));
        auto& concrete=owner->specialize_structure(*owner,*declaration_owner,*generic,arguments,site);
        return found->resolved=Type(concrete.definition,concrete.definition->managed?Type::managed_ref:Type::aggregate);
    };
}
