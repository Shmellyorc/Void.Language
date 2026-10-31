#include "void/native_work.hpp"
#include "void/async.hpp"
#include <array>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace vd {
struct NativeWorkers::State {
    Runtime& runtime;
    std::mutex start_mutex;
    std::shared_ptr<NativeWake> wake = std::make_shared<NativeWake>();
    std::mutex& mutex = wake->mutex;
    std::condition_variable& changed = wake->changed;
    std::vector<std::thread> threads;
    std::array<NativeWorkerItem*, 64> running{};
    NativeWorkerItem* head = nullptr;
    NativeWorkerItem* tail = nullptr;
    unsigned admitted = 0;
    bool started = false, startup_failed = false, stop_requested = false, stopping = false;
    explicit State(Runtime& owner) : runtime(owner) {}

    void loop(unsigned index) noexcept {
        bool announced = false;
        try {
            Runtime::Attachment attached(runtime);
            Runtime::Managed managed(runtime);
            { std::lock_guard lock(mutex); ++admitted; announced = true; }
            changed.notify_all();
            for (;;) {
                NativeWorkerItem* item;
                {
                    Runtime::Native safe(runtime);
                    std::unique_lock lock(mutex);
                    auto available = [&] { for (auto* next = head; next; next = next->next_) if (next->queue_ready()) return true; return false; };
                    changed.wait(lock, [&] { return stopping || available(); });
                    if (!head) return;
                    NativeWorkerItem* previous = nullptr; item = head;
                    while (item && !item->queue_ready()) { previous = item; item = item->next_; }
                    if (!item) std::terminate(); // shutdown requires admitted native jobs to have drained
                    if (previous) previous->next_ = item->next_; else head = item->next_;
                    if (tail == item) tail = previous;
                    item->next_ = nullptr; running[index] = item;
                }
                item->run(runtime);
                bool finished, cancel;
                {
                    std::lock_guard lock(mutex);
                    running[index] = nullptr; finished = item->finished();
                    cancel = !finished && stop_requested && item->runtime_owned();
                    finished |= cancel;
                    if (finished) item->queued_ = false;
                    else {
                        item->next_ = nullptr;
                        if (tail) tail->next_ = item; else head = item;
                        tail = item;
                    }
                }
                if (cancel) { Runtime::Native safe(runtime); item->cancel_queued(runtime); item->drain_queued(runtime); }
                // complete() is the last access to the item. It may publish a
                // release handoff allowing its Scope/resource to destroy it.
                if (finished) item->complete();
                else changed.notify_all();
            }
        } catch (...) {
            if (announced) std::terminate(); // Runtime/queue invariants, never a lost live item.
            // Item run/complete are noexcept; only Runtime admission can fail
            // during normal execution. Never leave startup waiters asleep.
            std::lock_guard lock(mutex);
            startup_failed = true;
            if (!announced) ++admitted;
            changed.notify_all();
        }
    }
};
NativeWorkers::NativeWorkers(Runtime& runtime) : state_(std::make_unique<State>(runtime)) {}
NativeWorkers::~NativeWorkers() { shutdown(); }
NativeCompletion NativeWorkers::completion() const { return NativeCompletion::pending(state_->wake); }
void NativeWorkers::start() {
    auto& state = *state_;
    std::lock_guard start(state.start_mutex);
    {
        std::lock_guard lock(state.mutex);
        if (state.stopping || state.startup_failed) throw std::runtime_error("Native workers are unavailable");
        if (state.started) return;
        if (state.stop_requested) throw std::runtime_error("Native workers are stopping");
        state.started = true;
    }
    const auto count = state.runtime.async_worker_limit();
    state.threads.reserve(count);
    for (unsigned index = 0; index < count; ++index) state.threads.emplace_back([&state, index] { state.loop(index); });
    Runtime::Native safe(state.runtime);
    std::unique_lock lock(state.mutex);
    state.changed.wait(lock, [&] { return state.admitted == count; });
    if (state.startup_failed || state.stop_requested) throw std::runtime_error("Native worker Runtime admission failed");
}
std::shared_ptr<NativeWorkers> NativeWorkers::get(Runtime& runtime) {
    runtime.begin_native_job(); // Also protects pool construction from Runtime close.
    struct Admission { Runtime& runtime; ~Admission() { runtime.end_native_job(); } } admission{runtime};
    std::shared_ptr<NativeWorkers> result;
    {
        std::lock_guard lock(runtime.mutex_);
        if (runtime.closing_) throw std::runtime_error("Native worker admission rejected during shutdown");
        result = runtime.native_workers_;
    }
    if (!result) {
        auto prepared = std::shared_ptr<NativeWorkers>(new NativeWorkers(runtime));
        std::lock_guard lock(runtime.mutex_);
        if (runtime.closing_) throw std::runtime_error("Native worker admission rejected during shutdown");
        if (!runtime.native_workers_) runtime.native_workers_ = prepared;
        result = runtime.native_workers_;
    }
    try { result->start(); }
    catch (...) {
        result->shutdown();
        std::lock_guard lock(runtime.mutex_);
        if (runtime.native_workers_ == result) runtime.native_workers_.reset();
        throw;
    }
    return result;
}
void NativeWorkers::submit(NativeWorkerItem& item) {
    auto& state = *state_;
    {
        std::lock_guard lock(state.mutex);
        if (state.stop_requested || state.startup_failed) throw std::runtime_error("Native workers reject new work");
        if (item.queued_) throw std::logic_error("Duplicate native work admission");
        item.queued_ = true; item.next_ = nullptr;
        if (state.tail) state.tail->next_ = &item; else state.head = &item;
        state.tail = &item;
    }
    state.changed.notify_one();
}
void NativeWorkers::submit_cleanup(NativeWorkerItem& item) noexcept {
    auto& state = *state_;
    {
        std::lock_guard lock(state.mutex);
        // Existing admitted cleanup remains queueable after stop notification.
        // Runtime waits for its owning Scope/jobs before final pool shutdown.
        if (state.stopping || item.queued_ || state.startup_failed) std::terminate();
        item.queued_ = true; item.next_ = nullptr;
        if (state.tail) state.tail->next_ = &item; else state.head = &item;
        state.tail = &item;
    }
    state.changed.notify_one();
}
bool NativeWorkers::cancel_queued(NativeWorkerItem& item, bool drain) noexcept {
    auto& state = *state_;
    if (state.runtime.is_managed()) { Runtime::Native safe(state.runtime); return cancel_queued(item, drain); }
    {
        std::lock_guard lock(state.mutex); NativeWorkerItem* previous = nullptr; auto* next = state.head;
        while (next && next != &item) { previous = next; next = next->next_; }
        if (!next) return false; // an entered syscall/step must drain on its worker
        if (previous) previous->next_ = item.next_; else state.head = item.next_;
        if (state.tail == &item) state.tail = previous;
        item.next_ = nullptr; item.queued_ = false; item.request_stop();
    }
    item.cancel_queued(state.runtime);
    if (drain) { item.drain_queued(state.runtime); item.complete(); }
    return true;
}
void NativeWorkers::submit_scoped(NativeWorkerItem& item) {
    auto& state = *state_;
    {
        std::lock_guard lock(state.mutex);
        // Runtime close preserves already admitted source Scope semantics.
        // The Scope still owns/drains this Work and its generation lease.
        if (state.stopping || state.startup_failed) throw std::runtime_error("Native workers are unavailable");
        if (item.queued_) throw std::logic_error("Duplicate scoped work admission");
        item.queued_ = true; item.next_ = nullptr;
        if (state.tail) state.tail->next_ = &item; else state.head = &item;
        state.tail = &item;
    }
    state.changed.notify_one();
}
void NativeWorkers::request_stop() noexcept {
    auto& state = *state_;
    if (state.runtime.is_managed()) { Runtime::Native safe(state.runtime); request_stop(); return; }
    NativeWorkerItem* retired = nullptr; NativeWorkerItem* retired_tail = nullptr;
    {
        std::lock_guard lock(state.mutex);
        state.stop_requested = true;
        NativeWorkerItem* previous = nullptr;
        for (auto* item = state.head; item;) {
            auto* next = item->next_;
            if (item->runtime_owned()) {
                if (previous) previous->next_ = next; else state.head = next;
                if (state.tail == item) state.tail = previous;
                item->queued_ = false; item->next_ = nullptr;
                if (retired_tail) retired_tail->next_ = item; else retired = item;
                retired_tail = item; item->request_stop();
            } else previous = item;
            item = next;
        }
        for (auto* item : state.running) if (item && item->runtime_owned()) item->request_stop();
    }
    // A source Work can occupy the sole worker until Runtime.close returns.
    // Unentered native items must therefore retire on this NativeSafe caller,
    // rather than waiting behind that already admitted source Work.
    for (auto* item = retired; item; item = item->next_) item->cancel_queued(state.runtime);
    while (retired) {
        auto* item = retired; retired = item->next_; item->next_ = nullptr;
        item->drain_queued(state.runtime); item->complete();
    }
    state.changed.notify_all();
}
void NativeWorkers::shutdown() noexcept {
    auto& state = *state_;
    if (state.runtime.is_managed()) { Runtime::Native safe(state.runtime); shutdown(); return; }
    request_stop();
    std::lock_guard start(state.start_mutex);
    { std::lock_guard lock(state.mutex); state.stopping = true; }
    state.changed.notify_all();
    for (auto& thread : state.threads) if (thread.joinable()) thread.join();
}
void NativeWorkers::request_stop_for(Runtime& runtime) noexcept {
    std::shared_ptr<NativeWorkers> pool;
    { std::lock_guard lock(runtime.mutex_); pool = runtime.native_workers_; }
    if (pool) pool->request_stop();
}
void NativeWorkers::shutdown_for(Runtime& runtime) noexcept {
    std::shared_ptr<NativeWorkers> pool;
    {
        std::lock_guard lock(runtime.mutex_);
        if(runtime.async_owner_!=std::thread::id{} || runtime.native_jobs_)return;
        pool = runtime.native_workers_;
    }
    if (pool) pool->shutdown();
}
}
