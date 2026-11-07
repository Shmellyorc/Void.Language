    VOID_SEMANTIC_HANDLER Type host_call(Expr& e, unsigned operation) {
        if (!current) error(source,e.token,Code::unsupported_type,"Host primitives require runtime evaluation.");
        const auto& contract=host::operations.at(operation);
        Function target;target.token=e.token;target.symbol="$host."+std::to_string(operation);
        target.result=contract.cleanup ? Type(Type::none) : Type(unit.structure_names.at("_host_reply")->definition);
        for(unsigned n=0;n<contract.arguments.size();++n) {
            char code=contract.arguments[n];Type type=Type::i64;
            if(code=='t')type=Type::string;
            else if(code=='b')type=ir::readonly_view(byte_list());
            else if(code=='a') {
                auto array=std::make_shared<ir::ValueType>();Type element=Type::string;
                array->identity=array->name=ir::array_identity(element,{});array->element=ir::FieldType(element);
                type=ir::readonly_view(Type(array,Type::growable_array));
            } else if(code!='s') {
                auto& owner=*units.at(std::string(host::resource_module(code)));
                type=Type(owner.structure_names.at(std::string(host::resource_name(code)))->definition,Type::managed_ref);
            }
            target.parameters.push_back({Token{TokenKind::name,"arg"+std::to_string(n),e.token.offset},type});
        }
        builtin_record(target,"Private checked owned host primitive.");e.target=e.source_target=target.symbol;
        return e.type=call_arguments(e,target,0);
    }
