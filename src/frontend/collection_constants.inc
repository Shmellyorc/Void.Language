// Pure collection templates compare contents; nullable value wrappers retain
// their existing active-case semantics. Allocated managed identities require
// runtime evaluation, since ConstantValue carries templates rather than objects.
bool enum_constant_equal(const SourceFile&,const Token&,Type,const ir::ConstantValue&,const ir::ConstantValue&,unsigned);
void collection_constant_type(const SourceFile& source,const Token& site,Type root) {
    std::vector<Type> pending{root};std::set<const ir::ValueType*> seen;
    while(!pending.empty()) {
        auto type=pending.back();pending.pop_back();
        if(type.kind==Type::managed_ref || type.kind==Type::interface_ref) {
            // Source constants can only carry the established all-null reference
            // template, never an allocated managed identity.
            if(!type.nullable || (type.definition && type.definition->custom_equality))
                error(source,site,Code::constant_call,"This collection identity comparison requires runtime evaluation.");
            continue;
        }
        if(ir::collection_type(type) && type.nullable)error(source,site,Code::constant_call,"This collection comparison requires runtime evaluation.");
        if(!type.definition || !seen.insert(type.definition.get()).second)continue;
        if(type.definition->custom_equality)error(source,site,Code::constant_call,"Compile-time collection equality cannot call a custom equality operator.");
        if(ir::collection_type(type)) {
            pending.push_back(type.kind==Type::dictionary?ir::dictionary_value(type):ir::array_element(type));
            if(type.kind==Type::dictionary)pending.push_back(ir::dictionary_key(type));
        }else if(type.kind==Type::aggregate && !type.definition->callable)for(const auto& field:type.definition->fields)pending.push_back(field.type);
    }
}
std::string collection_constant_key(const SourceFile& source,const Token& site,Type type,const ir::ConstantValue& value,unsigned depth) {
    if(depth>=128)error(source,site,Code::nesting,"Collection constant key nesting limit exceeded.");
    std::string key;
    auto word=[&](std::uint64_t bits){for(unsigned n=0;n<8;++n){key.push_back(static_cast<char>(bits&255));bits>>=8;}};
    if(type==Type::string){word(value.text.size());key+=value.text;}
    else if(type.kind==Type::aggregate) {
        if(!type.definition->cases.empty()) {
            const auto tag=value.fields.at(0).bits;word(tag);const auto& variant=type.definition->cases.at(tag);
            for(unsigned n=0;n<variant.count;++n){const auto field=variant.begin+n;key+=collection_constant_key(source,site,ir::field_type(type,field),value.fields.at(field),depth+1);}
        } else for(unsigned n=0;n<type.definition->fields.size();++n)key+=collection_constant_key(source,site,ir::field_type(type,n),value.fields.at(n),depth+1);
    } else word(value.bits);
    return key;
}
bool collection_constant_equal(const SourceFile& source,const Token& site,Type type,const ir::ConstantValue& left,const ir::ConstantValue& right,unsigned depth) {
    if(depth>=128)error(source,site,Code::nesting,"Collection constant comparison nesting limit exceeded.");
    if(type.kind==Type::dictionary) {
        // Canonicalize duplicate literal keys with the same last-write rule as
        // materialization; key schemas remain data-only, with no user operators.
        auto entries=[&](const ir::ConstantValue& value) {
            std::map<std::string,const ir::ConstantValue*> map;
            for(std::size_t n=0;n<value.fields.size();n+=2)map[collection_constant_key(source,site,ir::dictionary_key(type),value.fields.at(n),0)]=&value.fields.at(n+1);
            return map;
        };
        auto a=entries(left),b=entries(right);if(a.size()!=b.size())return false;
        auto other=b.begin();
        for(const auto& [key,value]:a){if(key!=other->first || !enum_constant_equal(source,site,ir::dictionary_value(type),*value,*other->second,depth+1))return false;++other;}
        return true;
    }
    const auto a=left.repeat.value_or(left.fields.size()),b=right.repeat.value_or(right.fields.size());
    if(a!=b)return false;if(!a)return true;
    const auto element=ir::array_element(type);
    if(left.repeat && right.repeat)return enum_constant_equal(source,site,element,left.fields.at(0),right.fields.at(0),depth+1);
    for(std::uint64_t n=0;n<a;++n)if(!enum_constant_equal(source,site,element,left.fields.at(left.repeat?0:n),right.fields.at(right.repeat?0:n),depth+1))return false;
    return true;
}
