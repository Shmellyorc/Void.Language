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
    virtual std::error_code close() = 0;
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
    std::shared_ptr<NativeResource> find(void*, bool cleanup = false);
    void* identity(const void* kind);
    void erase(void*);
    std::error_code shutdown();

private:
    Runtime& runtime_;
    std::mutex mutex_;
    std::mutex shutdown_mutex_;
    std::map<void*, std::shared_ptr<NativeResource>> resources_;
};

} // namespace vd
