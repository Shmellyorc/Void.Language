#include <assert.h>
#include <stdio.h>

#ifdef _WIN32
#include <io.h>
static int mode_changes;
static int counted_setmode(int descriptor, int mode)
{
    ++mode_changes;
    return _setmode(descriptor, mode);
}
#define _setmode counted_setmode
#endif
#include "vc_environment.h"

int main(void)
{
    for (int i = 0; i < 32; ++i)
        assert(vc_native_environment_init());
#ifdef _WIN32
    assert(mode_changes == 2); /* One initialization for stdout and stderr. */
#endif
    const char bytes[] = "shared\r\n";
    assert(fwrite(bytes, 1, sizeof(bytes) - 1, stdout) == sizeof(bytes) - 1);
    assert(fwrite(bytes, 1, sizeof(bytes) - 1, stderr) == sizeof(bytes) - 1);
    return 0;
}
