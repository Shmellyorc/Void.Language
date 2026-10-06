#include "vc_thread.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

extern int32_t void163_allocate_sum(int32_t seed, int32_t count);
extern int32_t void163_collect_value(int32_t value);

enum
{
    WORKER_COUNT = 4,
    ALLOCATION_COUNT = 400,
    LOCK_INCREMENT_COUNT = 10000
};

typedef struct WorkerState
{
    int32_t seed;
    int32_t result;
    bool completed;
} WorkerState;

static int ready_count = 0;
static bool start_workers = false;
static int heap_counter = 0;

static void check(bool condition)
{
    puts(condition ? "True" : "False");
}

static void worker_main(void *raw_state)
{
    WorkerState *state = (WorkerState *)raw_state;

    vc_native_thread_runtime_registry_lock();
    ready_count++;
    vc_native_thread_runtime_registry_unlock();

    for (;;)
    {
        bool start = false;
        vc_native_thread_runtime_registry_lock();
        start = start_workers;
        vc_native_thread_runtime_registry_unlock();
        if (start)
            break;
    }

    for (int i = 0; i < LOCK_INCREMENT_COUNT; i++)
    {
        vc_native_thread_runtime_heap_lock();
        heap_counter++;
        vc_native_thread_runtime_heap_unlock();
    }

    state->result = void163_allocate_sum(state->seed, ALLOCATION_COUNT);
    state->completed = true;
}

static int32_t expected_sum(int32_t seed)
{
    return seed * ALLOCATION_COUNT +
        (ALLOCATION_COUNT * (ALLOCATION_COUNT - 1)) / 2;
}

int main(void)
{
    VcNativeThread threads[WORKER_COUNT];
    WorkerState states[WORKER_COUNT];
    bool created = true;

    check(vc_native_thread_runtime_init());

    for (int i = 0; i < WORKER_COUNT; i++)
    {
        vc_native_thread_init(&threads[i]);
        states[i].seed = 1000 + (i * 500);
        states[i].result = 0;
        states[i].completed = false;
        if (!vc_native_thread_create(&threads[i], worker_main, &states[i]))
            created = false;
    }
    check(created);

    for (;;)
    {
        int ready = 0;
        vc_native_thread_runtime_registry_lock();
        ready = ready_count;
        vc_native_thread_runtime_registry_unlock();
        if (ready == WORKER_COUNT)
            break;
    }

    vc_native_thread_runtime_registry_lock();
    start_workers = true;
    vc_native_thread_runtime_registry_unlock();

    bool joined = true;
    for (int i = 0; i < WORKER_COUNT; i++)
    {
        if (!vc_native_thread_join(&threads[i]))
            joined = false;
    }
    check(joined);

    bool results = true;
    for (int i = 0; i < WORKER_COUNT; i++)
    {
        if (!states[i].completed || states[i].result != expected_sum(states[i].seed))
            results = false;
    }
    check(results);
    check(heap_counter == WORKER_COUNT * LOCK_INCREMENT_COUNT);
    check(void163_collect_value(163) == 163);
    check(void163_collect_value(326) == 326);

    vc_native_thread_runtime_shutdown();
    return 0;
}
