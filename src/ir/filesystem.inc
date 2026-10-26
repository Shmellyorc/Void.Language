                case Op::filesystem_control:
                case Op::filesystem_native:
                case Op::filesystem_async: {
                    bool asynchronous = i.op == Op::filesystem_async;
                    if (!m.coordinated || f.module_identity != filesystem::source_module ||
                        i.a || i.b || i.immediate < 0 ||
                        static_cast<std::size_t>(i.immediate) >= filesystem::source_operations.size() ||
                        i.receiver || !i.callee.empty() || i.initialization || i.array_repeat ||
                        (asynchronous && (!f.asynchronous || f.worker ||
                                          i.immediate >= filesystem::claim_operation))) {
                        fail("invalid private filesystem operation");
                    }
                    const auto& contract = filesystem::source_operations.at(i.immediate);
                    if (i.arguments.size() != contract.arguments.size()) fail("invalid filesystem arity");
                    for (unsigned n = 0; n < i.arguments.size(); ++n) {
                        auto argument = operand(i.arguments[n]);
                        switch (contract.arguments[n]) {
                        case 'f':
                        case 'F':
                            if (!filesystem::file_type(argument) || argument.construction ||
                                (contract.arguments[n] == 'F' &&
                                 (argument.readonly || !readonly_paths(argument).empty()))) {
                                fail("invalid filesystem resource authority");
                            }
                            break;
                        case 'b':
                            if (argument.kind != Type::growable_array || array_element(argument) != Type::u8) {
                                fail("invalid filesystem byte input");
                            }
                            break;
                        default:
                            if (argument != (contract.arguments[n] == 't' ? Type(Type::string) : Type(Type::i64))) {
                                fail("invalid filesystem argument type");
                            }
                            break;
                        }
                    }
                    if (i.op == Op::filesystem_control) {
                        if (i.immediate < filesystem::claim_operation ||
                            i.type != (i.immediate == filesystem::claim_operation ? Type(Type::i64) : Type(Type::none))) {
                            fail("invalid file resource control");
                        }
                        result = i.type;
                        break;
                    }
                    if (i.immediate >= filesystem::claim_operation) fail("file control requires control opcode");
                    valid_type(i.type);
                    auto reply = i.type;
                    if (asynchronous) {
                        if (reply.kind != Type::managed_ref || !reply.definition ||
                            !reply.definition->task_result || reply.nullable || reply.readonly) {
                            fail("invalid native filesystem task");
                        }
                        reply = field_type(reply, 1);
                    }
                    if (reply.kind != Type::aggregate || !reply.definition ||
                        reply.definition->identity != "/@stdlib/io.void::_reply" ||
                        reply.definition->module_identity != filesystem::source_module ||
                        reply.definition->fields.size() != 10 || reply.readonly || !readonly_paths(reply).empty()) {
                        fail("invalid filesystem reply");
                    }
                    for (unsigned n = 0; n < 8; ++n) {
                        if (field_type(reply, n) != (n < 6 ? Type(Type::i64) :
                                                    n == 6 ? Type(Type::boolean) : Type(Type::string))) {
                            fail("invalid filesystem reply field");
                        }
                    }
                    auto bytes = field_type(reply, 8);
                    auto entries = field_type(reply, 9);
                    if (bytes.kind != Type::growable_array || array_element(bytes) != Type::u8 ||
                        entries.kind != Type::growable_array) fail("invalid filesystem reply arrays");
                    auto entry = array_element(entries);
                    if (entry.kind != Type::aggregate || !entry.definition ||
                        entry.definition->identity != "/@stdlib/io.void::entry" ||
                        entry.definition->module_identity != filesystem::source_module ||
                        entry.definition->fields.size() != 6) fail("invalid filesystem entry");
                    for (unsigned n = 0; n < 6; ++n) {
                        if (n == 2) {
                            auto kind = field_type(entry, n);
                            if (kind.kind != Type::aggregate || !kind.definition ||
                                kind.definition->identity != "/@stdlib/io.void::entry_kind" ||
                                kind.definition->fields.size() != 1 || kind.definition->cases.size() != 4 ||
                                field_type(kind, 0) != Type::i32 ||
                                std::any_of(kind.definition->cases.begin(), kind.definition->cases.end(),
                                            [](const auto& item) { return item.count != 0; })) {
                                fail("invalid filesystem entry kind");
                            }
                        } else if (field_type(entry, n) != (n < 2 ? Type(Type::string) : Type(Type::i64))) {
                            fail("invalid filesystem entry field");
                        }
                    }
                    result = i.type;
                    break;
                }
