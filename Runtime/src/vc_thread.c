#if !defined(_WIN32) && !defined(__APPLE__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "vc_thread.h"

#include <stdlib.h>

#if defined(_MSC_VER)
#define VC_NATIVE_THREAD_LOCAL __declspec(thread)
#else
#define VC_NATIVE_THREAD_LOCAL _Thread_local
#endif

static VC_NATIVE_THREAD_LOCAL void *vc_native_thread_context = NULL;

void *vc_native_thread_context_current(void)
{
    return vc_native_thread_context;
}

bool vc_native_thread_context_attach(void *context)
{
    if (context == NULL || vc_native_thread_context != NULL)
        return false;
    vc_native_thread_context = context;
    return true;
}

bool vc_native_thread_context_detach(void *context)
{
    if (context == NULL || vc_native_thread_context != context)
        return false;
    vc_native_thread_context = NULL;
    return true;
}


static bool vc_native_clock_combine_ns(uint64_t seconds, uint64_t fraction, uint64_t *out)
{
    const uint64_t scale = UINT64_C(1000000000);
    if (seconds > UINT64_MAX / scale)
        return false;
    const uint64_t whole = seconds * scale;
    if (fraction > UINT64_MAX - whole)
        return false;
    *out = whole + fraction;
    return true;
}


#if defined(_WIN32)

/* Convert a proper fraction of a second without overflowing its product.
   The fast path covers ordinary QPC frequencies. Binary long multiplication
   keeps both the remainder and quotient bounded for full-width inputs. */
static inline uint64_t vc_native_clock_fraction_ns(uint64_t remainder, uint64_t frequency)
{
    const uint64_t scale = UINT64_C(1000000000);
    if (remainder <= UINT64_MAX / scale)
        return remainder * scale / frequency;
    uint64_t quotient = 0;
    uint64_t residue = 0;
    for (uint64_t bit = UINT64_C(1) << 29; bit != 0; bit >>= 1)
    {
        quotient *= 2;
        if (residue >= frequency - residue)
        {
            residue -= frequency - residue;
            quotient++;
        }
        else
            residue += residue;
        if ((scale & bit) != 0)
        {
            if (residue >= frequency - remainder)
            {
                residue -= frequency - remainder;
                quotient++;
            }
            else
                residue += remainder;
        }
    }
    return quotient;
}


#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static SRWLOCK vc_native_runtime_registry_srwlock = SRWLOCK_INIT;
static SRWLOCK vc_native_runtime_heap_srwlock = SRWLOCK_INIT;
static SRWLOCK vc_native_runtime_gc_srwlock = SRWLOCK_INIT;
static CONDITION_VARIABLE vc_native_runtime_gc_condition = CONDITION_VARIABLE_INIT;

void vc_native_thread_runtime_registry_lock(void)
{
    AcquireSRWLockExclusive(&vc_native_runtime_registry_srwlock);
}

void vc_native_thread_runtime_registry_unlock(void)
{
    ReleaseSRWLockExclusive(&vc_native_runtime_registry_srwlock);
}

void vc_native_thread_runtime_heap_lock(void)
{
    AcquireSRWLockExclusive(&vc_native_runtime_heap_srwlock);
}

void vc_native_thread_runtime_heap_unlock(void)
{
    ReleaseSRWLockExclusive(&vc_native_runtime_heap_srwlock);
}

void vc_native_thread_runtime_gc_lock(void)
{
    AcquireSRWLockExclusive(&vc_native_runtime_gc_srwlock);
}

void vc_native_thread_runtime_gc_unlock(void)
{
    ReleaseSRWLockExclusive(&vc_native_runtime_gc_srwlock);
}

void vc_native_thread_runtime_gc_wait(void)
{
    if (!SleepConditionVariableSRW(
            &vc_native_runtime_gc_condition,
            &vc_native_runtime_gc_srwlock,
            INFINITE,
            0u))
        abort();
}

void vc_native_thread_runtime_gc_broadcast(void)
{
    WakeAllConditionVariable(&vc_native_runtime_gc_condition);
}

bool vc_native_monotonic_time_ns(uint64_t *nanoseconds)
{
    if (nanoseconds == NULL)
        return false;
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    if (!QueryPerformanceCounter(&counter) || !QueryPerformanceFrequency(&frequency) ||
        counter.QuadPart < 0 || frequency.QuadPart <= 0)
        return false;
    const uint64_t count = (uint64_t)counter.QuadPart;
    const uint64_t freq = (uint64_t)frequency.QuadPart;
    const uint64_t seconds = count / freq;
    const uint64_t remainder = count % freq;
    return vc_native_clock_combine_ns(seconds,
        vc_native_clock_fraction_ns(remainder, freq), nanoseconds);
}

bool vc_native_sleep_ms(uint32_t milliseconds)
{
    if (milliseconds == VC_NATIVE_TIMEOUT_INFINITE)
    {
        for (;;)
            Sleep(INFINITE);
    }
    Sleep((DWORD)milliseconds);
    return true;
}

typedef struct VcNativeMonitorWaiter
{
    CONDITION_VARIABLE condition;
    bool signaled;
    struct VcNativeMonitorWaiter *next;
} VcNativeMonitorWaiter;

struct VcNativeMonitor
{
    SRWLOCK lock;
    CONDITION_VARIABLE entry_condition;
    void *owner;
    size_t recursion;
    size_t entry_waiters;
    size_t condition_waiters;
    VcNativeMonitorWaiter *waiter_head;
    VcNativeMonitorWaiter *waiter_tail;
};

VcNativeMonitor *vc_native_monitor_create(void)
{
    VcNativeMonitor *monitor = calloc(1u, sizeof(*monitor));
    if (monitor == NULL)
        return NULL;
    InitializeSRWLock(&monitor->lock);
    InitializeConditionVariable(&monitor->entry_condition);
    return monitor;
}

bool vc_native_monitor_enter(VcNativeMonitor *monitor, void *owner)
{
    if (monitor == NULL || owner == NULL)
        return false;
    AcquireSRWLockExclusive(&monitor->lock);
    while (monitor->owner != NULL && monitor->owner != owner)
    {
        monitor->entry_waiters++;
        if (!SleepConditionVariableSRW(&monitor->entry_condition, &monitor->lock, INFINITE, 0u))
            abort();
        monitor->entry_waiters--;
    }
    if (monitor->owner == owner)
        monitor->recursion++;
    else
    {
        monitor->owner = owner;
        monitor->recursion = 1u;
    }
    ReleaseSRWLockExclusive(&monitor->lock);
    return true;
}

bool vc_native_monitor_exit(VcNativeMonitor *monitor, void *owner)
{
    if (monitor == NULL || owner == NULL)
        return false;
    AcquireSRWLockExclusive(&monitor->lock);
    if (monitor->owner != owner || monitor->recursion == 0u)
    {
        ReleaseSRWLockExclusive(&monitor->lock);
        return false;
    }
    monitor->recursion--;
    if (monitor->recursion == 0u)
    {
        monitor->owner = NULL;
        WakeConditionVariable(&monitor->entry_condition);
    }
    ReleaseSRWLockExclusive(&monitor->lock);
    return true;
}

bool vc_native_monitor_wait(VcNativeMonitor *monitor, void *owner)
{
    if (monitor == NULL || owner == NULL)
        return false;

    VcNativeMonitorWaiter waiter = {0};
    InitializeConditionVariable(&waiter.condition);

    AcquireSRWLockExclusive(&monitor->lock);
    if (monitor->owner != owner || monitor->recursion == 0u)
    {
        ReleaseSRWLockExclusive(&monitor->lock);
        return false;
    }

    const size_t recursion = monitor->recursion;
    if (monitor->waiter_tail != NULL)
        monitor->waiter_tail->next = &waiter;
    else
        monitor->waiter_head = &waiter;
    monitor->waiter_tail = &waiter;
    monitor->condition_waiters++;

    monitor->owner = NULL;
    monitor->recursion = 0u;
    WakeConditionVariable(&monitor->entry_condition);

    while (!waiter.signaled)
    {
        if (!SleepConditionVariableSRW(&waiter.condition, &monitor->lock, INFINITE, 0u))
            abort();
    }

    VcNativeMonitorWaiter **link = &monitor->waiter_head;
    while (*link != NULL && *link != &waiter)
        link = &(*link)->next;
    if (*link != &waiter)
        abort();
    *link = waiter.next;
    if (monitor->waiter_tail == &waiter)
    {
        monitor->waiter_tail = NULL;
        for (VcNativeMonitorWaiter *item = monitor->waiter_head; item != NULL; item = item->next)
            monitor->waiter_tail = item;
    }
    if (monitor->condition_waiters == 0u)
        abort();
    monitor->condition_waiters--;

    while (monitor->owner != NULL && monitor->owner != owner)
    {
        monitor->entry_waiters++;
        if (!SleepConditionVariableSRW(&monitor->entry_condition, &monitor->lock, INFINITE, 0u))
            abort();
        monitor->entry_waiters--;
    }
    if (monitor->owner != NULL)
        abort();
    monitor->owner = owner;
    monitor->recursion = recursion;
    ReleaseSRWLockExclusive(&monitor->lock);
    return true;
}

VcNativeWaitResult vc_native_monitor_wait_timed(VcNativeMonitor *monitor, void *owner, uint32_t milliseconds)
{
    if (milliseconds == VC_NATIVE_TIMEOUT_INFINITE)
        return vc_native_monitor_wait(monitor, owner) ? VC_NATIVE_WAIT_SIGNALED : VC_NATIVE_WAIT_ERROR;
    if (monitor == NULL || owner == NULL)
        return VC_NATIVE_WAIT_ERROR;

    uint64_t started = 0u;
    if (!vc_native_monotonic_time_ns(&started))
        return VC_NATIVE_WAIT_ERROR;
    const uint64_t deadline = started + (uint64_t)milliseconds * UINT64_C(1000000);

    VcNativeMonitorWaiter waiter = {0};
    InitializeConditionVariable(&waiter.condition);

    AcquireSRWLockExclusive(&monitor->lock);
    if (monitor->owner != owner || monitor->recursion == 0u)
    {
        ReleaseSRWLockExclusive(&monitor->lock);
        return VC_NATIVE_WAIT_ERROR;
    }

    const size_t recursion = monitor->recursion;
    if (monitor->waiter_tail != NULL)
        monitor->waiter_tail->next = &waiter;
    else
        monitor->waiter_head = &waiter;
    monitor->waiter_tail = &waiter;
    monitor->condition_waiters++;

    monitor->owner = NULL;
    monitor->recursion = 0u;
    WakeConditionVariable(&monitor->entry_condition);

    VcNativeWaitResult result = VC_NATIVE_WAIT_ERROR;
    bool waited_once = false;
    while (!waiter.signaled)
    {
        uint64_t now = 0u;
        if (!vc_native_monotonic_time_ns(&now))
            break;
        DWORD wait_ms = 0u;
        if (now < deadline)
        {
            const uint64_t remaining_ns = deadline - now;
            uint64_t remaining_ms = (remaining_ns + UINT64_C(999999)) / UINT64_C(1000000);
            if (remaining_ms >= (uint64_t)INFINITE)
                remaining_ms = (uint64_t)INFINITE - 1u;
            wait_ms = (DWORD)remaining_ms;
        }
        else if (waited_once)
        {
            result = VC_NATIVE_WAIT_TIMED_OUT;
            break;
        }

        waited_once = true;
        if (SleepConditionVariableSRW(&waiter.condition, &monitor->lock, wait_ms, 0u))
            continue;
        const DWORD error = GetLastError();
        if (error != ERROR_TIMEOUT)
            break;
        if (waiter.signaled)
            break;
        if (!vc_native_monotonic_time_ns(&now))
            break;
        if (now >= deadline)
        {
            result = VC_NATIVE_WAIT_TIMED_OUT;
            break;
        }
    }
    if (waiter.signaled)
        result = VC_NATIVE_WAIT_SIGNALED;

    VcNativeMonitorWaiter **link = &monitor->waiter_head;
    while (*link != NULL && *link != &waiter)
        link = &(*link)->next;
    if (*link != &waiter)
        abort();
    *link = waiter.next;
    if (monitor->waiter_tail == &waiter)
    {
        monitor->waiter_tail = NULL;
        for (VcNativeMonitorWaiter *item = monitor->waiter_head; item != NULL; item = item->next)
            monitor->waiter_tail = item;
    }
    if (monitor->condition_waiters == 0u)
        abort();
    monitor->condition_waiters--;

    while (monitor->owner != NULL && monitor->owner != owner)
    {
        monitor->entry_waiters++;
        if (!SleepConditionVariableSRW(&monitor->entry_condition, &monitor->lock, INFINITE, 0u))
            abort();
        monitor->entry_waiters--;
    }
    if (monitor->owner != NULL)
        abort();
    monitor->owner = owner;
    monitor->recursion = recursion;
    ReleaseSRWLockExclusive(&monitor->lock);
    return result;
}

bool vc_native_monitor_signal(VcNativeMonitor *monitor, void *owner)
{
    if (monitor == NULL || owner == NULL)
        return false;
    AcquireSRWLockExclusive(&monitor->lock);
    if (monitor->owner != owner || monitor->recursion == 0u)
    {
        ReleaseSRWLockExclusive(&monitor->lock);
        return false;
    }
    for (VcNativeMonitorWaiter *waiter = monitor->waiter_head; waiter != NULL; waiter = waiter->next)
    {
        if (waiter->signaled)
            continue;
        waiter->signaled = true;
        WakeConditionVariable(&waiter->condition);
        break;
    }
    ReleaseSRWLockExclusive(&monitor->lock);
    return true;
}

bool vc_native_monitor_broadcast(VcNativeMonitor *monitor, void *owner)
{
    if (monitor == NULL || owner == NULL)
        return false;
    AcquireSRWLockExclusive(&monitor->lock);
    if (monitor->owner != owner || monitor->recursion == 0u)
    {
        ReleaseSRWLockExclusive(&monitor->lock);
        return false;
    }
    for (VcNativeMonitorWaiter *waiter = monitor->waiter_head; waiter != NULL; waiter = waiter->next)
    {
        if (waiter->signaled)
            continue;
        waiter->signaled = true;
        WakeConditionVariable(&waiter->condition);
    }
    ReleaseSRWLockExclusive(&monitor->lock);
    return true;
}

bool vc_native_monitor_destroy(VcNativeMonitor *monitor)
{
    if (monitor == NULL)
        return true;
    AcquireSRWLockExclusive(&monitor->lock);
    const bool can_destroy = monitor->owner == NULL && monitor->recursion == 0u &&
        monitor->entry_waiters == 0u && monitor->condition_waiters == 0u &&
        monitor->waiter_head == NULL && monitor->waiter_tail == NULL;
    ReleaseSRWLockExclusive(&monitor->lock);
    if (!can_destroy)
        return false;
    free(monitor);
    return true;
}

typedef struct VcNativeThreadPlatform
{
    HANDLE handle;
    DWORD id;
} VcNativeThreadPlatform;

static DWORD WINAPI vc_native_thread_trampoline(LPVOID raw_thread)
{
    VcNativeThread *thread = (VcNativeThread *)raw_thread;
    thread->start(thread->context);
    return 0u;
}

#elif defined(__unix__) || defined(__APPLE__)

#include <errno.h>
#include <pthread.h>
#include <time.h>

static pthread_mutex_t vc_native_runtime_registry_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t vc_native_runtime_heap_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t vc_native_runtime_gc_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t vc_native_runtime_gc_condition = PTHREAD_COND_INITIALIZER;

void vc_native_thread_runtime_registry_lock(void)
{
    (void)pthread_mutex_lock(&vc_native_runtime_registry_mutex);
}

void vc_native_thread_runtime_registry_unlock(void)
{
    (void)pthread_mutex_unlock(&vc_native_runtime_registry_mutex);
}

void vc_native_thread_runtime_heap_lock(void)
{
    (void)pthread_mutex_lock(&vc_native_runtime_heap_mutex);
}

void vc_native_thread_runtime_heap_unlock(void)
{
    (void)pthread_mutex_unlock(&vc_native_runtime_heap_mutex);
}

void vc_native_thread_runtime_gc_lock(void)
{
    (void)pthread_mutex_lock(&vc_native_runtime_gc_mutex);
}

void vc_native_thread_runtime_gc_unlock(void)
{
    (void)pthread_mutex_unlock(&vc_native_runtime_gc_mutex);
}

void vc_native_thread_runtime_gc_wait(void)
{
    (void)pthread_cond_wait(&vc_native_runtime_gc_condition, &vc_native_runtime_gc_mutex);
}

void vc_native_thread_runtime_gc_broadcast(void)
{
    (void)pthread_cond_broadcast(&vc_native_runtime_gc_condition);
}

bool vc_native_monotonic_time_ns(uint64_t *nanoseconds)
{
    if (nanoseconds == NULL)
        return false;
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec < 0 ||
        now.tv_nsec < 0 || now.tv_nsec >= 1000000000L)
        return false;
    const uint64_t seconds = (uint64_t)now.tv_sec;
    return vc_native_clock_combine_ns(seconds, (uint64_t)now.tv_nsec, nanoseconds);
}

bool vc_native_sleep_ms(uint32_t milliseconds)
{
    if (milliseconds == VC_NATIVE_TIMEOUT_INFINITE)
    {
        for (;;)
        {
            struct timespec request = { .tv_sec = 86400, .tv_nsec = 0 };
            while (nanosleep(&request, &request) != 0)
            {
                if (errno != EINTR)
                    return false;
            }
        }
    }

    struct timespec request = {
        .tv_sec = (time_t)(milliseconds / 1000u),
        .tv_nsec = (long)(milliseconds % 1000u) * 1000000L
    };
    while (nanosleep(&request, &request) != 0)
    {
        if (errno != EINTR)
            return false;
    }
    return true;
}

typedef struct VcNativeMonitorWaiter
{
    pthread_cond_t condition;
    bool signaled;
    struct VcNativeMonitorWaiter *next;
} VcNativeMonitorWaiter;

static bool vc_native_monitor_waiter_init(VcNativeMonitorWaiter *waiter)
{
#if defined(__APPLE__)
    return pthread_cond_init(&waiter->condition, NULL) == 0;
#else
    pthread_condattr_t attributes;
    if (pthread_condattr_init(&attributes) != 0)
        return false;
    bool ok = pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC) == 0 &&
        pthread_cond_init(&waiter->condition, &attributes) == 0;
    if (pthread_condattr_destroy(&attributes) != 0)
        ok = false;
    return ok;
#endif
}

struct VcNativeMonitor
{
    pthread_mutex_t mutex;
    pthread_cond_t entry_condition;
    void *owner;
    size_t recursion;
    size_t entry_waiters;
    size_t condition_waiters;
    VcNativeMonitorWaiter *waiter_head;
    VcNativeMonitorWaiter *waiter_tail;
};

VcNativeMonitor *vc_native_monitor_create(void)
{
    VcNativeMonitor *monitor = calloc(1u, sizeof(*monitor));
    if (monitor == NULL)
        return NULL;
    if (pthread_mutex_init(&monitor->mutex, NULL) != 0)
    {
        free(monitor);
        return NULL;
    }
    if (pthread_cond_init(&monitor->entry_condition, NULL) != 0)
    {
        (void)pthread_mutex_destroy(&monitor->mutex);
        free(monitor);
        return NULL;
    }
    return monitor;
}

bool vc_native_monitor_enter(VcNativeMonitor *monitor, void *owner)
{
    if (monitor == NULL || owner == NULL)
        return false;
    if (pthread_mutex_lock(&monitor->mutex) != 0)
        return false;
    while (monitor->owner != NULL && monitor->owner != owner)
    {
        monitor->entry_waiters++;
        const int result = pthread_cond_wait(&monitor->entry_condition, &monitor->mutex);
        monitor->entry_waiters--;
        if (result != 0)
        {
            (void)pthread_mutex_unlock(&monitor->mutex);
            return false;
        }
    }
    if (monitor->owner == owner)
        monitor->recursion++;
    else
    {
        monitor->owner = owner;
        monitor->recursion = 1u;
    }
    return pthread_mutex_unlock(&monitor->mutex) == 0;
}

bool vc_native_monitor_exit(VcNativeMonitor *monitor, void *owner)
{
    if (monitor == NULL || owner == NULL)
        return false;
    if (pthread_mutex_lock(&monitor->mutex) != 0)
        return false;
    if (monitor->owner != owner || monitor->recursion == 0u)
    {
        (void)pthread_mutex_unlock(&monitor->mutex);
        return false;
    }
    monitor->recursion--;
    if (monitor->recursion == 0u)
    {
        monitor->owner = NULL;
        (void)pthread_cond_signal(&monitor->entry_condition);
    }
    return pthread_mutex_unlock(&monitor->mutex) == 0;
}

bool vc_native_monitor_wait(VcNativeMonitor *monitor, void *owner)
{
    if (monitor == NULL || owner == NULL)
        return false;

    VcNativeMonitorWaiter waiter = {0};
    if (!vc_native_monitor_waiter_init(&waiter))
        return false;
    if (pthread_mutex_lock(&monitor->mutex) != 0)
    {
        (void)pthread_cond_destroy(&waiter.condition);
        return false;
    }
    if (monitor->owner != owner || monitor->recursion == 0u)
    {
        (void)pthread_mutex_unlock(&monitor->mutex);
        (void)pthread_cond_destroy(&waiter.condition);
        return false;
    }

    const size_t recursion = monitor->recursion;
    if (monitor->waiter_tail != NULL)
        monitor->waiter_tail->next = &waiter;
    else
        monitor->waiter_head = &waiter;
    monitor->waiter_tail = &waiter;
    monitor->condition_waiters++;

    monitor->owner = NULL;
    monitor->recursion = 0u;
    (void)pthread_cond_signal(&monitor->entry_condition);

    while (!waiter.signaled)
    {
        if (pthread_cond_wait(&waiter.condition, &monitor->mutex) != 0)
            abort();
    }

    VcNativeMonitorWaiter **link = &monitor->waiter_head;
    while (*link != NULL && *link != &waiter)
        link = &(*link)->next;
    if (*link != &waiter)
        abort();
    *link = waiter.next;
    if (monitor->waiter_tail == &waiter)
    {
        monitor->waiter_tail = NULL;
        for (VcNativeMonitorWaiter *item = monitor->waiter_head; item != NULL; item = item->next)
            monitor->waiter_tail = item;
    }
    if (monitor->condition_waiters == 0u)
        abort();
    monitor->condition_waiters--;

    while (monitor->owner != NULL && monitor->owner != owner)
    {
        monitor->entry_waiters++;
        const int result = pthread_cond_wait(&monitor->entry_condition, &monitor->mutex);
        monitor->entry_waiters--;
        if (result != 0)
            abort();
    }
    if (monitor->owner != NULL)
        abort();
    monitor->owner = owner;
    monitor->recursion = recursion;
    const bool unlocked = pthread_mutex_unlock(&monitor->mutex) == 0;
    const bool destroyed = pthread_cond_destroy(&waiter.condition) == 0;
    return unlocked && destroyed;
}

VcNativeWaitResult vc_native_monitor_wait_timed(VcNativeMonitor *monitor, void *owner, uint32_t milliseconds)
{
    if (milliseconds == VC_NATIVE_TIMEOUT_INFINITE)
        return vc_native_monitor_wait(monitor, owner) ? VC_NATIVE_WAIT_SIGNALED : VC_NATIVE_WAIT_ERROR;
    if (monitor == NULL || owner == NULL)
        return VC_NATIVE_WAIT_ERROR;

    VcNativeMonitorWaiter waiter = {0};
    if (!vc_native_monitor_waiter_init(&waiter))
        return VC_NATIVE_WAIT_ERROR;
    if (pthread_mutex_lock(&monitor->mutex) != 0)
    {
        (void)pthread_cond_destroy(&waiter.condition);
        return VC_NATIVE_WAIT_ERROR;
    }
    if (monitor->owner != owner || monitor->recursion == 0u)
    {
        (void)pthread_mutex_unlock(&monitor->mutex);
        (void)pthread_cond_destroy(&waiter.condition);
        return VC_NATIVE_WAIT_ERROR;
    }

    const size_t recursion = monitor->recursion;
    if (monitor->waiter_tail != NULL)
        monitor->waiter_tail->next = &waiter;
    else
        monitor->waiter_head = &waiter;
    monitor->waiter_tail = &waiter;
    monitor->condition_waiters++;

    monitor->owner = NULL;
    monitor->recursion = 0u;
    (void)pthread_cond_signal(&monitor->entry_condition);

    VcNativeWaitResult result = VC_NATIVE_WAIT_ERROR;
#if defined(__APPLE__)
    uint64_t started = 0u;
    if (vc_native_monotonic_time_ns(&started))
    {
        const uint64_t deadline_ns = started + (uint64_t)milliseconds * UINT64_C(1000000);
        bool waited_once = false;
        while (!waiter.signaled)
        {
            uint64_t now = 0u;
            if (!vc_native_monotonic_time_ns(&now))
                break;
            uint64_t remaining_ns = 0u;
            if (now < deadline_ns)
                remaining_ns = deadline_ns - now;
            else if (waited_once)
            {
                result = VC_NATIVE_WAIT_TIMED_OUT;
                break;
            }
            const struct timespec relative = {
                .tv_sec = (time_t)(remaining_ns / UINT64_C(1000000000)),
                .tv_nsec = (long)(remaining_ns % UINT64_C(1000000000))
            };
            waited_once = true;
            const int wait_result = pthread_cond_timedwait_relative_np(
                &waiter.condition,
                &monitor->mutex,
                &relative);
            if (wait_result == ETIMEDOUT)
            {
                result = waiter.signaled ? VC_NATIVE_WAIT_SIGNALED : VC_NATIVE_WAIT_TIMED_OUT;
                break;
            }
            if (wait_result != 0)
                break;
        }
    }
#else
    uint64_t started = 0u;
    if (vc_native_monotonic_time_ns(&started))
    {
        const uint64_t deadline_ns = started + (uint64_t)milliseconds * UINT64_C(1000000);
        const struct timespec deadline = {
            .tv_sec = (time_t)(deadline_ns / UINT64_C(1000000000)),
            .tv_nsec = (long)(deadline_ns % UINT64_C(1000000000))
        };
        while (!waiter.signaled)
        {
            const int wait_result = pthread_cond_timedwait(
                &waiter.condition,
                &monitor->mutex,
                &deadline);
            if (wait_result == ETIMEDOUT)
            {
                result = waiter.signaled ? VC_NATIVE_WAIT_SIGNALED : VC_NATIVE_WAIT_TIMED_OUT;
                break;
            }
            if (wait_result != 0)
                break;
        }
    }
#endif
    if (waiter.signaled)
        result = VC_NATIVE_WAIT_SIGNALED;

    VcNativeMonitorWaiter **link = &monitor->waiter_head;
    while (*link != NULL && *link != &waiter)
        link = &(*link)->next;
    if (*link != &waiter)
        abort();
    *link = waiter.next;
    if (monitor->waiter_tail == &waiter)
    {
        monitor->waiter_tail = NULL;
        for (VcNativeMonitorWaiter *item = monitor->waiter_head; item != NULL; item = item->next)
            monitor->waiter_tail = item;
    }
    if (monitor->condition_waiters == 0u)
        abort();
    monitor->condition_waiters--;

    while (monitor->owner != NULL && monitor->owner != owner)
    {
        monitor->entry_waiters++;
        const int wait_result = pthread_cond_wait(&monitor->entry_condition, &monitor->mutex);
        monitor->entry_waiters--;
        if (wait_result != 0)
            abort();
    }
    if (monitor->owner != NULL)
        abort();
    monitor->owner = owner;
    monitor->recursion = recursion;
    const bool unlocked = pthread_mutex_unlock(&monitor->mutex) == 0;
    const bool destroyed = pthread_cond_destroy(&waiter.condition) == 0;
    return unlocked && destroyed ? result : VC_NATIVE_WAIT_ERROR;
}

bool vc_native_monitor_signal(VcNativeMonitor *monitor, void *owner)
{
    if (monitor == NULL || owner == NULL)
        return false;
    if (pthread_mutex_lock(&monitor->mutex) != 0)
        return false;
    if (monitor->owner != owner || monitor->recursion == 0u)
    {
        (void)pthread_mutex_unlock(&monitor->mutex);
        return false;
    }
    for (VcNativeMonitorWaiter *waiter = monitor->waiter_head; waiter != NULL; waiter = waiter->next)
    {
        if (waiter->signaled)
            continue;
        waiter->signaled = true;
        (void)pthread_cond_signal(&waiter->condition);
        break;
    }
    return pthread_mutex_unlock(&monitor->mutex) == 0;
}

bool vc_native_monitor_broadcast(VcNativeMonitor *monitor, void *owner)
{
    if (monitor == NULL || owner == NULL)
        return false;
    if (pthread_mutex_lock(&monitor->mutex) != 0)
        return false;
    if (monitor->owner != owner || monitor->recursion == 0u)
    {
        (void)pthread_mutex_unlock(&monitor->mutex);
        return false;
    }
    bool ok = true;
    for (VcNativeMonitorWaiter *waiter = monitor->waiter_head; waiter != NULL; waiter = waiter->next)
    {
        if (waiter->signaled)
            continue;
        waiter->signaled = true;
        if (pthread_cond_signal(&waiter->condition) != 0)
            ok = false;
    }
    return pthread_mutex_unlock(&monitor->mutex) == 0 && ok;
}

bool vc_native_monitor_destroy(VcNativeMonitor *monitor)
{
    if (monitor == NULL)
        return true;
    if (pthread_mutex_lock(&monitor->mutex) != 0)
        return false;
    const bool can_destroy = monitor->owner == NULL && monitor->recursion == 0u &&
        monitor->entry_waiters == 0u && monitor->condition_waiters == 0u &&
        monitor->waiter_head == NULL && monitor->waiter_tail == NULL;
    if (pthread_mutex_unlock(&monitor->mutex) != 0 || !can_destroy)
        return false;
    if (pthread_cond_destroy(&monitor->entry_condition) != 0)
        return false;
    if (pthread_mutex_destroy(&monitor->mutex) != 0)
        return false;
    free(monitor);
    return true;
}

typedef struct VcNativeThreadPlatform
{
    pthread_t handle;
} VcNativeThreadPlatform;

static void *vc_native_thread_trampoline(void *raw_thread)
{
    VcNativeThread *thread = (VcNativeThread *)raw_thread;
    thread->start(thread->context);
    return NULL;
}

#else
#error "VOID native thread runtime is not supported on this platform"
#endif

bool vc_native_thread_create(
    VcNativeThread *thread,
    VcNativeThreadStartFn start,
    void *context)
{
    if (thread == NULL || start == NULL || thread->joinable || thread->platform_state != NULL)
        return false;

    VcNativeThreadPlatform *platform = calloc(1u, sizeof(*platform));
    if (platform == NULL)
        return false;

    thread->platform_state = platform;
    thread->start = start;
    thread->context = context;

#if defined(_WIN32)
    platform->handle = CreateThread(
        NULL,
        0u,
        vc_native_thread_trampoline,
        thread,
        0u,
        &platform->id);
    if (platform->handle == NULL)
    {
        free(platform);
        thread->platform_state = NULL;
        thread->start = NULL;
        thread->context = NULL;
        return false;
    }
#else
    if (pthread_create(&platform->handle, NULL, vc_native_thread_trampoline, thread) != 0)
    {
        free(platform);
        thread->platform_state = NULL;
        thread->start = NULL;
        thread->context = NULL;
        return false;
    }
#endif

    thread->joinable = true;
    return true;
}

bool vc_native_thread_join(VcNativeThread *thread)
{
    if (thread == NULL || !thread->joinable || thread->platform_state == NULL)
        return false;

    VcNativeThreadPlatform *platform = (VcNativeThreadPlatform *)thread->platform_state;

#if defined(_WIN32)
    if (WaitForSingleObject(platform->handle, INFINITE) != WAIT_OBJECT_0)
        return false;
    if (CloseHandle(platform->handle) == 0)
        return false;
#else
    if (pthread_join(platform->handle, NULL) != 0)
        return false;
#endif

    free(platform);
    thread->platform_state = NULL;
    thread->start = NULL;
    thread->context = NULL;
    thread->joinable = false;
    return true;
}
