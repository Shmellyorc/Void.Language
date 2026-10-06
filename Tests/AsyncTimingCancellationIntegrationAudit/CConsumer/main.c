#include <stdint.h>
#include <stdio.h>

extern int32_t void290_async_timing_cancellation_audit(int32_t value);

int main(void)
{
    puts(void290_async_timing_cancellation_audit(41) == 42 ? "True" : "False");
    return 0;
}
