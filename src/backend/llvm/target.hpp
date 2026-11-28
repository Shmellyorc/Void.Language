#pragma once
#include <filesystem>
#include <memory>
#include <llvm/TargetParser/Triple.h>
#include <string>
#include <utility>

namespace llvm { class Module; class TargetMachine; }

namespace vd::detail {
void initialize_native_target();
std::unique_ptr<llvm::TargetMachine> native_aot_machine(bool split_sections = false);
void write_object(llvm::Module&, llvm::TargetMachine&, const std::filesystem::path&);

// LLVM's SDK/host triple describes the machine that built LLVM. Generated
// objects must instead use the same deployment minimum as our native linker.
inline llvm::Triple deployment_triple(llvm::Triple triple, const std::string& minimum) {
    if (triple.isMacOSX() && !minimum.empty()) triple.setOSName("macosx" + minimum);
    return triple;
}
inline llvm::Triple native_deployment_triple(llvm::Triple triple) {
#ifdef VOID_MACOS_DEPLOYMENT_TARGET
    return deployment_triple(std::move(triple), VOID_MACOS_DEPLOYMENT_TARGET);
#else
    return triple;
#endif
}
}
