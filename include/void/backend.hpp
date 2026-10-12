#pragma once
#include "void/execution.hpp"
#include "void/ir.hpp"
#include <filesystem>
#include <memory>
#include <string>
namespace vd {
class Jit {
public:
    Jit();
    ~Jit();
    // Symbol mutation and compilation are serialized by the development host.
    void bind_address(std::string name, std::uintptr_t address);
    template<class R, class... Args> void bind(std::string name, R (*entry)(Args...)) {
        bind_address(std::move(name), reinterpret_cast<std::uintptr_t>(entry));
    }
    std::shared_ptr<const NativeCode> compile(const ir::Module&, const std::string& entry,
                                             bool optimize = true, Compatibility contract = {}, std::shared_ptr<Runtime> runtime = {});
    std::shared_ptr<const NativeCode> compile(const ValidatedAnalysis&, bool optimize = true, Compatibility contract = {});
    // Preflight complete validated source safety before code generation; publication rechecks.
    bool replace(ReloadSlot&, const ValidatedAnalysis&, bool optimize = true, Compatibility contract = {});
    // A generated optimized loop registers a stack root around native safepoints.
    std::shared_ptr<const NativeCode> compile_root_probe(bool optimize);
    std::size_t live_generations() const;
private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};
void emit_object(const ir::Module&, const std::filesystem::path&, bool optimize, bool source_program = false);
// Internal architectural harness supplies its own runtime entry guard/main.
void emit_runtime_object(const ir::Module&, const std::filesystem::path&, bool optimize);
}
