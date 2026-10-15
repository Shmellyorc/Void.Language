#pragma once
#include <llvm/ADT/StringRef.h>
#include <string>
namespace llvm { class TargetMachine; class Target; }
namespace vd::llvm_backend {
std::string inspect_research_cfg(llvm::StringRef bytes, llvm::StringRef function,
    llvm::StringRef certificate, llvm::StringRef profile,
    llvm::TargetMachine&, const llvm::Target&);
}
