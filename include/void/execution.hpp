#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
namespace vd {
struct Compatibility {
    std::string function_id;
    std::string signature = "c:i32(i32)";
    std::uint64_t layout = 0, captures = 0, constants = 0;
    bool operator==(const Compatibility&) const = default;
};
class NativeCode {
public:
    using Entry = std::int32_t (*)(std::int32_t);
    NativeCode(Entry entry, std::shared_ptr<void> owner, Compatibility compatibility);
    std::int32_t invoke(std::int32_t value) const;
    const Compatibility& compatibility() const { return compatibility_; }
private:
    Entry entry_;
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
