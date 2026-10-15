// Investigation helper, not a production layout provider or code generator.
#include <llvm/CodeGen/TargetFrameLowering.h>
#include <llvm/CodeGen/TargetSubtargetInfo.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/JSON.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <memory>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2 || std::string(LLVM_VERSION_STRING) != "19.1.7") return 1;
    LLVMInitializeX86TargetInfo(); LLVMInitializeX86Target(); LLVMInitializeX86TargetMC();
    LLVMInitializeAArch64TargetInfo(); LLVMInitializeAArch64Target(); LLVMInitializeAArch64TargetMC();
    std::string error;
    const auto* target = llvm::TargetRegistry::lookupTarget(argv[1], error);
    if (!target) { llvm::errs() << error << '\n'; return 1; }
    std::unique_ptr<llvm::TargetMachine> machine(target->createTargetMachine(
        argv[1], "generic", "", llvm::TargetOptions(), llvm::Reloc::PIC_));
    if (!machine) return 1;
    llvm::LLVMContext context;
    llvm::Module module("target-facts", context);
    module.setTargetTriple(argv[1]);
    module.setDataLayout(machine->createDataLayout());
    auto* function = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(context), false),
        llvm::GlobalValue::ExternalLinkage, "profile", module);
    const auto* frame = machine->getSubtargetImpl(*function)->getFrameLowering();
    const auto& layout = module.getDataLayout();
    auto* scalar = llvm::Type::getInt32Ty(context);
    llvm::json::Object facts{
        {"triple", argv[1]}, {"cpu", "generic"}, {"data_layout", layout.getStringRepresentation()},
        {"i32_bytes", static_cast<int64_t>(layout.getTypeAllocSize(scalar).getFixedValue())},
        {"i32_alignment", static_cast<int64_t>(layout.getABITypeAlign(scalar).value())},
        {"pointer_bytes", static_cast<int64_t>(layout.getPointerSize())},
        {"stack_alignment", static_cast<int64_t>(frame->getStackAlign().value())},
        {"local_area_offset", frame->getOffsetOfLocalArea()},
        {"stack_grows_down", frame->getStackGrowthDirection() == llvm::TargetFrameLowering::StackGrowsDown}
    };
    llvm::outs() << llvm::formatv("{0:2}", llvm::json::Value(std::move(facts))) << '\n';
}
