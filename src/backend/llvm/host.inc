                case Op::host_native: {
                    const auto& contract=host::operations.at(instruction.immediate);
                    if(contract.cleanup) {
                        b.CreateCall(module->getOrInsertFunction("vd_host_cleanup",b.getVoidTy(),b.getInt32Ty(),b.getPtrTy(),b.getPtrTy()),
                            {b.getInt32(instruction.immediate),values[instruction.arguments[0]],failure});selected_failure();
                        v=llvm::UndefValue::get(b.getInt8Ty());break;
                    }
                    auto* offsets_type=llvm::ArrayType::get(size_type,7);
                    auto* record_type=llvm::StructType::get(size_type,offsets_type,b.getPtrTy());
                    if(layout.getTypeAllocSize(record_type)!=sizeof(host::ReplyLayout) ||
                       layout.getStructLayout(record_type)->getElementOffset(2)!=offsetof(host::ReplyLayout,strings))throw std::logic_error("Host reply ABI mismatch");
                    std::vector<llvm::Constant*> offsets;
                    for(unsigned k=0;k<7;++k)offsets.push_back(llvm::ConstantInt::get(size_type,field_offset(instruction.type,k)));
                    llvm::Constant* strings=llvm::ConstantPointerNull::get(b.getPtrTy());
                    for(unsigned k=0;k<contract.arguments.size();++k)if(contract.arguments[k]=='a')strings=element_record(ir::array_element(operand_type(instruction.arguments[k])));
                    auto* record=new llvm::GlobalVariable(*module,record_type,true,llvm::GlobalValue::PrivateLinkage,
                        llvm::ConstantStruct::get(record_type,{llvm::ConstantInt::get(size_type,layout.getTypeAllocSize(type(instruction.type))),llvm::ConstantArray::get(offsets_type,offsets),strings}),"host.reply.layout");
                    llvm::IRBuilder<> allocations(&*fn->getEntryBlock().begin());
                    auto* rt=llvm::ArrayType::get(b.getPtrTy(),8);auto* nt=llvm::ArrayType::get(b.getInt64Ty(),8);
                    auto* refs=home(allocations.CreateAlloca(rt),StorageRole::known_abi,llvm_backend::HomeKind::value);
                    auto* nums=home(allocations.CreateAlloca(nt),StorageRole::known_abi,llvm_backend::HomeKind::value);
                    b.CreateStore(llvm::Constant::getNullValue(rt),refs);b.CreateStore(llvm::Constant::getNullValue(nt),nums);
                    unsigned r=0,s=0;
                    for(unsigned k=0;k<contract.arguments.size();++k) {
                        const bool scalar=contract.arguments[k]=='s';
                        b.CreateStore(values[instruction.arguments[k]],b.CreateInBoundsGEP(scalar?nt:rt,scalar?nums:refs,{b.getInt32(0),b.getInt32(scalar?s++:r++)}));
                    }
                    auto* output=value_roots[n].at(values.size());b.CreateStore(llvm::Constant::getNullValue(type(instruction.type)),output);
                    b.CreateCall(module->getOrInsertFunction("vd_host_call",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getInt32Ty(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),
                        {output,record,b.getInt32(instruction.immediate),refs,nums,failure});selected_failure();v=b.CreateLoad(type(instruction.type),output);break;
                }
