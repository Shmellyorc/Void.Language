// Specializations are ordinary declarations in their original owning Unit.
// Reparse their already syntax-checked tokens with concrete type bindings through
// the same Parser; all layout, initialization, effects and lowering stay shared.
std::size_t generic_key_work=0;
auto specialization_name=[&](const SourceFile& source,const Token& token,const std::string& name,const std::vector<Type>& arguments) {
    for(const auto& t:arguments) {
        const auto key=std::to_string(static_cast<unsigned>(t.kind))+":"+Type::key(t)+(t.nullable?"?":"");
        if(key.size()>8388608-generic_key_work)error(source,token,Code::limit,"Generic specialization identity work budget exceeded.");
        generic_key_work+=key.size();
    }
    return ir::generic_identity(name,arguments);
};
auto transfer_syntax=[](Unit& owner,Unit& parsed) {
    owner.function_types.splice(owner.function_types.end(),parsed.function_types);owner.results.splice(owner.results.end(),parsed.results);owner.arrays.splice(owner.arrays.end(),parsed.arrays);owner.applications.splice(owner.applications.end(),parsed.applications);
    owner.type_uses.insert(owner.type_uses.end(),std::make_move_iterator(parsed.type_uses.begin()),std::make_move_iterator(parsed.type_uses.end()));
};
std::list<std::function<void()>> pending_generic_constraints;
bool preparing_generic_interfaces=false,pending_generic_interfaces=false;
auto check_constraints=[&](Unit& caller,Unit& owner,const std::vector<GenericParameter>& parameters,const std::vector<Type>& arguments,const Token& site,std::map<std::string,Type> bindings=std::map<std::string,Type>{},bool defer=false) {
    const auto depth=diagnostics.current_specialization_depth();
    auto check=[&,caller=&caller,owner=&owner,parameters,arguments,site,depth,bindings=std::move(bindings)]() mutable {
        Diagnostics::Context constraint_context(diagnostics,{},depth);
        for(unsigned n=0;n<parameters.size();++n)bindings.emplace(parameters[n].token.text,arguments[n]);
        for(unsigned n=0;n<parameters.size();++n)if(parameters[n].constraint) {
            // Reparse the existing constraint type syntax with this job's concrete
            // bindings, including nested applications/collections and nullability.
            auto raw=parse_constraint(*owner,diagnostics,parameters[n].constraint_syntax,bindings);
            auto constraint=resolve_type(raw,*owner,root,units,parameters[n].token);
            try {ir::satisfy_constraint(*root.member_catalog,arguments[n],constraint,owner->identity);}
            catch(const std::invalid_argument& reason) {error(caller->source,site,Code::type_mismatch,"Generic constraint for '"+parameters[n].token.text+"' is not satisfied: "+reason.what(),{},{{span(owner->source,parameters[n].token),"Constraint declared here."}});}
        }
    };
    if(headers_ready && !preparing_generic_interfaces && !defer)check();else pending_generic_constraints.push_back(std::move(check));
};
bool checking_generic_constraints=false;
auto complete_generic_constraints=[&] {
    if(checking_generic_constraints)return;
    checking_generic_constraints=true;
    try {
        while(!pending_generic_constraints.empty()) {
            auto check=std::move(pending_generic_constraints.front());pending_generic_constraints.pop_front();check();
        }
        checking_generic_constraints=false;
    }catch(...){checking_generic_constraints=false;throw;}
};
auto complete_generic_interface_headers=[&] {
    const bool outer=preparing_generic_interfaces;preparing_generic_interfaces=true;
    try {
        while(pending_generic_interfaces) {
            pending_generic_interfaces=false;
            for(auto& [id,module]:units)for(auto& f:module->functions)if(f.interface_requirement && !f.prepared) {
                prepare_function(*module,f);
                root.member_catalog->interface_requirements.push_back(member_header(*module,f));
            }
        }
        preparing_generic_interfaces=outer;
        if(!outer)complete_generic_constraints();
    }catch(...){preparing_generic_interfaces=outer;throw;}
};
for(auto& [id,u]:units) {
    u->specialize_function=[&](Unit& caller,Unit& owner,Function& generic,const std::vector<Type>& arguments,const Token& site,unsigned depth,Type receiver)->Function& {
        if(arguments.size()!=generic.generics.size())error(caller.source,site,Code::argument_count,"Incorrect number of generic type arguments.");
        // Canonical Math's public contract is primitive-only even when nominal
        // custom arithmetic/comparison operators could satisfy its source body.
        if(owner.identity==math::source_module && !generic.receiver &&
           (math::numeric_function(generic.token.text) || math::floating_function(generic.token.text))) {
            const bool floating=math::floating_function(generic.token.text);
            if(arguments.size()!=1 || !(floating?numeric::floating(arguments[0]):numeric::number(arguments[0])))
                error(caller.source,site,Code::unsupported_type,floating?
                      "Math function '"+generic.token.text+"' requires exactly f32 or f64.":
                      "Math function '"+generic.token.text+"' requires a numeric primitive, excluding bool, enums and nominal types.");
        }
        const auto method_name=(generic.receiver || generic.static_owner)?specialization_name(caller.source,site,generic.token.text,arguments):std::string{};
        const auto name=(generic.receiver || generic.static_owner)?receiver.definition->identity+(generic.static_owner?"::static::":"::method::")+method_name:specialization_name(caller.source,site,generic.symbol,arguments);
        if(auto found=owner.specialized_functions.find(name);found!=owner.specialized_functions.end())return *found->second;
        if(depth>=128)error(caller.source,site,Code::nesting,"Generic specialization dependency depth limit exceeded.");
        std::vector<RelatedLocation> context{{span(caller.source,site),"Generic specialization requested here."},{span(owner.source,generic.token),"Generic declaration."}};
        Diagnostics::Context diagnostic_context(diagnostics,context,depth);
        std::map<std::string,Type> bindings;
        if(generic.receiver || generic.static_owner)for(const auto& s:owner.structures)if(s.definition==receiver.definition && s.generic_origin)
            for(unsigned n=0;n<s.type_arguments.size();++n)bindings.emplace(s.generic_origin->generics[n].token.text,s.type_arguments[n]);
        check_constraints(caller,owner,generic.generics,arguments,site,bindings);
        for(unsigned n=0;n<arguments.size();++n)bindings.emplace(generic.generics[n].token.text,arguments[n]);
        Unit parsed(owner.source,owner.identity);parse_specialization(parsed,diagnostics,generic.syntax,std::move(bindings));
        if(parsed.parse_failed || parsed.functions.size()!=1)throw Poison{};
        transfer_syntax(owner,parsed);owner.functions.splice(owner.functions.end(),parsed.functions);
        auto& concrete=owner.functions.back();concrete.accessor=generic.accessor;
        concrete.generic_arguments=arguments;
        for(const auto& p:generic.generics)concrete.substitution_sources.emplace(p.token.text,generic.symbol+"::type::"+p.token.text);
        if(generic.receiver || generic.static_owner)for(const auto& s:owner.structures)if(s.definition==receiver.definition && s.generic_origin)
            for(const auto& p:s.generic_origin->generics)concrete.substitution_sources.emplace(p.token.text,s.generic_origin->definition->identity+"::type::"+p.token.text);
        concrete.symbol=name;concrete.generic_origin=generic.symbol;concrete.concrete_name=name.substr(owner.identity.size()+2);concrete.display_name=generic.token.text;concrete.concrete_method_name=method_name;concrete.specialization_depth=depth;concrete.instantiation_context=context;
        owner.specialized_functions.emplace(name,&concrete);
        prepare_function(owner,concrete);
        semantics.parameter_defaults(owner,concrete);
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
        check_constraints(caller,owner,generic.generics,arguments,site,{},generic.definition->interface_type);
        std::map<std::string,Type> bindings;for(unsigned n=0;n<arguments.size();++n)bindings.emplace(generic.generics[n].token.text,arguments[n]);
        Unit parsed(owner.source,owner.identity);parse_specialization(parsed,diagnostics,generic.syntax,bindings);
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
        for(auto& f:parsed.functions) {
            f.instantiation_context=context;f.specialization_depth=depth;f.generic_origin=generic.definition->identity+(f.interface_requirement?"::method::"+f.token.text:"::operator-template::"+f.operator_pattern);
            for(const auto& p:generic.generics)f.substitution_sources.emplace(p.token.text,generic.definition->identity+"::type::"+p.token.text);
            methods.push_back(&f);
        }
        owner.functions.splice(owner.functions.end(),parsed.functions);
        for(auto& method:owner.generic_functions)if((method.receiver || method.static_owner) && method.generics.empty()) {
            const auto& receiver=method.receiver?*method.receiver:*method.static_owner;
            const auto application=std::find_if(owner.applications.begin(),owner.applications.end(),[&](const auto& a){return a.definition==receiver.type.definition;});
            if(application==owner.applications.end() || application->name!=generic.token.text)continue;
            Unit body(owner.source,owner.identity);parse_specialization(body,diagnostics,method.syntax,bindings);
            if(body.parse_failed || body.functions.size()!=1)throw Poison{};
            transfer_syntax(owner,body);owner.functions.splice(owner.functions.end(),body.functions);auto& f=owner.functions.back();f.accessor=method.accessor;
            f.generic_origin=generic.definition->identity+(method.static_owner?"::static::":"::method::")+method.token.text;f.display_name=generic.token.text+"::method::"+method.token.text;f.instantiation_context=context;f.specialization_depth=depth;
            for(const auto& p:generic.generics)f.substitution_sources.emplace(p.token.text,generic.definition->identity+"::type::"+p.token.text);
            methods.push_back(&f);
            if(f.token.text=="init")concrete.initializer=&f;
        }
        layout(owner,concrete);
        if(headers_ready && concrete.definition->interface_type) {
            root.member_catalog->nominal_types.push_back(Type::nominal(concrete.definition));
            // Drain the existing declaration lists, rather than recursively
            // preparing every interface mentioned in a requirement signature.
            // This also lets the normal specialization-depth guard diagnose an
            // expanding graph on the supported 1 MiB Debug stack.
            pending_generic_interfaces=true;
            if(!preparing_generic_interfaces)complete_generic_interface_headers();
        } else if(headers_ready) {
            for(auto* f:methods)prepare_function(owner,*f);
            validate_inheritance(units,root,diagnostics);
            root.member_catalog->nominal_types.push_back(Type::nominal(concrete.definition));
            for(auto* f:methods) {
                auto header=member_header(owner,*f);
                if(f->interface_requirement)root.member_catalog->interface_requirements.push_back(std::move(header));
                else root.member_catalog->functions.push_back(std::move(header));
            }
            defaults(owner,concrete);
            for(auto* f:methods) semantics.prepare_specialized_method(owner,*f);
        }
        return concrete;
    };
    u->resolve_application=[&,owner=u.get()](Type raw,const Token& site)->Type {
        auto found=std::find_if(owner->applications.begin(),owner->applications.end(),[&](const auto& a){return a.definition==raw.definition;});
        if(found==owner->applications.end())return raw;
        if(found->resolved!=Type::none)return found->resolved;
        auto [declaration_owner,generic]=structure_lookup(*owner,root,units,found->name,found->token);
        if(!generic && (found->name=="generator" || found->name=="async_generator")) {
            if(found->arguments.size()!=1)
                error(owner->source,found->token,Code::argument_count,"generator<T> requires exactly one item type.");
            auto item=resolve_type(found->arguments.front(),*owner,root,units,site);
            return found->resolved=owner->make_generator(item,found->name=="async_generator");
        }
        if(!generic && found->name=="weak") {
            if(found->arguments.size()!=1)
                error(owner->source,found->token,Code::argument_count,"weak<T> requires exactly one target type.");
            auto target=resolve_type(found->arguments.front(),*owner,root,units,site);
            if(!ir::weak_target_type(target))
                error(owner->source,found->token,Code::unsupported_type,"weak<T> requires a non-null concrete managed target type.");
            return found->resolved=owner->make_weak(target);
        }
        if(!generic && found->name=="option") {
            if(found->arguments.size()!=1)error(owner->source,found->token,Code::argument_count,"option<T> requires exactly one payload type.");
            auto payload=resolve_type(found->arguments.front(),*owner,root,units,site);
            return found->resolved=owner->make_option(payload);
        }
        if(!generic || generic->generics.empty())error(owner->source,found->token,Code::unsupported_type,"Type '"+found->name+"' is not a generic type.");
        std::vector<Type> arguments;for(auto argument:found->arguments)arguments.push_back(resolve_type(argument,*owner,root,units,site));
        auto& concrete=owner->specialize_structure(*owner,*declaration_owner,*generic,arguments,site);
        return found->resolved=Type::nominal(concrete.definition);
    };
}
