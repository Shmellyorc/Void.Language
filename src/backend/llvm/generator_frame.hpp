#pragma once

#include "void/ir.hpp"

namespace llvm { class Function; class Module; class StructType; }

namespace vd::llvm_backend {
class TypeLayout;

struct GeneratorPlan {
    enum HandleField { status, frame_pointer, generation, worker_thread, route, state, item };
    static constexpr int running = -1;
    llvm::StructType* frame;
    llvm::Function* resume;
    std::vector<unsigned> parameters, locals;
    std::vector<std::uint64_t> offsets;
    std::vector<std::vector<unsigned>> retire_at_yield;
};

std::vector<std::vector<unsigned>> generator_yield_retirements(TypeLayout&,const ir::Function&);
GeneratorPlan plan_generator_frame(llvm::Module&, TypeLayout&, const ir::Function&);
}
