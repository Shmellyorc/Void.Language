#include <stdint.h>

static int32_t callback_entries;
static int32_t after_steps;
static int32_t observed;

void voidc178_reset(void)
{
    callback_entries = 0;
    after_steps = 0;
    observed = 0;
}

int32_t voidc178_invoke(int32_t (*callback)(int32_t))
{
    int32_t value = callback(1);
    callback_entries++;
    after_steps++;
    observed = value;
    return value + 100;
}

int32_t voidc178_callback_entries(void) { return callback_entries; }
int32_t voidc178_after_steps(void) { return after_steps; }
int32_t voidc178_observed(void) { return observed; }
