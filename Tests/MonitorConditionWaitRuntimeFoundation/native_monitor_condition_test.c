#include "../native_wait.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#define PRINT_BOOL(value) printf("%s\n", (value) ? "True" : "False")

typedef struct WaitWorker
{
    VcNativeMonitor *monitor;
    int owner;
    atomic_int *ready_count;
    atomic_int *done_count;
    bool recursive;
    bool entered_once;
    bool entered_twice;
    bool waited;
    bool exited_once;
    bool exited_twice;
    bool rejected_extra_exit;
} WaitWorker;

static void wait_worker_start(void *raw_context)
{
    WaitWorker *worker = raw_context;
    if (worker->owner == 5 && getenv("VOID_TEST_SLOW_START") != NULL
        && !vc_native_sleep_ms(500u)) return;
    worker->entered_once = vc_native_monitor_enter(worker->monitor, &worker->owner);
    worker->entered_twice = !worker->recursive || vc_native_monitor_enter(worker->monitor, &worker->owner);
    atomic_fetch_add(worker->ready_count, 1);
    worker->waited = vc_native_monitor_wait(worker->monitor, &worker->owner);
    worker->exited_once = vc_native_monitor_exit(worker->monitor, &worker->owner);
    worker->exited_twice = !worker->recursive || vc_native_monitor_exit(worker->monitor, &worker->owner);
    worker->rejected_extra_exit = !vc_native_monitor_exit(worker->monitor, &worker->owner);
    atomic_fetch_add(worker->done_count, 1);
}


int main(void)
{
    int owner_main = 1;
    int owner_other = 2;
    VcNativeMonitor *monitor = vc_native_monitor_create();
    PRINT_BOOL(monitor != NULL);
    if (monitor == NULL)
        return 1;

    PRINT_BOOL(!vc_native_monitor_wait(monitor, &owner_main));
    PRINT_BOOL(!vc_native_monitor_signal(monitor, &owner_main));
    PRINT_BOOL(!vc_native_monitor_broadcast(monitor, &owner_main));

    PRINT_BOOL(vc_native_monitor_enter(monitor, &owner_main));
    PRINT_BOOL(!vc_native_monitor_wait(monitor, &owner_other));
    PRINT_BOOL(!vc_native_monitor_signal(monitor, &owner_other));
    PRINT_BOOL(!vc_native_monitor_broadcast(monitor, &owner_other));
    PRINT_BOOL(vc_native_monitor_signal(monitor, &owner_main));
    PRINT_BOOL(vc_native_monitor_broadcast(monitor, &owner_main));
    PRINT_BOOL(vc_native_monitor_exit(monitor, &owner_main));

    atomic_int ready_one = 0;
    atomic_int done_one = 0;
    WaitWorker recursive_worker = {
        .monitor = monitor,
        .owner = 3,
        .ready_count = &ready_one,
        .done_count = &done_one,
        .recursive = true
    };
    VcNativeThread one;
    vc_native_thread_init(&one);
    PRINT_BOOL(vc_native_thread_create(&one, wait_worker_start, &recursive_worker));
    const bool one_ready = void_test_wait_for_count(&ready_one, 1);
    PRINT_BOOL(one_ready);
    if (!one_ready) return 1;
    PRINT_BOOL(vc_native_monitor_enter(monitor, &owner_main));
    PRINT_BOOL(vc_native_monitor_signal(monitor, &owner_main));
    PRINT_BOOL(vc_native_monitor_exit(monitor, &owner_main));
    PRINT_BOOL(vc_native_thread_join(&one));
    PRINT_BOOL(recursive_worker.entered_once && recursive_worker.entered_twice);
    PRINT_BOOL(recursive_worker.waited);
    PRINT_BOOL(recursive_worker.exited_once && recursive_worker.exited_twice);
    PRINT_BOOL(recursive_worker.rejected_extra_exit);
    PRINT_BOOL(atomic_load(&done_one) == 1);

    atomic_int ready_many = 0;
    atomic_int done_many = 0;
    WaitWorker first = {
        .monitor = monitor,
        .owner = 4,
        .ready_count = &ready_many,
        .done_count = &done_many,
        .recursive = false
    };
    WaitWorker second = {
        .monitor = monitor,
        .owner = 5,
        .ready_count = &ready_many,
        .done_count = &done_many,
        .recursive = false
    };
    VcNativeThread first_thread;
    VcNativeThread second_thread;
    vc_native_thread_init(&first_thread);
    vc_native_thread_init(&second_thread);
    PRINT_BOOL(vc_native_thread_create(&first_thread, wait_worker_start, &first));
    PRINT_BOOL(vc_native_thread_create(&second_thread, wait_worker_start, &second));
    const bool many_ready = void_test_wait_for_count(&ready_many, 2);
    PRINT_BOOL(many_ready);
    if (!many_ready) return 1;
    PRINT_BOOL(vc_native_monitor_enter(monitor, &owner_main));
    PRINT_BOOL(vc_native_monitor_broadcast(monitor, &owner_main));
    PRINT_BOOL(vc_native_monitor_exit(monitor, &owner_main));
    PRINT_BOOL(vc_native_thread_join(&first_thread));
    PRINT_BOOL(vc_native_thread_join(&second_thread));
    PRINT_BOOL(first.waited && first.exited_once && first.rejected_extra_exit);
    PRINT_BOOL(second.waited && second.exited_once && second.rejected_extra_exit);
    PRINT_BOOL(atomic_load(&done_many) == 2);

    PRINT_BOOL(vc_native_monitor_destroy(monitor));
    PRINT_BOOL(vc_native_monitor_destroy(NULL));
    return 0;
}
