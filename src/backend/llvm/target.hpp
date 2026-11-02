#pragma once
#include <llvm/TargetParser/Triple.h>
#include <string>
#include <utility>

namespace vd::detail {
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
