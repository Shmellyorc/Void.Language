#include "../native_wait.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

static int failures = 0;

static void check(bool condition)
{
    puts(condition ? "True" : "False");
    if (!condition)
        failures++;
}

typedef struct TimedWaitWorker
{
    VcNativeMonitor *monitor;
    int owner;
    atomic_int *ready;
    VcNativeWaitResult result;
    bool enter_first;
    bool enter_second;
    bool exit_first;
    bool exit_second;
    bool rejected_extra_exit;
} TimedWaitWorker;

static void timed_wait_worker(void *raw_context)
{
    TimedWaitWorker *worker = raw_context;
    if (getenv("VOID_TEST_SLOW_START") != NULL
        && !vc_native_sleep_ms(500u)) return;
    worker->enter_first = vc_native_monitor_enter(worker->monitor, &worker->owner);
    worker->enter_second = vc_native_monitor_enter(worker->monitor, &worker->owner);
    atomic_store(worker->ready, 1);
    worker->result = vc_native_monitor_wait_timed(worker->monitor, &worker->owner, 2000u);
    worker->exit_first = vc_native_monitor_exit(worker->monitor, &worker->owner);
    worker->exit_second = vc_native_monitor_exit(worker->monitor, &worker->owner);
    worker->rejected_extra_exit = !vc_native_monitor_exit(worker->monitor, &worker->owner);
}


int main(void)
{
    uint64_t first_time = 0u;
    uint64_t second_time = 0u;
    check(!vc_native_monotonic_time_ns(NULL));
    check(vc_native_monotonic_time_ns(&first_time));
    check(vc_native_sleep_ms(20u));
    check(vc_native_monotonic_time_ns(&second_time));
    check(second_time >= first_time);
    check(second_time - first_time >= UINT64_C(10000000));
    check(second_time - first_time < UINT64_C(2000000000));
    check(vc_native_sleep_ms(0u));

    int owner_main = 1;
    int owner_other = 2;
    VcNativeMonitor *monitor = vc_native_monitor_create();
    check(monitor != NULL);
    if (monitor == NULL)
        return 1;

    check(vc_native_monitor_wait_timed(NULL, &owner_main, 0u) == VC_NATIVE_WAIT_ERROR);
    check(vc_native_monitor_wait_timed(monitor, NULL, 0u) == VC_NATIVE_WAIT_ERROR);
    check(vc_native_monitor_wait_timed(monitor, &owner_main, 0u) == VC_NATIVE_WAIT_ERROR);

    check(vc_native_monitor_enter(monitor, &owner_main));
    check(vc_native_monitor_enter(monitor, &owner_main));
    check(vc_native_monitor_wait_timed(monitor, &owner_other, 0u) == VC_NATIVE_WAIT_ERROR);
    check(vc_native_monitor_wait_timed(monitor, &owner_main, 0u) == VC_NATIVE_WAIT_TIMED_OUT);
    check(vc_native_monitor_exit(monitor, &owner_main));
    check(vc_native_monitor_exit(monitor, &owner_main));
    check(!vc_native_monitor_exit(monitor, &owner_main));

    check(vc_native_monitor_enter(monitor, &owner_main));
    uint64_t timeout_start = 0u;
    uint64_t timeout_end = 0u;
    check(vc_native_monotonic_time_ns(&timeout_start));
    check(vc_native_monitor_wait_timed(monitor, &owner_main, 30u) == VC_NATIVE_WAIT_TIMED_OUT);
    check(vc_native_monotonic_time_ns(&timeout_end));
    check(timeout_end >= timeout_start);
    check(timeout_end - timeout_start >= UINT64_C(15000000));
    check(timeout_end - timeout_start < UINT64_C(2000000000));
    check(vc_native_monitor_exit(monitor, &owner_main));

    atomic_int ready = 0;
    TimedWaitWorker worker = {
        .monitor = monitor,
        .owner = 3,
        .ready = &ready,
        .result = VC_NATIVE_WAIT_ERROR
    };
    VcNativeThread thread;
    vc_native_thread_init(&thread);
    check(vc_native_thread_create(&thread, timed_wait_worker, &worker));
    const bool worker_ready = void_test_wait_for_count(&ready, 1);
    check(worker_ready);
    if (!worker_ready) return 1;
    check(vc_native_monitor_enter(monitor, &owner_main));
    check(vc_native_monitor_signal(monitor, &owner_main));
    check(vc_native_monitor_exit(monitor, &owner_main));
    check(vc_native_thread_join(&thread));
    check(worker.enter_first && worker.enter_second);
    check(worker.result == VC_NATIVE_WAIT_SIGNALED);
    check(worker.exit_first && worker.exit_second);
    check(worker.rejected_extra_exit);

    check(vc_native_monitor_destroy(monitor));
    check(vc_native_monitor_destroy(NULL));
    return failures == 0 ? 0 : 1;
}
