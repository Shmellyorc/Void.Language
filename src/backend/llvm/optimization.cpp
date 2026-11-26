#include "lowering.hpp"
#include "runtime_bool_abi.hpp"
#include "void/compiler_observation.hpp"
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Transforms/Scalar/LoopPassManager.h>
#include <llvm/Transforms/Scalar/SimpleLoopUnswitch.h>
#include <stdexcept>

namespace vd::llvm_backend {
namespace {
void remove_dominated_scalar_defaults(llvm::Function& function, llvm::FunctionAnalysisManager& analyses) {
    // Focus on large CFGs, where an extra definition can force expensive
    // dominance-frontier searches during scalar promotion.
    if (function.size() < 128) return;
    auto& entry = function.getEntryBlock();
    llvm::DominatorTree* dominators = nullptr;
    for (auto& instruction : entry) {
        auto* slot = llvm::dyn_cast<llvm::AllocaInst>(&instruction);
        if (!slot || !slot->isStaticAlloca() || slot->isArrayAllocation() || slot->isUsedByMetadata()) continue;
        auto* type = slot->getAllocatedType();
        if (!type->isIntegerTy() && !type->isFloatingPointTy()) continue;

        llvm::StoreInst* initial = nullptr;
        llvm::StoreInst* replacement = nullptr;
        bool private_scalar = true;
        for (auto* user : slot->users()) {
            if (auto* load = llvm::dyn_cast<llvm::LoadInst>(user)) {
                if (load->isSimple() && load->getType() == type) continue;
            }
            auto* store = llvm::dyn_cast<llvm::StoreInst>(user);
            if (!store || !store->isSimple() || store->getPointerOperand() != slot ||
                store->getValueOperand()->getType() != type) {
                private_scalar = false;
                break;
            }
            auto* value = llvm::dyn_cast<llvm::Constant>(store->getValueOperand());
            const bool entry_zero = store->getParent() == &entry && value && value->isNullValue();
            auto*& definition = entry_zero ? initial : replacement;
            if (definition) {
                private_scalar = false;
                break;
            }
            definition = store;
        }
        if (!private_scalar || !initial || !replacement ||
            initial->getMetadata(llvm::LLVMContext::MD_DIAssignID)) continue;
        if (replacement->getParent() == &entry && !initial->comesBefore(replacement)) continue;
        if (!dominators) dominators = &analyses.getResult<llvm::DominatorTreeAnalysis>(function);
        if (llvm::all_of(slot->users(), [&](const llvm::User* user) {
                auto* load = llvm::dyn_cast<llvm::LoadInst>(user);
                return !load || dominators->dominates(replacement, load);
            }))
            initial->eraseFromParent();
    }
}
}

void finish(llvm::Module& module, bool optimize, llvm::TargetMachine* analysis_machine,
            bool version_loops) {
    std::string message;
    llvm::raw_string_ostream stream(message);
    { observation::Scope timing(observation::Phase::llvm_verification);
      if (llvm::verifyModule(module, &stream)) throw std::runtime_error("invalid LLVM lowering: " + message); }
    detail::complete_runtime_bool_abi(module);
    if (!optimize) observation::skipped(observation::Phase::llvm_optimization);
    if (optimize) {
        observation::Scope optimization_timing(observation::Phase::llvm_optimization);
        llvm::LoopAnalysisManager loops;
        llvm::FunctionAnalysisManager functions;
        llvm::CGSCCAnalysisManager cgscc;
        llvm::ModuleAnalysisManager modules;
        llvm::PipelineTuningOptions tuning;
        tuning.SLPVectorization = false;
        llvm::PassBuilder builder(analysis_machine, tuning);
        if (analysis_machine && version_loops) {
            // Runtime-free AOT: expose invariant control before vectorization.
            // Coordinated modules keep their accepted checkpoint/call pipeline.
            // LLVM owns legality/profitability; strict FP and failures remain.
            builder.registerVectorizerStartEPCallback([](llvm::FunctionPassManager& pipeline,
                                                        llvm::OptimizationLevel) {
                pipeline.addPass(llvm::createFunctionToLoopPassAdaptor(
                    llvm::SimpleLoopUnswitchPass(true), true));
            });
        }
        builder.registerModuleAnalyses(modules);
        builder.registerCGSCCAnalyses(cgscc);
        builder.registerFunctionAnalyses(functions);
        builder.registerLoopAnalyses(loops);
        builder.crossRegisterProxies(loops, functions, cgscc, modules);
        // The proof requests only the dominator tree. Removing stores leaves
        // it valid for the existing pipeline.
        for (auto& function : module) remove_dominated_scalar_defaults(function, functions);
        auto pipeline = builder.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
        pipeline.run(module, modules);
        optimization_timing.finish();
        { observation::Scope timing(observation::Phase::llvm_verification);
          if (llvm::verifyModule(module, &stream)) throw std::runtime_error("invalid optimized LLVM IR: " + message); }
    }
}
}
