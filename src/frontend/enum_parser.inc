// Flags declarations reuse the expression parser; their constants may reference
// earlier members and combine bits. No source function or Runtime evaluation.
std::uint64_t flags_discriminant(const Expr& root,const std::vector<ir::NumericMember>& members,Type backing) {
    unsigned nesting=0;
    const auto width=numeric::width(backing);
    const auto mask=width==64?~std::uint64_t{}:(std::uint64_t{1}<<width)-1;
    std::function<std::uint64_t(const Expr&)> value=[&](const Expr& e)->std::uint64_t {
        Depth guard(nesting,source,e.token,128);
        if(e.kind==Expr::Kind::literal && !e.literal_text.empty()) {
            try {return numeric::literal(e.literal_text,backing);}
            catch(const std::invalid_argument& failure){error(source,e.token,Code::integer_range,failure.what());}
        }
        if(e.kind==Expr::Kind::name)for(const auto& member:members)if(member.name==e.token.text)return member.bits;
        if(e.kind==Expr::Kind::unary && e.op=="~")return ~value(*e.parts[0])&mask;
        if(e.kind==Expr::Kind::binary) {
            auto left=value(*e.parts[0]),right=value(*e.parts[1]);
            if(e.op=="|")return left|right;
            if(e.op=="&")return left&right;
            if(e.op=="^")return left^right;
        }
        error(source,e.token,Code::integer_range,"A flag discriminant needs an integer literal, earlier member or bitwise constant expression.");
    };
    return value(root)&mask;
}
