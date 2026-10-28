                case Op::os_query: {
                    if (!m.coordinated || f.module_identity != os::source_module ||
                        i.a || i.b || i.immediate < 0 || static_cast<std::size_t>(i.immediate) >= os::operations.size() ||
                        i.receiver || !i.callee.empty() || !i.operator_glyph.empty() || i.initialization || i.array_repeat) fail("invalid private OS operation");
                    const auto& contract = os::operations.at(i.immediate);
                    if (i.arguments.size() != (contract.text_argument ? 1u : 0u)) fail("invalid OS arity");
                    if (contract.text_argument && operand(i.arguments[0]) != Type::string) fail("invalid OS input type");
                    if (i.immediate == static_cast<unsigned>(os::Operation::process_id)) {
                        if (i.type != Type::i64) fail("invalid OS scalar reply");
                    } else {
                        valid_type(i.type);
                        auto reply = i.type;
                        if (reply.kind != Type::aggregate || !reply.definition ||
                            reply.definition->identity != "/@stdlib/os.void::_reply" ||
                            reply.definition->module_identity != os::source_module ||
                            reply.definition->fields.size() != 6 || reply.readonly || !readonly_paths(reply).empty()) fail("invalid OS reply");
                        for (unsigned n = 0; n < 6; ++n) {
                            if (field_type(reply, n) != (n < 3 ? Type(Type::i64) : n == 3 ? Type(Type::boolean) : Type(Type::string))) fail("invalid OS reply field");
                        }
                    }
                    result = i.type;
                    break;
                }
