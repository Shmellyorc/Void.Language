// Source declarations are published once, independently of how many concrete
// instances are emitted. Native identities remain private to compilation/ABI.
auto parameter_records=[&](const auto& parameters,const std::string& owner) {
    for(const auto& p:parameters)declaration({AnalyzedDeclaration::Kind::type_parameter,owner+"::type::"+p.token.text,id,"private",location(p.token),{},Type::none,{},{},owner},false);
};
auto type_references=[&](const auto& uses,const std::map<std::string,std::string>& names,const std::string& owner) {
    for(const auto& [token,type,qualifier]:uses) {
        if(type.kind!=Type::unresolved){primitive_reference(owner,token,type);continue;}
        Expr use(Expr::Kind::name,token);
        if(auto parameter=names.find(type.definition->name);parameter!=names.end()) {
            reference(owner,parameter->second,use,*unit);continue;
        }
        auto spelling=type.definition->name;
        for(const auto& application:unit->applications)if(application.definition==type.definition){spelling=application.name;break;}
        // Resolve source names only: do not instantiate or type-check an unused
        // body merely to publish navigation. Unknown body types remain deferred.
        try {
            auto [target,declaration]=structure_lookup(*unit,context,*graph,spelling,token);(void)target;
            if(declaration){if(qualifier)namespace_reference(owner,*qualifier);reference(owner,declaration->definition->identity,use,*unit);}
        }catch(const CompilationError&){}
    }
};
for(const auto& s:unit->generic_structures) {
    auto key=s.definition->identity;std::map<std::string,std::string> names;
    std::string spelling=s.token.text+"<";
    for(const auto& p:s.generics){if(!names.empty())spelling+=", ";spelling+=p.token.text;names.emplace(p.token.text,key+"::type::"+p.token.text);}spelling+=">";
    auto symbolic=std::make_shared<ir::ValueType>();symbolic->name=spelling;symbolic->identity=key;
    const auto symbolic_type=Type(symbolic,Type::unresolved);
    AnalyzedDeclaration type_record{s.definition->interface_type?AnalyzedDeclaration::Kind::interface_type:s.definition->managed?AnalyzedDeclaration::Kind::managed_structure:AnalyzedDeclaration::Kind::value_structure,key,id,s.visibility,location(s.token),{},symbolic_type,{},{},id};
    const bool enumeration=!s.case_tokens.empty();
    if(enumeration) {
        type_record.generic_template=true;
        type_record.editor_symbol_kind=10;
        type_record.signature=std::string(s.definition->enum_flags?"flags enum ":"enum ")+spelling;
        if(s.definition->enum_backing)type_record.signature+="("+ir::type_name(*s.definition->enum_backing)+")";
    }
    if(s.definition->interface_type) {
        type_record.generic_template=true;type_record.editor_symbol_kind=11;
        type_record.signature="interface "+spelling;
        if(s.source_end>s.token.offset){auto r=location(s.token);r.end=s.source_end;type_record.extent=r;}
    }
    declaration(std::move(type_record),false);
    parameter_records(s.generics,key);
    for(const auto& f:s.fields)if(!enumeration){
        AnalyzedDeclaration record{s.definition->managed?AnalyzedDeclaration::Kind::managed_field:AnalyzedDeclaration::Kind::value_field,key+"::field::"+f.name,id,f.visibility,location(f.token),{},editor_pattern_type(*f.annotation),f.constant_syntax,{},key};
        record.readonly=f.readonly;declaration(std::move(record),false);
    }
    for(const auto& f:s.static_fields) {
        AnalyzedDeclaration record{AnalyzedDeclaration::Kind::global_variable,key+"::static::"+f.name,id,f.visibility,location(f.token),{},f.annotation?editor_pattern_type(*f.annotation):Type(Type::none),f.constant_syntax,{},key};
        record.generic_template=true;record.static_member=true;record.readonly=f.readonly;record.editor_symbol_kind=8;
        record.signature=f.visibility+" static "+std::string(f.readonly?"const ":"")+spelling+"."+f.name+(f.annotation?": "+ir::type_name(editor_pattern_type(*f.annotation)):"");
        declaration(std::move(record),false);
    }
    // Template declarations own symbolic case signatures. Concrete references
    // supply their selected signatures without replacing these source records.
    for(unsigned n=0;n<s.definition->numeric_members.size();++n) {
        const auto& member=s.definition->numeric_members[n];
        const auto value=numeric::integer_text(member.bits,*s.definition->enum_backing);
        AnalyzedDeclaration record{AnalyzedDeclaration::Kind::module_constant,key+"::variant::"+member.name,id,s.visibility,location(s.case_tokens[n]),{},symbolic_type,value,member.bits,key};
        record.generic_template=true;record.editor_symbol_kind=22;
        record.signature=spelling+"."+member.name+" = "+value+" ("+ir::type_name(*s.definition->enum_backing)+")";
        declaration(std::move(record),false);
    }
    for(unsigned n=0;n<s.definition->cases.size();++n) {
        const auto& variant=s.definition->cases[n];std::vector<Type> parameters;
        for(unsigned k=0;k<variant.count;++k)parameters.push_back(editor_pattern_type(*s.fields[variant.begin+k].annotation));
        const auto identity=key+"::variant::"+variant.name;
        AnalyzedDeclaration record{variant.count?AnalyzedDeclaration::Kind::function:AnalyzedDeclaration::Kind::module_constant,identity,id,s.visibility,location(s.case_tokens[n]),parameters,symbolic_type,{},{},key};
        record.generic_template=true;record.editor_symbol_kind=22;record.signature=variant.name;
        if(variant.count)record.signature+='(';
        for(unsigned k=0;k<variant.count;++k) {
            const auto& member=s.fields[variant.begin+k];
            if(k)record.signature+=", ";record.signature+=member.token.text+": "+ir::type_name(parameters[k]);
            record.written_parameters.push_back({member.token.text,parameters[k],location(member.token)});
            declaration({AnalyzedDeclaration::Kind::parameter,identity+"::parameter::"+member.token.text,id,"local",location(member.token),{},parameters[k],{},{},identity},false);
        }
        if(variant.count)record.signature+=')';record.signature+=" -> "+spelling;
        declaration(std::move(record),false);
    }
    if(!s.operators.empty()) {
        field(result->contract_,"generic-operator-owner");field(result->contract_,key);field(result->contract_,s.visibility);
        field(result->contract_,s.definition->managed?"managed":"value");field(result->contract_,std::to_string(s.generics.size()));
        for(const auto& parameter:s.generics){field(result->contract_,parameter.token.text);field(result->contract_,parameter.constraint?ir::type_name(*parameter.constraint):"");}
    }
    for(const auto& f:s.operators) {
        std::vector<Type> operands;for(const auto& p:f.parameters){auto t=editor_pattern_type(p.type);t.readonly=true;operands.push_back(t);}
        auto identity=key+"::operator-template::"+f.operator_pattern;
        AnalyzedDeclaration record{AnalyzedDeclaration::Kind::operator_function,identity,id,f.visibility,location(f.token),operands,editor_pattern_type(f.result),{},{},key};
        record.generic_template=true;record.editor_symbol_kind=25;record.operator_glyph=f.operator_glyph;record.constant_syntax=f.operator_pattern;
        record.signature=f.visibility+" operator "+f.operator_glyph+"(";
        for(unsigned n=0;n<operands.size();++n){if(n)record.signature+=", ";record.signature+=f.parameters[n].token.text+": "+ir::type_name(operands[n]);}
        record.signature+=") -> "+ir::type_name(record.type)+"\nDeclared by "+spelling+"\nEffects: checked per concrete specialization";
        for(unsigned n=0;n<f.parameters.size();++n)record.written_parameters.push_back({f.parameters[n].token.text,operands[n],location(f.parameters[n].token)});
        declaration(std::move(record));
    }
    for(const auto& f:s.requirements) {
        const auto identity=key+"::method::"+f.token.text;std::vector<Type> parameters;
        AnalyzedDeclaration record{AnalyzedDeclaration::Kind::interface_requirement,identity,id,s.visibility,location(f.token),{},editor_pattern_type(f.result),{},{},key};
        record.generic_template=true;record.editor_symbol_kind=6;record.receiver=symbolic_type;
        record.signature=f.token.text+"(";
        for(unsigned n=0;n<f.parameters.size();++n) {
            const auto& p=f.parameters[n];auto type=editor_pattern_type(p.type);parameters.push_back(type);
            if(n)record.signature+=", ";record.signature+=std::string(p.type.readonly?"const ":"")+p.token.text+": "+ir::type_name(type);
            record.written_parameters.push_back({p.token.text,type,location(p.token)});
            declaration({AnalyzedDeclaration::Kind::parameter,identity+"::parameter::"+p.token.text,id,"local",location(p.token),{},type,{},{},identity},false);
        }
        record.parameters=std::move(parameters);record.signature+=") -> "+editor_return_type(f,true);
        declaration(std::move(record),false);
    }
    type_references(s.type_uses,names,key);
}
for(const auto& f:unit->generic_functions) {
    std::vector<Type> parameters;for(const auto& p:f.parameters)parameters.push_back(editor_pattern_type(p.type));
    const auto owner=f.static_owner?f.symbol.substr(0,f.symbol.rfind("::static::")):f.receiver?f.symbol.substr(0,f.symbol.rfind("::method::")):id;
    AnalyzedDeclaration record{f.token.text=="init"?AnalyzedDeclaration::Kind::initializer:f.receiver?AnalyzedDeclaration::Kind::receiver_method:AnalyzedDeclaration::Kind::function,f.symbol,id,f.visibility,location(f.token),parameters,editor_pattern_type(f.result),{},{},owner};
    for(const auto& p:f.parameters) {
        AnalyzedParameter parameter{p.token.text,editor_pattern_type(p.type),location(p.token)};
        if(p.default_expression){auto range=location(p.token);range.begin=p.default_begin;range.end=p.default_end;parameter.default_source=range;}
        record.written_parameters.push_back(std::move(parameter));
        AnalyzedDeclaration binding{AnalyzedDeclaration::Kind::parameter,f.symbol+"::parameter::"+p.token.text,id,
            "local",location(p.token),{},editor_pattern_type(p.type),{},{},f.symbol};
        binding.rename_safe=valid_editor_identifier(p.token.text);
        declaration(std::move(binding),false);
    }
    if(f.receiver) {
        const auto& receiver=*f.receiver;
        AnalyzedDeclaration binding{AnalyzedDeclaration::Kind::parameter,f.symbol+"::parameter::"+receiver.token.text,id,
            "local",location(receiver.token),{},editor_pattern_type(receiver.type),{},{},f.symbol};
        binding.rename_safe=valid_editor_identifier(receiver.token.text);
        declaration(std::move(binding),false);
    }
    if(f.source_end>f.token.offset){auto r=location(f.token);r.begin=unit->source.line_starts().at(unit->source.location(f.token.offset).line-1);r.end=f.source_end;record.extent=r;}
    if(f.static_owner){record.static_member=true;record.editor_symbol_kind=6;record.signature=editor_signature(f);}
    if(f.accessor)record.editor_symbol_kind=7;
    record.generic_template=true;
    if(f.receiver)record.receiver=editor_pattern_type(f.receiver->type);
    if(!f.returns.empty())record.signature=editor_signature(f);
    record.rename_safe=!f.initializer && f.native_symbol.empty() && !f.c_primitive &&
        f.token.text!="entry" && f.token.text!="main" && f.token.text!="deinit" && f.token.text!="call" &&
        (f.accessor || valid_editor_identifier(f.token.text));
    declaration(std::move(record),false);parameter_records(f.generics,f.symbol);
    return_components(f);
    std::map<std::string,std::string> names;
    for(const auto& p:f.generics)names.emplace(p.token.text,f.symbol+"::type::"+p.token.text);
    if(f.receiver || f.static_owner)for(const auto& s:unit->generic_structures)if(s.definition->identity==owner)
        for(const auto& p:s.generics)names.emplace(p.token.text,owner+"::type::"+p.token.text);
    type_references(f.type_uses,names,f.symbol);
}
