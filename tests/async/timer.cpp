#include "../../src/runtime/timer.hpp"
#include "void/native_work.hpp"

#include <cstring>
#include <future>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
using namespace std::chrono_literals;
using vd::AsyncTimers;
using vd::NativeCompletion;
using Result = AsyncTimers::Result;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void queue_controls(vd::Runtime& runtime) {
    AsyncTimers queue(runtime);
    auto wake = std::make_shared<vd::NativeWake>();
    auto ticket = [&] { return NativeCompletion::pending(wake); };
    const AsyncTimers::Clock::time_point now{};
    auto rejected = ticket();
    for (auto [seconds, nanos] : {
        std::pair<std::int64_t, std::int64_t>{-1, 999999999}, {0, -1}, {0, 1000000000},
        {AsyncTimers::max_seconds, 1}, {std::numeric_limits<std::int64_t>::max(), 999999999}
    }) {
        require(queue.start(rejected, seconds, nanos, now) == Result::invalid_duration &&
                !rejected.ready() && !queue.next_deadline(), "invalid duration entered queue");
    }
    require(queue.start(rejected, 1, 0, AsyncTimers::Clock::time_point::max()) == Result::clock_range &&
            !queue.next_deadline(), "deadline addition overflow");
    auto pre_cancelled = ticket();
    require(pre_cancelled.complete_cancelled(), "test cancellation initialization failed");
    queue.start(pre_cancelled, 1, 0, now);
    require(!queue.next_deadline() && pre_cancelled.cancellation_requested(),
            "pre-cancelled timer entered queue");

    auto first = ticket(), equal = ticket(), later = ticket(), cancelled = ticket();
    queue.start(later, 3, 0, now);
    queue.start(first, 1, 0, now);
    queue.start(equal, 1, 0, now);
    queue.start(cancelled, 1, 0, now);
    queue.expire(now + 999ms);
    require(!first.ready() && !equal.ready() && !later.ready(), "timer fired before deadline");
    queue.cancel(cancelled);
    queue.expire(now + 1s);
    require(first.ready() && equal.ready() && !later.ready() &&
            cancelled.ready() && cancelled.cancellation_requested() && !cancelled.complete(),
            "equal deadlines or near-expiration cancellation failed");
    require(queue.next_deadline() == now + 3s, "earliest deadline was not retired");
    queue.expire(now + 3s);
    queue.cancel(later);
    require(later.ready() && !later.complete_cancelled() && !later.cancellation_requested() &&
            !queue.next_deadline(), "completed timer changed after retirement");

    std::vector<NativeCompletion> pending;
    for (std::size_t n = 0; n < AsyncTimers::capacity; ++n) {
        pending.push_back(ticket());
        require(queue.start(pending.back(), AsyncTimers::max_seconds, 0, now) == Result::accepted,
                "inclusive maximum or queue capacity rejected");
    }
    require(queue.start(rejected, 1, 0, now) == Result::capacity && !rejected.ready(),
            "over-capacity timer was admitted");
    auto zero = ticket();
    require(queue.start(zero, 0, 0, now) == Result::accepted && zero.ready(),
            "zero duration waited or consumed capacity");
    for (auto& entry : pending) queue.cancel(entry);
    for (int n = 0; n < 1024; ++n) {
        auto recycled = ticket();
        require(queue.start(recycled, 0, 1, now) == Result::accepted, "timer capacity not recycled");
        queue.expire(now + 1ms);
        require(recycled.ready() && !recycled.complete(), "recycled timer not retired exactly once");
    }
    auto stopping = ticket(), stopped = ticket();
    queue.start(stopping, 30, 0, now);
    queue.request_stop();
    queue.start(stopped, 0, 0, now);
    require(!queue.next_deadline() && stopping.ready() && stopping.cancellation_requested() &&
            stopped.ready() && stopped.cancellation_requested(), "stop did not drain and close admission");
}

enum class Case { worker, cancel_after_admission, expire_before_publication, after_publication, repeated, shutdown };
char generation;
std::promise<void> shutdown_admitted;
std::thread::id executor;

struct Worker final : vd::NativeJob {
    bool ran = false;
    void execute(const std::atomic<bool>&, vd::RuntimeFailure*) noexcept override {
        ran = std::this_thread::get_id() != executor;
    }
    void publish(vd::Runtime&, void*) override {
        require(ran && std::this_thread::get_id() == executor, "native worker did not hand off to owner");
    }
};

struct Activation {
    void* self = nullptr;
    void* timer = nullptr;
    void* worker = nullptr;
    Case test = Case::worker;
    bool started = false;
    bool draining = false;
};
constexpr std::size_t offsets[] = {offsetof(Activation, self), offsetof(Activation, timer),
                                   offsetof(Activation, worker)};

void resume(void* address, vd::RuntimeFailure* failure) {
    auto& runtime = vd::Runtime::current();
    auto& frame = *static_cast<Activation*>(address);
    if (frame.test == Case::repeated) {
        for (int i = 0; i < 1024; ++i) {
            {
                void* timer = nullptr;
                const std::size_t offset = 0;
                const vd::Runtime::Slot slot{&timer, sizeof(timer), &offset, 1};
                vd::Runtime::Frame roots(runtime, &slot, 1);
                vd_async_sleep(&timer, 0, 0, "timer-result", &generation, failure);
                if (failure->message) return;
                require(vd_async_ready(timer, failure) && !failure->message, "zero duration suspended");
            }
            if (i % 32 == 31) {
                runtime.collect();
                vd_async_checkpoint(failure);
                if (failure->message) return;
                const auto snapshot = vd::async_snapshot();
                require(snapshot.work == 0 && snapshot.tasks == 1 && runtime.snapshot().objects == 2,
                        "collected timers retained native or managed state");
            }
        }
        if (vd_async_drain(address, failure) && !failure->message) vd_async_complete(frame.self);
        return;
    }
    if (!frame.started) {
        Activation temp;
        const vd::Runtime::Slot slot{&temp, sizeof(temp), offsets, 3};
        vd::Runtime::Frame roots(runtime, &slot, 1);
        const bool immediate = frame.test == Case::after_publication;
        vd_async_sleep(&temp.timer, immediate ? 0 : AsyncTimers::max_seconds, 0,
                       "timer-result", &generation, failure);
        if (failure->message) return;
        runtime.store(address, offsetof(Activation, timer), temp.timer);
        frame.started = true;
        if (frame.test == Case::shutdown) shutdown_admitted.set_value();
        if (frame.test == Case::cancel_after_admission || frame.test == Case::expire_before_publication) {
            if (frame.test == Case::expire_before_publication) {
                auto queue = AsyncTimers::get(runtime);
                const auto deadline = queue->next_deadline();
                require(bool(deadline), "expected a queued timer");
                queue->expire(*deadline);
            }
            vd_async_cancel(frame.timer, failure);
        }
        if (frame.test == Case::worker) {
            vd::TypeLayout layout{8, {}};
            layout.descriptor = runtime.descriptor("timer-result");
            vd::async_submit_native(&temp.worker, layout, &generation, std::make_shared<Worker>());
            runtime.store(address, offsetof(Activation, worker), temp.worker);
        }
    }
    if (!frame.draining && frame.test == Case::worker) {
        if (!vd_async_ready(frame.worker, failure)) {
            vd_async_suspend(address, frame.worker, failure);
            return;
        }
        if (failure->message) return;
        require(!vd_async_ready(frame.timer, failure) && vd::async_snapshot().work == 1,
                "ordinary worker did not finish while the long timer remained pending");
        vd_async_cancel(frame.timer, failure);
        if (failure->message) return;
    } else if (!frame.draining) {
        if (!vd_async_ready(frame.timer, failure)) {
            vd_async_suspend(address, frame.timer, failure);
            return;
        }
        if (failure->message) return;
        require(frame.test == Case::after_publication, "cancelled timer returned ordinary success");
        vd_async_cancel(frame.timer, failure);
        require(vd_async_ready(frame.timer, failure) && !failure->message &&
                vd::async_snapshot().work == 0, "published result changed after later cancellation");
    }
    frame.draining = true;
    if (!vd_async_drain(address, failure)) return;
    if (failure->message) return;
    const std::int32_t success = 42;
    std::memcpy(static_cast<char*>(frame.self) + 4, &success, sizeof(success));
    vd_async_complete(frame.self);
}

void* start(std::int32_t test, vd::RuntimeFailure* failure) {
    auto& runtime = vd::Runtime::current();
    Activation temp;
    const vd::Runtime::Slot slot{&temp, sizeof(temp), offsets, 3};
    vd::Runtime::Frame roots(runtime, &slot, 1);
    vd_async_new(&temp.self, &temp.timer, sizeof(Activation), offsets, 3, 8, nullptr, 0,
                 "timer-result", &generation, resume, failure);
    if (failure->message) return nullptr;
    auto& frame = *static_cast<Activation*>(temp.timer);
    frame.self = temp.self;
    frame.test = static_cast<Case>(test);
    vd_async_start(temp.self, failure);
    return temp.self;
}

void owner_controls(vd::Runtime& runtime) {
    for (auto test : {Case::worker, Case::cancel_after_admission, Case::expire_before_publication,
                      Case::after_publication, Case::repeated}) {
        vd::RuntimeFailure failure;
        const auto result = vd_async_root(start, static_cast<int>(test), 4, &generation, &failure);
        if (test == Case::worker || test == Case::after_publication || test == Case::repeated) {
            if (failure.message) throw std::runtime_error(failure.message);
            require(result == (test == Case::repeated ? 0 : 42), "async timer result not published");
        } else {
            require(failure.message && std::string_view(failure.message) == "Async task cancelled",
                    "timer cancellation did not propagate");
        }
        runtime.collect();
        require(runtime.snapshot().objects == 0 && !AsyncTimers::get(runtime)->next_deadline(),
                "timer task, frame or native ticket retained after root retirement");
    }
}
}

int main() {
    try {
        vd::Runtime runtime;
        runtime.async_worker_limit(1);
        const vd::NominalDescriptor descriptor{"timer-result", "", "plain", {8, {}}};
        runtime.admit_types({&descriptor, 1});
        vd::Runtime::Attachment attached(runtime);
        vd::Runtime::Managed managed(runtime);
        queue_controls(runtime);
        executor = std::this_thread::get_id();
        owner_controls(runtime);

        auto queue = AsyncTimers::get(runtime);
        auto admitted = shutdown_admitted.get_future();
        auto closer = std::async(std::launch::async, [&] {
            if (admitted.wait_for(5s) != std::future_status::ready)
                throw std::runtime_error("shutdown test never admitted a timer");
            return runtime.close();
        });
        // Bound the test even if the Runtime-to-provider notification breaks.
        auto watchdog = std::async(std::launch::async, [&] {
            const bool closed = closer.wait_for(5s) == std::future_status::ready;
            if (!closed) queue->request_stop();
            return closed;
        });
        vd::RuntimeFailure failure;
        vd_async_root(start, static_cast<int>(Case::shutdown), 4, &generation, &failure);
        require(watchdog.get() && !closer.get() && failure.message &&
                std::string_view(failure.message) == "Async task cancelled",
                "Runtime shutdown failed to wake and cancel a pending timer");
        runtime.collect();
        require(runtime.snapshot().objects == 0 && !queue->next_deadline(),
                "shutdown retained timer resources");
        std::cout << "Timer queue, cancellation, one worker, retirement and shutdown passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
