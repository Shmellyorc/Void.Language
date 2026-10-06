#include "vc_thread.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

extern int32_t void164_blocked_root(int32_t value);
extern int32_t void164_managed_root(int32_t value, int32_t iterations);
extern int32_t void164_collect_stress(int32_t rounds);
extern int32_t void164_collect_after_join(int32_t value);

static bool blocked_ready = false;
static bool managed_ready = false;
static bool release_blocked = false;

static int32_t blocked_result = 0;
static int32_t managed_result = 0;

static void check(bool condition)
{
    puts(condition ? "True" : "False");
}

void void164_blocked_ready(void)
{
    vc_native_thread_runtime_registry_lock();
    blocked_ready = true;
    vc_native_thread_runtime_registry_unlock();
}

void void164_wait_blocked_release(void)
{
    for (;;)
    {
        bool release = false;
        vc_native_thread_runtime_registry_lock();
        release = release_blocked;
        vc_native_thread_runtime_registry_unlock();
        if (release)
            return;
    }
}

void void164_managed_ready(void)
{
    vc_native_thread_runtime_registry_lock();
    managed_ready = true;
    vc_native_thread_runtime_registry_unlock();
}

static void blocked_worker(void *context)
{
    (void)context;
    blocked_result = void164_blocked_root(641);
}

static void managed_worker(void *context)
{
    (void)context;
    managed_result = void164_managed_root(642, 2000000);
}

static bool both_workers_ready(void)
{
    bool ready = false;
    vc_native_thread_runtime_registry_lock();
    ready = blocked_ready && managed_ready;
    vc_native_thread_runtime_registry_unlock();
    return ready;
}

int main(void)
{
    VcNativeThread blocked_thread;
    VcNativeThread managed_thread;
    vc_native_thread_init(&blocked_thread);
    vc_native_thread_init(&managed_thread);

    check(vc_native_thread_runtime_init());

    bool created =
        vc_native_thread_create(&blocked_thread, blocked_worker, NULL) &&
        vc_native_thread_create(&managed_thread, managed_worker, NULL);
    check(created);
    if (!created)
        return 1;

    while (!both_workers_ready())
    {
    }

    check(void164_collect_stress(4) == 164);

    vc_native_thread_runtime_registry_lock();
    release_blocked = true;
    vc_native_thread_runtime_registry_unlock();

    bool joined =
        vc_native_thread_join(&blocked_thread) &&
        vc_native_thread_join(&managed_thread);
    check(joined);
    check(blocked_result == 1641);
    check(managed_result == 2642);
    check(void164_collect_after_join(643) == 3643);

    vc_native_thread_runtime_shutdown();
    return 0;
}
