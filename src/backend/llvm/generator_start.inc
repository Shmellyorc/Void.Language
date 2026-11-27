for(const auto& f:input.functions)if(f.generator) {
    const auto* asynchronous=f.asynchronous?&async_plans.at(f.name):nullptr;
    const auto* synchronous=f.asynchronous?nullptr:&generator_plans.at(f.name);
    auto* frame_type=asynchronous?asynchronous->frame:synchronous->frame;
    const auto& offsets=asynchronous?asynchronous->offsets:synchronous->offsets;
    const auto& parameters=asynchronous?asynchronous->parameters:synchronous->parameters;
    auto* start=functions.at(f.name);
    b.SetCurrentDebugLocation(llvm::DebugLoc());
    b.SetInsertPoint(llvm::BasicBlock::Create(context,"generator.create",start));
    auto* failure=start->getArg(f.parameters.size());
    auto* absent=llvm::ConstantPointerNull::get(b.getPtrTy());
    auto make_root=[&] {
        auto* slot=home(b.CreateAlloca(b.getPtrTy()),StorageRole::roots,llvm_backend::HomeKind::root);
        b.CreateStore(absent,slot);
        return slot;
    };
    auto* handle=make_root();
    auto* frame=make_root();
    auto* slot_type=llvm::StructType::get(b.getPtrTy(),size_type,b.getPtrTy(),size_type);
    std::vector<llvm::Value*> entries;
    auto root=[&](llvm::Value* address,std::uint64_t bytes,const std::vector<std::uint64_t>& offsets) {
        if(offsets.empty())return;
        llvm::Value* slot=llvm::UndefValue::get(slot_type);
        slot=b.CreateInsertValue(slot,address,0);
        slot=b.CreateInsertValue(slot,llvm::ConstantInt::get(size_type,bytes),1);
        slot=b.CreateInsertValue(slot,offset_map(offsets),2);
        slot=b.CreateInsertValue(slot,llvm::ConstantInt::get(size_type,offsets.size()),3);
        entries.push_back(slot);
    };
    root(handle,layout.getPointerSize(),{0});
    root(frame,layout.getPointerSize(),{0});
    for(unsigned n=0;n<f.parameters.size();++n) {
        std::vector<std::uint64_t> offsets;
        type.reference_offsets(f.parameters[n],0,offsets);
        if(offsets.empty())continue;
        auto* storage=home(b.CreateAlloca(type(f.parameters[n])),StorageRole::parameter,llvm_backend::HomeKind::argument);
        b.CreateStore(start->getArg(n),storage);
        root(storage,layout.getTypeAllocSize(type(f.parameters[n])),offsets);
    }
    auto* roots_type=llvm::ArrayType::get(slot_type,entries.size());
    auto* roots=home(b.CreateAlloca(roots_type),StorageRole::roots,llvm_backend::HomeKind::slots);
    for(unsigned n=0;n<entries.size();++n)
        b.CreateStore(entries[n],b.CreateInBoundsGEP(roots_type,roots,{b.getInt32(0),b.getInt32(n)}));
    auto* registration=home(b.CreateAlloca(b.getInt8Ty(),b.getInt64(sizeof(Runtime::Frame))),StorageRole::roots,llvm_backend::HomeKind::frame);
    registration->setAlignment(llvm::Align(alignof(Runtime::Frame)));
    b.CreateCall(module->getOrInsertFunction("vd_gc_frame_enter",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),size_type),
        {registration,roots,llvm::ConstantInt::get(size_type,entries.size())});
    auto leave=[&] {
        b.CreateCall(module->getOrInsertFunction("vd_gc_frame_leave",b.getVoidTy(),b.getPtrTy()),{registration});
    };
    auto* failed=llvm::BasicBlock::Create(context,"generator.create_failure",start);
    auto check=[&] {
        auto* okay=llvm::BasicBlock::Create(context,"generator.create_valid",start);
        b.CreateCondBr(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),failure)),failed,okay);
        b.SetInsertPoint(okay);
    };
    b.CreateCall(module->getOrInsertFunction("vd_gc_allocate",b.getVoidTy(),b.getPtrTy(),size_type,b.getPtrTy(),size_type,b.getPtrTy()),
        {frame,llvm::ConstantInt::get(size_type,layout.getTypeAllocSize(frame_type)),offset_map(offsets),
         llvm::ConstantInt::get(size_type,offsets.size()),failure});
    check();
    auto* activation=b.CreateLoad(b.getPtrTy(),frame);
    for(unsigned n=0;n<f.parameters.size();++n)
        b.CreateStore(start->getArg(n),b.CreateStructGEP(frame_type,activation,parameters[n]));

    auto* state=llvm::cast<llvm::StructType>(type(ir::Type(f.result.definition)));
    auto* storage=home(b.CreateAlloca(state),StorageRole::known_abi,llvm_backend::HomeKind::value);
    b.CreateStore(llvm::Constant::getNullValue(state),storage);
    auto put=[&](unsigned field,llvm::Value* value) {b.CreateStore(value,b.CreateStructGEP(state,storage,field));};
    put(GeneratorField::frame_pointer,activation);
    put(GeneratorField::generation,generator_generation());
    put(GeneratorField::worker_thread,generator_thread());
    put(GeneratorField::route,b.getInt64(function_ids.at(f.name)));
    std::vector<std::uint64_t> handle_offsets;
    type.reference_offsets(ir::Type(f.result.definition),0,handle_offsets);
    b.CreateCall(module->getOrInsertFunction("vd_gc_construct_typed",b.getVoidTy(),b.getPtrTy(),size_type,b.getPtrTy(),
        size_type,b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),
        {handle,llvm::ConstantInt::get(size_type,layout.getTypeAllocSize(state)),offset_map(handle_offsets),
         llvm::ConstantInt::get(size_type,handle_offsets.size()),storage,b.CreateGlobalString(ir::Type::key(f.result)),failure});
    check();
    auto* value=b.CreateLoad(b.getPtrTy(),handle);
    if(asynchronous)b.CreateStore(value,b.CreateStructGEP(frame_type,activation,0));
    leave();
    b.CreateRet(value);
    b.SetInsertPoint(failed);
    leave();
    b.CreateRet(absent);
}
