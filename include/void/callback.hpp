#pragma once
#include "void/runtime.hpp"
#include <utility>
namespace vd {
// A binding MUST implement a barrier covering every dispatched trampoline,
// including calls that have not yet reached admission. In-flight counts alone
// cannot establish safe destruction of a native callback context.
class CallbackProvider {
public:
    virtual ~CallbackProvider() = default;
    virtual void unregister_and_quiesce() noexcept = 0;
};
class CallbackRegistration {
public:
    CallbackRegistration(Runtime&, CallbackProvider&);
    ~CallbackRegistration();
    CallbackRegistration(const CallbackRegistration&) = delete;
    CallbackRegistration& operator=(const CallbackRegistration&) = delete;
    // The native binding translates false into its OWN ABI's failure result.
    // Neither C++ exceptions nor generated checked failures cross the trampoline.
    template<class F> bool run(F&& body) noexcept {
        if (Runtime::current_runtime_ && Runtime::current_runtime_ != &runtime_) return false;
        {
            std::lock_guard lock(mutex_);
            std::lock_guard runtime_lock(runtime_.mutex_);
            if (closing_ || runtime_.closing_ || (Runtime::current_runtime_ == &runtime_ && runtime_.thread().state != Runtime::State::native_safe)) return false;
            ++in_flight_;
        }
        Active active{this, current_};
        current_ = &active;
        bool success = false;
        try {
            Runtime::Attachment attachment(runtime_, true);
            Runtime::Managed managed(runtime_);
            std::forward<F>(body)();
            success = true;
        } catch (...) { /* Binding-specific failure is returned after cleanup. */ }
        current_ = active.previous;
        {
            std::lock_guard lock(mutex_);
            --in_flight_;
            changed_.notify_all();
        }
        return success;
    }
    void close(); // NativeSafe/unattached; rejects self-drain without closing.
private:
    struct Active { CallbackRegistration* registration; Active* previous; };
    static thread_local Active* current_;
    Runtime& runtime_;
    CallbackProvider& provider_;
    std::mutex mutex_;
    std::condition_variable changed_;
    std::size_t in_flight_ = 0;
    bool closing_ = false, closed_ = false;
};
}
