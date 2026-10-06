#include "vc_thread.h"

#include <stdbool.h>
#include <stdio.h>

typedef struct WorkerState
{
    int input;
    int output;
} WorkerState;

static void worker_main(void *context)
{
    WorkerState *state = (WorkerState *)context;
    state->output = state->input * 3;
}

static void print_result(bool value)
{
    puts(value ? "True" : "False");
}

int main(void)
{
    print_result(vc_native_thread_runtime_init());

    VcNativeThread threads[4];
    WorkerState states[4];
    bool created = true;
    for (size_t i = 0; i < 4u; i++)
    {
        vc_native_thread_init(&threads[i]);
        states[i].input = (int)i + 1;
        states[i].output = 0;
        created = vc_native_thread_create(&threads[i], worker_main, &states[i]) && created;
    }
    print_result(created);

    bool joined = true;
    for (size_t i = 0; i < 4u; i++)
        joined = vc_native_thread_join(&threads[i]) && joined;
    print_result(joined);

    bool results = true;
    for (size_t i = 0; i < 4u; i++)
        results = states[i].output == ((int)i + 1) * 3 && results;
    print_result(results);

    print_result(!vc_native_thread_join(&threads[0]));

    VcNativeThread invalid;
    vc_native_thread_init(&invalid);
    print_result(!vc_native_thread_create(&invalid, NULL, NULL));
    print_result(!vc_native_thread_create(NULL, worker_main, NULL));

    vc_native_thread_runtime_shutdown();
    print_result(true);
    return 0;
}
