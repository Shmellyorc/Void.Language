#include "vc_atomic.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

static int failures = 0;

static void check(int condition)
{
    puts(condition ? "True" : "False");
    if (!condition)
        failures++;
}

typedef struct IncrementState
{
    int32_t value;
    int iterations;
} IncrementState;

static void *increment_worker(void *raw_state)
{
    IncrementState *state = (IncrementState *)raw_state;
    for (int i = 0; i < state->iterations; i++)
        (void)vc_native_atomic_i32_fetch_add(&state->value, 1, VC_NATIVE_MEMORY_ORDER_RELAXED);
    return NULL;
}

typedef struct PublishState
{
    int32_t ready;
    int32_t payload;
} PublishState;

static void *publish_worker(void *raw_state)
{
    PublishState *state = (PublishState *)raw_state;
    state->payload = 1729;
    vc_native_atomic_i32_store(&state->ready, 1, VC_NATIVE_MEMORY_ORDER_RELEASE);
    return NULL;
}

int main(void)
{
    int32_t i32 = 10;
    check(vc_native_atomic_i32_load(&i32, VC_NATIVE_MEMORY_ORDER_RELAXED) == 10);
    vc_native_atomic_i32_store(&i32, 12, VC_NATIVE_MEMORY_ORDER_RELEASE);
    check(vc_native_atomic_i32_load(&i32, VC_NATIVE_MEMORY_ORDER_ACQUIRE) == 12);
    check(vc_native_atomic_i32_exchange(&i32, 20, VC_NATIVE_MEMORY_ORDER_ACQ_REL) == 12);
    check(i32 == 20);
    check(vc_native_atomic_i32_compare_exchange(&i32, 30, 20, VC_NATIVE_MEMORY_ORDER_SEQ_CST) == 20);
    check(i32 == 30);
    check(vc_native_atomic_i32_compare_exchange(&i32, 40, 99, VC_NATIVE_MEMORY_ORDER_SEQ_CST) == 30);
    check(i32 == 30);
    check(vc_native_atomic_i32_fetch_add(&i32, 5, VC_NATIVE_MEMORY_ORDER_RELAXED) == 30);
    check(i32 == 35);

    int64_t i64 = INT64_C(10000000000);
    check(vc_native_atomic_i64_load(&i64, VC_NATIVE_MEMORY_ORDER_RELAXED) == INT64_C(10000000000));
    vc_native_atomic_i64_store(&i64, INT64_C(20000000000), VC_NATIVE_MEMORY_ORDER_RELEASE);
    check(vc_native_atomic_i64_load(&i64, VC_NATIVE_MEMORY_ORDER_ACQUIRE) == INT64_C(20000000000));
    check(vc_native_atomic_i64_exchange(&i64, INT64_C(30000000000), VC_NATIVE_MEMORY_ORDER_SEQ_CST) == INT64_C(20000000000));
    check(i64 == INT64_C(30000000000));
    check(vc_native_atomic_i64_compare_exchange(&i64, INT64_C(40000000000), INT64_C(30000000000), VC_NATIVE_MEMORY_ORDER_ACQ_REL) == INT64_C(30000000000));
    check(i64 == INT64_C(40000000000));
    check(vc_native_atomic_i64_fetch_add(&i64, INT64_C(7), VC_NATIVE_MEMORY_ORDER_RELAXED) == INT64_C(40000000000));
    check(i64 == INT64_C(40000000007));

    intptr_t iptr = (intptr_t)7;
    check(vc_native_atomic_iptr_load(&iptr, VC_NATIVE_MEMORY_ORDER_RELAXED) == (intptr_t)7);
    check(vc_native_atomic_iptr_exchange(&iptr, (intptr_t)9, VC_NATIVE_MEMORY_ORDER_SEQ_CST) == (intptr_t)7);
    check(iptr == (intptr_t)9);
    check(vc_native_atomic_iptr_compare_exchange(&iptr, (intptr_t)11, (intptr_t)9, VC_NATIVE_MEMORY_ORDER_ACQ_REL) == (intptr_t)9);
    check(iptr == (intptr_t)11);
    check(vc_native_atomic_iptr_fetch_add(&iptr, (intptr_t)3, VC_NATIVE_MEMORY_ORDER_RELAXED) == (intptr_t)11);
    check(iptr == (intptr_t)14);

    int first = 1;
    int second = 2;
    int third = 3;
    void *pointer = &first;
    check(vc_native_atomic_ptr_load(&pointer, VC_NATIVE_MEMORY_ORDER_RELAXED) == &first);
    vc_native_atomic_ptr_store(&pointer, &second, VC_NATIVE_MEMORY_ORDER_RELEASE);
    check(vc_native_atomic_ptr_load(&pointer, VC_NATIVE_MEMORY_ORDER_ACQUIRE) == &second);
    check(vc_native_atomic_ptr_exchange(&pointer, &third, VC_NATIVE_MEMORY_ORDER_SEQ_CST) == &second);
    check(pointer == &third);
    check(vc_native_atomic_ptr_compare_exchange(&pointer, &first, &third, VC_NATIVE_MEMORY_ORDER_ACQ_REL) == &third);
    check(pointer == &first);
    check(vc_native_atomic_ptr_compare_exchange(&pointer, &second, &third, VC_NATIVE_MEMORY_ORDER_SEQ_CST) == &first);
    check(pointer == &first);

    IncrementState increment = {0, 25000};
    pthread_t workers[4];
    for (size_t i = 0; i < 4; i++)
        check(pthread_create(&workers[i], NULL, increment_worker, &increment) == 0);
    for (size_t i = 0; i < 4; i++)
        check(pthread_join(workers[i], NULL) == 0);
    check(vc_native_atomic_i32_load(&increment.value, VC_NATIVE_MEMORY_ORDER_SEQ_CST) == 100000);

    PublishState publish = {0, 0};
    pthread_t publisher;
    check(pthread_create(&publisher, NULL, publish_worker, &publish) == 0);
    while (vc_native_atomic_i32_load(&publish.ready, VC_NATIVE_MEMORY_ORDER_ACQUIRE) == 0)
    {
    }
    check(publish.payload == 1729);
    check(pthread_join(publisher, NULL) == 0);

    return failures == 0 ? 0 : 1;
}
