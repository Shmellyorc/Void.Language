#pragma once
#include "void/backend.hpp"
#ifdef VOID_LLVM_RESEARCH
#include "void/llvm_storage_audit.hpp"
#endif
#include <stdexcept>
namespace storage_fixture {
inline void emit(const vd::ir::Module& module, const std::filesystem::path& output,
                 bool optimize, bool proof, const std::string& suffix = {}) {
    if (proof) {
#ifdef VOID_LLVM_RESEARCH
        vd::llvm_backend::emit_aot_storage_proof(module, output, optimize,
            output.string()+".certificate.json", vd::llvm_backend::ProofAdapter::runtime, suffix);
#else
        throw std::invalid_argument("Storage proof requires -DVOID_LLVM_RESEARCH=ON");
#endif
    } else vd::emit_runtime_object(module, output, optimize, suffix);
}
}
