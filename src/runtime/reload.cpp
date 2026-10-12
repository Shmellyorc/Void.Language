#include "void/execution.hpp"
#include "void/runtime.hpp"
#include <stdexcept>
namespace vd {
NativeCode::NativeCode(Entry entry, std::shared_ptr<void> owner, Compatibility compatibility)
    : entry_(entry), owner_(std::move(owner)), compatibility_(std::move(compatibility)) {
    if (!entry_ || !owner_) throw std::invalid_argument("native code requires an entry and lifetime owner");
}
NativeCode::NativeCode(CheckedEntry entry, std::shared_ptr<void> owner, Compatibility compatibility, std::shared_ptr<Runtime> runtime)
    : checked_(entry), owner_(std::move(owner)), compatibility_(std::move(compatibility)), runtime_(std::move(runtime)) {
    if (!checked_ || !owner_) throw std::invalid_argument("native code requires an entry and lifetime owner");
}
std::int32_t NativeCode::invoke(std::int32_t value) const {
    // Host dispatch/diagnostic copying/lease retirement must run NativeSafe,
    // including reentrant dispatch from a Managed callback. Generated direct
    // calls stay Managed and use their ordinary argument/return root handoff.
    if (auto* attached = Runtime::attached(); attached && attached->is_managed()) {
        Runtime::Native native(*attached);
        return invoke(value);
    }
    if (!checked_) return entry_(value);
    RuntimeFailure failure;
    std::int32_t result;
    if (runtime_) {
        Runtime::Attachment attachment(*runtime_);
        Runtime::Managed managed(*runtime_);
        result = checked_(value, &failure);
    } else result = checked_(value, &failure);
    if (failure.message) {
        // Copy borrowed generation strings before leaving the owning code lease.
        std::string message = failure.message;
        for (std::uint32_t i = 0; i < failure.count && i < 32; ++i) message += failure.frames[i];
        if (failure.count > 32) message += "\nSource call trace limited to 32 callers.\n";
        throw std::runtime_error(message); // Outside generated code; no native unwind.
    }
    return result;
}
ReloadSlot::ReloadSlot(std::shared_ptr<const NativeCode> initial) : current_(std::move(initial)) {
    if (!current_) throw std::invalid_argument("reload slot requires initial code");
}
std::shared_ptr<const NativeCode> ReloadSlot::acquire() const {
    std::lock_guard lock(mutex_);
    return current_;
}
bool ReloadSlot::replace(std::shared_ptr<const NativeCode> replacement) {
    if (!replacement) return false;
    std::shared_ptr<const NativeCode> previous;
    {
        std::lock_guard lock(mutex_);
        if (!(current_->compatibility() == replacement->compatibility())) return false;
        previous = std::move(current_);
        current_ = std::move(replacement);
    }
    return true; // Destruction and ORC retirement happen outside the publication lock.
}
std::int32_t ReloadSlot::invoke(std::int32_t value) const { return acquire()->invoke(value); }
}
