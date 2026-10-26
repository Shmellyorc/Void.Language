#pragma once
#include "void/execution.hpp"
#include "void/runtime.hpp"
#include <atomic>
#include <cstddef>
#include <memory>
// Private same-build compiler/runtime ABI; all transitions run on the owning executor.
extern "C" {
using vd_async_start_entry = void* (*)(std::int32_t,vd::RuntimeFailure*);
using vd_async_resume_entry = void (*)(void*,vd::RuntimeFailure*);
std::int32_t vd_async_root(vd_async_start_entry,std::int32_t,std::size_t,const void*,vd::RuntimeFailure*) noexcept;
void vd_async_new(void**,void**,std::size_t,const std::size_t*,std::size_t,std::size_t,const std::size_t*,std::size_t,const char*,const void*,vd_async_resume_entry,vd::RuntimeFailure*,std::int32_t worker=0) noexcept;
void vd_async_start(void*,vd::RuntimeFailure*) noexcept;
bool vd_async_ready(void*,vd::RuntimeFailure*) noexcept;
void vd_async_suspend(void*,void*,vd::RuntimeFailure*) noexcept;
void vd_async_complete(void*) noexcept;
void vd_async_cancel(void*,vd::RuntimeFailure*) noexcept;
void vd_async_cleanup_enter() noexcept;
void vd_async_cleanup_leave() noexcept;
void vd_async_finish(vd::RuntimeFailure*) noexcept;
void vd_async_cancel_children() noexcept;
void vd_async_checkpoint(vd::RuntimeFailure*) noexcept;
bool vd_worker_checkpoint(vd::RuntimeFailure*) noexcept;
bool vd_worker_cleanup_failure(vd::RuntimeFailure*,const vd::RuntimeFailure*) noexcept;
std::uint64_t vd_async_group_enter(vd::RuntimeFailure*) noexcept;
bool vd_async_group_join(std::uint64_t,std::int32_t,void*,vd::RuntimeFailure*) noexcept;
bool vd_async_drain(void*,vd::RuntimeFailure*) noexcept;
}

namespace vd {
const void* async_execution_context() noexcept;
bool async_context_descends_from(const void*) noexcept;

// Private native jobs share the source-worker pool. execute/abandon run in
// NativeSafe state and must use only owned native memory. publish runs on the
// owning executor with the task rooted; it may report allocation failure.
class NativeJob {
public:
    virtual ~NativeJob() = default;
    virtual void execute(const std::atomic<bool>&, RuntimeFailure*) noexcept = 0;
    virtual void publish(Runtime&, void*) = 0;
    virtual void cancel_queued() noexcept {}
    virtual bool acquired() const noexcept { return false; }
    virtual void abandon(RuntimeFailure*) noexcept {}
    virtual void commit() noexcept {}
};

void async_submit_native(void**, const TypeLayout&, const void*,
                         std::shared_ptr<NativeJob>);
}
