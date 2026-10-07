#include "host_process.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

static bool fail(char *error, size_t size, const char *format, ...)
{
    if (error != NULL && size != 0)
    {
        va_list args;
        va_start(args, format);
        vsnprintf(error, size, format, args);
        va_end(args);
    }
    return false;
}

char *vc_host_process_windows_command_line(char *const argv[])
{
    if (argv == NULL || argv[0] == NULL || argv[0][0] == '\0' ||
        strchr(argv[0], '"') != NULL)
        return NULL;
    size_t capacity = 1;
    for (size_t i = 0; argv[i] != NULL; ++i)
    {
        const size_t length = strlen(argv[i]);
        if (capacity > SIZE_MAX - 3 || length > (SIZE_MAX - capacity - 3) / 2)
            return NULL;
        capacity += length * 2 + 3;
    }
    char *line = malloc(capacity);
    if (line == NULL)
        return NULL;
    char *out = line;
    for (size_t i = 0; argv[i] != NULL; ++i)
    {
        if (i != 0)
            *out++ = ' ';
        *out++ = '"';
        /* The CRT parses argv[0] specially: backslashes are literal. */
        if (i == 0)
        {
            const size_t length = strlen(argv[i]);
            memcpy(out, argv[i], length);
            out += length;
        }
        else
        {
            const char *in = argv[i];
            while (*in != '\0')
            {
                size_t slashes = 0;
                while (*in == '\\')
                {
                    ++slashes;
                    ++in;
                }
                size_t copies = slashes;
                if (*in == '"' || *in == '\0')
                    copies *= 2;
                while (copies-- != 0)
                    *out++ = '\\';
                if (*in == '\0')
                    break;
                if (*in == '"')
                    *out++ = '\\';
                *out++ = *in++;
            }
        }
        *out++ = '"';
    }
    *out = '\0';
    return line;
}

bool vc_host_process_run_status(char *const argv[], VcHostProcessResult *result,
                                char *error, size_t error_size)
{
    if (argv == NULL || argv[0] == NULL || argv[0][0] == '\0' || result == NULL)
        return fail(error, error_size, "invalid host process arguments");
    memset(result, 0, sizeof(*result));
    result->termination = VC_HOST_PROCESS_EXITED;
#ifdef _WIN32
    char *line = vc_host_process_windows_command_line(argv);
    if (line == NULL)
        return fail(error, error_size, "could not encode process command line");
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, line, -1, NULL, 0);
    if (length == 0 || length > 32767)
    {
        free(line);
        return fail(error, error_size, "invalid UTF-8 or oversized process command line");
    }
    wchar_t *wide = malloc((size_t)length * sizeof(*wide));
    if (wide == NULL)
    {
        free(line);
        return fail(error, error_size, "could not allocate process command line");
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, line, -1, wide, length) == 0)
    {
        free(wide);
        free(line);
        return fail(error, error_size, "could not convert process command line to UTF-16");
    }
    free(line);
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    memset(&startup, 0, sizeof(startup));
    memset(&process, 0, sizeof(process));
    startup.cb = sizeof(startup);
    /* Quoted argv[0] prevents ambiguous space-containing executable names.
     * NULL application name retains native executable search and .exe lookup.
     * No cmd.exe / batch wrapper: CC and AR each name one executable. */
    const BOOL created = CreateProcessW(NULL, wide, NULL, NULL, TRUE, 0,
                                       NULL, NULL, &startup, &process);
    const DWORD launch_error = created ? 0 : GetLastError();
    free(wide);
    if (!created)
        return fail(error, error_size, "could not execute '%s': Windows error %lu",
                    argv[0], (unsigned long)launch_error);
    CloseHandle(process.hThread);
    const DWORD waited = WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 0;
    const BOOL got_code = waited == WAIT_OBJECT_0 && GetExitCodeProcess(process.hProcess, &code);
    const DWORD wait_error = got_code ? 0 : GetLastError();
    CloseHandle(process.hProcess);
    if (!got_code)
        return fail(error, error_size, "could not wait for process: Windows error %lu",
                    (unsigned long)wait_error);
    result->exit_code = (unsigned long)code;
    return true;
#else
    /* A close-on-exec pipe separates exec failure from a real child exit 127.
     * Child-side operations after fork are async-signal-safe. */
    int channel[2];
    if (pipe(channel) != 0)
        return fail(error, error_size, "could not create process error pipe: %s", strerror(errno));
    if (fcntl(channel[1], F_SETFD, FD_CLOEXEC) < 0)
    {
        const int saved = errno;
        close(channel[0]);
        close(channel[1]);
        return fail(error, error_size, "could not configure process error pipe: %s", strerror(saved));
    }
    const pid_t child = fork();
    if (child < 0)
    {
        const int saved = errno;
        close(channel[0]);
        close(channel[1]);
        return fail(error, error_size, "could not create process: %s", strerror(saved));
    }
    if (child == 0)
    {
        close(channel[0]);
        execvp(argv[0], argv);
        const int saved = errno;
        ssize_t written;
        do { written = write(channel[1], &saved, sizeof(saved)); } while (written < 0 && errno == EINTR);
        _exit(127);
    }
    close(channel[1]);
    int launch_error = 0;
    ssize_t received;
    do { received = read(channel[0], &launch_error, sizeof(launch_error)); } while (received < 0 && errno == EINTR);
    const int read_error = errno;
    close(channel[0]);
    int status = 0;
    pid_t waited;
    do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
    if (waited < 0)
        return fail(error, error_size, "could not wait for process: %s", strerror(errno));
    if (received < 0)
        return fail(error, error_size, "could not read process launch status: %s", strerror(read_error));
    if (received != 0)
        return fail(error, error_size, "could not execute '%s': %s", argv[0], strerror(launch_error));
    if (WIFEXITED(status))
    {
        result->exit_code = (unsigned long)WEXITSTATUS(status);
        return true;
    }
    if (WIFSIGNALED(status))
    {
        const int signal = WTERMSIG(status);
        const char *name = "unknown signal";
        switch (signal)
        {
            case SIGSEGV: name = "SIGSEGV (invalid memory access)"; break;
            case SIGILL: name = "SIGILL (illegal instruction)"; break;
            case SIGABRT: name = "SIGABRT (abort)"; break;
            case SIGFPE: name = "SIGFPE (arithmetic fault)"; break;
            case SIGTERM: name = "SIGTERM (termination request)"; break;
            case SIGINT: name = "SIGINT (interrupt)"; break;
            default: break;
        }
        result->termination = VC_HOST_PROCESS_SIGNALED;
        result->signal_number = signal;
        result->signal_name = name;
        return true;
    }
    return fail(error, error_size, "process ended unexpectedly");
#endif
}

bool vc_host_process_run(char *const argv[], unsigned long *exit_code,
                         char *error, size_t error_size)
{
    if (exit_code == NULL)
        return fail(error, error_size, "invalid host process arguments");
    VcHostProcessResult result;
    if (!vc_host_process_run_status(argv, &result, error, error_size))
        return false;
    if (result.termination == VC_HOST_PROCESS_SIGNALED)
        return fail(error, error_size, "process terminated by signal %d: %s",
                    result.signal_number, result.signal_name != NULL ? result.signal_name : "unknown signal");
    *exit_code = result.exit_code;
    return true;
}
