#include <stdint.h>
#include <stdio.h>

extern int32_t void274_async_lambda_delegate_event(int32_t value);

int main(void)
{
    puts(void274_async_lambda_delegate_event(40) == 42 ? "True" : "False");
    return 0;
}
