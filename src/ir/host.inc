                case Op::host_native: {
                    if(!m.coordinated || i.immediate<0 || static_cast<std::size_t>(i.immediate)>=host::operations.size() ||
                       i.a || i.b || i.receiver || !i.callee.empty() || !i.operator_glyph.empty() || i.initialization || i.array_repeat)
                        fail("invalid private host operation");
                    const auto& contract=host::operations.at(i.immediate);
                    if(f.module_identity!=contract.module || i.arguments.size()!=contract.arguments.size())fail("invalid host authority/arity");
                    const bool network=host::network_operation(i.immediate);
                    if(network && i.immediate!=host::net_parse && f.worker)fail("network operation on a worker");
                    for(unsigned n=0;n<i.arguments.size();++n) {
                        const auto t=operand(i.arguments[n]);char c=contract.arguments[n];
                        if(c=='t' || c=='s') {if(t!=(c=='t'?Type(Type::string):Type(Type::i64)))fail("invalid host scalar");}
                        else if(c=='b' || c=='a') {
                            if(t.kind!=Type::growable_array || array_element(t)!=(c=='b'?Type(Type::u8):Type(Type::string)))fail("invalid host array");
                        } else if(c=='z') {
                            if(t.kind!=Type::growable_array || f.worker)fail("invalid ZIP input list");
                            const auto entry=array_element(t);
                            if(entry.kind!=Type::aggregate || !entry.definition ||
                               entry.definition->identity!=std::string(host::archive)+"::zip_input" ||
                               entry.definition->fields.size()!=2 || field_type(entry,0)!=Type::string)
                                fail("invalid ZIP input element");
                            const auto bytes=field_type(entry,1);
                            if(bytes.kind!=Type::growable_array || array_element(bytes)!=Type::u8)
                                fail("invalid ZIP input bytes");
                        } else if(t.kind!=Type::managed_ref || !t.definition || t.nullable || t.construction || t.readonly || !readonly_paths(t).empty() ||
                            t.definition->identity!=std::string(host::resource_module(c))+"::"+std::string(host::resource_name(c)))fail("invalid host resource");
                    }
                    if(contract.cleanup) {if(i.type!=Type::none)fail("invalid host cleanup result");}
                    else {
                        valid_type(i.type);auto t=i.type;
                        if(host::asynchronous(i.immediate)) {
                            if(!f.asynchronous || f.worker || t.kind!=Type::managed_ref || !t.definition ||
                               !t.definition->task_result || t.nullable || t.readonly || !readonly_paths(t).empty())
                                fail("invalid native host task");
                            t=field_type(t,1);
                        }
                        const bool capture=host::capture_operation(i.immediate);
                        const unsigned scalars=network?11:capture?9:6, field_count=network?12:capture?11:7;
                        const auto name=network?"::_network_reply":capture?"::_capture_reply":"::_host_reply";
                        if(t.kind!=Type::aggregate || !t.definition || t.readonly || !readonly_paths(t).empty() ||
                           t.definition->identity!=std::string(contract.module)+name || t.definition->fields.size()!=field_count)
                            fail("invalid host reply");
                        for(unsigned n=0;n<scalars;++n) {
                            const auto expected=network?(n<9?Type::i64:n==9?Type::boolean:Type::string):
                                capture?(n<6?Type::i64:Type::boolean):
                                (n<4?Type::i64:n==4?Type::boolean:Type::string);
                            if(field_type(t,n)!=expected)fail("invalid host reply field");
                        }
                        for(unsigned n=scalars;n<field_count;++n) {
                            auto bytes=field_type(t,n);
                            if(bytes.kind!=Type::growable_array || array_element(bytes)!=Type::u8)fail("invalid host reply bytes");
                        }
                    }
                    result=i.type;break;
                }
