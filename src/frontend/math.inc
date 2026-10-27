// Private hooks are available only inside the canonical stdlib module. They use
// the ordinary binder, including named argument checking and evaluation order.
bool math_private_selected(const Expr& e) {
    if (!e.qualifier.empty() || lookup(e.token.text) ||
        (current && current->substitutions.contains(e.token.text)) ||
        unit.declarations.contains(e.token.text) || unit.constant_names.contains(e.token.text) ||
        unit.global_names.contains(e.token.text) || unit.structure_names.contains(e.token.text) ||
        unit.invalid_declarations.contains(e.token.text)) return false;
    for (const auto& import : unit.imports) if (!import.explicit_alias && !import.target.empty()) {
        const auto& owner=*units.at(import.target);
        if (owner.declarations.contains(e.token.text) || owner.constant_names.contains(e.token.text) ||
            owner.global_names.contains(e.token.text) || owner.structure_names.contains(e.token.text) ||
            owner.invalid_declarations.contains(e.token.text)) return false;
    }
    return true;
}
VOID_SEMANTIC_HANDLER Type math_private_call(Expr& e, const math::Operation& operation) {
    if (e.target.starts_with("$math.")) return e.type;
    if (!current) error(source, e.token, Code::constant_call, "Math operations require runtime evaluation.");
    if (e.type_arguments.size() != 1)
        error(source, e.token, Code::argument_count, "Private Math operations require exactly one type argument.");
    auto type = resolve_type(e.type_arguments[0], unit, root, units, e.token);
    if (!numeric::floating(type))
        error(source, e.token, Code::unsupported_type, "Math floating operations require exactly f32 or f64.");
    Function target;
    target.token = e.token;
    target.symbol = "$math." + std::to_string(static_cast<std::int64_t>(operation.kind));
    target.result = type;
    if (operation.arity == 1) {
        target.parameters.push_back({Token{TokenKind::name, "value", e.token.offset}, type});
    } else if (operation.kind == math::Kind::atan2) {
        target.parameters.push_back({Token{TokenKind::name, "y", e.token.offset}, type});
        target.parameters.push_back({Token{TokenKind::name, "x", e.token.offset}, type});
    } else {
        target.parameters.push_back({Token{TokenKind::name, "base", e.token.offset}, type});
        target.parameters.push_back({Token{TokenKind::name, "exponent", e.token.offset}, type});
    }
    e.target = target.symbol;
    return e.type = call_arguments(e, target, 0);
}
