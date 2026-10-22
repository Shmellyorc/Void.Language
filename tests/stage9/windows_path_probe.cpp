// Native diagnostic control: build this source with and without longPathAware.
#include <filesystem>
#include <iostream>
#include <string>
#include <algorithm>
#define NOMINMAX
#include <windows.h>

namespace fs = std::filesystem;
static void report(const char* operation, const std::error_code& error) {
    // Format the actual system code in UTF-16, independently of the ANSI page.
    wchar_t buffer[2048]{};
    auto length = error.category() == std::system_category()
        ? FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, error.value(), 0, buffer, 2048, nullptr) : 0;
    auto message = length ? fs::path(std::wstring(buffer, length)).u8string()
                          : fs::path(error.message()).u8string();
    std::replace(message.begin(), message.end(), u8'\r', u8' ');
    std::replace(message.begin(), message.end(), u8'\n', u8' ');
    std::cout << operation << "\t" << error.value() << "\t" << error.category().name()
              << "\t" << std::string(message.begin(), message.end()) << '\n';
}
int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 2;
    std::error_code error;
    auto absolute = fs::absolute(fs::path(argv[1]), error);
    report("absolute", error);
    if (error) return 1;
    std::cout << "utf16_length\t" << absolute.native().size() << '\n';
    (void)fs::canonical(absolute, error);
    report("canonical", error);
    auto raw = CreateFileW(absolute.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    auto code = raw == INVALID_HANDLE_VALUE ? GetLastError() : ERROR_SUCCESS;
    report("CreateFileW", {static_cast<int>(code), std::system_category()});
    if (raw != INVALID_HANDLE_VALUE) {
        std::wstring final(512, L'\0');
        auto length = GetFinalPathNameByHandleW(raw, final.data(),
            static_cast<DWORD>(final.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        if (length >= final.size()) {
            final.resize(length + 1);
            length = GetFinalPathNameByHandleW(raw, final.data(),
                static_cast<DWORD>(final.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        }
        code = length ? ERROR_SUCCESS : GetLastError();
        report("GetFinalPathNameByHandleW", {static_cast<int>(code), std::system_category()});
        CloseHandle(raw);
    }
    // An experimental OS-call control only, never used as a compiler fallback.
    // Input is already absolute; change separators/prefix only, retaining aliases.
    auto native = absolute.make_preferred().native();
    if (native.starts_with(L"\\\\?\\")) {}
    else if (native.starts_with(L"\\\\")) native = L"\\\\?\\UNC\\" + native.substr(2);
    else native = L"\\\\?\\" + native;
    (void)fs::canonical(fs::path(native), error);
    report("extended_canonical", error);
    return error ? 1 : 0;
}
