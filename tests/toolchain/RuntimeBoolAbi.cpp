#include "../../src/backend/llvm/runtime_bool_abi.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <stdexcept>

int main(int argc, char** argv) {
    if (argc != 2) throw std::runtime_error("expected target triple");
    llvm::LLVMContext context;
    llvm::Module module("runtime-bool-abi", context);
    module.setTargetTriple(llvm::Triple(argv[1]));
    llvm::IRBuilder<> builder(context);
    auto* signature = llvm::FunctionType::get(builder.getInt1Ty(),
        {builder.getInt1Ty(), builder.getInt1Ty(), builder.getInt8Ty()}, false);
    auto function = [&](const char* name) {
        return llvm::Function::Create(signature, llvm::GlobalValue::ExternalLinkage, name, module);
    };
    auto* runtime = function("vd_abi_bool_probe");
    auto* unrelated = function("foreign_bool_probe");
    auto* caller = function("vd_source_body");
    builder.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", caller));
    auto* negated = builder.CreateNot(caller->getArg(0));
    auto* call = builder.CreateCall(runtime, {negated, caller->getArg(1), caller->getArg(2)});
    auto* foreign = builder.CreateCall(unrelated, {call, negated, caller->getArg(2)});
    builder.CreateRet(foreign);
    vd::detail::complete_runtime_bool_abi(module);
    vd::detail::complete_runtime_bool_abi(module); // Reapplying must be harmless.
    const bool expected = module.getTargetTriple().str() != "wasm32-unknown-unknown";
    auto require = [](bool ok) { if (!ok) throw std::runtime_error("Runtime bool ABI attribute mismatch"); };
    require(runtime->hasRetAttribute(llvm::Attribute::ZExt) == expected);
    require(call->hasRetAttr(llvm::Attribute::ZExt) == expected);
    for (unsigned index : {0u, 1u}) {
        require(runtime->hasParamAttribute(index, llvm::Attribute::ZExt) == expected);
        require(call->paramHasAttr(index, llvm::Attribute::ZExt) == expected);
    }
    require(!runtime->hasParamAttribute(2, llvm::Attribute::ZExt));
    require(!runtime->hasParamAttribute(2, llvm::Attribute::SExt));
    require(!call->paramHasAttr(2, llvm::Attribute::ZExt));
    require(caller->getAttributes().isEmpty() && unrelated->getAttributes().isEmpty());
    require(foreign->getAttributes().isEmpty());
    if (llvm::verifyModule(module, &llvm::errs())) return 1;
    module.print(llvm::outs(), nullptr);
}
