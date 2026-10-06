#include "vc_thread.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef struct WorkerState
{
    int token;
    bool initial_empty;
    bool attached_current;
    bool isolated_from_main;
    bool wrong_detach_rejected;
    bool detached_empty;
} WorkerState;

static int main_token;

static void worker_main(void *raw_context)
{
    WorkerState *state = (WorkerState *)raw_context;
    int wrong_token = -1;
    state->initial_empty = vc_native_thread_context_current() == NULL;
    state->attached_current = vc_native_thread_context_attach(&state->token) &&
        vc_native_thread_context_current() == &state->token;
    state->isolated_from_main = vc_native_thread_context_current() != &main_token;
    state->wrong_detach_rejected = !vc_native_thread_context_detach(&wrong_token);
    state->detached_empty = vc_native_thread_context_detach(&state->token) &&
        vc_native_thread_context_current() == NULL;
}

static void print_result(bool value)
{
    puts(value ? "True" : "False");
}

int main(void)
{
    print_result(vc_native_thread_runtime_init());
    print_result(vc_native_thread_context_current() == NULL);
    print_result(vc_native_thread_context_attach(&main_token));
    print_result(vc_native_thread_context_current() == &main_token);

    int second_main_token = 1;
    print_result(!vc_native_thread_context_attach(&second_main_token));

    VcNativeThread threads[4];
    WorkerState states[4];
    bool created = true;
    for (size_t i = 0; i < 4u; i++)
    {
        vc_native_thread_init(&threads[i]);
        states[i] = (WorkerState){ .token = (int)i + 10 };
        created = vc_native_thread_create(&threads[i], worker_main, &states[i]) && created;
    }
    print_result(created);

    bool joined = true;
    for (size_t i = 0; i < 4u; i++)
        joined = vc_native_thread_join(&threads[i]) && joined;
    print_result(joined);

    bool initial_empty = true;
    bool attached_current = true;
    bool isolated = true;
    bool wrong_detach = true;
    bool detached_empty = true;
    for (size_t i = 0; i < 4u; i++)
    {
        initial_empty = states[i].initial_empty && initial_empty;
        attached_current = states[i].attached_current && attached_current;
        isolated = states[i].isolated_from_main && isolated;
        wrong_detach = states[i].wrong_detach_rejected && wrong_detach;
        detached_empty = states[i].detached_empty && detached_empty;
    }
    print_result(initial_empty);
    print_result(attached_current);
    print_result(isolated);
    print_result(wrong_detach);
    print_result(detached_empty);

    print_result(vc_native_thread_context_current() == &main_token);
    print_result(!vc_native_thread_context_detach(&second_main_token));
    print_result(vc_native_thread_context_detach(&main_token));
    print_result(vc_native_thread_context_current() == NULL);

    vc_native_thread_runtime_shutdown();
    print_result(true);
    return 0;
}
