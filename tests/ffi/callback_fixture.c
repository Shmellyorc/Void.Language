#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "callback_fixture.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <windows.h>
typedef HANDLE Thread;
#define WORKER DWORD WINAPI
static void yield_thread(void) { Sleep(1); }
#else
#include <pthread.h>
#include <time.h>
typedef pthread_t Thread;
#define WORKER void*
static void yield_thread(void) { nanosleep(&(const struct timespec){0, 1000000}, NULL); }
#endif

int32_t g5_twice(int32_t x, G5Int callback, void* context) {
    int32_t first = callback(context, x);
    return first + callback(context, x + 3);
}
double g5_mix(double x, float y, G5Float callback, void* context) {
    double first = callback(context, x, y);
    return first + callback(context, x * 2, y + 1);
}
void g5_notify(int32_t x, G5Notify callback, void* context) {
    callback(context, x);
    callback(context, x + 1);
}

struct G5Registration {
    G5Int callback;
    void* context;
    Thread thread;
    atomic_bool go;
    atomic_bool ready;
    int32_t input;
    int32_t result;
    int mode;
};

static atomic_bool occupied;
static atomic_bool entered;
static atomic_bool finish;

G5Registration* g5_register(G5Int callback, void* context) {
    if (atomic_exchange(&occupied, 1)) return NULL;
    G5Registration* owner = calloc(1, sizeof *owner);
    if (!owner) {
        occupied = 0;
        return NULL;
    }
    owner->callback = callback;
    owner->context = context;
    atomic_init(&owner->go, 0);
    atomic_init(&owner->ready, 0);
    return owner;
}

static WORKER worker(void* data) {
    G5Registration* owner = data;
    // Reserve the pair before unregister or Runtime admission.
    G5Int callback = owner->callback;
    void* context = owner->context;
    owner->ready = 1;
    while (!owner->go) yield_thread();
    owner->result = callback(context, owner->input);
    return 0;
}

static int start(G5Registration* owner) {
#if defined(_WIN32)
    owner->thread = CreateThread(NULL, 0, worker, owner, 0, NULL);
    return owner->thread != NULL;
#else
    return pthread_create(&owner->thread, NULL, worker, owner) == 0;
#endif
}

static void join(Thread thread) {
#if defined(_WIN32)
    if (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0) abort();
    CloseHandle(thread);
#else
    if (pthread_join(thread, NULL)) abort();
#endif
}

int32_t g5_fire(const G5Registration* owner, int32_t input, int32_t foreign) {
    if (!foreign) return owner->callback(owner->context, input);
    G5Registration dispatch = {.callback = owner->callback, .context = owner->context, .input = input};
    atomic_init(&dispatch.ready, 0);
    atomic_init(&dispatch.go, 1);
    if (!start(&dispatch)) return -1000;
    join(dispatch.thread);
    return dispatch.result;
}

int32_t g5_queue(G5Registration* owner, int32_t mode) {
    if (owner->mode || (mode != 1 && mode != 2)) return -1;
    entered = 0;
    finish = 0;
    owner->mode = mode;
    owner->input = mode == 2 ? 99 : 50;
    owner->go = mode == 2;
    if (!start(owner)) {
        owner->mode = 0;
        return -2;
    }
    while (!owner->ready || (mode == 2 && !entered)) yield_thread();
    return 0;
}

int32_t g5_wait(void) {
    entered = 1;
    while (!finish) yield_thread();
    return 0;
}

int32_t g5_unregister(G5Registration* owner) {
    owner->callback = NULL;
    owner->context = NULL;
    occupied = 0;
    if (owner->mode) {
        owner->go = 1;
        finish = 1;
        join(owner->thread);
        printf("G5 barrier mode=%d result=%d\n", owner->mode, owner->result);
    }
    puts("G5 native unregistered and drained");
    fflush(stdout);
    free(owner);
    return 0;
}

int32_t g5_late(void) { return occupied ? -1 : 0; }
