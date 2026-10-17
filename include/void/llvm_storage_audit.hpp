#pragma once
#include "void/native_storage.hpp"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
namespace llvm { class Module; class Function; class AllocaInst; class DataLayout; }
namespace vd::llvm_backend {
// Lowering declares every home by identity and purpose. Extents are read from
// the actual LLVM allocation and selected target, never supplied as estimates.
enum class HomeKind { value, argument, result, copy, root, frame, slots, staging, native_guard, cli_parse, cli_failure };
struct LoweredHome {
    llvm::AllocaInst* allocation;
    StorageRole role;
    HomeKind kind;
    std::string identity;
};
struct FunctionInventory {
    llvm::Function* function;
    bool array_carrying;
    std::vector<LoweredHome> homes;
    std::set<StorageRole> accounted;
};
class LoweringInventory {
    std::vector<FunctionInventory> functions_;
public:
    void function(llvm::Function&, bool array_carrying);
    void home(llvm::AllocaInst&, StorageRole, HomeKind, std::string identity);
    void complete(llvm::Function&, std::set<StorageRole> accounted);
    const std::vector<FunctionInventory>& functions() const { return functions_; }
};
// Target-derived private signature proposal, not an execution certificate.
// Home addresses carry a single precisely rooted handle, never byval payloads.
struct FixedCallOperand {
    enum class Mode { scalar, snapshot_home, result_home, failure };
    Mode mode;
    ir::Type logical;
    StorageExtent storage;
};
struct FixedCallLayout {
    std::string profile, logical_identity;
    StorageExtent handle, slot, failure;
    std::vector<std::uint64_t> slot_offsets, failure_offsets;
    std::vector<FixedCallOperand> operands;
    bool result_is_home;
};
// Owns the exact TargetMachine and options used for both freeze and emission.
// No ORC/LSP integration or source native-array permission is implied.
class AotObjectEmitter {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    AotObjectEmitter(std::string triple, bool optimize);
    ~AotObjectEmitter();
    const llvm::DataLayout& data_layout() const;
    const std::string& triple() const;
    std::string identity() const;
    FixedCallLayout fixed_call_layout(const ir::Function&) const;
    // Freezes the original inventory before the existing production optimizer.
    // This object is single-use. Replacing its module/profile is forbidden.
    void freeze(llvm::Module&, const LoweringInventory&);
    void optimize();
    // All certificate and final-byte checks run before touching the destination.
    void deliver(const std::filesystem::path&);
    const std::string& report() const;
    // Read-only Linux x64 experiment. Never grants delivery or ownership credit.
    std::string research_encoded_cfg(const std::filesystem::path&, const std::string& function,
                                    const std::string& machine_certificate) const;
};
std::set<StorageRole> all_storage_roles();
// Explicit investigation entry, never called by public emit_object/runtime paths.
// Reuses their shared module/adapter producer and the staged certificate service.
enum class ProofAdapter { source, architecture, runtime };
void emit_aot_storage_proof(const ir::Module&, const std::filesystem::path& object,
                            bool optimize, const std::filesystem::path& report,
                            ProofAdapter adapter = ProofAdapter::source,
                            const std::string& generation_suffix = {});
}
