#include <stdint.h>
#include <stdio.h>

extern int32_t void289_async_timing_cancellation_integration(int32_t value);

int main(void)
{
    puts(void289_async_timing_cancellation_integration(41) == 42 ? "True" : "False");
    return 0;
}
