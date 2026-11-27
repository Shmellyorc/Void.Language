// Pure collection templates compare contents; nullable value wrappers retain
// their existing active-case semantics. Allocated managed identities require
// runtime evaluation, since ConstantValue carries templates rather than objects.
bool enum_constant_equal(const SourceFile&,const Token&,Type,const ir::ConstantValue&,const ir::ConstantValue&,unsigned,CompileEvaluation*,bool);
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
void append_constant_key(const SourceFile& source,const Token& site,Type type,const ir::ConstantValue& value,
                         std::string& key,unsigned depth,CompileEvaluation* evaluation) {
    if(depth>=128)error(source,site,Code::nesting,"Collection constant key nesting limit exceeded.");
    if(evaluation)evaluation->step(source,site);
    auto word=[&](std::uint64_t bits) {
        if(evaluation)evaluation->charge_bytes(source,site,sizeof(bits));
        for(unsigned n=0;n<8;++n){key.push_back(static_cast<char>(bits&255));bits>>=8;}
    };
    if(type==Type::string) {
        if(evaluation)evaluation->charge_bytes(source,site,value.text.size());
        word(value.text.size());key+=value.text;
    } else if(type.kind==Type::aggregate) {
        unsigned begin=0,count=static_cast<unsigned>(type.definition->fields.size());
        if(!type.definition->cases.empty()) {
            const auto tag=value.fields.at(0).bits;word(tag);
            const auto& variant=type.definition->cases.at(tag);
            begin=variant.begin;count=variant.count;
        }
        for(unsigned n=begin;n<begin+count;++n)
            append_constant_key(source,site,ir::field_type(type,n),value.fields.at(n),key,depth+1,evaluation);
    } else {
        const auto primitive=type.kind==Type::numeric_enum?Type(*type.definition->enum_backing):type;
        auto bits=value.bits;
        if(numeric::integer(primitive) && numeric::width(primitive)<64)
            bits&=(std::uint64_t{1}<<numeric::width(primitive))-1;
        word(bits);
    }
}

std::string collection_constant_key(const SourceFile& source,const Token& site,Type type,
                                    const ir::ConstantValue& value,CompileEvaluation* evaluation) {
    std::string key;
    append_constant_key(source,site,type,value,key,0,evaluation);
    return key;
}

const ir::ConstantValue* constant_dictionary_find(const SourceFile& source,const Token& site,Type type,
    const ir::ConstantValue& dictionary,const ir::ConstantValue& key,CompileEvaluation* evaluation) {
    const auto key_type=ir::dictionary_key(type);
    const auto selected=collection_constant_key(source,site,key_type,key,evaluation);
    // Templates retain authored pairs. The last duplicate supplies the value.
    for(std::size_t n=dictionary.fields.size();n>0;n-=2)
        if(selected==collection_constant_key(source,site,key_type,dictionary.fields.at(n-2),evaluation))
            return &dictionary.fields.at(n-1);
    return nullptr;
}

auto constant_dictionary_entries(const SourceFile& source,const Token& site,Type type,
                                  const ir::ConstantValue& value,CompileEvaluation* evaluation) {
    std::map<std::string,const ir::ConstantValue*> entries;
    for(std::size_t n=0;n<value.fields.size();n+=2) {
        if(evaluation)evaluation->charge_bytes(source,site,sizeof(decltype(entries)::value_type)+3*sizeof(void*));
        entries[collection_constant_key(source,site,ir::dictionary_key(type),value.fields.at(n),evaluation)]=&value.fields.at(n+1);
    }
    return entries;
}
bool collection_constant_equal(const SourceFile& source,const Token& site,Type type,const ir::ConstantValue& left,const ir::ConstantValue& right,unsigned depth,CompileEvaluation* evaluation) {
    if(depth>=128)error(source,site,Code::nesting,"Collection constant comparison nesting limit exceeded.");
    if(type.kind==Type::dictionary) {
        // Canonicalize duplicate literal keys with the same last-write rule as
        // materialization; key schemas remain data-only, with no user operators.
        auto a=constant_dictionary_entries(source,site,type,left,evaluation);
        auto b=constant_dictionary_entries(source,site,type,right,evaluation);if(a.size()!=b.size())return false;
        auto other=b.begin();
        for(const auto& [key,value]:a){if(key!=other->first || !enum_constant_equal(source,site,ir::dictionary_value(type),*value,*other->second,depth+1,evaluation,false))return false;++other;}
        return true;
    }
    const auto a=left.repeat.value_or(left.fields.size()),b=right.repeat.value_or(right.fields.size());
    if(a!=b)return false;if(!a)return true;
    const auto element=ir::array_element(type);
    if(left.repeat && right.repeat)return enum_constant_equal(source,site,element,left.fields.at(0),right.fields.at(0),depth+1,evaluation,false);
    for(std::uint64_t n=0;n<a;++n)if(!enum_constant_equal(source,site,element,left.fields.at(left.repeat?0:n),right.fields.at(right.repeat?0:n),depth+1,evaluation,false))return false;
    return true;
}
