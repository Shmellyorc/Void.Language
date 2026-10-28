                case Op::os_query: {
                    auto scalar = instruction.immediate == static_cast<unsigned>(os::Operation::process_id);
                    llvm::Constant* record = llvm::ConstantPointerNull::get(b.getPtrTy());
                    if (!scalar) {
                        auto* offsets_type = llvm::ArrayType::get(size_type, 6);
                        auto* record_type = llvm::StructType::get(size_type, offsets_type);
                        if (layout.getTypeAllocSize(record_type) != sizeof(os::ReplyLayout) ||
                            layout.getStructLayout(record_type)->getElementOffset(1) != offsetof(os::ReplyLayout, fields)) throw std::logic_error("OS layout ABI mismatch");
                        std::vector<llvm::Constant*> fields;
                        for (unsigned k = 0; k < 6; ++k) fields.push_back(llvm::ConstantInt::get(size_type, field_offset(instruction.type, k)));
                        record = new llvm::GlobalVariable(*module, record_type, true, llvm::GlobalValue::PrivateLinkage,
                            llvm::ConstantStruct::get(record_type, {
                                llvm::ConstantInt::get(size_type, layout.getTypeAllocSize(type(instruction.type))),
                                llvm::ConstantArray::get(offsets_type, fields)}), "os.reply.layout");
                    }
                    llvm::Value* output;
                    if (scalar) {
                        llvm::IRBuilder<> allocations(&*fn->getEntryBlock().begin());
                        output = home(allocations.CreateAlloca(b.getInt64Ty()), StorageRole::known_abi, llvm_backend::HomeKind::value);
                    } else output = value_roots[n].at(values.size());
                    b.CreateStore(llvm::Constant::getNullValue(type(instruction.type)), output);
                    llvm::Value* input = instruction.arguments.empty() ? static_cast<llvm::Value*>(llvm::ConstantPointerNull::get(b.getPtrTy())) : values[instruction.arguments[0]];
                    b.CreateCall(module->getOrInsertFunction("vd_os_call", b.getVoidTy(), b.getPtrTy(), b.getPtrTy(), b.getInt32Ty(), b.getPtrTy(), b.getPtrTy()),
                        {output, record, b.getInt32(instruction.immediate), input, failure});
                    selected_failure();
                    v = b.CreateLoad(type(instruction.type), output);
                    break;
                }
