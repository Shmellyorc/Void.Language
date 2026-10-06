#include <stdint.h>

static int32_t callback_entries;
static int32_t after_steps;
static int32_t observed_sum;

void voidc168_reset(void)
{
    callback_entries = 0;
    after_steps = 0;
    observed_sum = 0;
}

int32_t voidc168_invoke_twice(int32_t (*callback)(int32_t))
{
    int32_t first = callback(1);
    callback_entries++;
    after_steps++;
    int32_t second = callback(2);
    callback_entries++;
    after_steps++;
    observed_sum = first + second;
    return observed_sum + 100;
}

int32_t voidc168_callback_entries(void) { return callback_entries; }
int32_t voidc168_after_steps(void) { return after_steps; }
int32_t voidc168_observed_sum(void) { return observed_sum; }
