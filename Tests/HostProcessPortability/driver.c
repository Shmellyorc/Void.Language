#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "host_process.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static int handle_count(const char *stage, int trace, DWORD *count)
{
    if (!GetProcessHandleCount(GetCurrentProcess(), count))
    {
        fprintf(stderr, "repeat-spawn: %s GetProcessHandleCount failed: %lu\n",
                stage, (unsigned long)GetLastError());
        return 0;
    }
    if (trace)
        fprintf(stderr, "repeat-spawn: %s handles=%lu\n", stage, (unsigned long)*count);
    return 1;
}
#else
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
static void interrupted(int signal_number) { (void)signal_number; }
#endif

static int driver_main(int argc, char **argv)
{
    if (strstr(argv[0], "failure compiler") != NULL) return 127;
    if (argc < 2) return 2;
    if (strcmp(argv[1], "encode") == 0)
    {
        char *line = vc_host_process_windows_command_line(argv + 2);
        if (line == NULL) return 3;
        puts(line);
        free(line);
        return 0;
    }
    if (strcmp(argv[1], "child") == 0)
    {
        for (int i = 2; i < argc; ++i)
            printf("%zu:%s\n", strlen(argv[i]), argv[i]);
        return 0;
    }
    if (strcmp(argv[1], "io") == 0)
    {
        char input[128];
        const char *environment = getenv("VOID_HOST_PROCESS_TEST");
        FILE *marker = fopen("cwd-marker", "rb");
        if (marker == NULL || environment == NULL || fgets(input, sizeof(input), stdin) == NULL) return 2;
        fclose(marker);
        printf("%s:%s", environment, input);
        fputs("inherited stderr\n", stderr);
        return 0;
    }
    if (strcmp(argv[1], "exit") == 0)
    {
        if (argc != 3) return 2;
#ifdef _WIN32
        ExitProcess((UINT)strtoul(argv[2], NULL, 10));
#else
        return (int)strtoul(argv[2], NULL, 10);
#endif
    }
    if (strcmp(argv[1], "delay") == 0)
    {
#ifdef _WIN32
        Sleep(1200);
#else
        sleep(2);
#endif
        puts("waited");
        return 0;
    }
#ifndef _WIN32
    if (strcmp(argv[1], "signal") == 0)
    {
        raise(SIGTERM);
        return 2;
    }
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = interrupted;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGALRM, &action, NULL) != 0) return 2;
    alarm(1);
#endif
    if (strcmp(argv[1], "repeat-spawn") == 0
#ifdef _WIN32
        || strcmp(argv[1], "repeat-spawn-leak") == 0
#endif
       )
    {
        if (argc != 3) return 2;
        char *child[] = {argv[2], "exit", "0", NULL};
        char *missing[] = {"voidc-330-absent-process-9c83", NULL};
#ifdef _WIN32
        DWORD before = 0, after = 0, first_pair = 0;
        const int trace = getenv("VOID_HOST_PROCESS_TRACE") != NULL;
        const int inject_leak = strcmp(argv[1], "repeat-spawn-leak") == 0;
        if (!handle_count("initial", trace, &before)) return 2;
        /* Native CreateProcessW lazily opens process-lifetime Windows handles.
         * Check that seven more pairs stabilize, then measure all 64 pairs.
         * Do not subtract an allowance or leak during the warm-up control. */
        const int first_iteration = -8;
#else
        int before = 0, after = 0;
        const int first_iteration = 0;
        for (int fd = 0; fd < 1024; ++fd)
            if (fcntl(fd, F_GETFD) != -1) ++before;
#endif
        for (int i = first_iteration; i < 64; ++i)
        {
            unsigned long result = 999;
            char message[128] = "";
            if (!vc_host_process_run(child, &result, message, sizeof(message)) || result != 0)
            {
                fprintf(stderr, "repeat-spawn: child iteration=%d exit=%lu: %s\n",
                        i < 0 ? i + 8 : i, result, message);
                return 2;
            }
            result = 999;
#ifdef _WIN32
            if (i == -8 && !handle_count("first child", trace, &after)) return 2;
#endif
            if (vc_host_process_run(missing, &result, message, sizeof(message)) ||
                result != 999 || strstr(message, "could not execute") == NULL)
            {
                fprintf(stderr, "repeat-spawn: missing executable iteration=%d exit=%lu: %s\n",
                        i < 0 ? i + 8 : i, result, message);
                return 2;
            }
#ifdef _WIN32
            if (i == -8 && !handle_count("first missing executable", trace, &first_pair)) return 2;
            if (i == -1)
            {
                if (!handle_count("warm-up", trace, &before)) return 2;
                if (before != first_pair)
                {
                    fprintf(stderr, "repeat-spawn: warm-up handle count mismatch before=%lu after=%lu\n",
                            (unsigned long)first_pair, (unsigned long)before);
                    return 2;
                }
            }
            /* Negative control: a real leaked kernel handle per iteration. */
            if (inject_leak && i >= 0 && CreateEventW(NULL, FALSE, FALSE, NULL) == NULL)
            {
                fprintf(stderr, "repeat-spawn: leak control CreateEventW failed: %lu\n",
                        (unsigned long)GetLastError());
                return 2;
            }
#endif
        }
#ifdef _WIN32
        if (!handle_count("repeated loop", trace, &after)) return 2;
        if (trace)
            fprintf(stderr, "repeat-spawn: before=%lu after=%lu\n",
                    (unsigned long)before, (unsigned long)after);
        if (after != before)
        {
            fprintf(stderr, "repeat-spawn: handle count mismatch before=%lu after=%lu\n",
                    (unsigned long)before, (unsigned long)after);
            return 2;
        }
#else
        for (int fd = 0; fd < 1024; ++fd)
            if (fcntl(fd, F_GETFD) != -1) ++after;
        if (before != after)
        {
            fprintf(stderr, "repeat-spawn: descriptor count before=%d after=%d\n", before, after);
            return 2;
        }
#endif
        return 0;
    }

#ifdef _WIN32
    if (strcmp(argv[1], "oversized") == 0 || strcmp(argv[1], "invalid-utf8") == 0)
    {
        char huge[32768];
        memset(huge, 'x', sizeof(huge) - 1);
        huge[sizeof(huge) - 1] = '\0';
        char invalid[] = {(char)0xc0, (char)0xaf, 0};
        char *arguments[] = {argv[0], strcmp(argv[1], "oversized") == 0 ? huge : invalid, NULL};
        unsigned long result = 999;
        char message[128];
        if (vc_host_process_run(arguments, &result, message, sizeof(message)) || result != 999) return 2;
        puts(message);
        return 0;
    }
#endif
    unsigned long code = 999;
    char error[1024] = "";
    char **arguments = strcmp(argv[1], "invalid") == 0 ? NULL : argv + 2;
    const bool success = vc_host_process_run(arguments, &code, error, sizeof(error));
    printf("RESULT:%d:%lu:%s\n", success, code, error);
    return 0;
}

#ifdef _WIN32
/* Exercise the UTF-8 API from a real Unicode argv without depending on ACP. */
int wmain(int argc, wchar_t **wide)
{
    char **argv = calloc((size_t)argc + 1, sizeof(*argv));
    if (argv == NULL) return 2;
    for (int i = 0; i < argc; ++i)
    {
        int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[i], -1, NULL, 0, NULL, NULL);
        if (size == 0 || (argv[i] = malloc((size_t)size)) == NULL) return 2;
        if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[i], -1, argv[i], size, NULL, NULL) == 0) return 2;
    }
    int result = driver_main(argc, argv);
    for (int i = 0; i < argc; ++i) free(argv[i]);
    free(argv);
    return result;
}
#else
int main(int argc, char **argv) { return driver_main(argc, argv); }
#endif
