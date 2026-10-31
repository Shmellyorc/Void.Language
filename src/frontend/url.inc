    VOID_SEMANTIC_HANDLER Type url_call(Expr& expression, unsigned operation) {
        if (!current) error(source, expression.token, Code::unsupported_type, "URL operations require runtime evaluation.");
        const auto& contract = url::source_operations.at(operation);
        if (expression.parts.size() != contract.arguments.size())
            error(source, expression.token, Code::argument_count, "Invalid private URL argument count.");
        Function target;
        target.token = expression.token;
        target.symbol = "$url." + std::to_string(operation);
        target.result = Type(unit.structure_names.at("_reply")->definition);
        for (unsigned n = 0; n < contract.arguments.size(); ++n) {
            Type type = Type::i64;
            switch (contract.arguments[n]) {
            case 't': type = Type::string; break;
            case 'b': type = Type::boolean; break;
            case 'q': type = ir::readonly_view(ir::field_type(target.result, 12)); break;
            default: break;
            }
            target.parameters.push_back({Token{TokenKind::name, "arg" + std::to_string(n), expression.token.offset}, type});
        }
        builtin_record(target, "Private checked URL kernel operation.");
        expression.target = expression.source_target = target.symbol;
        return expression.type = call_arguments(expression, target, 0);
    }
