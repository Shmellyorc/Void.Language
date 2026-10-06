#ifndef VC_THREAD_H
#define VC_THREAD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*VcNativeThreadStartFn)(void *context);
typedef struct VcNativeMonitor VcNativeMonitor;

typedef enum VcNativeWaitResult
{
    VC_NATIVE_WAIT_ERROR = 0,
    VC_NATIVE_WAIT_SIGNALED = 1,
    VC_NATIVE_WAIT_TIMED_OUT = 2
} VcNativeWaitResult;

#define VC_NATIVE_TIMEOUT_INFINITE UINT32_MAX

typedef struct VcNativeThread
{
    void *platform_state;
    VcNativeThreadStartFn start;
    void *context;
    bool joinable;
} VcNativeThread;

static inline bool vc_native_thread_runtime_init(void)
{
    return true;
}

static inline void vc_native_thread_runtime_shutdown(void)
{
}

static inline void vc_native_thread_init(VcNativeThread *thread)
{
    if (thread == NULL)
        return;
    thread->platform_state = NULL;
    thread->start = NULL;
    thread->context = NULL;
    thread->joinable = false;
}

bool vc_native_thread_create(
    VcNativeThread *thread,
    VcNativeThreadStartFn start,
    void *context);

bool vc_native_thread_join(VcNativeThread *thread);

void *vc_native_thread_context_current(void);
bool vc_native_thread_context_attach(void *context);
bool vc_native_thread_context_detach(void *context);

void vc_native_thread_runtime_registry_lock(void);
void vc_native_thread_runtime_registry_unlock(void);
void vc_native_thread_runtime_heap_lock(void);
void vc_native_thread_runtime_heap_unlock(void);
void vc_native_thread_runtime_gc_lock(void);
void vc_native_thread_runtime_gc_unlock(void);
void vc_native_thread_runtime_gc_wait(void);
void vc_native_thread_runtime_gc_broadcast(void);

bool vc_native_monotonic_time_ns(uint64_t *nanoseconds);
bool vc_native_sleep_ms(uint32_t milliseconds);

VcNativeMonitor *vc_native_monitor_create(void);
bool vc_native_monitor_enter(VcNativeMonitor *monitor, void *owner);
bool vc_native_monitor_exit(VcNativeMonitor *monitor, void *owner);
bool vc_native_monitor_wait(VcNativeMonitor *monitor, void *owner);
VcNativeWaitResult vc_native_monitor_wait_timed(VcNativeMonitor *monitor, void *owner, uint32_t milliseconds);
bool vc_native_monitor_signal(VcNativeMonitor *monitor, void *owner);
bool vc_native_monitor_broadcast(VcNativeMonitor *monitor, void *owner);
bool vc_native_monitor_destroy(VcNativeMonitor *monitor);

#ifdef __cplusplus
}
#endif

#endif
