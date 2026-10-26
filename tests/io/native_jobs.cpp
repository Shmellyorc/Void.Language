#include "void/async.hpp"
#include "void/runtime.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {
char generation;
std::thread::id owner;
int scenario;
std::atomic<bool> entered, release_job, executed, published, abandoned, committed;
std::atomic<bool> native_safe, owner_progress;

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

struct Job final : vd::NativeJob {
    bool owns = false;

    void execute(const std::atomic<bool>&, vd::RuntimeFailure*) noexcept override {
        native_safe = !vd::Runtime::current().is_managed() && std::this_thread::get_id() != owner;
        executed = true;
        owns = true;
        entered = true;
        if (scenario == 0 || scenario == 3) {
            release_job.wait(false);
        }
    }

    void publish(vd::Runtime& runtime, void* task) override {
        require(runtime.is_managed() && std::this_thread::get_id() == owner,
                "native job published off owner");
        if (scenario == 2 || scenario == 5) throw std::bad_alloc();
        std::int32_t result = 42;
        std::memcpy(static_cast<char*>(task) + 4, &result, sizeof(result));
        published = true;
    }

    bool acquired() const noexcept override { return owns; }

    void abandon(vd::RuntimeFailure* failure) noexcept override {
        native_safe = native_safe && !vd::Runtime::current().is_managed() &&
                      std::this_thread::get_id() != owner;
        owns = false;
        abandoned = true;
        if (scenario == 5) failure->message = "injected native close failure";
    }

    void commit() noexcept override {
        owns = false;
        committed = true;
    }
};

struct Activation {
    void* self;
    void* pending;
    void* sibling;
    int stage;
};
constexpr std::size_t offsets[] = {
    offsetof(Activation, self), offsetof(Activation, pending), offsetof(Activation, sibling)
};

void sibling_resume(void* address, vd::RuntimeFailure* failure) {
    auto& frame = *static_cast<Activation*>(address);
    if (scenario == 0 || scenario == 3) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!entered.load()) {
            vd::Runtime::Native safe(vd::Runtime::current());
            if (std::chrono::steady_clock::now() > deadline) {
                failure->message = "native job did not enter";
                release_job = true;
                release_job.notify_all();
                return;
            }
            std::this_thread::yield();
        }
        // The native job is held behind its gate. This ready owner activation
        // must execute before it can complete, proving real suspension.
        owner_progress = std::this_thread::get_id() == owner;
        if (scenario == 3) vd_async_cancel(frame.pending, failure);
        release_job = true;
        release_job.notify_all();
    }
    if (!vd_async_drain(address, failure)) return;
    if (!failure->message) vd_async_complete(frame.self);
}

void root_resume(void* address, vd::RuntimeFailure* failure) {
    auto& frame = *static_cast<Activation*>(address);
    if (frame.stage == 0) {
        vd::TypeLayout layout{8, {}};
        layout.descriptor = vd::Runtime::current().descriptor("io-test-task");
        Activation local{};
        vd::Runtime::Slot temporary{&local, sizeof(local), offsets, 3};
        vd::Runtime::Frame temporary_roots(vd::Runtime::current(), &temporary, 1);
        vd::async_submit_native(&local.pending, layout, &generation, std::make_shared<Job>());
        vd::Runtime::current().store(address, offsetof(Activation, pending), local.pending);
        if (scenario == 1) vd_async_cancel(frame.pending, failure);
        if (scenario == 0 || scenario == 3) {
            vd_async_new(&local.self, &local.sibling, sizeof(Activation), offsets, 3,
                         8, nullptr, 0, "io-test-task", &generation, sibling_resume, failure);
            if (failure->message) return;
            vd::Runtime::current().store(address, offsetof(Activation, sibling), local.self);
            auto& sibling = *static_cast<Activation*>(local.sibling);
            sibling.self = local.self;
            sibling.pending = frame.pending;
            vd_async_start(frame.sibling, failure);
        }
        frame.stage = 1;
    }
    if (frame.stage == 1) {
        if (!vd_async_ready(frame.pending, failure)) {
            vd_async_suspend(address, frame.pending, failure);
            return;
        }
        frame.stage = 2;
        if (!failure->message && scenario == 4) failure->message = "injected opening activation failure";
    }
    if (!vd_async_drain(address, failure)) return;
    if (failure->message) return;
    std::int32_t result = 42;
    std::memcpy(static_cast<char*>(frame.self) + 4, &result, sizeof(result));
    vd_async_complete(frame.self);
}

void* start(std::int32_t, vd::RuntimeFailure* failure) {
    Activation roots{};
    vd::Runtime::Slot slot{&roots, sizeof(roots), offsets, 3};
    vd::Runtime::Frame registered(vd::Runtime::current(), &slot, 1);
    vd_async_new(&roots.self, &roots.pending, sizeof(Activation), offsets, 3,
                 8, nullptr, 0, "io-test-task", &generation, root_resume, failure);
    if (failure->message) return nullptr;
    static_cast<Activation*>(roots.pending)->self = roots.self;
    vd_async_start(roots.self, failure);
    return roots.self;
}
}

int main() {
    try {
        for (scenario = 0; scenario != 6; ++scenario) {
            entered = false;
            release_job = false;
            executed = false;
            published = false;
            abandoned = false;
            committed = false;
            native_safe = false;
            owner_progress = false;
            owner = std::this_thread::get_id();
            vd::Runtime runtime;
            runtime.async_worker_limit(1);
            vd::NominalDescriptor descriptor{"io-test-task", "", "two-i32", {8, {}}};
            runtime.admit_types({&descriptor, 1});
            vd::Runtime::Attachment attachment(runtime);
            vd::Runtime::Managed managed(runtime);
            vd::RuntimeFailure failure;
            auto answer = vd_async_root(start, 0, 4, &generation, &failure);
            if (scenario == 0) {
                require(!failure.message && answer == 42, "native job result");
                require(owner_progress && committed && !abandoned, "owner progress or native handoff");
            } else {
                require(failure.message, "failure/cancellation disappeared");
                require(!committed, "failed opening task committed an acquisition");
                if (scenario != 1) require(abandoned, "native acquisition leaked after failure");
                if (scenario == 5) {
                    require(std::string(failure.message).find("allocation") != std::string::npos,
                            "publication failure lost priority");
                    bool retained_cleanup = false;
                    for (unsigned i = 0; i < std::min(failure.count, 32u); ++i) {
                        retained_cleanup |= std::string(failure.frames[i]).find("close failure") != std::string::npos;
                    }
                    require(retained_cleanup, "native cleanup failure disappeared");
                }
            }
            if (executed) require(native_safe, "native execution/cleanup violated worker boundary");
            require(runtime.collect() == vd::Runtime::Collection::completed, "native task collection");
            require(runtime.snapshot().objects == 0, "native task roots retained");
        }
        std::cout << "native jobs: result, blocked-owner progress, queued/pending cancellation, "
                     "publication failure, acquisition rollback and cleanup diagnostics passed\n";
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
