#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <string>
#include <string_view>

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 2;
    const std::wstring path = argv[1];
    if (path.size() < 4 || path[1] != L':' || path[2] != L'\\') return 2;
    auto parent = path.substr(0,path.find_last_of(L'\\'));
    HANDLE file = CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
                              FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    BY_HANDLE_FILE_INFORMATION held{};
    if (file == INVALID_HANDLE_VALUE || !GetFileInformationByHandle(file,&held) ||
        held.nNumberOfLinks != 1 || (held.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) return 3;
    wchar_t expected[1024], actual[1024], windows[1024];
    DWORD len = GetFinalPathNameByHandleW(file,expected,1024,VOLUME_NAME_NT);
    UINT root_size = GetWindowsDirectoryW(windows,1024);
    if (!len || len >= 1024 || !root_size || root_size >= 1024) return 4;
    std::wstring environment = L"SystemRoot=" + std::wstring(windows);
    environment.push_back(L'\0');
    environment.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION child{};
    if (!CreateProcessW(path.c_str(),nullptr,nullptr,nullptr,FALSE,CREATE_SUSPENDED|CREATE_UNICODE_ENVIRONMENT,
                        environment.data(),parent.c_str(),&startup,&child)) {
        std::wprintf(L"CreateProcessW failed %lu\n",GetLastError());
        return 5;
    }
    DWORD size = 1024;
    const bool matching = QueryFullProcessImageNameW(child.hProcess,PROCESS_NAME_NATIVE,actual,&size) &&
        std::wstring_view(actual,size) == expected;
    std::wprintf(L"child image matches held executable path before resume=%d\n",matching);
    DWORD result = 0;
    const bool complete = matching && ResumeThread(child.hThread) != static_cast<DWORD>(-1) &&
        WaitForSingleObject(child.hProcess,10000) == WAIT_OBJECT_0 && GetExitCodeProcess(child.hProcess,&result);
    if (!complete) {
        TerminateProcess(child.hProcess,6);
        WaitForSingleObject(child.hProcess,10000);
    }
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
    CloseHandle(file);
    if (!complete) return 6;
    std::wprintf(L"child first-entry observation=%lu\n",result);
    return 0;
}
