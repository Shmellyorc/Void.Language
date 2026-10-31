                case Op::http_control:
                case Op::http_native:
                case Op::http_async: {
                    const bool asynchronous = i.op == Op::http_async;
                    if (!m.coordinated || f.module_identity != http::source_module || i.a || i.b ||
                        i.immediate < 0 || static_cast<std::size_t>(i.immediate) >= http::source_operations.size() ||
                        i.receiver || !i.callee.empty() || !i.operator_glyph.empty() || i.initialization || i.array_repeat ||
                        f.worker || (asynchronous && (!f.asynchronous || !http::source_operations[i.immediate].asynchronous)))
                        fail("invalid private HTTP operation");
                    const auto& contract = http::source_operations[i.immediate];
                    if (i.arguments.size() != contract.arguments.size()) fail("invalid HTTP arity");
                    auto nominal = [&](std::string_view name) {
                        for (auto t : m.nominal_types)
                            if (t.definition && t.definition->module_identity == http::source_module &&
                                t.definition->identity == std::string(http::source_module) + "::" + std::string(name))
                                return Type(t.definition);
                        fail("missing HTTP record"); return Type(Type::invalid);
                    };
                    auto named = [&](Type t, std::string_view name) {
                        return t.kind == Type::aggregate && t.definition && !t.nullable && !t.construction &&
                            t.definition->module_identity == http::source_module &&
                            t.definition->identity == std::string(http::source_module) + "::" + std::string(name);
                    };
                    for (const auto& schema : http::source_records) {
                        auto record = nominal(schema.name); valid_type(record);
                        if (record.definition->fields.size() != schema.fields.size() || !record.definition->cases.empty())
                            fail("invalid HTTP record geometry");
                        for (unsigned n = 0; n < schema.fields.size(); ++n) {
                            auto field = field_type(record, n); const char code = schema.fields[n];
                            if (code == 'n' || code == 'b' || code == 't') {
                                if (field != (code == 'n' ? Type(Type::i64) : code == 'b' ? Type(Type::boolean) : Type(Type::string))) fail("invalid HTTP scalar field");
                            } else if (code == 'f') {
                                if (field.kind != Type::managed_ref || !field.nullable || field.construction ||
                                    Type::key(field) != "/@stdlib/io.void::file" || field.readonly || !readonly_paths(field).empty())
                                    fail("invalid HTTP file authority");
                            } else {
                                if (field.kind != Type::growable_array) fail("invalid HTTP array field");
                                auto element = array_element(field);
                                if (code == 'B' ? element != Type::u8 : code == 'S' ? element != Type::string :
                                    !named(element, code == 'H' ? "header" : code == 'D' ? "_history" : "_part"))
                                    fail("invalid HTTP array element");
                            }
                        }
                    }
                    for (unsigned n = 0; n < i.arguments.size(); ++n) {
                        auto actual = operand(i.arguments[n]); const char code = contract.arguments[n];
                        if (code == 'C' || code == 'T' || code == 'F') {
                            if (!(code == 'F' ? filesystem::file_type(actual) : http::resource_type(actual, code == 'C' ? "client" : "transfer")) ||
                                actual.construction || actual.readonly || !readonly_paths(actual).empty()) fail("invalid HTTP resource authority");
                        } else if (code == 'g' || code == 'r' || code == 'o') {
                            if (!named(actual, code == 'g' ? "_config" : code == 'r' ? "_request" : "_options") ||
                                actual.readonly || !readonly_paths(actual).empty()) fail("invalid HTTP input record");
                        } else if (code == 'q') {
                            if (actual.kind != Type::growable_array || !named(array_element(actual), "_form_item") || actual.construction)
                                fail("invalid HTTP form input");
                        } else if (code == 'b') {
                            if (actual.kind != Type::growable_array || array_element(actual) != Type::u8) fail("invalid HTTP bytes input");
                        } else if (actual != Type::i64) fail("invalid HTTP numeric input");
                    }
                    const bool cleanup = i.immediate >= static_cast<unsigned>(http::SourceOperation::cleanup_client);
                    if (i.op == Op::http_control) {
                        if (!cleanup || i.type != Type::none) fail("invalid HTTP cleanup control");
                        result = Type::none; break;
                    }
                    if (cleanup) fail("HTTP cleanup requires control opcode");
                    valid_type(i.type); auto reply = i.type;
                    if (asynchronous) {
                        if (reply.kind != Type::managed_ref || !reply.definition || !reply.definition->task_result || reply.nullable || reply.readonly)
                            fail("invalid HTTP native task");
                        reply = field_type(reply, 1);
                    }
                    if (!named(reply, "_reply") || reply.readonly || !readonly_paths(reply).empty()) fail("invalid HTTP reply");
                    result = i.type; break;
                }
