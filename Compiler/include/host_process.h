#ifndef VC_HOST_PROCESS_H
#define VC_HOST_PROCESS_H

#include <stdbool.h>
#include <stddef.h>

/* argv is a NULL-terminated UTF-8 vector; no shell splitting or expansion.
 * Inherits cwd, environment and standard handles; waits synchronously.
 * Success means a normal child exit (including nonzero), not exit code zero.
 * Failure leaves exit_code untouched and writes a diagnostic when space permits.
 * unsigned long preserves all Windows DWORD exit codes on either host ABI. */
bool vc_host_process_run(char *const argv[], unsigned long *exit_code,
                         char *error, size_t error_size);

/* Allocated UTF-8 Windows CRT command line. Caller frees it. Available on
 * every host so the actual Windows encoder can be regression-tested on POSIX.
 * argv[0] is an executable name/path, not a shell command; quotes are invalid. */
char *vc_host_process_windows_command_line(char *const argv[]);

#endif
