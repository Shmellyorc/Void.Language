// Constants follow the same active-case and primitive rules as generated runtime
// helpers. User operators remain runtime calls, as in other constant expressions.
bool enum_constant_equal(const SourceFile& source,const Token& site,Type type,
                         const ir::ConstantValue& left,const ir::ConstantValue& right,unsigned depth=0) {
    if(depth>=128)error(source,site,Code::nesting,"Enum constant comparison nesting limit exceeded.");
    if(type.definition && type.definition->custom_equality)error(source,site,Code::constant_call,"A compile-time enum comparison cannot call a payload's custom equality operator.");
    // Function values are aggregates too, but compare by callable identity.
    if(type.definition && type.definition->callable)return left.text==right.text;
    if(type.kind==Type::aggregate) {
        if(type.definition->cases.empty())error(source,site,Code::constant_call,"This payload has no compile-time equality.");
        if(left.fields.at(0).bits!=right.fields.at(0).bits)return false;
        const auto& variant=type.definition->cases.at(left.fields.at(0).bits);
        for(unsigned n=0;n<variant.count;++n){const auto field=variant.begin+n;if(!enum_constant_equal(source,site,ir::field_type(type,field),left.fields.at(field),right.fields.at(field),depth+1))return false;}
        return true;
    }
    if(type==Type::string)return left.text==right.text;
    if(ir::reference_value(type))return left.bits==right.bits;
    auto primitive=type.kind==Type::numeric_enum?Type(*type.definition->enum_backing):type;
    if(!numeric::number(primitive) && primitive!=Type::boolean)error(source,site,Code::constant_call,"This enum payload has no compile-time equality.");
    return numeric::binary(ir::Op::equal,left.bits,right.bits,primitive);
}
std::optional<ir::ConstantValue> enum_constant(const SourceFile& source,const Expr& e) {
    if(e.kind!=Expr::Kind::call || (!e.op.starts_with("$enum.") && e.op!="$enum_equal"))return {};
    const auto type=e.parts.at(0)->type;const auto value=constant_value(source,*e.parts[0]);
    if(e.op=="$enum_equal")return ir::ConstantValue{enum_constant_equal(source,e.token,type,value,constant_value(source,*e.parts[1])),{}};
    if(e.op=="$enum.name" || e.op=="$enum.is_defined") {
        bool defined=false;std::string name;
        if(type.kind==Type::numeric_enum) {
            for(const auto& member:type.definition->numeric_members)if(member.bits==value.bits){defined=true;name=member.name;break;}
            if(!defined)name=numeric::integer_text(value.bits,*type.definition->enum_backing);
        } else {const auto tag=value.fields.at(0).bits;defined=tag<type.definition->cases.size();if(defined)name=type.definition->cases.at(tag).name;}
        return e.op=="$enum.name"?ir::ConstantValue{0,{},std::move(name)}:ir::ConstantValue{defined,{}};
    }
    const auto mask=constant_value(source,*e.parts.at(1)).bits;
    if(e.op=="$enum.has")return ir::ConstantValue{(value.bits&mask)==mask,{}};
    if(e.op=="$enum.has_any")return ir::ConstantValue{(value.bits&mask)!=0,{}};
    if(e.op=="$enum.with")return ir::ConstantValue{value.bits|mask,{}};
    if(e.op=="$enum.without")return ir::ConstantValue{value.bits&~mask,{}};
    if(e.op=="$enum.toggled")return ir::ConstantValue{value.bits^mask,{}};
    return {};
}
