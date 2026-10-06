#include <stdint.h>
#include <stdio.h>

extern int32_t void119_library_handled(void);

int main(void)
{
    puts(void119_library_handled() == 42 ? "True" : "False");
    return 0;
}
