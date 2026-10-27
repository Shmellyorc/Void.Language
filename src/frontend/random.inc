// The only arity family in the language is owned by the canonical random source.
// Resolve public visibility normally, then select an ordinary private source body.
VOID_SEMANTIC_HANDLER const Function& random_arity(Expr& e, const Function& public_target, std::size_t first) {
    if (!public_target.symbol.starts_with("/@stdlib/random.void::")) return public_target;
    auto name = public_target.token.text;
    auto suffix = name.starts_with("try_rand_") ? name.substr(9) :
                  name.starts_with("rand_") ? name.substr(5) : "";
    if (!numeric::number(numeric::primitive(suffix))) return public_target;
    if (public_target.receiver && Type::key(public_target.receiver->type) != "/@stdlib/random.void::generator") return public_target;
    const auto count = e.parts.size() - first;
    const bool checked_method = first && name.starts_with("try_");
    if (count > 2 || (checked_method && !count))
        error(source, e.token, Code::argument_count,
              checked_method ? "Checked generator numeric calls require one or two bounds." :
                               "random numeric calls require zero, one or two arguments.");
    if (count == public_target.parameters.size()) return public_target;
    auto helper = "_arity_" + name + "_" + std::to_string(count);
    for (const auto& f : units.at("/@stdlib/random.void")->functions)
        if (f.token.text == helper && bool(f.receiver) == bool(public_target.receiver)) {
            e.source_target = public_target.symbol;
            return f;
        }
    error(source, e.token, Code::unknown_method, "Canonical random arity implementation is missing.");
}

VOID_SEMANTIC_HANDLER Type random_private_call(Expr& e) {
    if (e.target.starts_with("$random.")) return e.type;
    if (!current) error(source, e.token, Code::constant_call, "Random primitives require runtime evaluation.");
    Function target;
    target.token = e.token;
    const bool domain = e.token.text == "_random_domain_max";
    if (e.type_arguments.size() != (domain ? 1u : 0u))
        error(source, e.token, Code::argument_count, "Invalid private random type arguments.");
    if (domain) {
        auto type = resolve_type(e.type_arguments[0], unit, root, units, e.token);
        if (!numeric::integer(type)) error(source, e.token, Code::unsupported_type, "Random integer ranges require an integer primitive, excluding bool and floats.");
        auto width = numeric::width(type);
        e.value = static_cast<std::int64_t>(width == 64 ? UINT64_MAX : (UINT64_C(1) << width) - 1);
        target.symbol = "$random.domain"; target.result = Type::u64;
    } else if (e.token.text == "_random_xor") {
        target.symbol = "$random.xor"; target.result = Type::u64;
        for (auto name : {"a", "b"}) target.parameters.push_back({Token{TokenKind::name, name, e.token.offset}, Type::u64});
    } else {
        target.symbol = "$random.seed"; target.result = unit.make_result(Type::u64, Type::i32);
    }
    e.target = target.symbol;
    return e.type = call_arguments(e, target, 0);
}
