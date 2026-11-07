                case Op::host_native: {
                    if(!m.coordinated || i.immediate<0 || static_cast<std::size_t>(i.immediate)>=host::operations.size() ||
                       i.a || i.b || i.receiver || !i.callee.empty() || !i.operator_glyph.empty() || i.initialization || i.array_repeat)
                        fail("invalid private host operation");
                    const auto& contract=host::operations.at(i.immediate);
                    if(f.module_identity!=contract.module || i.arguments.size()!=contract.arguments.size())fail("invalid host authority/arity");
                    for(unsigned n=0;n<i.arguments.size();++n) {
                        const auto t=operand(i.arguments[n]);char c=contract.arguments[n];
                        if(c=='t' || c=='s') {if(t!=(c=='t'?Type(Type::string):Type(Type::i64)))fail("invalid host scalar");}
                        else if(c=='b' || c=='a') {
                            if(t.kind!=Type::growable_array || array_element(t)!=(c=='b'?Type(Type::u8):Type(Type::string)))fail("invalid host array");
                        } else if(t.kind!=Type::managed_ref || !t.definition || t.nullable || t.construction || t.readonly || !readonly_paths(t).empty() ||
                            t.definition->identity!=std::string(host::resource_module(c))+"::"+std::string(host::resource_name(c)))fail("invalid host resource");
                    }
                    if(contract.cleanup) {if(i.type!=Type::none)fail("invalid host cleanup result");}
                    else {
                        valid_type(i.type);auto t=i.type;
                        if(t.kind!=Type::aggregate || !t.definition || t.readonly || !readonly_paths(t).empty() ||
                           t.definition->identity!=std::string(contract.module)+"::_host_reply" || t.definition->fields.size()!=7)fail("invalid host reply");
                        for(unsigned n=0;n<6;++n)if(field_type(t,n)!=(n<4?Type(Type::i64):n==4?Type(Type::boolean):Type(Type::string)))fail("invalid host reply field");
                        auto bytes=field_type(t,6);if(bytes.kind!=Type::growable_array || array_element(bytes)!=Type::u8)fail("invalid host reply bytes");
                    }
                    result=i.type;break;
                }
