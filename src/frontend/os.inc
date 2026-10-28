    VOID_SEMANTIC_HANDLER Type os_call(Expr& expression, unsigned operation) {
        if (!current) error(source, expression.token, Code::unsupported_type, "OS queries require runtime evaluation.");
        const auto& contract = os::operations.at(operation);
        if (expression.parts.size() != (contract.text_argument ? 1u : 0u)) {
            error(source, expression.token, Code::argument_count, "Invalid private OS argument count.");
        }
        Function target;
        target.token = expression.token;
        target.symbol = "$os." + std::to_string(operation);
        target.result = operation == static_cast<unsigned>(os::Operation::process_id) ?
            Type(Type::i64) : Type(unit.structure_names.at("_reply")->definition);
        if (contract.text_argument) target.parameters.push_back({Token{TokenKind::name, "name", expression.token.offset}, Type::string});
        builtin_record(target, "Private checked owner-side OS query.");
        expression.target = expression.source_target = target.symbol;
        return expression.type = call_arguments(expression, target, 0);
    }
