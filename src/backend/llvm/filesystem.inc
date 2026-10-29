                case Op::filesystem_control: {
                    auto call = module->getOrInsertFunction("vd_file_control", b.getInt64Ty(),
                                                           b.getInt32Ty(), b.getPtrTy(), b.getPtrTy());
                    auto* result = b.CreateCall(call, {b.getInt32(instruction.immediate),
                                                       values[instruction.arguments[0]], failure});
                    selected_failure();
                    v = instruction.type == ir::Type::none ?
                        static_cast<llvm::Value*>(llvm::UndefValue::get(b.getInt8Ty())) : result;
                    break;
                }
                case Op::filesystem_native:
                case Op::filesystem_async: {
                    bool asynchronous = instruction.op == Op::filesystem_async;
                    auto reply = asynchronous ? ir::field_type(instruction.type, 1) : instruction.type;
                    auto* reply_type = type(reply);
                    auto* payload = asynchronous ? type(ir::Type(instruction.type.definition)) : nullptr;
                    auto* offsets_type = llvm::ArrayType::get(size_type, 10);
                    auto* record_type = llvm::StructType::get(size_type, offsets_type, b.getPtrTy(),
                        size_type, b.getPtrTy(), size_type, size_type, b.getPtrTy());
                    if (layout.getTypeAllocSize(record_type) != sizeof(filesystem::ReplyLayout) ||
                        layout.getStructLayout(record_type)->getElementOffset(2) != offsetof(filesystem::ReplyLayout, entry) ||
                        layout.getStructLayout(record_type)->getElementOffset(7) != offsetof(filesystem::ReplyLayout, task_key)) {
                        throw std::logic_error("Filesystem reply ABI mismatch");
                    }
                    std::vector<llvm::Constant*> fields;
                    for (unsigned field = 0; field < 10; ++field) {
                        fields.push_back(llvm::ConstantInt::get(size_type, field_offset(reply, field)));
                    }
                    std::vector<std::uint64_t> task_offsets;
                    if (asynchronous) reference_offsets(ir::Type(instruction.type.definition), 0, task_offsets);
                    auto* entry = element_record(ir::array_element(ir::field_type(reply, 9)));
                    auto* record = new llvm::GlobalVariable(*module, record_type, true,
                        llvm::GlobalValue::PrivateLinkage,
                        llvm::ConstantStruct::get(record_type, {
                            llvm::ConstantInt::get(size_type, layout.getTypeAllocSize(reply_type)),
                            llvm::ConstantArray::get(offsets_type, fields), entry,
                            llvm::ConstantInt::get(size_type, asynchronous ? layout.getTypeAllocSize(payload) : 0),
                            offset_map(task_offsets), llvm::ConstantInt::get(size_type, task_offsets.size()),
                            llvm::ConstantInt::get(size_type, asynchronous ? field_offset(instruction.type, 1) : 0),
                            b.CreateGlobalString(asynchronous ? instruction.type.definition->identity : "")
                        }), "filesystem.reply.layout");
                    llvm::IRBuilder<> allocations(&*fn->getEntryBlock().begin());
                    auto* references_type = llvm::ArrayType::get(b.getPtrTy(), 4);
                    auto* numbers_type = llvm::ArrayType::get(b.getInt64Ty(), 4);
                    auto* references = home(allocations.CreateAlloca(references_type),
                                             StorageRole::known_abi, llvm_backend::HomeKind::value);
                    auto* numbers = home(allocations.CreateAlloca(numbers_type),
                                          StorageRole::known_abi, llvm_backend::HomeKind::value);
                    b.CreateStore(llvm::Constant::getNullValue(references_type), references);
                    b.CreateStore(llvm::Constant::getNullValue(numbers_type), numbers);
                    auto contract = filesystem::source_operations.at(instruction.immediate);
                    unsigned reference = 0;
                    unsigned number = 0;
                    for (unsigned argument = 0; argument < instruction.arguments.size(); ++argument) {
                        auto* value = values[instruction.arguments[argument]];
                        if (contract.arguments[argument] == 's') {
                            b.CreateStore(value, b.CreateInBoundsGEP(numbers_type, numbers,
                                {b.getInt32(0), b.getInt32(number++)}));
                        } else {
                            b.CreateStore(value, b.CreateInBoundsGEP(references_type, references,
                                {b.getInt32(0), b.getInt32(reference++)}));
                        }
                    }
                    auto* output = value_roots[n].at(values.size());
                    b.CreateStore(llvm::Constant::getNullValue(type(instruction.type)), output);
                    auto call = module->getOrInsertFunction("vd_filesystem_call", b.getVoidTy(),
                        b.getPtrTy(), b.getPtrTy(), b.getInt32Ty(), b.getPtrTy(), b.getPtrTy(),
                        b.getPtrTy(), b.getInt1Ty(), b.getPtrTy());
                    b.CreateCall(call, {output, record, b.getInt32(instruction.immediate), references,
                                         numbers, async_generation, b.getInt1(asynchronous), failure});
                    selected_failure();
                    v = b.CreateLoad(type(instruction.type), output);
                    break;
                }
