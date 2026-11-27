// Native addresses remain inside this LLVM module. Handle payloads carry only
// a generation number and a checked route to their own resume body.
std::map<std::string,GeneratorPlan> generator_plans;
std::map<std::string,llvm::Function*> generator_dispatchers;
llvm::GlobalVariable* generator_epoch=nullptr;
auto generator_generation=[&]() {
    if(!generator_epoch) {
        generator_epoch=new llvm::GlobalVariable(*module,b.getInt64Ty(),false,
            llvm::GlobalValue::PrivateLinkage,b.getInt64(0),"generator.epoch");
        generator_epoch->setAlignment(llvm::Align(alignof(std::uint64_t)));
    }
    return b.CreateCall(module->getOrInsertFunction("vd_generator_generation",b.getInt64Ty(),b.getPtrTy()),{generator_epoch});
};
auto generator_thread=[&]() {
    return b.CreateCall(module->getOrInsertFunction("vd_generator_thread",b.getInt64Ty()));
};
using GeneratorField=GeneratorPlan::HandleField;
for(const auto& function:input.functions)if(function.generator && !function.asynchronous)
    generator_plans.emplace(function.name,plan_generator_frame(*module,type,function));
for(auto state:input.nominal_types)if(ir::generator_type(state) && !ir::async_generator_type(state)) {
    auto result=ir::Type(*state.definition->generator_result);
    auto* signature=llvm::FunctionType::get(type(result),{b.getPtrTy(),b.getInt1Ty(),b.getPtrTy()},false);
    auto* dispatch=llvm::Function::Create(signature,llvm::GlobalValue::InternalLinkage,"generator.dispatch",*module);
    generator_dispatchers.emplace(ir::Type::key(state),dispatch);
    auto* handle=dispatch->getArg(0);
    auto* close=dispatch->getArg(1);
    auto* failure=dispatch->getArg(2);
    auto* payload=llvm::cast<llvm::StructType>(type(ir::Type(state.definition)));
    auto* entry=llvm::BasicBlock::Create(context,"generator.enter",dispatch);
    auto* owner=llvm::BasicBlock::Create(context,"generator.check_owner",dispatch);
    auto* idle=llvm::BasicBlock::Create(context,"generator.check_state",dispatch);
    auto* route=llvm::BasicBlock::Create(context,"generator.route",dispatch);
    auto* done=llvm::BasicBlock::Create(context,"generator.done",dispatch);
    auto* invalid=llvm::BasicBlock::Create(context,"generator.invalid",dispatch);
    b.SetInsertPoint(entry);
    b.CreateCondBr(b.CreateIsNull(handle),invalid,owner);
    b.SetInsertPoint(owner);
    auto read=[&](unsigned field) {
        return b.CreateLoad(payload->getElementType(field),b.CreateStructGEP(payload,handle,field));
    };
    auto* same_generation=b.CreateICmpEQ(read(GeneratorField::generation),generator_generation());
    auto* same_thread=b.CreateICmpEQ(read(GeneratorField::worker_thread),generator_thread());
    b.CreateCondBr(b.CreateAnd(same_generation,same_thread),idle,invalid);
    b.SetInsertPoint(idle);
    auto* active=llvm::BasicBlock::Create(context,"generator.active",dispatch);
    b.CreateCondBr(b.CreateIsNull(read(GeneratorField::frame_pointer)),done,active);
    b.SetInsertPoint(active);
    b.CreateCondBr(b.CreateICmpEQ(read(GeneratorField::state),b.getInt32(GeneratorPlan::running)),invalid,route);
    b.SetInsertPoint(route);
    auto* select=b.CreateSwitch(read(GeneratorField::route),invalid);
    for(const auto& function:input.functions)if(function.generator && function.result==state) {
        auto* resume=llvm::BasicBlock::Create(context,"generator.call",dispatch);
        select->addCase(b.getInt64(function_ids.at(function.name)),resume);
        b.SetInsertPoint(resume);
        b.CreateRet(b.CreateCall(generator_plans.at(function.name).resume,{handle,close,failure}));
    }
    b.SetInsertPoint(invalid);
    b.CreateStore(b.CreateGlobalString("Invalid generator owner, generation or reentrant resumption"),failure);
    b.CreateBr(done);
    b.SetInsertPoint(done);
    b.CreateRet(llvm::Constant::getNullValue(type(result)));
}

std::map<std::string,llvm::Function*> async_generator_dispatchers;
for(auto state:input.nominal_types)if(ir::async_generator_type(state)) {
    auto* signature=llvm::FunctionType::get(b.getVoidTy(),{b.getPtrTy(),b.getPtrTy()},false);
    auto* dispatch=llvm::Function::Create(signature,llvm::GlobalValue::InternalLinkage,"generator.bind",*module);
    async_generator_dispatchers.emplace(ir::Type::key(state),dispatch);
    auto* handle=dispatch->getArg(0);
    auto* failure=dispatch->getArg(1);
    auto* payload=llvm::cast<llvm::StructType>(type(ir::Type(state.definition)));
    auto* entry=llvm::BasicBlock::Create(context,"generator.enter",dispatch);
    auto* owner=llvm::BasicBlock::Create(context,"generator.check_owner",dispatch);
    auto* claim=llvm::BasicBlock::Create(context,"generator.claim",dispatch);
    auto* invalid=llvm::BasicBlock::Create(context,"generator.invalid",dispatch);
    b.SetInsertPoint(entry);
    b.CreateCondBr(b.CreateIsNull(handle),invalid,owner);
    b.SetInsertPoint(owner);
    auto read=[&](unsigned field) {
        return b.CreateLoad(payload->getElementType(field),b.CreateStructGEP(payload,handle,field));
    };
    auto* same_generation=b.CreateICmpEQ(read(GeneratorField::generation),generator_generation());
    auto* same_thread=b.CreateICmpEQ(read(GeneratorField::worker_thread),generator_thread());
    auto* unused=b.CreateICmpEQ(read(GeneratorField::state),b.getInt32(0));
    b.CreateCondBr(b.CreateAnd(b.CreateAnd(same_generation,same_thread),unused),claim,invalid);
    b.SetInsertPoint(claim);
    auto* select=b.CreateSwitch(read(GeneratorField::route),invalid);
    for(const auto& function:input.functions)if(function.generator && function.result==state) {
        auto* bind=llvm::BasicBlock::Create(context,"generator.bind_owner",dispatch);
        select->addCase(b.getInt64(function_ids.at(function.name)),bind);
        b.SetInsertPoint(bind);
        b.CreateCall(module->getOrInsertFunction("vd_async_generator_bind",b.getVoidTy(),b.getPtrTy(),
            b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),
            {handle,read(GeneratorField::frame_pointer),async_generation,async_plans.at(function.name).resume,failure});
        auto* claimed=llvm::BasicBlock::Create(context,"generator.bound",dispatch);
        auto* done=llvm::BasicBlock::Create(context,"generator.bind_done",dispatch);
        b.CreateCondBr(b.CreateIsNull(b.CreateLoad(b.getPtrTy(),failure)),claimed,done);
        b.SetInsertPoint(claimed);
        b.CreateStore(b.getInt32(GeneratorPlan::running),b.CreateStructGEP(payload,handle,GeneratorField::state));
        b.CreateBr(done);
        b.SetInsertPoint(done);
        b.CreateRetVoid();
    }
    b.SetInsertPoint(invalid);
    b.CreateStore(b.CreateGlobalString("Async generator already has a consuming scope or belongs to another generation"),failure);
    b.CreateRetVoid();
}
