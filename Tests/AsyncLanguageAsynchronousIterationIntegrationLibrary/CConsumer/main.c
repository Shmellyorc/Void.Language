#include <stdbool.h>
#include <stdio.h>

extern int void220_async_stream_sum(int value);

int main(void)
{
    puts(void220_async_stream_sum(109) == 220 ? "True" : "False");
    return 0;
}
