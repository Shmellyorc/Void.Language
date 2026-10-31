                case Op::url_native: {
                    if (!m.coordinated || f.module_identity != url::source_module ||
                        i.a || i.b || i.immediate < 0 || static_cast<std::size_t>(i.immediate) >= url::source_operations.size() ||
                        i.receiver || !i.callee.empty() || !i.operator_glyph.empty() || i.initialization || i.array_repeat)
                        fail("invalid private URL operation");
                    const auto& contract = url::source_operations.at(i.immediate);
                    if (i.arguments.size() != contract.arguments.size()) fail("invalid URL arity");
                    valid_type(i.type);
                    auto reply = i.type;
                    if (reply.kind != Type::aggregate || !reply.definition || reply.readonly || !readonly_paths(reply).empty() ||
                        reply.definition->identity != "/@stdlib/url.void::_reply" ||
                        reply.definition->module_identity != url::source_module || reply.definition->fields.size() != 13)
                        fail("invalid URL reply");
                    for (unsigned n = 0; n < 11; ++n) {
                        auto expected = n < 2 || n == 5 ? Type(Type::i64) : n > 8 ? Type(Type::boolean) : Type(Type::string);
                        if (field_type(reply, n) != expected) fail("invalid URL reply field");
                    }
                    auto bytes = field_type(reply, 11), items = field_type(reply, 12);
                    if (bytes.kind != Type::growable_array || array_element(bytes) != Type::u8 || items.kind != Type::growable_array)
                        fail("invalid URL reply arrays");
                    auto item = array_element(items);
                    if (item.kind != Type::aggregate || !item.definition || item.definition->identity != "/@stdlib/url.void::query_item" ||
                        item.definition->module_identity != url::source_module || item.definition->fields.size() != 3 ||
                        field_type(item, 0) != Type::string || field_type(item, 1) != Type::string || field_type(item, 2) != Type::boolean)
                        fail("invalid URL query item");
                    for (unsigned n = 0; n < i.arguments.size(); ++n) {
                        auto actual = operand(i.arguments[n]);
                        auto expected = contract.arguments[n] == 't' ? Type(Type::string) :
                            contract.arguments[n] == 'b' ? Type(Type::boolean) : Type(Type::i64);
                        if (contract.arguments[n] == 'q') {
                            if (actual.kind != Type::growable_array || array_element(actual) != item || actual.construction)
                                fail("invalid URL query input");
                        } else if (actual != expected) fail("invalid URL argument type");
                    }
                    result = i.type;
                    break;
                }
