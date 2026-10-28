#pragma once
#include "void/backend.hpp"
namespace vd::llvm_backend {
// Closed native fixture, available only with VOID_LLVM_RESEARCH=ON. Uses the
// shared lowerer, LLVM verifier/optimizer and ordinary object emission. This
// does not enable source arrays or promise a public array/foreign C ABI.
void emit_private_fixed_call_research(const ir::Module&, const std::filesystem::path&, bool optimize);
}
