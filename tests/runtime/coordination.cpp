#include "void/runtime.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <latch>
#include <stdexcept>
using namespace std::chrono_literals;
namespace {
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
template<class F> void rejected(F&& f) {
    bool failed = false;
    try { f(); } catch (const std::logic_error&) { failed = true; }
    check(failed, "invalid runtime operation accepted");
}
constexpr std::size_t pointer_offset = 0;
void states_and_roots() {
    vd::Runtime r, other;
    rejected([&] { r.poll(); });
    {
        vd::Runtime::Attachment attachment(r);
        vd::Runtime::Attachment nested(r);
        rejected([&] { vd::Runtime::Attachment cross(other); });
        rejected([&] { r.poll(); });
        vd::Runtime::Managed managed(r);
        struct Nested { int number; struct { void* reference; } child; } value{};
        const std::size_t offset = offsetof(Nested, child) + offsetof(decltype(Nested::child), reference);
        vd::Runtime::Slot slot{&value, sizeof(value), &offset, 1};
        vd::Runtime::Frame frame(r, &slot, 1);
        r.allocate_into(&value.child.reference, {sizeof(void*), {0}});
        void* child = nullptr;
        vd::Runtime::Slot child_slot{&child, sizeof(child), &pointer_offset, 1};
        {
            vd::Runtime::Frame temporary(r, &child_slot, 1);
            r.allocate_into(&child, {8, {}});
            r.store(value.child.reference, 0, child);
        }
        check(r.collect() == vd::Runtime::Collection::completed && r.snapshot().objects == 2, "nested root lost");
        vd::Runtime::Handle handle(r, value.child.reference);
        value.child.reference = nullptr;
        check(r.collect() == vd::Runtime::Collection::completed && r.snapshot().objects == 2, "persistent graph lost");
        {
            vd::Runtime::Native native(r);
            rejected([&] { r.poll(); });
            rejected([&] { handle.get(); });
            // Reentrant callback preserves the outer, frozen frame chain.
            vd::Runtime::Attachment callback(r);
            vd::Runtime::Managed callback_entry(r);
            check(handle.get() != nullptr, "callback handle lost");
            check(r.collect() == vd::Runtime::Collection::completed, "callback collection failed");
        }
        handle.reset(nullptr);
        check(r.collect() == vd::Runtime::Collection::completed && r.snapshot().objects == 0, "dead graph retained");
    }
    check(r.snapshot().threads == 0, "attachment leaked");
    r.close();
    rejected([&] { vd::Runtime::Attachment a(r); });
    vd::Runtime closing;
    vd::Runtime::Attachment idle(closing);
    closing.close();
    rejected([&] { vd::Runtime::Managed entry(closing); });
}
void timeout_and_recovery() {
    vd::Runtime r;
    std::latch active(1), release(1);
    auto uncooperative = std::async(std::launch::async, [&] {
        vd::Runtime::Attachment a(r);
        vd::Runtime::Managed m(r);
        active.count_down();
        release.wait(); // Deliberately simulate a missing safepoint.
        r.poll();
    });
    active.wait();
    {
        vd::Runtime::Attachment a(r);
        vd::Runtime::Managed m(r);
        void* dead = nullptr;
        {
            vd::Runtime::Slot slot{&dead,sizeof(dead),&pointer_offset,1};
            vd::Runtime::Frame frame(r,&slot,1);
            r.allocate_into(&dead, {8, {}});
        }
        check(r.collect(20ms) == vd::Runtime::Collection::timed_out, "rendezvous did not time out");
        check(r.snapshot().objects == 1 && !r.snapshot().requested, "timeout swept or left request active");
        release.count_down();
        { vd::Runtime::Native n(r); uncooperative.get(); }
        check(r.collect() == vd::Runtime::Collection::completed && r.snapshot().objects == 0, "post-timeout collection failed");
    }
}
void competing_collectors() {
    vd::Runtime r;
    std::latch ready(1), release(1);
    auto worker = std::async(std::launch::async, [&] {
        vd::Runtime::Attachment a(r);
        vd::Runtime::Managed m(r);
        ready.count_down();
        release.wait();
        // This is the losing requester: it must park, not wait as Managed.
        return r.collect(2s);
    });
    ready.wait();
    auto collector = std::async(std::launch::async, [&] {
        vd::Runtime::Attachment a(r);
        vd::Runtime::Managed m(r);
        return r.collect(2s);
    });
    check(r.wait_for_request(2s), "request not observable");
    check(r.snapshot().parked == 1, "leader did not freeze itself");
    release.count_down();
    check(collector.get() == vd::Runtime::Collection::completed, "leader failed");
    check(worker.get() == vd::Runtime::Collection::completed, "loser failed");
    check(r.snapshot().completed_epoch == 1 && r.snapshot().threads == 0, "epoch completion inconsistent");
}
void timeout_releases_losing_collector() {
    vd::Runtime runtime;
    std::latch ready(2), release_blocker(1), release_loser(1);
    auto blocker = std::async(std::launch::async,[&] {
        vd::Runtime::Attachment a(runtime); vd::Runtime::Managed m(runtime);
        ready.count_down(); release_blocker.wait(); runtime.poll();
    });
    auto loser = std::async(std::launch::async,[&] {
        vd::Runtime::Attachment a(runtime); vd::Runtime::Managed m(runtime);
        ready.count_down(); release_loser.wait(); return runtime.collect();
    });
    ready.wait();
    auto leader = std::async(std::launch::async,[&] {
        vd::Runtime::Attachment a(runtime); vd::Runtime::Managed m(runtime);
        return runtime.collect(2s);
    });
    bool requested = runtime.wait_for_request(2s);
    release_loser.count_down();
    bool both_parked = false;
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto state = runtime.snapshot();
        if (state.parked == 2) { both_parked = true; break; }
        if (!state.requested) break;
        std::this_thread::yield();
    }
    auto first = leader.get();
    release_blocker.count_down();
    auto second = loser.get(); blocker.get();
    check(requested && both_parked && first == vd::Runtime::Collection::timed_out && second == first,
        "timed-out epoch did not release its losing collector with the same outcome");
    check(!runtime.snapshot().requested && runtime.snapshot().threads == 0,"failed rendezvous left frozen participants");
}
void native_and_stress() {
    vd::Runtime r;
    std::latch native_ready(1), release(1);
    auto blocked = std::async(std::launch::async, [&] {
        vd::Runtime::Attachment a(r);
        vd::Runtime::Managed m(r);
        void* value = nullptr;
        vd::Runtime::Slot slot{&value, sizeof(value), &pointer_offset, 1};
        vd::Runtime::Frame frame(r, &slot, 1);
        r.allocate_into(&value, {8, {}});
        {
            vd::Runtime::Native n(r);
            native_ready.count_down();
            release.wait();
        }
        check(r.contains(value), "NativeSafe frame lost");
    });
    native_ready.wait();
    {
        vd::Runtime::Attachment a(r);
        vd::Runtime::Managed m(r);
        double total_us = 0, maximum_us = 0;
        for (int i = 0; i < 1000; ++i) {
            auto start = std::chrono::steady_clock::now();
            check(r.collect(2s) == vd::Runtime::Collection::completed, "native-safe collection failed");
            auto elapsed = std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
            total_us += elapsed; maximum_us = std::max(maximum_us,elapsed);
        }
        std::cout << "NativeSafe rendezvous baseline (1000, one frozen object): mean " << total_us/1000 << " us, max " << maximum_us << " us\n";
        check(r.snapshot().objects == 1, "frozen native root lost");
        release.count_down();
        { vd::Runtime::Native n(r); blocked.get(); }
        check(r.collect() == vd::Runtime::Collection::completed && r.snapshot().objects == 0, "native root leaked");
    }
    std::atomic<bool> done = false;
    std::latch started(3);
    std::vector<std::future<void>> workers;
    for (int n = 0; n < 3; ++n) workers.push_back(std::async(std::launch::async, [&] {
        vd::Runtime::Attachment a(r);
        vd::Runtime::Managed m(r);
        void* value = nullptr;
        vd::Runtime::Slot slot{&value, sizeof(value), &pointer_offset, 1};
        vd::Runtime::Frame frame(r, &slot, 1);
        started.count_down();
        while (!done.load()) {
            r.allocate_into(&value, {8, {}});
            r.poll();
        }
    }));
    started.wait();
    {
        vd::Runtime::Attachment a(r);
        vd::Runtime::Managed m(r);
        for (int i = 0; i < 1000; ++i) check(r.collect(2s) == vd::Runtime::Collection::completed, "stress rendezvous failed");
        done.store(true);
        { vd::Runtime::Native n(r); for (auto& w : workers) w.get(); }
        check(r.collect() == vd::Runtime::Collection::completed && r.snapshot().objects == 0, "stress roots leaked");
    }
}
void repeated_competing_epochs() {
    vd::Runtime runtime;
    std::latch ready(3), start(1);
    std::vector<std::future<void>> workers;
    for (int i = 0; i < 3; ++i) workers.push_back(std::async(std::launch::async,[&] {
        vd::Runtime::Attachment a(runtime); vd::Runtime::Managed m(runtime);
        ready.count_down(); start.wait();
        for (int epoch = 0; epoch < 1000; ++epoch)
            check(runtime.collect(2s) == vd::Runtime::Collection::completed,"competing epoch failed");
    }));
    ready.wait(); start.count_down();
    for (auto& worker : workers) worker.get();
    auto result = runtime.snapshot();
    check(!result.requested && result.epoch == result.completed_epoch && result.threads == 0 && result.cooperative_parks > 0,"epoch/state invariants after competing stress");
}
}
int main() {
    try {
        states_and_roots(); timeout_and_recovery(); competing_collectors(); timeout_releases_losing_collector(); native_and_stress(); repeated_competing_epochs();
        std::cout << "runtime coordinator: states, nested roots, timeout, competing collectors, NativeSafe and stress passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
