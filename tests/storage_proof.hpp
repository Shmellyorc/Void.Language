#pragma once
#include "void/backend.hpp"
#include "void/llvm_storage_audit.hpp"
namespace storage_fixture {
inline void emit(const vd::ir::Module& module, const std::filesystem::path& output,
                 bool optimize, bool proof, const std::string& suffix = {}) {
    if (proof) vd::llvm_backend::emit_aot_storage_proof(module, output, optimize,
        output.string()+".certificate.json", vd::llvm_backend::ProofAdapter::runtime, suffix);
    else vd::emit_runtime_object(module, output, optimize, suffix);
}
}
