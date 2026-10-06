/* Observe actual generated output and native calls without replacing them. */
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
static void trace(const void *bytes, size_t length)
{
    const char *path = getenv("VOID_TEST_CONSOLE_TRACE");
    if (path == NULL) abort();
    FILE *file = fopen(path, "ab");
    if (file == NULL) abort();
    if (fwrite(bytes, 1, length, file) != length || fclose(file) != 0) abort();
}
size_t void_test_console_fwrite(const void *bytes, size_t size, size_t count, FILE *file)
{
    size_t written = fwrite(bytes, size, count, file);
    if (file == stdout) trace(bytes, size * written);
    return written;
}
BOOL WINAPI void_test_console_title(LPCWSTR value)
{
    BOOL ok = SetConsoleTitleW(value);
    if (ok) {
        WCHAR actual[256];
        DWORD length = GetConsoleTitleW(actual, 256);
        if (length == 0 || wcscmp(value, actual) != 0) abort();
        char text[1024];
        int bytes = WideCharToMultiByte(CP_UTF8, 0, actual, (int)length, text, 1024, NULL, NULL);
        if (bytes <= 0) abort();
        trace("TITLE:", 6);
        trace(text, (size_t)bytes);
        trace("\n", 1);
    }
    return ok;
}
BOOL WINAPI void_test_console_position(HANDLE file, PCONSOLE_SCREEN_BUFFER_INFO info)
{
    BOOL ok = GetConsoleScreenBufferInfo(file, info);
    if (ok) trace("QUERY_CURSOR\n", 13);
    return ok;
}
