#pragma once
#include "void/ir.hpp"
#include <functional>
#include <set>
namespace vd {
// Shared analysis/tooling/backend contract. Layouts must come from the selected
// target's DataLayout and actual ABI, never sizeof(host) or source leaf counts.
enum class StorageRole { parameter, result, local, instruction, copy, roots, staging, known_abi };
struct StorageExtent { std::uint64_t bytes, alignment; };
struct StorageTarget {
    std::string triple, data_layout;
    std::uint64_t maximum_object_bytes, stack_alignment;
    std::function<StorageExtent(ir::Type)> value_layout;
};
struct StorageHome {
    StorageRole role;
    std::string identity;
    StorageExtent extent;
};
struct NativeStoragePlan {
    std::string target, data_layout;
    std::vector<StorageHome> homes;
    std::uint64_t bytes;
    std::uint64_t unrounded_bytes;
};
inline constexpr std::uint64_t array_function_storage_budget = 65536;
class NativeStoragePlanner {
    StorageTarget target_;
    std::vector<StorageHome> homes_;
public:
    explicit NativeStoragePlanner(StorageTarget target);
    void value(StorageRole role, std::string identity, ir::Type type, std::uint64_t copies = 1);
    // Root records, frames, target call scratch and other actual non-value homes.
    void storage(StorageRole role, std::string identity, StorageExtent extent);
    // A lowering inventory must explicitly account for every category, including
    // those proved empty. Incomplete inventories cannot authorize native arrays.
    NativeStoragePlan finish(const std::set<StorageRole>& accounted, bool array_carrying) const;
};
}
