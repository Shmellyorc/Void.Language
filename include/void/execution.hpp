#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
namespace vd {
class ValidatedAnalysis;
// Per-invocation storage: generated code only borrows it until normal return.
struct RuntimeFailure {
    const char* message = nullptr;
    std::uint32_t count = 0;
    const char* frames[32] = {};
};
struct Compatibility {
    std::string function_id;
    std::string signature = "c:i32(i32)";
    std::uint64_t layout = 0, captures = 0, constants = 0;
    std::string constant_contract = {};
    std::string source_contract = {};
    std::shared_ptr<const ValidatedAnalysis> analysis = {};
    bool operator==(const Compatibility& other) const {
        return function_id == other.function_id && signature == other.signature &&
            layout == other.layout && captures == other.captures && constants == other.constants &&
            constant_contract == other.constant_contract && source_contract == other.source_contract;
    }
};
class NativeCode {
public:
    using CheckedEntry = std::int32_t (*)(std::int32_t, RuntimeFailure*);
    using Entry = std::int32_t (*)(std::int32_t);
    NativeCode(Entry entry, std::shared_ptr<void> owner, Compatibility compatibility);
    NativeCode(CheckedEntry entry, std::shared_ptr<void> owner, Compatibility compatibility);
    std::int32_t invoke(std::int32_t value) const;
    const Compatibility& compatibility() const { return compatibility_; }
private:
    Entry entry_ = nullptr;
    CheckedEntry checked_ = nullptr;
    std::shared_ptr<void> owner_;
    Compatibility compatibility_;
};
// A lease keeps its generation alive through the entire call, including native reentry.
class ReloadSlot {
public:
    explicit ReloadSlot(std::shared_ptr<const NativeCode> initial);
    std::shared_ptr<const NativeCode> acquire() const;
    bool replace(std::shared_ptr<const NativeCode> replacement);
    std::int32_t invoke(std::int32_t value) const;
private:
    mutable std::mutex mutex_;
    std::shared_ptr<const NativeCode> current_;
};
}
