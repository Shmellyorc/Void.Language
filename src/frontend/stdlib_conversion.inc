// Canonical convert-only access to scalar parsing and checked result extraction.
// Enum cases come from the already validated closed nominal declaration.
VOID_SEMANTIC_HANDLER Type conversion_call(Expr& e) {
    if (e.target == "$convert.parse" || e.target == "$convert.unwrap") {
        return e.type;
    }
    const bool parsing = e.token.text == "_convert_parse";
    const unsigned arity = parsing ? 1 : 2;
    if (!current || e.type_arguments.size() != arity) {
        error(source, e.token, Code::unsupported_type,
              "Private conversion operations require concrete type arguments and runtime evaluation.");
    }
    auto value = resolve_type(e.type_arguments[0], unit, root, units, e.token);
    Function target;
    target.token = e.token;
    if (parsing) {
        const bool enumeration = value.kind == Type::aggregate && value.definition &&
            !value.nullable && !value.definition->cases.empty() &&
            !value.definition->result_type &&
            std::all_of(value.definition->cases.begin(), value.definition->cases.end(),
                        [](const auto& variant) { return variant.count == 0; });
        if (!numeric::number(value) && value != Type::boolean && !enumeration) {
            error(source, e.token, Code::unsupported_type,
                  "convert.parse supports numeric primitives, bool and payload-free enums only.");
        }
        target.symbol = "$convert.parse";
        target.result = unit.make_result(value, Type::i32);
        target.parameters.push_back({Token{TokenKind::name, "text", e.token.offset}, Type::string});
    } else {
        auto reason = resolve_type(e.type_arguments[1], unit, root, units, e.token);
        target.symbol = "$convert.unwrap";
        target.result = value;
        target.parameters.push_back({Token{TokenKind::name, "outcome", e.token.offset},
                                     unit.make_result(value, reason)});
    }
    e.target = target.symbol;
    return e.type = call_arguments(e, target, 0);
}
