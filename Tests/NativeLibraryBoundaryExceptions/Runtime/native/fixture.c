#include <stdint.h>

static int32_t callback_entries;
static int32_t after_steps;
static int32_t observed_sum;
static int32_t ref_observed;

void voidc119_reset(void)
{
    callback_entries = 0;
    after_steps = 0;
    observed_sum = 0;
    ref_observed = 0;
}

int32_t voidc119_invoke_twice(int32_t (*callback)(int32_t))
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

int32_t voidc119_invoke_once(int32_t value, int32_t (*callback)(int32_t))
{
    int32_t result = callback(value);
    callback_entries++;
    after_steps++;
    observed_sum = result;
    return result + 10;
}

int32_t voidc119_ref_then_continue(int32_t value, void (*callback)(int32_t *))
{
    callback(&value);
    callback_entries++;
    ref_observed = value;
    after_steps++;
    return value + 1;
}

int32_t voidc119_callback_entries(void) { return callback_entries; }
int32_t voidc119_after_steps(void) { return after_steps; }
int32_t voidc119_observed_sum(void) { return observed_sum; }
int32_t voidc119_ref_observed(void) { return ref_observed; }
int32_t voidc119_plain_add(int32_t left, int32_t right) { return left + right; }
