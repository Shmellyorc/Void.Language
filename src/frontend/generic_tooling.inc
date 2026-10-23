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
    for(const auto& f:s.fields){
        AnalyzedDeclaration record{s.definition->managed?AnalyzedDeclaration::Kind::managed_field:AnalyzedDeclaration::Kind::value_field,key+"::field::"+f.name,id,f.visibility,location(f.token),{},pattern_type(*f.annotation),f.constant_syntax,{},key};
        record.readonly=f.readonly;declaration(std::move(record),false);
    }
    if(!s.operators.empty()) {
        field(result->contract_,"generic-operator-owner");field(result->contract_,key);field(result->contract_,s.visibility);
        field(result->contract_,s.definition->managed?"managed":"value");field(result->contract_,std::to_string(s.generics.size()));
        for(const auto& parameter:s.generics){field(result->contract_,parameter.token.text);field(result->contract_,parameter.constraint?ir::type_name(*parameter.constraint):"");}
    }
    for(const auto& f:s.operators) {
        std::vector<Type> operands;for(const auto& p:f.parameters){auto t=pattern_type(p.type);t.readonly=true;operands.push_back(t);}
        auto identity=key+"::operator-template::"+f.operator_pattern;
        AnalyzedDeclaration record{AnalyzedDeclaration::Kind::operator_function,identity,id,f.visibility,location(f.token),operands,pattern_type(f.result),{},{},key};
        record.generic_template=true;record.editor_symbol_kind=25;record.operator_glyph=f.operator_glyph;record.constant_syntax=f.operator_pattern;
        record.signature=f.visibility+" operator "+f.operator_glyph+"(";
        for(unsigned n=0;n<operands.size();++n){if(n)record.signature+=", ";record.signature+=f.parameters[n].token.text+": "+ir::type_name(operands[n]);}
        record.signature+=") -> "+ir::type_name(record.type)+"\nDeclared by "+spelling+"\nEffects: checked per concrete specialization";
        for(unsigned n=0;n<f.parameters.size();++n)record.written_parameters.push_back({f.parameters[n].token.text,operands[n],location(f.parameters[n].token)});
        declaration(std::move(record));
    }
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
