    VOID_SEMANTIC_HANDLER Type filesystem_call(Expr& expression, unsigned operation, bool asynchronous) {
        if (!current || (asynchronous && !current->asynchronous)) {
            error(source, expression.token, Code::unsupported_type,
                  "Native file jobs require an owning async activation; filesystem calls require runtime evaluation.");
        }
        const auto& contract = filesystem::source_operations.at(operation);
        if (expression.parts.size() != contract.arguments.size()) {
            error(source, expression.token, Code::argument_count, "Invalid private filesystem argument count.");
        }
        Function target;
        target.token = expression.token;
        target.symbol = std::string(asynchronous ? "$io.async." : "$io.sync.") + std::to_string(operation);
        target.result = Type(unit.structure_names.at("_reply")->definition);
        if (operation >= filesystem::claim_operation) {
            target.result = operation == filesystem::claim_operation ? Type::i64 : Type::none;
        }
        if (asynchronous) target.result = unit.make_task(target.result);
        for (unsigned n = 0; n < contract.arguments.size(); ++n) {
            Type type = Type::i64;
            switch (contract.arguments[n]) {
            case 't': type = Type::string; break;
            case 'b': type = ir::readonly_view(byte_list()); break;
            case 'f':
            case 'F':
                type = Type(unit.structure_names.at("file")->definition, Type::managed_ref);
                if (contract.arguments[n] == 'f') type = ir::readonly_view(type);
                break;
            default: break;
            }
            target.parameters.push_back({Token{TokenKind::name, "arg" + std::to_string(n), expression.token.offset}, type});
        }
        builtin_record(target, "Private checked filesystem Runtime operation.");
        expression.target = expression.source_target = target.symbol;
        return expression.type = call_arguments(expression, target, 0);
    }
