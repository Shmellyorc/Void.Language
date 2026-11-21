// Native Windows regression fixture. Source review only; not executed here.
#include <windows.h>
static LONG WINAPI recover(EXCEPTION_POINTERS* error) {
    return error->ExceptionRecord->ExceptionCode == 0xE0420090u
        ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
}
int main() {
    SetUnhandledExceptionFilter(recover);
    RaiseException(0xE0420090u, 0, 0, nullptr); // continuable application exception
    return 17;
}
