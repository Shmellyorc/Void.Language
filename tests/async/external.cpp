#include "void/async.hpp"
#include "void/native_resource.hpp"
#include "void/runtime.hpp"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <future>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
char generation, provider_kind;
int scenario;
std::thread::id owner;
std::atomic<bool> produced, producer_off_owner;
std::optional<vd::NativeCompletion> retained_ticket;
std::optional<vd::NativeCompletion> cleanup_ticket;
std::atomic<std::size_t> native_result_bytes{0};
std::size_t minimum_payload, maximum_payload, maximum_handles;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// An isolated test provider, not an HTTP engine. Its native thread never attaches
// to the Runtime, invokes source, dereferences managed memory or publishes GC data.
class External final : public vd::ExternalNativeJob {
public:
    vd::Runtime& runtime;
    std::thread thread;
    std::atomic<bool> entered{false}, cancel_requested{false}, completed{false};
    bool owns = false, published = false, abandoned = false, committed = false;
    bool duplicate_rejected = false;
    const int mode;
    explicit External(vd::Runtime& r, int value) : runtime(r), mode(value) {
        runtime.begin_native_job();
    }
    ~External() override {
        if (thread.joinable()) {
            if (runtime.is_managed()) {
                vd::Runtime::Native safe(runtime);
                thread.join();
            } else thread.join();
        }
        runtime.end_native_job();
    }
    void start(vd::NativeCompletion ticket) noexcept override {
        retained_ticket = ticket;
        try {
            thread = std::thread([this, ticket] {
                entered = true;
                while (!produced.load(std::memory_order_acquire) &&
                       !cancel_requested.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
                owns = mode == 2 || mode == 7 || mode == 9;
                vd::RuntimeFailure failure;
                if (mode == 8) failure.message = "external native failure";
                const bool first = ticket.complete(failure);
                duplicate_rejected = !ticket.complete(failure) && first;
                completed.store(true, std::memory_order_release);
            });
        } catch (...) {
            vd::RuntimeFailure failure;
            failure.message = "external test thread allocation failed";
            ticket.complete(failure);
        }
    }
    void cancel() noexcept override {
        cancel_requested.store(true, std::memory_order_release);
    }
    void publish(vd::Runtime& r, void* result) override {
        require(r.is_managed() && std::this_thread::get_id() == owner,
                "external result published off executor");
        if (mode == 2 || mode == 7 || mode == 9) throw std::bad_alloc();
        std::int32_t answer = 42;
        std::memcpy(static_cast<char*>(result) + 4, &answer, sizeof(answer));
        published = true;
    }
    bool acquired() const noexcept override { return owns; }
    void abandon_external(vd::NativeCompletion cleanup) noexcept override {
        owns = false;
        abandoned = true;
        if (mode == 9) {
            cleanup_ticket = cleanup;
            return;
        }
        vd::RuntimeFailure failure;
        if (mode == 7) failure.message = "external cleanup failure";
        cleanup.complete(failure);
    }
    void commit() noexcept override {
        owns = false;
        committed = true;
    }
};

std::shared_ptr<External> external;
struct ResetExternal {
    ~ResetExternal() {
        external.reset();
        retained_ticket.reset();
        cleanup_ticket.reset();
    }
};

struct Producer final : vd::NativeJob {
    void execute(const std::atomic<bool>&, vd::RuntimeFailure*) noexcept override {
        producer_off_owner = std::this_thread::get_id() != owner;
        produced.store(true, std::memory_order_release);
    }
    void publish(vd::Runtime&, void* address) override {
        std::int32_t answer = 1;
        std::memcpy(static_cast<char*>(address) + 4, &answer, sizeof(answer));
    }
};

struct Activation {
    void* self = nullptr;
    void* pending = nullptr;
    void* producer = nullptr;
    std::uint64_t group = 0;
    int stage = 0;
};
constexpr std::size_t offsets[] = {offsetof(Activation, self), offsetof(Activation, pending),
                                   offsetof(Activation, producer)};

void cleanup_cancel_resume(void* address, vd::RuntimeFailure* failure) {
    auto& runtime = vd::Runtime::current();
    auto& frame = *static_cast<Activation*>(address);
    {
        vd::Runtime::Native safe(runtime);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!external->completed.load(std::memory_order_acquire)) {
            require(std::chrono::steady_clock::now() < deadline, "cleanup test completion stalled");
            std::this_thread::yield();
        }
    }
    vd_async_checkpoint(failure); // Receive publication failure; cleanup starts.
    require(bool(cleanup_ticket), "external cleanup was not entered");
    vd_async_cancel(frame.pending, failure);
    const bool shielded = !cleanup_ticket->cancellation_requested() && !external->cancel_requested;
    cleanup_ticket->complete(); // Release the controlled test barrier even on failure.
    if (!shielded) failure->message = "external abandonment cleanup was cancelled";
    if (!vd_async_drain(address, failure)) return;
    if (!failure->message) vd_async_complete(frame.self);
}

void root_resume(void* address, vd::RuntimeFailure* failure) {
    auto& runtime = vd::Runtime::current();
    auto& frame = *static_cast<Activation*>(address);
    if (!frame.stage) {
        vd::TypeLayout layout{8, {}};
        layout.descriptor = runtime.descriptor("external-test-task");
        Activation temporary;
        vd::Runtime::Slot slot{&temporary, sizeof(temporary), offsets, 3};
        vd::Runtime::Frame roots(runtime, &slot, 1);
        if (scenario == 5) frame.group = vd_async_group_enter(failure);
        external = std::make_shared<External>(runtime, scenario);
        vd::async_submit_external(&temporary.pending, layout, &generation, external,
                                  scenario == 4 || scenario == 5);
        runtime.store(address, offsetof(Activation, pending), temporary.pending);
        if (scenario == 0) {
            // The external operation is already waiting for its producer. With
            // one source worker, this ordinary native job must still run.
            vd::async_submit_native(&temporary.producer, layout, &generation,
                                    std::make_shared<Producer>());
            runtime.store(address, offsetof(Activation, producer), temporary.producer);
        } else if (scenario == 9) {
            vd_async_new(&temporary.producer, &temporary.self, sizeof(Activation), offsets, 3,
                         8, nullptr, 0, "external-test-task", &generation, cleanup_cancel_resume, failure);
            if (failure->message) return;
            auto& sibling = *static_cast<Activation*>(temporary.self);
            sibling.self = temporary.producer;
            sibling.pending = frame.pending;
            runtime.store(address, offsetof(Activation, producer), temporary.producer);
            vd_async_start(frame.producer, failure);
        } else if (scenario == 1) {
            vd_async_cancel(frame.pending, failure);
        } else if (scenario == 3) {
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            vd::Runtime::Native safe(runtime);
            while (!external->completed.load(std::memory_order_acquire)) {
                require(std::chrono::steady_clock::now() < deadline, "external completion stalled");
                std::this_thread::yield();
            }
            // Provider completion preceded cancellation, owner publication did
            // not. Preserve the existing native Work cancellation policy.
        }
        if (scenario == 3) vd_async_cancel(frame.pending, failure);
        frame.stage = 1;
    }
    if (scenario == 5 && frame.stage == 1) {
        if (!vd_async_group_join(frame.group, 0, address, failure)) return;
        frame.stage = 2;
    } else if (scenario != 4 && frame.stage == 1) {
        if (!vd_async_ready(frame.pending, failure)) {
            vd_async_suspend(address, frame.pending, failure);
            return;
        }
        frame.stage = 2;
        if (!failure->message && scenario == 6) {
            require(vd_async_ready(frame.pending, failure), "repeated external await suspended");
            std::int32_t result = 0;
            std::memcpy(&result, static_cast<char*>(frame.pending) + 4, sizeof(result));
            require(result == 42, "repeated external result changed");
        }
    }
    if (!vd_async_drain(address, failure)) return;
    if (!failure->message) {
        std::int32_t result = 42;
        std::memcpy(static_cast<char*>(frame.self) + 4, &result, sizeof(result));
        vd_async_complete(frame.self);
    }
}

void* start(std::int32_t, vd::RuntimeFailure* failure) {
    auto& runtime = vd::Runtime::current();
    Activation temporary;
    vd::Runtime::Slot slot{&temporary, sizeof(temporary), offsets, 3};
    vd::Runtime::Frame roots(runtime, &slot, 1);
    vd_async_new(&temporary.self, &temporary.pending, sizeof(Activation), offsets, 3,
                 8, nullptr, 0, "external-test-task", &generation, root_resume, failure);
    if (failure->message) return nullptr;
    static_cast<Activation*>(temporary.pending)->self = temporary.self;
    vd_async_start(temporary.self, failure);
    return temporary.self;
}

class ShutdownDriver final : public vd::NativeResource {
public:
    std::atomic<bool> stopped{false};
    std::thread driver;
    ShutdownDriver(vd::Runtime& r, void* identity) : NativeResource(r, identity) {
        r.begin_native_job();
        try {
            driver = std::thread([this] {
                stopped.wait(false);
                runtime.end_native_job();
            });
        } catch (...) { r.end_native_job(); throw; }
    }
    const void* kind() const noexcept override { return &provider_kind; }
    void request_stop() noexcept override {
        stopped.store(true);
        stopped.notify_all();
    }
    std::error_code close() override {
        if (driver.joinable()) driver.join();
        wrapper.reset(nullptr);
        return {};
    }
    ~ShutdownDriver() override {
        request_stop();
        if (driver.joinable()) driver.join();
    }
};

struct LargeResult final : vd::NativeJob {
    std::string bytes = std::string(1048576, 'x');
    explicit LargeResult() { native_result_bytes += bytes.size(); }
    ~LargeResult() override { native_result_bytes -= bytes.size(); }
    void execute(const std::atomic<bool>&, vd::RuntimeFailure*) noexcept override {}
    void publish(vd::Runtime& runtime, void* task) override {
        void* value = nullptr;
        const std::size_t offset = 0;
        vd::Runtime::Slot slot{&value, sizeof(value), &offset, 1};
        vd::Runtime::Frame roots(runtime, &slot, 1);
        runtime.string_into(&value, bytes);
        runtime.store(task, 8, value);
    }
};

struct PullFrame {
    void* self = nullptr;
    void* pending = nullptr;
    void* held = nullptr;
    unsigned iteration = 0;
    bool waiting = false;
};
constexpr std::size_t pull_offsets[] = {offsetof(PullFrame, self), offsetof(PullFrame, pending),
                                        offsetof(PullFrame, held)};

void check_blob(vd::Runtime& runtime, void* task) {
    void* value = nullptr;
    std::memcpy(&value, static_cast<char*>(task) + 8, sizeof(value));
    auto bytes = runtime.string_bytes(value);
    require(bytes.size() == 1048576 && bytes.front() == 'x' && bytes.back() == 'x',
            "retained large task result changed");
}

void pull_resume(void* address, vd::RuntimeFailure* failure) {
    auto& runtime = vd::Runtime::current();
    auto& frame = *static_cast<PullFrame*>(address);
    while (frame.iteration != 128) {
        if (!frame.waiting) {
            void* temporary = nullptr;
            const std::size_t reference = 0;
            vd::Runtime::Slot slot{&temporary, sizeof(temporary), &reference, 1};
            vd::Runtime::Frame roots(runtime, &slot, 1);
            vd::TypeLayout layout{16, {8}};
            layout.descriptor = runtime.descriptor("external-blob-task");
            vd::async_submit_native(&temporary, layout, &generation, std::make_shared<LargeResult>());
            runtime.store(address, offsetof(PullFrame, pending), temporary);
            frame.waiting = true;
        }
        if (!vd_async_ready(frame.pending, failure)) {
            vd_async_suspend(address, frame.pending, failure);
            return;
        }
        if (failure->message) break;
        check_blob(runtime, frame.pending);
        if (!frame.iteration) runtime.store(address, offsetof(PullFrame, held), frame.pending);
        runtime.store(address, offsetof(PullFrame, pending), nullptr);
        require(runtime.collect() == vd::Runtime::Collection::completed, "pull-result collection failed");
        const auto memory = runtime.memory_snapshot();
        maximum_handles = std::max(maximum_handles, memory.handles);
        if (frame.iteration >= 32) {
            minimum_payload = std::min(minimum_payload, memory.heap.payload_bytes);
            maximum_payload = std::max(maximum_payload, memory.heap.payload_bytes);
        }
        require(native_result_bytes == 0, "completed native Work retained large result bytes");
        require(vd_async_ready(frame.held, failure), "held result lost repeated-await identity");
        check_blob(runtime, frame.held);
        ++frame.iteration;
        frame.waiting = false;
    }
    if (!vd_async_drain(address, failure)) return;
    if (!failure->message) {
        vd_async_cancel(frame.held, failure); // Completed cancellation stays idempotent.
        require(vd_async_ready(frame.held, failure), "completed handle changed after cancel");
        std::int32_t result = 42;
        std::memcpy(static_cast<char*>(frame.self) + 4, &result, sizeof(result));
        vd_async_complete(frame.self);
    }
}

void* pull_start(std::int32_t, vd::RuntimeFailure* failure) {
    auto& runtime = vd::Runtime::current();
    PullFrame temporary;
    vd::Runtime::Slot slot{&temporary, sizeof(temporary), pull_offsets, 3};
    vd::Runtime::Frame roots(runtime, &slot, 1);
    vd_async_new(&temporary.self, &temporary.pending, sizeof(PullFrame), pull_offsets, 3,
                 8, nullptr, 0, "external-test-task", &generation, pull_resume, failure);
    if (failure->message) return nullptr;
    static_cast<PullFrame*>(temporary.pending)->self = temporary.self;
    vd_async_start(temporary.self, failure);
    return temporary.self;
}
}

int main() {
    try {
        for (scenario = 0; scenario != 10; ++scenario) {
            produced = scenario != 0 && scenario != 1 && scenario != 4 && scenario != 5;
            producer_off_owner = false;
            owner = std::this_thread::get_id();
            vd::Runtime runtime;
            runtime.async_worker_limit(1);
            vd::NominalDescriptor descriptor{"external-test-task", "", "two-i32", {8, {}}};
            runtime.admit_types({&descriptor, 1});
            vd::Runtime::Attachment attached(runtime);
            vd::Runtime::Managed managed(runtime);
            ResetExternal cleanup;
            vd::RuntimeFailure failure;
            auto answer = vd_async_root(start, 0, 4, &generation, &failure);
            require(bool(external), "external job not admitted");
            if (external->thread.joinable()) {
                vd::Runtime::Native safe(runtime);
                external->thread.join();
            }
            require(external->duplicate_rejected, "external ticket completed more than once");
            require(retained_ticket && !retained_ticket->complete(), "ticket retained stale executor access");
            if (scenario == 1 || scenario == 3) {
                require(failure.message && std::strstr(failure.message, "cancelled"), "external cancellation became Err/success");
                require(!external->published, "cancelled external operation published");
            } else if (scenario == 2 || scenario == 7 || scenario == 9) {
                require(failure.message && std::strstr(failure.message, "allocation"), "external publication failure lost");
                require(external->abandoned && !external->owns, "external acquisition leaked");
                if (scenario == 9) require(!external->cancel_requested, "abandonment was not shielded");
                if (scenario == 7) {
                    bool cleanup_found = false;
                    for (unsigned i = 0; i < std::min(failure.count, 32u); ++i)
                        cleanup_found |= std::strstr(failure.frames[i], "external cleanup failure") != nullptr;
                    require(cleanup_found, "external cleanup failure discarded");
                }
            } else if (scenario == 8) {
                require(failure.message && std::strstr(failure.message, "external native failure"), "external native diagnostic lost");
            } else {
                require(!failure.message && answer == 42, "external result or lifetime drain failed");
                if (scenario == 0) require(producer_off_owner, "external wait occupied producer worker");
                if (scenario == 4 || scenario == 5)
                    require(external->cancel_requested, "unobserved streaming lifetime escaped its owner");
                else require(external->published, "external owner publication missing");
            }
            external.reset();
            retained_ticket.reset();
            cleanup_ticket.reset();
            require(runtime.collect() == vd::Runtime::Collection::completed && runtime.snapshot().objects == 0,
                    "external job roots leaked");
        }
        {
            vd::Runtime runtime;
            runtime.async_worker_limit(1);
            vd::NominalDescriptor descriptors[] = {
                {"external-test-task", "", "two-i32", {8, {}}},
                {"external-blob-task", "", "rooted-string", {16, {8}}}
            };
            runtime.admit_types(descriptors);
            vd::Runtime::Attachment attached(runtime);
            vd::Runtime::Managed managed(runtime);
            minimum_payload = static_cast<std::size_t>(-1);
            maximum_payload = maximum_handles = 0;
            vd::RuntimeFailure failure;
            require(vd_async_root(pull_start, 0, 4, &generation, &failure) == 42 && !failure.message,
                    "sustained result pull failed");
            require(maximum_payload - minimum_payload <= 65536, "unreachable completed results accumulated");
            require(maximum_handles <= 4, "completed task Handle registrations accumulated");
            require(runtime.collect() == vd::Runtime::Collection::completed && runtime.snapshot().objects == 0,
                    "pull-result root drain leaked");
            std::cout << "128 MiB logical pulls: post-GC payload " << minimum_payload << ".."
                      << maximum_payload << ", persistent handles <= " << maximum_handles
                      << "; unreachable native records retire at quiescent GC checkpoints\n";
        }
        {
            vd::Runtime runtime;
            vd::Runtime::Attachment attached(runtime);
            vd::Runtime::Managed managed(runtime);
            void* identity = nullptr;
            const std::size_t reference = 0;
            vd::Runtime::Slot slot{&identity, sizeof(identity), &reference, 1};
            vd::Runtime::Frame roots(runtime, &slot, 1);
            runtime.allocate_into(&identity, {1, {}});
            auto resource = std::make_shared<ShutdownDriver>(runtime, identity);
            vd::NativeLibrary::get(runtime)->publish(identity, resource);
            // The owner deliberately does no managed publication. Runtime close
            // must first wake the provider, allowing its native-job count to end.
            vd::Runtime::Native safe(runtime);
            auto closer = std::async(std::launch::async, [&] { return runtime.close(); });
            const bool ready = closer.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
            if (!ready) resource->request_stop(); // Permit clean failure, never hang the fixture.
            require(!closer.get(), "Runtime shutdown reported provider failure");
            require(ready && resource->stopped, "Runtime waited before native close notification");
        }
        std::cout << "external native completion: ten lifecycle cases and pre-wait shutdown passed\n";
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
