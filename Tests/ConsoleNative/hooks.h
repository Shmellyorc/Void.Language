#include <stdio.h>
#include <windows.h>
size_t void_test_console_fwrite(const void *, size_t, size_t, FILE *);
BOOL WINAPI void_test_console_title(LPCWSTR);
BOOL WINAPI void_test_console_position(HANDLE, PCONSOLE_SCREEN_BUFFER_INFO);
#define fwrite void_test_console_fwrite
#define SetConsoleTitleW void_test_console_title
#define GetConsoleScreenBufferInfo void_test_console_position
