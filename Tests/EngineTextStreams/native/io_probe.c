#include "hooks_redirect.h"
#include <assert.h>
#include <errno.h>

int main(void)
{
#ifdef _WIN32
    HANDLE file = CreateFileW(L"hook-probe", GENERIC_READ | GENERIC_WRITE, 0,
        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (getenv("VOID_TEST_DENIED") != NULL) {
        assert(file == INVALID_HANDLE_VALUE && GetLastError() == ERROR_ACCESS_DENIED);
        return 0;
    }
    assert(file != INVALID_HANDLE_VALUE);
    DWORD actual = 0;
    assert(WriteFile(file, "abcde", 5, &actual, NULL) && actual == 3);
    assert(SetFilePointer(file, 0, NULL, FILE_BEGIN) == 0);
    char bytes[5] = {0};
    assert(ReadFile(file, bytes, 5, &actual, NULL) && actual == 2);
    assert(bytes[0] == 'a' && bytes[1] == 'b');
    assert(CloseHandle(file));
    assert(DeleteFileW(L"hook-probe"));
#else
    int file = open("hook-probe", O_CREAT | O_TRUNC | O_RDWR, 0600);
    if (getenv("VOID_TEST_DENIED") != NULL) {
        assert(file == -1 && errno == EACCES);
        return 0;
    }
    assert(file >= 0);
    assert(write(file, "abcde", 5) == 3);
    assert(lseek(file, 0, SEEK_SET) == 0);
    char bytes[5] = {0};
    assert(read(file, bytes, 5) == 2);
    assert(bytes[0] == 'a' && bytes[1] == 'b');
    assert(close(file) == 0);
    assert(unlink("hook-probe") == 0);
#endif
    return 0;
}
