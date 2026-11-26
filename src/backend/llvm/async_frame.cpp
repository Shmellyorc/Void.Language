#include "async_frame.hpp"
#include "type_layout.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

namespace vd::llvm_backend {
AsyncPlan plan_async_frame(llvm::Module& module, TypeLayout& type, const ir::Function& f,
                           const std::vector<std::vector<ir::Type>>& checked_types,
                           const std::string& suffix, llvm::Function* root_entry) {
    auto& context = module.getContext();
    const auto& layout = module.getDataLayout();
    llvm::IRBuilder<> b(context);
    AsyncPlan plan;
    std::vector<llvm::Type*> fields{b.getPtrTy(),b.getInt32Ty()};std::vector<ir::Type> logical{f.task,ir::Type::i32};
    auto add=[&](ir::Type t){auto i=static_cast<unsigned>(fields.size());fields.push_back(t==ir::Type::none?b.getInt8Ty():type(t));logical.push_back(t);return i;};
    for(auto t:f.parameters)plan.parameters.push_back(add(t));for(auto t:f.locals)plan.locals.push_back(add(t));
    for(const auto& block:checked_types){std::vector<unsigned> values;for(auto t:block)values.push_back(add(t));plan.values.push_back(std::move(values));}
    plan.frame=llvm::StructType::create(context,fields,"async.frame"+suffix);auto* native=layout.getStructLayout(plan.frame);
    for(unsigned n=0;n<logical.size();++n)if(logical[n]!=ir::Type::none)type.reference_offsets(logical[n],native->getElementOffset(n),plan.offsets);
    plan.resume=llvm::Function::Create(llvm::FunctionType::get(b.getVoidTy(),{b.getPtrTy(),b.getPtrTy()},false),llvm::GlobalValue::InternalLinkage,"async.resume"+suffix,module);
#ifdef VOID_ASYNC_START_SANITIZER_HASH
    auto metadata=[&](llvm::Function* function,std::int32_t magic,std::int32_t hash){function->setMetadata("func_sanitize",llvm::MDNode::get(context,{llvm::ConstantAsMetadata::get(b.getInt32(magic)),llvm::ConstantAsMetadata::get(b.getInt32(hash))}));};
    if(root_entry)metadata(root_entry,VOID_ASYNC_START_SANITIZER_MAGIC,VOID_ASYNC_START_SANITIZER_HASH);
    metadata(plan.resume,VOID_ASYNC_RESUME_SANITIZER_MAGIC,VOID_ASYNC_RESUME_SANITIZER_HASH);
#else
    (void)root_entry;
#endif
    return plan;
}
}
