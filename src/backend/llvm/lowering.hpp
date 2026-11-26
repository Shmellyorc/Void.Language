#pragma once

#include "void/ir.hpp"

namespace llvm { class DataLayout; class LLVMContext; class Module; class TargetMachine; }

namespace vd {
struct InterfaceWitness;
struct NominalDescriptor;
namespace project { enum class DebugInfo; }

namespace llvm_backend {
class LoweringInventory;

// Both native execution paths call this lowerer with their own LLVM layout.
std::unique_ptr<llvm::Module> lower(const ir::Module&, llvm::LLVMContext&, const std::string& suffix,
    const llvm::DataLayout&, const std::string& exported_entry,
    std::vector<NominalDescriptor>* host_catalog = nullptr,
    std::vector<InterfaceWitness>* host_witnesses = nullptr,
    LoweringInventory* = nullptr, bool fixed_call_research = false,
    std::optional<project::DebugInfo> = {}, bool optimized = false);

void finish(llvm::Module&, bool optimize, llvm::TargetMachine* analysis_machine = nullptr,
            bool version_loops = false);
}
}
