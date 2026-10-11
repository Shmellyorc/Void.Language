#include "void/execution.hpp"
#include <stdexcept>
namespace vd {
NativeCode::NativeCode(Entry entry, std::shared_ptr<void> owner, Compatibility compatibility)
    : entry_(entry), owner_(std::move(owner)), compatibility_(std::move(compatibility)) {
    if (!entry_ || !owner_) throw std::invalid_argument("native code requires an entry and lifetime owner");
}
std::int32_t NativeCode::invoke(std::int32_t value) const { return entry_(value); }
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
