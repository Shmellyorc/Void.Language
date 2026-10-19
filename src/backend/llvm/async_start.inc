// Async starts only initialize and enqueue. Resume bodies use the ordinary
// validated lowering above; task/frame roots use the same Runtime Frame ABI.
for(const auto& f:input.functions)if(f.asynchronous){
    const auto& plan=async_plans.at(f.name);auto* start=functions.at(f.name);
    b.SetCurrentDebugLocation(llvm::DebugLoc());b.SetInsertPoint(llvm::BasicBlock::Create(context,"async.create",start));
    auto* failure=start->getArg(static_cast<unsigned>(f.parameters.size()));
    auto* task=home(b.CreateAlloca(b.getPtrTy()),StorageRole::roots,llvm_backend::HomeKind::root);
    auto* frame=home(b.CreateAlloca(b.getPtrTy()),StorageRole::roots,llvm_backend::HomeKind::root);
    b.CreateStore(llvm::ConstantPointerNull::get(b.getPtrTy()),task);b.CreateStore(llvm::ConstantPointerNull::get(b.getPtrTy()),frame);
    auto* slot_type=llvm::StructType::get(b.getPtrTy(),size_type,b.getPtrTy(),size_type);
    std::vector<llvm::Value*> entries;
    auto root=[&](llvm::Value* address,std::uint64_t bytes,const std::vector<std::uint64_t>& offsets){if(offsets.empty())return;llvm::Value* slot=llvm::UndefValue::get(slot_type);slot=b.CreateInsertValue(slot,address,0);slot=b.CreateInsertValue(slot,llvm::ConstantInt::get(size_type,bytes),1);slot=b.CreateInsertValue(slot,offset_map(offsets),2);slot=b.CreateInsertValue(slot,llvm::ConstantInt::get(size_type,offsets.size()),3);entries.push_back(slot);};
    root(task,layout.getPointerSize(),{0});root(frame,layout.getPointerSize(),{0});
    std::vector<llvm::Value*> parameters;
    for(unsigned n=0;n<f.parameters.size();++n){auto* storage=home(b.CreateAlloca(type(f.parameters[n])),StorageRole::parameter,llvm_backend::HomeKind::argument);b.CreateStore(start->getArg(n),storage);parameters.push_back(storage);std::vector<std::uint64_t> offsets;reference_offsets(f.parameters[n],0,offsets);root(storage,layout.getTypeAllocSize(type(f.parameters[n])),offsets);}
    auto* roots_type=llvm::ArrayType::get(slot_type,entries.size());auto* roots=home(b.CreateAlloca(roots_type),StorageRole::roots,llvm_backend::HomeKind::slots);
    for(unsigned n=0;n<entries.size();++n)b.CreateStore(entries[n],b.CreateInBoundsGEP(roots_type,roots,{b.getInt32(0),b.getInt32(n)}));
    auto* registration=home(b.CreateAlloca(b.getInt8Ty(),b.getInt64(sizeof(Runtime::Frame))),StorageRole::roots,llvm_backend::HomeKind::frame);registration->setAlignment(llvm::Align(alignof(Runtime::Frame)));
    b.CreateCall(module->getOrInsertFunction("vd_gc_frame_enter",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),size_type),{registration,roots,llvm::ConstantInt::get(size_type,entries.size())});
    auto leave=[&]{b.CreateCall(module->getOrInsertFunction("vd_gc_frame_leave",b.getVoidTy(),b.getPtrTy()),{registration});};
    auto* failed=llvm::BasicBlock::Create(context,"async.create_failure",start);
    auto check=[&]{auto* okay=llvm::BasicBlock::Create(context,"async.create_valid",start);b.CreateCondBr(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),failure)),failed,okay);b.SetInsertPoint(okay);};
    auto* payload=llvm::cast<llvm::StructType>(type(ir::Type(f.task.definition)));std::vector<std::uint64_t> task_offsets;
    for(unsigned n=0;n<f.task.definition->fields.size();++n)reference_offsets(f.task.definition->fields[n].type,layout.getStructLayout(payload)->getElementOffset(n),task_offsets);
    b.CreateCall(module->getOrInsertFunction("vd_async_new",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),size_type,b.getPtrTy(),size_type,size_type,b.getPtrTy(),size_type,b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),
        {task,frame,llvm::ConstantInt::get(size_type,layout.getTypeAllocSize(plan.frame)),offset_map(plan.offsets),llvm::ConstantInt::get(size_type,plan.offsets.size()),llvm::ConstantInt::get(size_type,layout.getTypeAllocSize(payload)),offset_map(task_offsets),llvm::ConstantInt::get(size_type,task_offsets.size()),b.CreateGlobalStringPtr(f.task.definition->identity),async_generation,plan.resume,failure});check();
    auto* activation=b.CreateLoad(b.getPtrTy(),frame);auto* handle=b.CreateLoad(b.getPtrTy(),task);b.CreateStore(handle,b.CreateStructGEP(plan.frame,activation,0));b.CreateStore(b.getInt32(0),b.CreateStructGEP(plan.frame,activation,1));
    for(unsigned n=0;n<f.parameters.size();++n){llvm::Value* value=b.CreateLoad(type(f.parameters[n]),parameters[n]);if(contains_arrays(f.parameters[n])){value=b.CreateCall(functions.at(f.name+"::$asynccopy."+std::to_string(n)),{value,failure});check();b.CreateStore(value,parameters[n]);}b.CreateStore(value,b.CreateStructGEP(plan.frame,activation,plan.parameters[n]));if(f.receiver && n==0)b.CreateStore(value,b.CreateStructGEP(plan.frame,activation,plan.locals[0]));}
    b.CreateCall(module->getOrInsertFunction("vd_async_start",b.getVoidTy(),b.getPtrTy(),b.getPtrTy()),{handle,failure});check();leave();b.CreateRet(handle);
    b.SetInsertPoint(failed);leave();b.CreateRet(llvm::ConstantPointerNull::get(b.getPtrTy()));
    if(input.async_root && f.name==exported_entry){
        auto* adapter=llvm::Function::Create(llvm::FunctionType::get(b.getInt32Ty(),{b.getInt32Ty(),b.getPtrTy()},false),llvm::GlobalValue::ExternalLinkage,native_symbol(f,suffix),*module);
        b.SetInsertPoint(llvm::BasicBlock::Create(context,"async.root",adapter));auto* value=b.CreateCall(module->getOrInsertFunction("vd_async_root",b.getInt32Ty(),b.getPtrTy(),b.getInt32Ty(),size_type,b.getPtrTy(),b.getPtrTy()),{start,adapter->getArg(0),llvm::ConstantInt::get(size_type,layout.getStructLayout(payload)->getElementOffset(1)),async_generation,adapter->getArg(1)});b.CreateRet(value);
    }
}
