                case Op::url_native: {
                    auto* offsets_type = llvm::ArrayType::get(size_type, 13);
                    auto* record_type = llvm::StructType::get(size_type, offsets_type, b.getPtrTy());
                    if (layout.getTypeAllocSize(record_type) != sizeof(url::ReplyLayout) ||
                        layout.getStructLayout(record_type)->getElementOffset(2) != offsetof(url::ReplyLayout, item))
                        throw std::logic_error("URL reply ABI mismatch");
                    std::vector<llvm::Constant*> offsets;
                    for (unsigned field = 0; field < 13; ++field)
                        offsets.push_back(llvm::ConstantInt::get(size_type, field_offset(instruction.type, field)));
                    auto* item = element_record(ir::array_element(ir::field_type(instruction.type, 12)));
                    auto* record = new llvm::GlobalVariable(*module, record_type, true, llvm::GlobalValue::PrivateLinkage,
                        llvm::ConstantStruct::get(record_type, {
                            llvm::ConstantInt::get(size_type, layout.getTypeAllocSize(type(instruction.type))),
                            llvm::ConstantArray::get(offsets_type, offsets), item}), "url.reply.layout");
                    llvm::IRBuilder<> allocations(&*fn->getEntryBlock().begin());
                    auto* references_type = llvm::ArrayType::get(b.getPtrTy(), 2);
                    auto* numbers_type = llvm::ArrayType::get(b.getInt64Ty(), 2);
                    auto* references = home(allocations.CreateAlloca(references_type), StorageRole::known_abi, llvm_backend::HomeKind::value);
                    auto* numbers = home(allocations.CreateAlloca(numbers_type), StorageRole::known_abi, llvm_backend::HomeKind::value);
                    b.CreateStore(llvm::Constant::getNullValue(references_type), references);
                    b.CreateStore(llvm::Constant::getNullValue(numbers_type), numbers);
                    auto contract = url::source_operations.at(instruction.immediate);
                    unsigned reference = 0, number = 0;
                    for (unsigned argument = 0; argument < instruction.arguments.size(); ++argument) {
                        auto* value = values[instruction.arguments[argument]];
                        if (contract.arguments[argument] == 't' || contract.arguments[argument] == 'q')
                            b.CreateStore(value, b.CreateInBoundsGEP(references_type, references, {b.getInt32(0), b.getInt32(reference++)}));
                        else {
                            if (contract.arguments[argument] == 'b') value = b.CreateZExt(value, b.getInt64Ty());
                            b.CreateStore(value, b.CreateInBoundsGEP(numbers_type, numbers, {b.getInt32(0), b.getInt32(number++)}));
                        }
                    }
                    auto* output = value_roots[n].at(values.size());
                    b.CreateStore(llvm::Constant::getNullValue(type(instruction.type)), output);
                    b.CreateCall(module->getOrInsertFunction("vd_url_call", b.getVoidTy(), b.getPtrTy(), b.getPtrTy(),
                        b.getInt32Ty(), b.getPtrTy(), b.getPtrTy(), b.getPtrTy()),
                        {output, record, b.getInt32(instruction.immediate), references, numbers, failure});
                    selected_failure();
                    v = b.CreateLoad(type(instruction.type), output);
                    break;
                }
