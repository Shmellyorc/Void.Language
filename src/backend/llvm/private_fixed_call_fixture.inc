// Closed native experiment inside the shared lowerer. The input body is already
// checked by ir::validate. This is not general source-array lowering.
if(input.functions.size()!=1 || !input.coordinated || !input.imports.empty())
    throw std::invalid_argument("Private fixed-call fixture requires one coordinated closed function");
const auto& logical=input.functions.front();
if(!logical.fixed_ownership || logical.receiver || logical.initializer || !logical.virtual_slot.empty() || !logical.locals.empty() || logical.parameters.size()!=8 ||
   !ir::scalar_fixed_array(logical.result) || logical.result.definition->element->kind!=ir::Type::u64 ||
   !*logical.result.definition->extent || logical.blocks.size()!=1 ||
   logical.blocks[0].instructions.size()!=1 || logical.blocks[0].instructions[0].op!=ir::Op::argument ||
   logical.blocks[0].instructions[0].a>=8 || logical.blocks[0].end.kind!=ir::Terminator::Kind::return_value ||
   logical.blocks[0].end.value!=0)
    throw std::invalid_argument("Private fixed-call fixture admits only eight-u64-fixed-array argument return");
for(auto parameter:logical.parameters) if(parameter!=logical.result)
    throw std::invalid_argument("Private fixed-call fixture requires identical argument/result types");
auto* ptr=b.getPtrTy();auto* word=b.getInt64Ty();
auto* handles=llvm::ArrayType::get(ptr,8);
auto* slot_type=llvm::StructType::get(ptr,word,ptr,word);
// The Runtime Frame ABI is shared with normal managed lowering. Payloads remain
// in Runtime buffers; native homes contain only precisely rooted handles.
auto* slot_layout=layout.getStructLayout(slot_type);
const std::array<std::size_t,4> slot_offsets{offsetof(Runtime::Slot,storage),offsetof(Runtime::Slot,size),offsetof(Runtime::Slot,offsets),offsetof(Runtime::Slot,count)};
if(slot_layout->getSizeInBytes()!=sizeof(Runtime::Slot) || layout.getABITypeAlign(slot_type).value()!=alignof(Runtime::Slot))
    throw std::invalid_argument("Private fixed-call Slot layout mismatch");
for(unsigned i=0;i<4;++i) if(slot_layout->getElementOffset(i)!=slot_offsets[i])
    throw std::invalid_argument("Private fixed-call Slot field mismatch");
auto* offset=new llvm::GlobalVariable(*module,word,true,llvm::GlobalValue::InternalLinkage,b.getInt64(0),"root_offset");
auto call=[&](const char* name,llvm::Type* result,std::initializer_list<llvm::Value*> arguments) {
    std::vector<llvm::Type*> types;for(auto* argument:arguments) types.push_back(argument->getType());
    return b.CreateCall(module->getOrInsertFunction(name,llvm::FunctionType::get(result,types,false)),arguments);
};
auto element=[&](llvm::Value* base,unsigned index) {
    return b.CreateInBoundsGEP(handles,base,{b.getInt32(0),b.getInt32(index)});
};
auto roots=[&]() {
    auto* homes=b.CreateAlloca(handles);
    b.CreateStore(llvm::ConstantAggregateZero::get(handles),homes);
    auto* slots_type=llvm::ArrayType::get(slot_type,8);
    auto* slots=b.CreateAlloca(slots_type);
    auto* frame=b.CreateAlloca(llvm::ArrayType::get(b.getInt8Ty(),sizeof(Runtime::Frame)));
    frame->setAlignment(llvm::Align(alignof(Runtime::Frame)));
    for(unsigned i=0;i<8;++i) {
        auto* slot=b.CreateInBoundsGEP(slots_type,slots,{b.getInt32(0),b.getInt32(i)});
        for(auto [field,value]:std::initializer_list<std::pair<unsigned,llvm::Value*>>{
            {0,element(homes,i)},{1,b.getInt64(sizeof(void*))},{2,offset},{3,b.getInt64(1)}})
            b.CreateStore(value,b.CreateStructGEP(slot_type,slot,field));
    }
    return std::array<llvm::Value*,3>{homes,slots,frame};
};
auto enter=[&](const auto& r) {call("vd_gc_frame_enter",b.getVoidTy(),{r[2],r[1],b.getInt64(8)});};
auto failed=[&](llvm::Value* failure) {return b.CreateIsNotNull(b.CreateLoad(ptr,failure));};
// Eight argument homes plus result/layout/quota/failure. LLVM's ordinary calling
// convention owns all register and stack transport, including optimized forms.
auto* callee=llvm::Function::Create(llvm::FunctionType::get(b.getVoidTy(),std::vector<llvm::Type*>(12,ptr),false),
    llvm::GlobalValue::InternalLinkage,"void_fixed_callee",*module);
callee->addFnAttr(llvm::Attribute::NoInline); // Exercise a real native call.
b.SetInsertPoint(llvm::BasicBlock::Create(context,"entry",callee));
auto cr=roots();
for(unsigned i=0;i<8;++i) b.CreateStore(b.CreateLoad(ptr,callee->getArg(i)),element(cr[0],i));
// No safepoint between loading rooted incoming handles and callee registration.
enter(cr);
auto* callee_exit=llvm::BasicBlock::Create(context,"cleanup",callee);
auto* failure=callee->getArg(11);
auto guard_callee=[&]() {
    auto* next=llvm::BasicBlock::Create(context,"continue",callee);
    b.CreateCondBr(failed(failure),callee_exit,next);b.SetInsertPoint(next);
};
// Test observation/fault hooks return no permission and own no call metadata.
call("research_fixed_callee_ready",b.getVoidTy(),{cr[0]});guard_callee();
call("vd_gc_collect",b.getVoidTy(),{failure});
call("research_fixed_after_poll",b.getVoidTy(),{});guard_callee();
call("vd_fixed_copy",b.getVoidTy(),{callee->getArg(8),element(cr[0],logical.blocks[0].instructions[0].a),callee->getArg(9),callee->getArg(10),failure});
b.CreateBr(callee_exit);b.SetInsertPoint(callee_exit);
call("vd_gc_frame_leave",b.getVoidTy(),{cr[2]});
call("research_fixed_frame_left",b.getVoidTy(),{b.getInt32(1)});b.CreateRetVoid();
// Embedding boundary: source/result/layout/quota/failure are borrowed for the
// call. Existing Runtime helpers check roots and publish complete copies only.
auto* caller=llvm::Function::Create(llvm::FunctionType::get(word,std::vector<llvm::Type*>(5,ptr),false),
    llvm::GlobalValue::ExternalLinkage,"void_fixed_caller",*module);
b.SetInsertPoint(llvm::BasicBlock::Create(context,"entry",caller));
auto rr=roots();enter(rr);
auto* source=caller->getArg(0);auto* result=caller->getArg(1);
auto* l=caller->getArg(2);auto* q=caller->getArg(3);auto* fail=caller->getArg(4);
auto* cleanup=llvm::BasicBlock::Create(context,"cleanup",caller);
auto* answer=llvm::PHINode::Create(word,20,"processed_result",cleanup->end());
auto guard=[&]() {
    auto* next=llvm::BasicBlock::Create(context,"continue",caller);auto* previous=b.GetInsertBlock();
    b.CreateCondBr(failed(fail),cleanup,next);answer->addIncoming(b.getInt64(0),previous);b.SetInsertPoint(next);
};
guard();
auto* distinct=llvm::BasicBlock::Create(context,"distinct_context",caller);
auto* alias=llvm::BasicBlock::Create(context,"aliased_context",caller);
b.CreateCondBr(b.CreateICmpNE(source,result),distinct,alias);b.SetInsertPoint(alias);
b.CreateStore(b.CreateGlobalString("Invalid aliased context"),fail);
b.CreateBr(cleanup);answer->addIncoming(b.getInt64(0),alias);b.SetInsertPoint(distinct);
// Check the compiler's scalar type/extent once at the embedding boundary.
// Per-operation root/buffer/publication validation stays in Runtime helpers.
call("vd_fixed_validate_layout",b.getVoidTy(),{l,b.getInt64(*logical.result.definition->extent),b.getInt32(static_cast<unsigned>(ScalarEncoding::u64)),fail});guard();
std::vector<llvm::Value*> arguments;
for(unsigned i=0;i<8;++i) {
    auto* destination=element(rr[0],i);arguments.push_back(destination);
    call("vd_fixed_copy",b.getVoidTy(),{destination,source,l,q,fail});guard();
    call("vd_fixed_write",b.getVoidTy(),{source,l,b.getInt64(0),b.getInt64(8+i),fail});guard();
}
call("research_fixed_snapshots_ready",b.getVoidTy(),{rr[0]});guard();
arguments.insert(arguments.end(),{result,l,q,fail});b.CreateCall(callee,arguments);guard();
auto* value=call("vd_fixed_read",word,{result,l,b.getInt64(0),fail});guard();
auto* processed=b.CreateAdd(value,b.getInt64(1));auto* previous=b.GetInsertBlock();
b.CreateBr(cleanup);answer->addIncoming(processed,previous);
b.SetInsertPoint(cleanup);call("vd_gc_frame_leave",b.getVoidTy(),{rr[2]});
call("research_fixed_frame_left",b.getVoidTy(),{b.getInt32(0)});b.CreateRet(answer);
return module;
