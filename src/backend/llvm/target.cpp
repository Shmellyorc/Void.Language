#include "target.hpp"
#include "void/compiler_observation.hpp"
#include "void/path.hpp"
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Host.h>
#include <mutex>
#include <stdexcept>

namespace vd::detail {
void initialize_native_target() {
    static std::once_flag once;
    std::call_once(once, [] {
        if (llvm::InitializeNativeTarget() || llvm::InitializeNativeTargetAsmPrinter())
            throw std::runtime_error("native LLVM target unavailable");
        const auto host = llvm::Triple(llvm::sys::getDefaultTargetTriple()).getArch();
        if (host != llvm::Triple::x86_64) {
            LLVMInitializeX86TargetInfo();
            LLVMInitializeX86Target();
            LLVMInitializeX86TargetMC();
            LLVMInitializeX86AsmPrinter();
        }
        if (host != llvm::Triple::aarch64) {
            LLVMInitializeAArch64TargetInfo();
            LLVMInitializeAArch64Target();
            LLVMInitializeAArch64TargetMC();
            LLVMInitializeAArch64AsmPrinter();
        }
    });
}

std::unique_ptr<llvm::TargetMachine> native_aot_machine(bool split_sections) {
    return aot_machine(native_deployment_triple(llvm::Triple(llvm::sys::getDefaultTargetTriple())), split_sections);
}

std::unique_ptr<llvm::TargetMachine> aot_machine(const llvm::Triple& triple, bool split_sections) {
    initialize_native_target();
    std::string error;
    auto* target = llvm::TargetRegistry::lookupTarget(triple, error);
    if (!target) throw std::runtime_error(error);
    llvm::TargetOptions options;
    options.FunctionSections = split_sections;
    options.DataSections = split_sections;
    std::unique_ptr<llvm::TargetMachine> machine(target->createTargetMachine(triple, "generic", "",
        options, llvm::Reloc::PIC_));
    if (!machine) throw std::runtime_error("cannot create LLVM target machine: " + triple.str());
    return machine;
}
void write_object(llvm::Module& module, llvm::TargetMachine& machine, const std::filesystem::path& output) {
    observation::Scope timing(observation::Phase::native_object_emission);
    std::error_code ec;
    llvm::raw_fd_ostream file(path_utf8(output), ec, llvm::sys::fs::OF_None);
    if (ec) throw std::runtime_error("cannot write object: " + ec.message());
    llvm::legacy::PassManager passes;
    if (machine.addPassesToEmitFile(passes, file, nullptr, llvm::CodeGenFileType::ObjectFile))
        throw std::runtime_error("object emission unavailable");
    passes.run(module);
    file.flush();
    if (file.has_error()) throw std::runtime_error("object write failed");
}
}
