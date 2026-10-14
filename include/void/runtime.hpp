#pragma once
#include "void/heap.hpp"
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>
#include <map>

namespace vd {
class CallbackRegistration;
// Immutable binding data: no executable addresses, generation leases or permissions.
struct InterfaceWitness {
    std::string key, interface_identity, view_identity, contract;
};
// These are runtime embedding interfaces, not source-language thread features.
class Runtime {
public:
    enum class State { native_safe, managed, parked };
    enum class Collection { completed, timed_out, allocation_failure };
    struct Snapshot { std::uint64_t epoch, completed_epoch; std::size_t threads, parked, objects; bool requested; std::uint64_t cooperative_parks; };
    struct Slot { const void* storage; std::size_t size; const std::size_t* offsets; std::size_t count; };
    class Attachment;
    class Managed;
    class Native;
    class Frame;
    class Handle;
    explicit Runtime(std::size_t allocation_budget = 0) : allocation_budget_(allocation_budget) {}
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    static Runtime& current(); // Internal generated-code TLS boundary.
    static Runtime* attached() noexcept { return current_runtime_; }
    bool is_managed() const { return current_runtime_ == this && thread().state == State::managed; }
    void poll();
    Collection collect(std::chrono::milliseconds timeout = std::chrono::milliseconds::max());
    void allocate_into(void** rooted_slot, const TypeLayout&);
    void construct_into(void** rooted_slot, const TypeLayout&, const void* prepared_payload);
    void string_into(void** rooted_slot, std::string_view);
    void concat_into(void** rooted_slot, void* left, void* right);
    static std::size_t string_payload_size(std::size_t left, std::size_t right);
    std::string_view string_bytes(void*) const; // Managed-only immutable borrow.
    void store(void* object, std::size_t offset, void* value);
    bool contains(void*) const;
    void admit_types(std::span<const NominalDescriptor>);
    void admit_metadata(std::span<const NominalDescriptor>,std::span<const InterfaceWitness>);
    const InterfaceWitness* witness(std::string_view) const;
    std::int64_t interface_binding(void* object,const void* witness,std::span<const char* const> keys) const;
    std::shared_ptr<const NominalDescriptor> descriptor(std::string_view) const;
    std::int64_t dynamic_type(void*,std::span<const char* const>) const;
    Snapshot snapshot() const;
    bool wait_for_request(std::chrono::milliseconds);
    void close(); // Reject new attachments; existing participants must drain.
private:
    friend class CallbackRegistration;
    struct Thread {
        std::thread::id owner = std::this_thread::get_id();
        State state = State::native_safe;
        Frame* frames = nullptr;
        unsigned depth = 0;
        unsigned managed_depth = 0, admissions = 0;
        std::uint64_t epoch = 0;
        std::uint64_t collecting_epoch = 0;
        Collection result = Collection::completed;
    };
    static thread_local Runtime* current_runtime_;
    static thread_local Thread* current_thread_;
    Thread& thread() const;
    void require_managed() const;
    void require_output(void**) const;
    void string_parts_into(void**,std::string_view,std::string_view);
    void validate_descriptor(const TypeLayout&) const; // Caller holds mutex_.
    void park(Thread&, std::unique_lock<std::mutex>&);
    void enter(Thread&);
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    Heap heap_;
    using NominalCatalog=std::map<std::string,std::shared_ptr<const NominalDescriptor>,std::less<>>;
    struct TypeCatalog {
        NominalCatalog types;
        std::map<std::string,std::shared_ptr<const InterfaceWitness>,std::less<>> witnesses;
    };
    std::shared_ptr<const TypeCatalog> types_ = {};
    std::vector<Thread*> threads_;
    std::vector<Handle*> handles_;
    bool closing_ = false;
    bool requested_ = false;
    std::size_t callbacks_ = 0;
    std::size_t allocation_budget_ = 0, allocations_ = 0;
    std::uint64_t epoch_ = 0, completed_epoch_ = 0;
    std::uint64_t cooperative_parks_ = 0;
};

class Runtime::Attachment {
public:
    explicit Attachment(Runtime&);
    ~Attachment();
    Attachment(const Attachment&) = delete;
    Attachment& operator=(const Attachment&) = delete;
private:
    friend class CallbackRegistration;
    Attachment(Runtime&, bool admitted_callback);
    Runtime& runtime_;
    Thread record_;
    bool outer_ = false;
    bool callback_ = false;
};
class Runtime::Managed {
public:
    explicit Managed(Runtime&);
    ~Managed();
    Managed(const Managed&) = delete;
    Managed& operator=(const Managed&) = delete;
private:
    Runtime& runtime_;
    State previous_;
};
class Runtime::Native {
public:
    explicit Native(Runtime&);
    ~Native();
    Native(const Native&) = delete;
    Native& operator=(const Native&) = delete;
private:
    Runtime& runtime_;
};
class Runtime::Frame {
public:
    Frame(Runtime&, const Slot*, std::size_t count);
    ~Frame();
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
private:
    friend class Runtime;
    Runtime& runtime_;
    Frame* previous_;
    const Slot* slots_;
    std::size_t count_;
};
class Runtime::Handle {
public:
    explicit Handle(Runtime&, void* value = nullptr);
    ~Handle();
    void reset(void*);
    // Only Managed code may dereference this whole-object reference.
    void* get() const;
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
private:
    friend class Runtime;
    Runtime& runtime_;
    void* value_;
};
}
