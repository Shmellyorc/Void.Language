    VOID_SEMANTIC_HANDLER Type http_call(Expr& expression, unsigned operation, bool asynchronous) {
        if (!current || (asynchronous && !current->asynchronous))
            error(source, expression.token, Code::unsupported_type, "HTTP operations require an owning runtime activation.");
        const auto& contract = http::source_operations.at(operation);
        if (expression.parts.size() != contract.arguments.size())
            error(source, expression.token, Code::argument_count, "Invalid private HTTP argument count.");
        Function target;
        target.token = expression.token;
        target.symbol = std::string(asynchronous ? "$http.async." : "$http.sync.") + std::to_string(operation);
        target.result = Type(unit.structure_names.at("_reply")->definition);
        if (operation >= static_cast<unsigned>(http::SourceOperation::cleanup_client)) target.result = Type::none;
        else {
            auto task = unit.make_task(target.result);
            if (asynchronous) target.result = task;
        }
        for (unsigned n = 0; n < contract.arguments.size(); ++n) {
            Type type = Type::i64;
            switch (contract.arguments[n]) {
            case 'C': case 'T':
                type = Type(unit.structure_names.at(contract.arguments[n] == 'C' ? "client" : "transfer")->definition, Type::managed_ref); break;
            case 'g': case 'r': case 'o':
                type = Type(unit.structure_names.at(contract.arguments[n] == 'g' ? "_config" : contract.arguments[n] == 'r' ? "_request" : "_options")->definition); break;
            case 'F': type = Type(units.at(std::string(filesystem::source_module))->structure_names.at("file")->definition, Type::managed_ref); break;
            case 'b': type = ir::readonly_view(byte_list()); break;
            case 'q': {
                Type item(unit.structure_names.at("_form_item")->definition);
                auto array = std::make_shared<ir::ValueType>();
                array->identity = array->name = ir::array_identity(item, {}); array->element = ir::FieldType(item);
                type = ir::readonly_view(Type(array, Type::growable_array)); break;
            }
            default: break;
            }
            target.parameters.push_back({Token{TokenKind::name, "arg" + std::to_string(n), expression.token.offset}, type});
        }
        builtin_record(target, "Private checked HTTP operation.");
        expression.target = expression.source_target = target.symbol;
        return expression.type = call_arguments(expression, target, 0);
    }
