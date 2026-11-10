#pragma once

#include "void/runtime.hpp"

#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <system_error>

namespace vd {

// One Runtime ledger for the statically admitted native providers and files.
// This is an embedding boundary, not source-visible FFI or a GC finalizer.
class NativeResource {
public:
    NativeResource(Runtime& owner, void* identity)
        : runtime(owner), wrapper(owner, identity) {}
    virtual ~NativeResource() = default;
    virtual const void* kind() const noexcept = 0;
    // An immutable or atomically invalidated exclusive native identity. Legacy
    // providers retain their own ownership rules with the default empty key.
    virtual std::uint64_t exclusive_native_identity() const noexcept { return 0; }
    virtual std::error_code close() = 0;
    // Drain dependent native operations/borrows before any ledger resource is
    // closed. Default resources have no dependencies. No source callbacks.
    virtual std::error_code quiesce() { return {}; }
    virtual void require_shutdown_thread() const {}
    // Called before Runtime waits for native jobs. Signal cancellation/wakeup
    // only: no waits, managed allocation, source callbacks or ledger reentry.
    virtual void request_stop() noexcept {}
    void own_thread() const;

protected:
    Runtime& runtime;
    Runtime::Handle wrapper;
    std::thread::id owner = std::this_thread::get_id();
};

class NativeLibrary {
public:
    explicit NativeLibrary(Runtime& owner) : runtime_(owner) {}
    static std::shared_ptr<NativeLibrary> get(Runtime&, bool cleanup = false);
    static std::shared_ptr<NativeLibrary> existing(Runtime&);
    void publish(void*, std::shared_ptr<NativeResource>);
    // False means the existing owner retains the pointer and the candidate
    // must discard its claim without calling the native release operation.
    bool publish_exclusive(void*, const std::shared_ptr<NativeResource>&);
    std::shared_ptr<NativeResource> find(void*, bool cleanup = false);
    void* identity(const void* kind);
    void erase(void*, const NativeResource* expected = nullptr);
    std::error_code shutdown();
    void request_stop() noexcept;
    void require_shutdown_thread();

private:
    void check_shutdown_thread_locked() const;
    Runtime& runtime_;
    std::mutex mutex_;
    std::mutex shutdown_mutex_;
    std::map<void*, std::shared_ptr<NativeResource>> resources_;
};

void native_request_stop(Runtime&);

} // namespace vd
