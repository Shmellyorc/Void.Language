#include "native_abi.hpp"
#include "native_cache.hpp"
#include "void/c_primitive.hpp"
#include "void/frontend.hpp"
#include "void/native_runtime.hpp"
#include <llvm/Config/llvm-config.h>
#include <llvm/IR/Module.h>

namespace vd::llvm_backend {
void check_c_primitive_target(const CPrimitiveAdmission::Owner& admission, const llvm::Triple& target) {
    if (!admission) return;
    const bool windows = admission->target() == "x86_64-pc-windows-msvc";
    const bool matching = windows ? target.isOSWindows() && target.getEnvironment() == llvm::Triple::MSVC :
        target.isOSLinux() && target.getEnvironment() == llvm::Triple::GNU;
    if (target.getArch() != llvm::Triple::x86_64 || !matching)
        throw std::invalid_argument("C primitive descriptor does not match the native LLVM target");
}

std::string native_symbol(const ir::Function& function, const std::string& suffix) {
    if (function.module_identity.empty()) return function.name + suffix;
    // Injective ASCII encoding, independent of host paths, aliases and addresses.
    // Keep arbitrary VFS punctuation out of platform assembler/linker grammars.
    const char* hex = "0123456789abcdef";
    std::string symbol = "__vd_";
    for (unsigned char c : function.name) { symbol += hex[c >> 4]; symbol += hex[c & 15]; }
    return symbol + suffix;
}

std::string global_contract(const ValidatedAnalysis& analysis) {
    auto field = [](std::string_view text) { return std::to_string(text.size()) + ":" + std::string(text); };
    return "void.globals.v1" + field(analysis.entry_identity()) + field(analysis.compatibility_contract());
}
std::string global_contract(const ir::Module& input) {
    if (input.globals.empty()) return {};
    if (input.analysis) return global_contract(*input.analysis);
    std::string out = "void.globals.v1";
    auto field = [&](std::string_view text) { out += std::to_string(text.size()) + ":" + std::string(text); };
    field(input.identity);
    for (const auto& value : input.globals) { field(value.identity); field(value.visibility); field(value.syntax); field(ir::type_name(value.cell)); field(ir::capability_key(value.cell)); }
    field(input.analysis ? input.analysis->compatibility_contract() : input.constant_contract);
    return out;
}

std::string application_build_provenance(const ValidatedAnalysis* analysis, const ApplicationMetadata& metadata,
                                                std::string_view target, bool optimize, std::string_view inputs) {
    std::string recipe="VOID-application-build-v1";
    auto field=[&](std::string_view value){llvm_backend::cache_field(recipe,value);};
    field(LLVM_VERSION_STRING);field(target);field(optimize?"O2":"O0");field(inputs);field(metadata.bytes());
    if(analysis) {
        field(analysis->entry_identity());field(analysis->compatibility_contract());
        for(const auto& source:analysis->modules()) {field(source.identity);field(source.source.text());}
    }
    return llvm_backend::digest(recipe);
}

llvm::CallInst* call_c_primitive(llvm::IRBuilder<>& b, llvm::Module& module, const CPrimitiveAdmission& admission,
                                      std::uint32_t ordinal, llvm::Value* words, std::uint32_t count, llvm::Value* result,
                                      llvm::Value* spans, unsigned span_count) {
    if (const auto& plan = admission.dynamic()) {
        auto bridge = module.getOrInsertFunction(c_dynamic_call, b.getInt32Ty(), b.getPtrTy(), b.getInt32Ty(),
            b.getInt32Ty(), b.getPtrTy(), b.getInt32Ty(), b.getPtrTy(), b.getPtrTy(), b.getInt32Ty());
        return b.CreateCall(bridge, {b.CreateGlobalString(plan->bytes()), b.getInt32(plan->bytes().size()),
            b.getInt32(ordinal), words, b.getInt32(count), result,
            spans ? spans : llvm::ConstantPointerNull::get(b.getPtrTy()), b.getInt32(span_count)});
    }
    auto transport = module.getOrInsertFunction(admission.transport(), b.getInt32Ty(), b.getPtrTy(),
        b.getInt32Ty(), b.getInt32Ty(), b.getPtrTy(), b.getInt32Ty(), b.getPtrTy());
    const auto& descriptor = admission.descriptor();
    auto* bytes = b.CreateGlobalString(descriptor);
    if (spans) {
        auto bridge = module.getOrInsertFunction(c_span_bridge, b.getInt32Ty(), b.getPtrTy(),
            b.getPtrTy(), b.getInt32Ty(), b.getInt32Ty(), b.getPtrTy(), b.getInt32Ty(),
            b.getPtrTy(), b.getPtrTy(), b.getInt32Ty());
        return b.CreateCall(bridge, {transport.getCallee(), bytes, b.getInt32(descriptor.size()),
            b.getInt32(ordinal), words, b.getInt32(count), result, spans, b.getInt32(span_count)});
    }
    return b.CreateCall(transport, {bytes, b.getInt32(descriptor.size()),
        b.getInt32(ordinal), words, b.getInt32(count), result});
}
}
