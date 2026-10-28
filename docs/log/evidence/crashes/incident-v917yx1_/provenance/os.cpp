#include "void/os.hpp"
#include "void/detail/os_integer.hpp"
#include "void/path.hpp"
#include "void/utf8.hpp"
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <new>
#include <utility>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#ifdef __linux__
#include <pwd.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

namespace vd::os {
Result native_error(std::int64_t code, std::string domain, ErrorKind kind) {
    Result result; result.kind = kind; result.os_code = code;
    result.native_domain = std::move(domain); return result;
}
namespace {
Result fail(ErrorKind kind) { Result r; r.kind = kind; return r; }
#ifndef _WIN32
Result posix_error(int code) {
    if (code == ENOMEM) throw std::bad_alloc();
    return native_error(code, "errno", code == EACCES || code == EPERM ? ErrorKind::permission_denied :
                        code == ENOENT || code == ESRCH ? ErrorKind::unavailable :
                        code == ENOSYS || code == ENOTSUP ? ErrorKind::unsupported : ErrorKind::system_failure);
}
#endif
// All growing native buffers have a finite attempt bound and checked heap extents.
std::size_t grow(std::size_t size) {
    if (size > std::numeric_limits<std::size_t>::max() / 2) throw std::length_error("OS buffer extent");
    return size * 2;
}
#ifdef _WIN32
Result win_error(DWORD code) {
    if (code == ERROR_NOT_ENOUGH_MEMORY || code == ERROR_OUTOFMEMORY) throw std::bad_alloc();
    return native_error(static_cast<std::int64_t>(code), "win32",
        code == ERROR_ACCESS_DENIED ? ErrorKind::permission_denied :
        code == ERROR_NOT_SUPPORTED || code == ERROR_CALL_NOT_IMPLEMENTED ? ErrorKind::unsupported : ErrorKind::system_failure);
}
Result wide_text(std::wstring_view value, bool path = false, bool executable = false) {
    if (value.size() > INT_MAX) throw std::length_error("OS UTF-16 extent");
    if (value.empty()) return path ? fail(ErrorKind::unavailable) : Result{};
    int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                   static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (!count) { auto code = GetLastError(); auto r = win_error(code);
        if (code == ERROR_NO_UNICODE_TRANSLATION) r.kind = ErrorKind::invalid_utf8; return r; }
    std::string text(static_cast<std::size_t>(count), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                            text.data(), count, nullptr, nullptr)) return win_error(GetLastError());
    if (path) return checked_path(std::move(text), executable);
    Result r; r.text = std::move(text); return r;
}
Result known_folder(REFKNOWNFOLDERID id) {
    struct Apartment {
        HRESULT status = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ~Apartment() { if (SUCCEEDED(status)) CoUninitialize(); }
    } apartment;
    auto hresult = [](HRESULT code) {
        if (code == E_OUTOFMEMORY || (HRESULT_FACILITY(code) == FACILITY_WIN32 &&
            (HRESULT_CODE(code) == ERROR_NOT_ENOUGH_MEMORY || HRESULT_CODE(code) == ERROR_OUTOFMEMORY))) throw std::bad_alloc();
        auto kind = ErrorKind::system_failure;
        if (code == E_ACCESSDENIED || (HRESULT_FACILITY(code) == FACILITY_WIN32 && HRESULT_CODE(code) == ERROR_ACCESS_DENIED)) kind = ErrorKind::permission_denied;
        return native_error(static_cast<std::uint32_t>(code), "hresult", kind);
    };
    if (FAILED(apartment.status) && apartment.status != RPC_E_CHANGED_MODE) return hresult(apartment.status);
    struct Buffer { PWSTR data = nullptr; ~Buffer() { CoTaskMemFree(data); } } buffer;
    auto status = SHGetKnownFolderPath(id, KF_FLAG_DONT_VERIFY, nullptr, &buffer.data);
    if (FAILED(status)) return hresult(status);
    if (!buffer.data) return fail(ErrorKind::unavailable);
    return wide_text(buffer.data, true);
}
Result environment(std::string_view name) {
    if (name.size() > INT_MAX) throw std::length_error("OS environment name extent");
    auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(), static_cast<int>(name.size()), nullptr, 0);
    if (!count) return win_error(GetLastError());
    std::wstring key(static_cast<std::size_t>(count), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(), static_cast<int>(name.size()), key.data(), count)) return win_error(GetLastError());
    std::vector<wchar_t> buffer(256);
    DWORD last = ERROR_SUCCESS;
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        SetLastError(ERROR_SUCCESS);
        auto length = GetEnvironmentVariableW(key.c_str(), buffer.data(), static_cast<DWORD>(buffer.size()));
        last = GetLastError();
        if (!length) {
            if (last == ERROR_SUCCESS) return Result{};
            auto r = win_error(last); if (last == ERROR_ENVVAR_NOT_FOUND) r.kind = ErrorKind::not_found; return r;
        }
        if (length < buffer.size()) return wide_text({buffer.data(), length});
        buffer.resize(static_cast<std::size_t>(length));
    }
    return last ? native_error(last, "win32", ErrorKind::unavailable) : fail(ErrorKind::unavailable);
}
#else
Result environment(std::string_view name) {
    std::string key(name);
    const char* value = std::getenv(key.c_str());
    if (!value) return fail(ErrorKind::not_found);
    Result result; result.text = value; // Copy the borrowed pointer before any managed allocation.
    if (!valid_utf8(result.text)) return fail(ErrorKind::invalid_utf8);
    return result;
}
#endif
#ifdef __linux__
Result home() {
    if (const char* value = std::getenv("HOME"); value && *value) return checked_path(value);
    std::vector<char> buffer(1024);
    for (unsigned attempt = 0; attempt < 20; ++attempt) {
        passwd record{}; passwd* found = nullptr;
        auto code = getpwuid_r(getuid(), &record, buffer.data(), buffer.size(), &found);
        if (code == ERANGE) { buffer.resize(grow(buffer.size())); continue; }
        if (code) return posix_error(code); // getpwuid_r returns the diagnostic, not errno.
        if (!found || !found->pw_dir || !*found->pw_dir) return fail(ErrorKind::unavailable);
        return checked_path(found->pw_dir);
    }
    return native_error(ERANGE, "errno", ErrorKind::unavailable);
}
Result root(Operation operation) {
    constexpr std::array names = {"XDG_DATA_HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME", "XDG_STATE_HOME"};
    constexpr std::array defaults = {".local/share", ".config", ".cache", ".local/state"};
    auto index = static_cast<unsigned>(operation);
    // Relative overrides are ignored even when their bytes are malformed.
    if (const char* value = std::getenv(names[index]); value && *value == '/') return checked_path(value);
    auto result = home();
    if (result.kind != ErrorKind::success) return result;
    result.text = path_utf8(utf8_path(result.text) / defaults[index]);
    return result;
}
#endif
Result executable() {
#ifdef _WIN32
    std::vector<wchar_t> buffer(256);
    for (unsigned attempt = 0; attempt < 24; ++attempt) {
        if (buffer.size() > MAXDWORD) throw std::length_error("OS image extent");
        SetLastError(ERROR_SUCCESS);
        auto count = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (!count) return win_error(GetLastError());
        if (count < buffer.size()) return wide_text({buffer.data(), count}, true, true);
        buffer.resize(grow(buffer.size()));
    }
    return fail(ErrorKind::unavailable);
#elif defined(__linux__)
    std::vector<char> buffer(256);
    for (unsigned attempt = 0; attempt < 24; ++attempt) {
        auto count = readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (count < 0) return posix_error(errno);
        if (static_cast<std::size_t>(count) < buffer.size()) return checked_path(std::string(buffer.data(), count), true);
        buffer.resize(grow(buffer.size()));
    }
    return fail(ErrorKind::unavailable);
#elif defined(__APPLE__)
    std::vector<char> buffer(256);
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        if (buffer.size() > UINT32_MAX) throw std::length_error("OS image extent");
        auto size = static_cast<std::uint32_t>(buffer.size());
        if (_NSGetExecutablePath(buffer.data(), &size) == 0) return checked_path(buffer.data(), true);
        if (size <= buffer.size()) return fail(ErrorKind::unavailable);
        buffer.resize(size);
    }
    return fail(ErrorKind::unavailable);
#else
    return fail(ErrorKind::unsupported);
#endif
}
}
Result checked_path(std::string text, bool executable) {
    if (text.empty()) return fail(ErrorKind::unavailable);
    if (text.find('\0') != std::string::npos) return fail(ErrorKind::invalid_path);
#ifndef _WIN32
    if (text.front() != '/') return fail(executable ? ErrorKind::unavailable : ErrorKind::invalid_path);
#endif
    if (!valid_utf8(text)) return fail(ErrorKind::invalid_utf8);
    if (!utf8_path(text).is_absolute()) return fail(executable ? ErrorKind::unavailable : ErrorKind::invalid_path);
    Result result; result.text = std::move(text); return result;
}
Result query(Operation operation, std::string_view input) {
    if (static_cast<unsigned>(operation) >= operations.size()) throw std::invalid_argument("Invalid OS operation");
    if (operation == Operation::environment) {
        if (input.empty() || input.find('\0') != std::string_view::npos || input.find('=') != std::string_view::npos) return fail(ErrorKind::invalid_argument);
        if (!valid_utf8(input)) return fail(ErrorKind::invalid_utf8);
        return environment(input);
    }
    if (!input.empty()) throw std::invalid_argument("Unexpected OS input");
    Result result;
    if (operation == Operation::platform) {
#ifdef _WIN32
        result.text = "windows";
#elif defined(__APPLE__)
        result.text = "macos";
#elif defined(__linux__)
        result.text = "linux";
#else
        throw std::logic_error("Unsupported OS target");
#endif
        return result;
    }
    if (operation == Operation::architecture) {
#if defined(__x86_64__) || defined(_M_X64)
        result.text = "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
        result.text = "aarch64";
#elif defined(__i386__) || defined(_M_IX86)
        result.text = "x86";
#elif defined(__arm__) || defined(_M_ARM)
        result.text = "arm";
#else
        result.text = "unknown";
#endif
        return result;
    }
    if (operation == Operation::process_id) {
#ifdef _WIN32
        result.number = detail::process_id_from_dword(GetCurrentProcessId());
#else
        result.number = static_cast<std::int64_t>(getpid());
#endif
        return result;
    }
    if (operation == Operation::executable) return executable();
#ifdef __APPLE__
    return apple_query(operation);
#else
    if (operation == Operation::bundle || operation == Operation::resources) return result;
#ifdef _WIN32
    switch (operation) {
    case Operation::home: return known_folder(FOLDERID_Profile);
    case Operation::data_root: case Operation::cache_root: case Operation::state_root: return known_folder(FOLDERID_LocalAppData);
    case Operation::config_root: return known_folder(FOLDERID_RoamingAppData);
    case Operation::temp: {
        std::vector<wchar_t> buffer(256);
        for (unsigned attempt = 0; attempt < 8; ++attempt) {
            auto count = GetTempPathW(static_cast<DWORD>(buffer.size()), buffer.data());
            if (!count) return win_error(GetLastError());
            if (count < buffer.size()) return wide_text({buffer.data(), count}, true);
            buffer.resize(static_cast<std::size_t>(count) + 1);
        }
        return fail(ErrorKind::unavailable);
    }
    default: return fail(ErrorKind::unsupported);
    }
#elif defined(__linux__)
    if (operation <= Operation::state_root) return root(operation);
    if (operation == Operation::home) return home();
    if (operation == Operation::temp) {
        auto* value = std::getenv("TMPDIR");
        return checked_path(value && *value ? value : "/tmp");
    }
    return fail(ErrorKind::unsupported);
#else
    return fail(ErrorKind::unsupported);
#endif
#endif
}
}
