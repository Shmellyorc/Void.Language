#include "generator_frame.hpp"
#include "type_layout.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

namespace vd::llvm_backend {
namespace {
std::vector<std::set<unsigned>> live_on_entry(const ir::Function& function) {
    const auto& blocks=function.blocks;
    std::vector<std::set<unsigned>> uses(blocks.size()),definitions(blocks.size()),live(blocks.size());
    for(unsigned block=0;block<blocks.size();++block) {
        auto use=[&](unsigned slot) {
            if(!definitions[block].contains(slot))uses[block].insert(slot);
        };
        for(const auto& instruction:blocks[block].instructions) {
            if(instruction.receiver)use(instruction.receiver->slot);
            if(instruction.op==ir::Op::load)use(instruction.a);
            if(instruction.op==ir::Op::local_cleanup) {
                use(instruction.a);
                use(instruction.b);
            }
            if(instruction.op==ir::Op::store)definitions[block].insert(instruction.a);
        }
    }
    bool changed;
    do {
        changed=false;
        for(unsigned block=blocks.size();block-->0;) {
            auto incoming=uses[block];
            auto absorb=[&](unsigned successor) {
                for(auto slot:live[successor])
                    if(!definitions[block].contains(slot))incoming.insert(slot);
            };
            using End=ir::Terminator::Kind;
            const auto& end=blocks[block].end;
            if(end.kind==End::jump || end.kind==End::branch || end.kind==End::yield)absorb(end.yes);
            if(end.kind==End::branch)absorb(end.no);
            // LLVM failures can leave any body block for the same cleanup.
            // Preserve those locals even when the failure is before a store.
            if(function.cleanup_entry && block<*function.cleanup_entry)
                incoming.insert(live[*function.cleanup_entry].begin(),live[*function.cleanup_entry].end());
            if(incoming!=live[block]) {
                live[block]=std::move(incoming);
                changed=true;
            }
        }
    } while(changed);
    return live;
}
}

GeneratorPlan plan_generator_frame(llvm::Module& module, TypeLayout& type, const ir::Function& function) {
    auto& context=module.getContext();
    llvm::IRBuilder<> builder(context);
    GeneratorPlan plan;
    std::vector<ir::Type> logical;
    std::vector<llvm::Type*> native;
    auto add=[&](ir::Type item) {
        const auto index=static_cast<unsigned>(logical.size());
        logical.push_back(item);
        native.push_back(type(item));
        return index;
    };
    for(auto parameter:function.parameters)plan.parameters.push_back(add(parameter));
    for(auto local:function.locals)plan.locals.push_back(add(local));
    if(native.empty())native.push_back(builder.getInt8Ty());
    plan.frame=llvm::StructType::create(context,native,"generator.frame");
    const auto& geometry=*module.getDataLayout().getStructLayout(plan.frame);
    for(unsigned field=0;field<logical.size();++field)
        type.reference_offsets(logical[field],geometry.getElementOffset(field),plan.offsets);
    auto result=ir::Type(*function.result.definition->generator_result);
    auto signature=llvm::FunctionType::get(type(result),{builder.getPtrTy(),builder.getInt1Ty(),builder.getPtrTy()},false);
    plan.resume=llvm::Function::Create(signature,llvm::GlobalValue::InternalLinkage,"generator.resume",module);

    plan.retire_at_yield=generator_yield_retirements(type,function);
    return plan;
}

std::vector<std::vector<unsigned>> generator_yield_retirements(TypeLayout& type,const ir::Function& function) {
    auto live=live_on_entry(function);
    std::vector<std::vector<unsigned>> retirement(function.blocks.size());
    for(unsigned block=0;block<function.blocks.size();++block) {
        const auto& end=function.blocks[block].end;
        if(end.kind!=ir::Terminator::Kind::yield)continue;
        for(unsigned slot=0;slot<function.locals.size();++slot) {
            auto logical=function.locals[slot];
            if(logical.construction || logical.kind==ir::Type::prepared_receiver) {
                retirement[block].push_back(slot);
                continue;
            }
            if(live[end.yes].contains(slot) || (function.cleanup_entry && live[*function.cleanup_entry].contains(slot)))continue;
            std::vector<std::uint64_t> roots;
            type.reference_offsets(function.locals[slot],0,roots);
            if(!roots.empty())retirement[block].push_back(slot);
        }
    }
    return retirement;
}
}
