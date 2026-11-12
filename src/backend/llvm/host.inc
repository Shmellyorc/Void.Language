                case Op::host_native: {
                    const auto& contract=host::operations.at(instruction.immediate);
                    const bool network=host::network_operation(instruction.immediate);
                    if(contract.cleanup) {
                        b.CreateCall(module->getOrInsertFunction(network?"vd_net_cleanup":"vd_host_cleanup",b.getVoidTy(),b.getInt32Ty(),b.getPtrTy(),b.getPtrTy()),
                            {b.getInt32(instruction.immediate),values[instruction.arguments[0]],failure});selected_failure();
                        v=llvm::UndefValue::get(b.getInt8Ty());break;
                    }
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
                    llvm::Constant* strings=llvm::ConstantPointerNull::get(b.getPtrTy());
                    for(unsigned k=0;k<contract.arguments.size();++k)if(contract.arguments[k]=='a')strings=element_record(ir::array_element(operand_type(instruction.arguments[k])));
                    const bool capture=host::capture_operation(instruction.immediate);
                    const bool asynchronous=host::asynchronous(instruction.immediate);
                    const auto reply=asynchronous?ir::field_type(instruction.type,1):instruction.type;
                    const unsigned fields=network?12:capture?11:7;
                    auto* offsets_type=llvm::ArrayType::get(size_type,fields);
                    std::vector<llvm::Constant*> offsets;
                    for(unsigned k=0;k<fields;++k)offsets.push_back(llvm::ConstantInt::get(size_type,field_offset(reply,k)));
                    std::vector<llvm::Constant*> geometry{
                        llvm::ConstantInt::get(size_type,layout.getTypeAllocSize(type(reply))),llvm::ConstantArray::get(offsets_type,offsets),strings};
                    auto* record_type=llvm::StructType::get(size_type,offsets_type,b.getPtrTy());
                    if(capture || network) {
                        record_type=llvm::StructType::get(size_type,offsets_type,b.getPtrTy(),size_type,b.getPtrTy(),size_type,size_type,b.getPtrTy());
                        if(layout.getTypeAllocSize(record_type)!=(network?sizeof(host::NetworkLayout):sizeof(host::CaptureLayout)) ||
                           layout.getStructLayout(record_type)->getElementOffset(7)!=(network?offsetof(host::NetworkLayout,task_key):offsetof(host::CaptureLayout,task_key)))
                            throw std::logic_error("Extended host reply ABI mismatch");
                        auto task=asynchronous?ir::Type(instruction.type.definition):ir::Type(ir::Type::none);
                        std::vector<std::uint64_t> task_refs;
                        if(asynchronous)reference_offsets(task,0,task_refs);
                        geometry.insert(geometry.end(),{
                            llvm::ConstantInt::get(size_type,asynchronous?layout.getTypeAllocSize(type(task)):0),
                            offset_map(task_refs),llvm::ConstantInt::get(size_type,task_refs.size()),
                            llvm::ConstantInt::get(size_type,asynchronous?field_offset(instruction.type,1):0),
                            b.CreateGlobalString(asynchronous?instruction.type.definition->identity:"")});
                    } else if(layout.getTypeAllocSize(record_type)!=sizeof(host::ReplyLayout) ||
                        layout.getStructLayout(record_type)->getElementOffset(2)!=offsetof(host::ReplyLayout,strings))
                        throw std::logic_error("Host reply ABI mismatch");
                    auto* record=new llvm::GlobalVariable(*module,record_type,true,llvm::GlobalValue::PrivateLinkage,
                        llvm::ConstantStruct::get(record_type,geometry),"host.reply.layout");
                    if(capture || network)b.CreateCall(module->getOrInsertFunction(network?"vd_net_call":"vd_host_capture",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getInt32Ty(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),
                        {output,record,b.getInt32(instruction.immediate),refs,nums,async_generation,failure});
                    else b.CreateCall(module->getOrInsertFunction("vd_host_call",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getInt32Ty(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),
                        {output,record,b.getInt32(instruction.immediate),refs,nums,failure});
                    selected_failure();v=b.CreateLoad(type(instruction.type),output);break;
                }
