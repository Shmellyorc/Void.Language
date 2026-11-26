#pragma once

#include "void/ir.hpp"

namespace llvm { class Function; class Module; class StructType; }

namespace vd::llvm_backend {
class TypeLayout;

struct AsyncPlan {
    llvm::StructType* frame;
    llvm::Function* resume;
    std::vector<unsigned> parameters, locals;
    std::vector<std::vector<unsigned>> values;
    std::vector<std::uint64_t> offsets;
};

AsyncPlan plan_async_frame(llvm::Module&, TypeLayout&, const ir::Function&,
                           const std::vector<std::vector<ir::Type>>& checked_types,
                           const std::string& suffix, llvm::Function* root_entry);
}
