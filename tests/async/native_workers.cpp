#include "void/async.hpp"
#include "void/native_work.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {
using namespace vd;
void require(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
struct Gate {
    std::mutex mutex;
    std::condition_variable changed;
    unsigned entered = 0;
    bool released = false;
};
struct Done { std::mutex mutex; std::condition_variable changed; bool value = false; };
class Controlled final : public NativeWorkerItem {
    Runtime& runtime_;
    std::shared_ptr<Gate> gate_;
    std::shared_ptr<Done> done_ = std::make_shared<Done>();
    std::atomic<bool> admitted_{true};
public:
    bool submitted = false;
    std::atomic<bool> stopped{false};
    std::thread::id thread;
    Controlled(Runtime& runtime, std::shared_ptr<Gate> gate) : runtime_(runtime), gate_(std::move(gate)) { runtime_.begin_native_job(); }
    ~Controlled() override {
        if (submitted) { request_stop(); Runtime::Native safe(runtime_); wait(); }
        if (admitted_.exchange(false)) runtime_.end_native_job();
    }
    void run(Runtime& runtime) noexcept override {
        Runtime::Native safe(runtime);
        thread = std::this_thread::get_id();
        std::unique_lock lock(gate_->mutex); ++gate_->entered; gate_->changed.notify_all();
        gate_->changed.wait(lock, [&] { return gate_->released; });
    }
    void request_stop() noexcept override {
        stopped = true;
        std::lock_guard lock(gate_->mutex); gate_->released = true; gate_->changed.notify_all();
    }
    void complete() noexcept override {
        auto done = done_;
        if (admitted_.exchange(false)) runtime_.end_native_job();
        { std::lock_guard lock(done->mutex); done->value = true; }
        done->changed.notify_all(); // local state survives immediate item destruction
    }
    void wait() { std::unique_lock lock(done_->mutex); done_->changed.wait(lock, [&] { return done_->value; }); }
};
char generation;
std::thread::id source_thread;
std::shared_ptr<Gate> source_gate;
class SourceJob final : public NativeJob {
    void execute(const std::atomic<bool>&, RuntimeFailure*) noexcept override {
        source_thread = std::this_thread::get_id();
        if (source_gate) {
            std::unique_lock lock(source_gate->mutex); ++source_gate->entered; source_gate->changed.notify_all();
            source_gate->changed.wait(lock, [&] { return source_gate->released; });
        }
    }
    void publish(Runtime&, void* task) override { const std::int32_t value = 42; std::memcpy(static_cast<char*>(task) + 4, &value, sizeof(value)); }
};
struct Frame { void* self = nullptr; void* pending = nullptr; unsigned stage = 0; };
constexpr std::size_t offsets[] = {offsetof(Frame, self), offsetof(Frame, pending)};
void resume(void* address, RuntimeFailure* failure) {
    auto& runtime = Runtime::current(); auto& frame = *static_cast<Frame*>(address);
    if (!frame.stage) {
        void* pending = nullptr; constexpr std::size_t zero = 0;
        Runtime::Slot slot{&pending, sizeof(pending), &zero, 1}; Runtime::Frame root(runtime, &slot, 1);
        TypeLayout task{8, {}}; task.descriptor = runtime.descriptor("worker-lifetime-task");
        async_submit_native(&pending, task, &generation, std::make_shared<SourceJob>());
        runtime.store(address, offsetof(Frame, pending), pending); frame.stage = 1;
    }
    if (!vd_async_ready(frame.pending, failure)) { vd_async_suspend(address, frame.pending, failure); return; }
    if (!vd_async_drain(address, failure)) return;
    if (!failure->message) {
        const std::int32_t value = 42; std::memcpy(static_cast<char*>(frame.self) + 4, &value, sizeof(value)); vd_async_complete(frame.self);
    }
}
void* start(std::int32_t, RuntimeFailure* failure) {
    auto& runtime = Runtime::current(); Frame temporary;
    Runtime::Slot slot{&temporary, sizeof(temporary), offsets, 2}; Runtime::Frame root(runtime, &slot, 1);
    vd_async_new(&temporary.self, &temporary.pending, sizeof(Frame), offsets, 2, 8, nullptr, 0,
        "worker-lifetime-task", &generation, resume, failure);
    if (failure->message) return nullptr;
    static_cast<Frame*>(temporary.pending)->self = temporary.self; vd_async_start(temporary.self, failure); return temporary.self;
}
void queued_shutdown() {
    Runtime runtime; runtime.async_worker_limit(1);
    const NominalDescriptor descriptor{"worker-lifetime-task", "", "two-i32", {8, {}}}; runtime.admit_types({&descriptor, 1});
    Runtime::Attachment attached(runtime); Runtime::Managed managed(runtime);
    auto pool = NativeWorkers::get(runtime); source_gate = std::make_shared<Gate>();
    auto root = std::async(std::launch::async, [&] {
        Runtime::Attachment owner(runtime); Runtime::Managed managed(runtime); RuntimeFailure failure;
        auto answer = vd_async_root(start, 0, 4, &generation, &failure);
        if (failure.message) throw std::runtime_error(failure.message);
        return answer;
    });
    {
        Runtime::Native safe(runtime); std::unique_lock lock(source_gate->mutex);
        bool entered = source_gate->changed.wait_for(lock, std::chrono::seconds(5), [&] { return source_gate->entered == 1; });
        if (!entered) { source_gate->released = true; source_gate->changed.notify_all(); }
        require(entered, "source Work did not occupy the sole worker");
    }
    auto queued_gate = std::make_shared<Gate>(); Controlled queued(runtime, queued_gate);
    pool->submit(queued); queued.submitted = true;
    {
        Runtime::Native safe(runtime);
        auto closer = std::async(std::launch::async, [&] { return runtime.close(); });
        const bool closed = closer.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
        // Existing source Work is allowed to wait until close returns. Always
        // release the harness gate so a failed implementation reports cleanly.
        { std::lock_guard lock(source_gate->mutex); source_gate->released = true; source_gate->changed.notify_all(); }
        require(!closer.get() && closed, "queued native jobs waited behind Scope-owned source Work during Runtime.close");
        require(root.get() == 42, "Runtime.close cancelled existing source Work");
    }
    require(queued.stopped && queued_gate->entered == 0, "cancelled unentered native item executed its body");
    source_gate.reset();
}

}
int main() {
    try {
        queued_shutdown();
        Runtime runtime; runtime.async_worker_limit(1);
        const NominalDescriptor descriptor{"worker-lifetime-task", "", "two-i32", {8, {}}}; runtime.admit_types({&descriptor, 1});
        Runtime::Attachment attached(runtime); Runtime::Managed managed(runtime);
        auto pool = NativeWorkers::get(runtime);
        auto immediate = std::make_shared<Gate>(); immediate->released = true;
        Controlled standalone(runtime, immediate); pool->submit(standalone); standalone.submitted = true;
        { Runtime::Native safe(runtime); standalone.wait(); }
        require(standalone.thread != std::this_thread::get_id(), "native-only work ran on owner");
        for (unsigned root = 0; root < 3; ++root) {
            RuntimeFailure failure; require(vd_async_root(start, 0, 4, &generation, &failure) == 42 && !failure.message, "repeated worker root failed");
            require(source_thread == standalone.thread && NativeWorkers::get(runtime) == pool, "source/native work did not reuse the same pool");
            require(runtime.collect() == Runtime::Collection::completed && runtime.snapshot().objects == 0, "persistent workers retained root frames");
        }
        runtime.async_worker_limit(2);
        auto replacement = NativeWorkers::get(runtime); require(replacement != pool, "idle worker limit change retained old width");
        auto blocked = std::make_shared<Gate>(); Controlled first(runtime, blocked), second(runtime, blocked);
        replacement->submit(first); first.submitted = true; replacement->submit(second); second.submitted = true;
        {
            Runtime::Native safe(runtime);
            std::unique_lock lock(blocked->mutex);
            const bool concurrent = blocked->changed.wait_for(lock, std::chrono::seconds(5), [&] { return blocked->entered == 2; });
            if (!concurrent) { blocked->released = true; blocked->changed.notify_all(); }
            require(concurrent, "new worker width did not execute two native items");
        }
        bool rejected = false;
        try { runtime.async_worker_limit(1); } catch (const std::logic_error&) { rejected = true; }
        require(rejected, "worker limit changed during admitted native work");
        {
            Runtime::Native safe(runtime);
            auto closer = std::async(std::launch::async, [&] { return runtime.close(); });
            const bool ready = closer.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
            if (!ready) { first.request_stop(); second.request_stop(); }
            require(!closer.get() && ready, "Runtime waited before cancelling native worker items");
        }
        require(first.stopped && second.stopped, "Runtime did not cancel both native worker items");
        std::cout << "Runtime workers: native-only admission, three roots, same pool/thread, idle reconfiguration, busy rejection and shutdown passed\n";
        return 0;
    } catch (const std::exception& failure) { std::cerr << failure.what() << '\n'; return 1; }
}
