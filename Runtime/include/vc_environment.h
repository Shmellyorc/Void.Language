#ifndef VC_ENVIRONMENT_H
#define VC_ENVIRONMENT_H

#include <stdbool.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>

static inline BOOL CALLBACK vc_native_environment_initialize_outputs(
    PINIT_ONCE once, PVOID parameter, PVOID *context)
{
    (void)once;
    (void)parameter;
    (void)context;
    /* VOID supplies newline bytes; the CRT must not translate them again. */
    return _setmode(_fileno(stdout), _O_BINARY) != -1 &&
           _setmode(_fileno(stderr), _O_BINARY) != -1;
}
#endif

/* Shared by executable/library startup and standard-stream byte writers.
 * Windows initialization is synchronized and runs once per runtime image.
 * POSIX standard streams already preserve bytes and need no setup. */
static inline bool vc_native_environment_init(void)
{
#ifdef _WIN32
    static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
    return InitOnceExecuteOnce(&once, vc_native_environment_initialize_outputs,
                               NULL, NULL) != 0;
#else
    return true;
#endif
}

#endif
