#pragma once
#include "void/runtime.hpp"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <memory>

namespace vd {
class NativeCompletion;
struct NativeWake {
    std::mutex mutex;
    std::condition_variable changed;
};
// Private queue used by the existing source workers and native file producers.
// A submitting Scope/resource owns each item until complete() publishes its
// final handoff. No source scheduler or transport callback is added here.
class NativeWorkerItem {
    friend class NativeWorkers;
    NativeWorkerItem* next_ = nullptr;
    bool queued_ = false;
public:
    virtual ~NativeWorkerItem() = default;
    virtual void run(Runtime&) noexcept = 0;
    virtual void request_stop() noexcept = 0;
    virtual void complete() noexcept = 0;
    virtual bool runtime_owned() const noexcept { return true; }
    // Native-only IO continuations can yield while a provider operation is
    // pending. Ordinary source Work remains ready and completes in one run.
    virtual bool queue_ready() const noexcept { return true; }
    virtual bool finished() const noexcept { return true; }
    // Called only after an unentered native item is removed from the queue.
    // First make every producer quiescent without blocking; only then drain
    // dependent native cleanup. No filesystem call or source callback here.
    virtual void cancel_queued(Runtime&) noexcept {}
    virtual void drain_queued(Runtime&) noexcept {}
};
class NativeWorkers final {
    struct State;
    std::unique_ptr<State> state_;
    explicit NativeWorkers(Runtime&);
    void start();
public:
    ~NativeWorkers();
    static std::shared_ptr<NativeWorkers> get(Runtime&);
    static void request_stop_for(Runtime&) noexcept;
    static void shutdown_for(Runtime&) noexcept;
    void submit(NativeWorkerItem&);
    void submit_scoped(NativeWorkerItem&);
    void submit_cleanup(NativeWorkerItem&) noexcept;
    bool cancel_queued(NativeWorkerItem&, bool drain = true) noexcept;
    NativeCompletion completion() const;
    void request_stop() noexcept;
    void shutdown() noexcept;
};
}
