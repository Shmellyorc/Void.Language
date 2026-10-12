#include "void/callback.hpp"
#include <stdexcept>
namespace vd {
thread_local CallbackRegistration::Active* CallbackRegistration::current_ = nullptr;
CallbackRegistration::CallbackRegistration(Runtime& runtime, CallbackProvider& provider)
    : runtime_(runtime), provider_(provider) {
    std::lock_guard lock(runtime_.mutex_);
    if (runtime_.closing_) throw std::logic_error("runtime is closing");
    ++runtime_.callbacks_;
}
CallbackRegistration::~CallbackRegistration() { if (!closed_) std::terminate(); }
void CallbackRegistration::close() {
    if (Runtime::current_runtime_ == &runtime_ && runtime_.thread().state == Runtime::State::managed)
        throw std::logic_error("callback drain requires NativeSafe state");
    for (auto* active = current_; active; active = active->previous)
        if (active->registration == this) throw std::logic_error("callback cannot drain itself");
    {
        std::unique_lock lock(mutex_);
        if (closing_) { changed_.wait(lock,[&] { return closed_; }); return; }
        closing_ = true;
    }
    // No runtime or registration lock is held while calling the provider.
    provider_.unregister_and_quiesce();
    std::unique_lock lock(mutex_);
    changed_.wait(lock,[&] { return in_flight_ == 0; });
    {
        std::lock_guard runtime_lock(runtime_.mutex_);
        --runtime_.callbacks_;
    }
    closed_ = true;
    changed_.notify_all();
}
}
