#pragma once
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/TargetParser/Triple.h>

namespace vd::detail {
// Clang 23.1.4 uses zeroext for C/C++ bool on all three supported native
// ABIs, including Windows x64 (whose i8 extension rules differ from SysV).
// Source functions use VOID's internal ABI; only external Runtime bridges
// need the C ABI annotations. ABI attributes belong on declarations AND calls.
inline void complete_runtime_bool_abi(llvm::Module& module) {
    const auto& target = module.getTargetTriple();
    const bool supported =
        (target.getArch() == llvm::Triple::x86_64 &&
         (target.isOSLinux() || (target.isOSWindows() && target.getEnvironment() == llvm::Triple::MSVC))) ||
        (target.getArch() == llvm::Triple::aarch64 && target.isMacOSX());
    if (!supported) return;
    for (auto& function : module) {
        if (!function.isDeclaration() || !function.getName().starts_with("vd_")) continue;
        if (function.getReturnType()->isIntegerTy(1)) function.addRetAttr(llvm::Attribute::ZExt);
        for (auto& argument : function.args())
            if (argument.getType()->isIntegerTy(1))
                function.addParamAttr(argument.getArgNo(), llvm::Attribute::ZExt);
        for (auto* user : function.users()) {
            auto* call = llvm::dyn_cast<llvm::CallBase>(user);
            if (!call || call->getCalledFunction() != &function) continue;
            if (function.getReturnType()->isIntegerTy(1)) call->addRetAttr(llvm::Attribute::ZExt);
            for (auto& argument : function.args())
                if (argument.getType()->isIntegerTy(1))
                    call->addParamAttr(argument.getArgNo(), llvm::Attribute::ZExt);
        }
    }
}
}
