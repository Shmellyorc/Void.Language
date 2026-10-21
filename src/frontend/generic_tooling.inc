// Source declarations are published once, independently of how many concrete
// instances are emitted. Native identities remain private to compilation/ABI.
auto pattern_type=[](Type raw) {
    if(raw.kind!=Type::unresolved && !ir::collection_type(raw) && !(raw.definition && (raw.definition->result_type || raw.definition->callable)))return raw;
    auto name=std::make_shared<ir::ValueType>();name->name=raw.definition->name;name->identity=name->name;
    return Type(name,Type::unresolved);
};
auto parameter_records=[&](const auto& parameters,const std::string& owner) {
    for(const auto& p:parameters)declaration({AnalyzedDeclaration::Kind::type_parameter,owner+"::type::"+p.token.text,id,"private",location(p.token),{},Type::none,{},{},owner},false);
};
auto type_references=[&](const auto& uses,const std::map<std::string,std::string>& names,const std::string& owner) {
    for(const auto& [token,type]:uses)if(type.kind==Type::unresolved) {
        Expr use(Expr::Kind::name,token);
        if(auto parameter=names.find(type.definition->name);parameter!=names.end()) {
            reference(owner,parameter->second,use,*unit);continue;
        }
        auto spelling=type.definition->name;
        for(const auto& application:unit->applications)if(application.definition==type.definition){spelling=application.name;break;}
        // Resolve source names only: do not instantiate or type-check an unused
        // body merely to publish navigation. Unknown body types remain deferred.
        try {
            auto [target,declaration]=structure_lookup(*unit,*graph->at(entry),*graph,spelling,token);(void)target;
            if(declaration)reference(owner,declaration->definition->identity,use,*unit);
        }catch(const CompilationError&){}
    }
};
for(const auto& s:unit->generic_structures) {
    auto key=s.definition->identity;std::map<std::string,std::string> names;
    std::string spelling=s.token.text+"<";
    for(const auto& p:s.generics){if(!names.empty())spelling+=", ";spelling+=p.token.text;names.emplace(p.token.text,key+"::type::"+p.token.text);}spelling+=">";
    auto symbolic=std::make_shared<ir::ValueType>();symbolic->name=spelling;symbolic->identity=key;
    declaration({s.definition->managed?AnalyzedDeclaration::Kind::managed_structure:AnalyzedDeclaration::Kind::value_structure,key,id,s.visibility,location(s.token),{},Type(symbolic,Type::unresolved),{},{},id},false);
    parameter_records(s.generics,key);
    for(const auto& f:s.fields)declaration({s.definition->managed?AnalyzedDeclaration::Kind::managed_field:AnalyzedDeclaration::Kind::value_field,key+"::field::"+f.name,id,s.visibility,location(f.token),{},pattern_type(*f.annotation),f.constant_syntax,{},key},false);
    type_references(s.type_uses,names,key);
}
for(const auto& f:unit->generic_functions) {
    std::vector<Type> parameters;for(const auto& p:f.parameters)parameters.push_back(pattern_type(p.type));
    const auto owner=f.receiver?f.symbol.substr(0,f.symbol.rfind("::method::")):id;
    AnalyzedDeclaration record{f.token.text=="init"?AnalyzedDeclaration::Kind::initializer:f.receiver?AnalyzedDeclaration::Kind::receiver_method:AnalyzedDeclaration::Kind::function,f.symbol,id,f.visibility,location(f.token),parameters,pattern_type(f.result),{},{},owner};
    for(const auto& p:f.parameters) {
        AnalyzedParameter parameter{p.token.text,pattern_type(p.type),location(p.token)};
        if(p.default_expression){auto range=location(p.token);range.begin=p.default_begin;range.end=p.default_end;parameter.default_source=range;}
        record.written_parameters.push_back(std::move(parameter));
    }
    if(f.source_end>f.token.offset){auto r=location(f.token);r.begin=unit->source.line_starts().at(unit->source.location(f.token.offset).line-1);r.end=f.source_end;record.extent=r;}
    if(f.accessor)record.editor_symbol_kind=7;
    record.generic_template=true;
    if(f.receiver)record.receiver=pattern_type(f.receiver->type);
    declaration(std::move(record),false);parameter_records(f.generics,f.symbol);
    std::map<std::string,std::string> names;
    for(const auto& p:f.generics)names.emplace(p.token.text,f.symbol+"::type::"+p.token.text);
    if(f.receiver)for(const auto& s:unit->generic_structures)if(s.definition->identity==owner)
        for(const auto& p:s.generics)names.emplace(p.token.text,owner+"::type::"+p.token.text);
    type_references(f.type_uses,names,f.symbol);
}
