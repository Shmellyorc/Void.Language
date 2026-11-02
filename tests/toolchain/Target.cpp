// Exercise the backend's configured-policy wrapper on every native test host.
#define VOID_MACOS_DEPLOYMENT_TARGET "14.0"
#include "../../src/backend/llvm/target.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    using vd::detail::deployment_triple;
    for (const auto* original : {"arm64-apple-macosx26.0.0", "aarch64-apple-darwin25.0.0", "x86_64-apple-macosx26.0.0"}) {
        const llvm::Triple host(original);
        const auto target = deployment_triple(host, "14.0");
        if (vd::detail::native_deployment_triple(host) != target) throw std::runtime_error("configured minimum lost");
        if (!target.isMacOSX() || target.getOSVersion().getMajor() != 14 ||
            target.getArch() != host.getArch() || target.getVendor() != host.getVendor() ||
            target.getEnvironment() != host.getEnvironment()) throw std::runtime_error("macOS deployment triple");
        if (deployment_triple(target, "14.0") != target) throw std::runtime_error("deployment not idempotent");
        if (deployment_triple(host, "") != host) throw std::runtime_error("empty minimum changed target");
    }
    for (const auto* original : {"x86_64-unknown-linux-gnu", "x86_64-pc-windows-msvc", "arm64-apple-ios18.0"}) {
        const llvm::Triple host(original);
        if (deployment_triple(host, "14.0") != host) throw std::runtime_error("unrelated platform changed");
    }
    std::cout << deployment_triple(llvm::Triple("arm64-apple-macosx26.0.0"), "14.0").str() << '\n';
}
