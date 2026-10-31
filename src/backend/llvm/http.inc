                case Op::http_control: {
                    b.CreateCall(module->getOrInsertFunction("vd_http_cleanup", b.getVoidTy(), b.getInt32Ty(), b.getPtrTy(), b.getPtrTy()),
                        {b.getInt32(instruction.immediate), values[instruction.arguments[0]], failure});
                    selected_failure(); v = llvm::UndefValue::get(b.getInt8Ty()); break;
                }
                case Op::http_native:
                case Op::http_async: {
                    const bool asynchronous = instruction.op == Op::http_async;
                    constexpr unsigned count = static_cast<unsigned>(http::SourceRecord::count);
                    auto* descriptors_type = llvm::ArrayType::get(b.getPtrTy(), count);
                    auto* record_type = llvm::StructType::get(descriptors_type, size_type, b.getPtrTy(), size_type, size_type, b.getPtrTy());
                    if (layout.getTypeAllocSize(record_type) != sizeof(http::SourceLayout) ||
                        layout.getStructLayout(record_type)->getElementOffset(5) != offsetof(http::SourceLayout, task_key))
                        throw std::logic_error("HTTP source ABI mismatch");
                    std::vector<llvm::Constant*> descriptors;
                    for (const auto& schema : http::source_records) {
                        auto found = nominal_types.find(std::string(http::source_module) + "::" + std::string(schema.name));
                        if (found == nominal_types.end()) throw std::logic_error("Missing HTTP source record");
                        descriptors.push_back(element_record(ir::Type(found->second.definition)));
                    }
                    ir::Type task = asynchronous ? instruction.type : ir::Type(ir::Type::none);
                    if (!asynchronous) for (auto candidate : input.nominal_types)
                        if (candidate.definition && candidate.definition->task_result &&
                            ir::Type::key(static_cast<ir::Type>(*candidate.definition->task_result)) == std::string(http::source_module) + "::_reply") {
                            task = ir::Type(candidate.definition, ir::Type::managed_ref); break;
                        }
                    const bool have_task = task.kind == ir::Type::managed_ref;
                    std::vector<std::uint64_t> task_offsets;
                    if (have_task) reference_offsets(ir::Type(task.definition), 0, task_offsets);
                    auto* record = new llvm::GlobalVariable(*module, record_type, true, llvm::GlobalValue::PrivateLinkage,
                        llvm::ConstantStruct::get(record_type, {
                            llvm::ConstantArray::get(descriptors_type, descriptors),
                            llvm::ConstantInt::get(size_type, have_task ? layout.getTypeAllocSize(type(ir::Type(task.definition))) : 0),
                            offset_map(task_offsets), llvm::ConstantInt::get(size_type, task_offsets.size()),
                            llvm::ConstantInt::get(size_type, have_task ? field_offset(task, 1) : 0),
                            b.CreateGlobalString(have_task ? task.definition->identity : "")}), "http.source.layout");
                    llvm::IRBuilder<> allocations(&*fn->getEntryBlock().begin());
                    auto* references_type = llvm::ArrayType::get(b.getPtrTy(), 8);
                    auto* numbers_type = llvm::ArrayType::get(b.getInt64Ty(), 2);
                    auto* references = home(allocations.CreateAlloca(references_type), StorageRole::known_abi, llvm_backend::HomeKind::value);
                    auto* numbers = home(allocations.CreateAlloca(numbers_type), StorageRole::known_abi, llvm_backend::HomeKind::value);
                    b.CreateStore(llvm::Constant::getNullValue(references_type), references);
                    b.CreateStore(llvm::Constant::getNullValue(numbers_type), numbers);
                    const auto& contract = http::source_operations.at(instruction.immediate);
                    unsigned reference = 0, number = 0;
                    for (unsigned argument = 0; argument < instruction.arguments.size(); ++argument) {
                        auto id = instruction.arguments[argument]; auto* value = values[id];
                        const char code = contract.arguments[argument];
                        if (code == 's') b.CreateStore(value, b.CreateInBoundsGEP(numbers_type, numbers, {b.getInt32(0), b.getInt32(number++)}));
                        else {
                            if (code == 'g' || code == 'r' || code == 'o') {
                                auto* storage = home(allocations.CreateAlloca(type(operand_type(id))), StorageRole::known_abi, llvm_backend::HomeKind::value);
                                b.CreateStore(value, storage); value = storage;
                            }
                            b.CreateStore(value, b.CreateInBoundsGEP(references_type, references, {b.getInt32(0), b.getInt32(reference++)}));
                        }
                    }
                    auto* output = value_roots[n].at(values.size());
                    b.CreateStore(llvm::Constant::getNullValue(type(instruction.type)), output);
                    b.CreateCall(module->getOrInsertFunction("vd_http_call", b.getVoidTy(), b.getPtrTy(), b.getPtrTy(),
                        b.getInt32Ty(), b.getPtrTy(), b.getPtrTy(), b.getPtrTy(), b.getInt1Ty(), b.getPtrTy()),
                        {output, record, b.getInt32(instruction.immediate), references, numbers, async_generation, b.getInt1(asynchronous), failure});
                    selected_failure(); v = b.CreateLoad(type(instruction.type), output); break;
                }
