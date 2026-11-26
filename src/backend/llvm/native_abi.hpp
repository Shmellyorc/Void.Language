#pragma once

#include "void/c_primitive.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/TargetParser/Triple.h>
#include <cstdint>
#include <string>
#include <string_view>

namespace vd {
class ApplicationMetadata;
class ValidatedAnalysis;
namespace ir { struct Function; struct Module; }

namespace llvm_backend {
void check_c_primitive_target(const CPrimitiveAdmission::Owner&, const llvm::Triple&);
std::string native_symbol(const ir::Function&, const std::string& suffix);
std::string global_contract(const ValidatedAnalysis&);
std::string global_contract(const ir::Module&);
std::string application_build_provenance(const ValidatedAnalysis*, const ApplicationMetadata&,
                                         std::string_view target, bool optimize, std::string_view inputs);

llvm::CallInst* call_c_primitive(llvm::IRBuilder<>&, llvm::Module&, const CPrimitiveAdmission&,
    std::uint32_t ordinal, llvm::Value* words, std::uint32_t count, llvm::Value* result,
    llvm::Value* spans = nullptr, unsigned span_count = 0);
}
}
