// Private representation access for the canonical source codec. Byte ordering
// and exact-buffer validation remain ordinary library code, shared by callers.
VOID_SEMANTIC_HANDLER Type binary_codec_call(Expr& e) {
    if (e.target.starts_with("$binary.")) {
        return e.type;
    }

    if (!current || e.type_arguments.size() != 1) {
        error(source, e.token, Code::unsupported_type,
              "Binary representation operations require one concrete numeric type and runtime evaluation.");
    }

    const auto numeric_type = resolve_type(e.type_arguments[0], unit, root, units, e.token);

    if (!numeric::number(numeric_type)) {
        error(source, e.token, Code::unsupported_type,
              "Binary encoding requires i8/u8, i16/u16, i32/u32, i64/u64, f32 or f64.");
    }

    Function target;
    target.token = e.token;

    if (e.token.text == "_binary_width") {
        target.symbol = "$binary.width";
        target.result = Type::i64;
        e.value = numeric::width(numeric_type) / 8;
    } else {
        const bool encoding = e.token.text == "_binary_bits";
        target.symbol = encoding ? "$binary.bits" : "$binary.value";
        target.result = encoding ? Type(Type::u64) : numeric_type;
        target.parameters.push_back({Token{TokenKind::name, "value", e.token.offset},
                                     encoding ? numeric_type : Type(Type::u64)});
    }

    e.target = target.symbol;
    e.source_target = target.symbol;
    e.type = call_arguments(e, target, 0);
    return e.type;
}
