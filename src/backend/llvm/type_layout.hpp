#pragma once

#include "void/ir.hpp"

namespace llvm { class DataLayout; class LLVMContext; class Module; class StructType; class Type; }

namespace vd::llvm_backend {

// One mapping per lowered module. StructLayout keys are owned by the module's
// LLVMContext, so use the module's layout for all native offsets.
class TypeLayout {
    llvm::LLVMContext& context_;
    const llvm::DataLayout& layout_;
    bool coordinated_;
    llvm::StructType* interface_;
    llvm::StructType* prepared_;
    std::map<std::string, llvm::StructType*> aggregates_;
public:
    TypeLayout(llvm::Module&, bool coordinated);
    llvm::StructType* interface_type() const { return interface_; }
    llvm::StructType* prepared_receiver_type() const { return prepared_; }
    llvm::Type* operator()(ir::Type);
    std::uint64_t field_offset(ir::Type owner, unsigned field);
    void reference_offsets(ir::Type, std::uint64_t base, std::vector<std::uint64_t>& result);
};
}
