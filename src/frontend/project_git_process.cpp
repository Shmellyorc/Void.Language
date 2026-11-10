#include "project_git_process.hpp"
#include "void/path.hpp"
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace vd::project {
namespace {
namespace fs = std::filesystem;
constexpr std::uintmax_t storage_limit = 96 * 1024 * 1024;
constexpr std::uintmax_t diagnostic_limit = 256 * 1024;
constexpr std::size_t entry_limit = 1024;

[[noreturn]] void storage_exceeded() {
    throw std::runtime_error("HTTPS Git transport exceeded its private storage limit.");
}

void check_storage(const fs::path& work) {
    std::uintmax_t bytes = 0;
    std::size_t entries = 0;
    for (const auto& entry : fs::recursive_directory_iterator(work)) {
        if (++entries > entry_limit) storage_exceeded();
        std::error_code error;
        const auto status = entry.symlink_status(error);
        // Git renames or unlinks temporary pack and lock files during fetch.
        if (error == std::errc::no_such_file_or_directory || fs::is_directory(status)) continue;
        if (error || !fs::is_regular_file(status))
            throw std::runtime_error("Cannot inspect private HTTPS Git workspace.");
        const auto size = entry.file_size(error);
        if (error == std::errc::no_such_file_or_directory) continue;
        if (error) throw std::runtime_error("Cannot measure private HTTPS Git workspace.");
        if (size > storage_limit - bytes ||
            (entry.path().filename() == "fetch-error" && size > diagnostic_limit)) storage_exceeded();
        bytes += size;
    }
}

#ifdef _WIN32
[[noreturn]] void process_error(const char* operation) {
    throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), operation);
}

struct Handle {
    HANDLE value;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};

struct ProcessAttributes {
    std::vector<unsigned char> storage;
    LPPROC_THREAD_ATTRIBUTE_LIST list = nullptr;
    ~ProcessAttributes() { if (list) DeleteProcThreadAttributeList(list); }
};

// Quote an argument using the Windows CRT backslash and quote rules.
void append_argument(std::wstring& command, std::wstring_view arg) {
    if (!command.empty()) command += L' ';
    command += L'"';
    std::size_t slashes = 0;
    for (const auto c : arg) {
        if (c == L'\\') { ++slashes; continue; }
        command.append(slashes, L'\\');
        if (c == L'"') command.append(slashes + 1, L'\\');
        slashes = 0;
        command += c;
    }
    command.append(2 * slashes, L'\\');
    command += L'"';
}

class FetchProcess {
    Handle job{CreateJobObjectW(nullptr, nullptr)};
    Handle child{nullptr};
public:
    FetchProcess(const fs::path& work, const std::vector<std::string>& args,
                 const std::vector<std::string>& environment) {
        if (!job.value) process_error("Cannot create HTTPS Git job");
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
                                                  JOB_OBJECT_LIMIT_JOB_MEMORY;
        limits.JobMemoryLimit = 512 * 1024 * 1024;
        if (!SetInformationJobObject(job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
            process_error("Cannot limit HTTPS Git job");

        std::wstring command;
        for (const auto& arg : args) append_argument(command, utf8_path(arg).wstring());
        std::vector<std::wstring> settings;
        for (const auto& entry : environment) settings.push_back(utf8_path(entry).wstring());
        wchar_t windows[MAX_PATH];
        const auto length = GetWindowsDirectoryW(windows, MAX_PATH);
        if (!length || length >= MAX_PATH) process_error("Cannot locate Windows system directory");
        settings.push_back(std::wstring(L"SystemRoot=") + windows);
        std::sort(settings.begin(), settings.end(), [](const auto& a, const auto& b) {
            return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_LESS_THAN;
        });
        std::wstring block;
        for (const auto& entry : settings) { block += entry; block += L'\0'; }

        SECURITY_ATTRIBUTES inherit{sizeof(inherit), nullptr, TRUE};
        const Handle discard{CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                          &inherit, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
        const Handle errors{CreateFileW((work / "fetch-error").c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                         &inherit, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
        if (discard.value == INVALID_HANDLE_VALUE || errors.value == INVALID_HANDLE_VALUE)
            process_error("Cannot redirect HTTPS Git output");

        ProcessAttributes attributes;
        SIZE_T size = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
        attributes.storage.resize(size);
        const auto list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.storage.data());
        if (!InitializeProcThreadAttributeList(list, 1, 0, &size))
            process_error("Cannot initialize HTTPS Git process attributes");
        attributes.list = list;
        HANDLE inherited[]{discard.value, errors.value};
        if (!UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                       inherited, sizeof(inherited), nullptr, nullptr))
            process_error("Cannot isolate HTTPS Git output handles");

        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput = startup.StartupInfo.hStdOutput = discard.value;
        startup.StartupInfo.hStdError = errors.value;
        startup.lpAttributeList = list;
        PROCESS_INFORMATION info{};
        if (!CreateProcessW(utf8_path(args.front()).c_str(), command.data(), nullptr, nullptr, TRUE,
                            CREATE_SUSPENDED | CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT |
                                EXTENDED_STARTUPINFO_PRESENT,
                            block.data(), work.c_str(), &startup.StartupInfo, &info))
            process_error("Cannot start HTTPS Git transport");
        child.value = info.hProcess;
        const Handle thread{info.hThread};
        if (!AssignProcessToJobObject(job.value, child.value) || ResumeThread(thread.value) == DWORD(-1)) {
            const auto error = GetLastError();
            TerminateProcess(child.value, 1);
            WaitForSingleObject(child.value, INFINITE);
            SetLastError(error);
            process_error("Cannot supervise HTTPS Git transport");
        }
    }

    ~FetchProcess() {
        TerminateJobObject(job.value, 1);
        WaitForSingleObject(child.value, INFINITE);
    }

    std::optional<int> poll() const {
        JOBOBJECT_BASIC_AND_IO_ACCOUNTING_INFORMATION usage{};
        if (!QueryInformationJobObject(job.value, JobObjectBasicAndIoAccountingInformation,
                                       &usage, sizeof(usage), nullptr))
            process_error("Cannot account for HTTPS Git job");
        // Includes I/O between Git and its helpers as well as pack/index writes.
        if (usage.IoInfo.WriteTransferCount > 3 * storage_limit) storage_exceeded();
        const auto state = WaitForSingleObject(child.value, 0);
        if (state == WAIT_TIMEOUT) return {};
        DWORD status;
        if (state != WAIT_OBJECT_0 || !GetExitCodeProcess(child.value, &status))
            process_error("Cannot wait for HTTPS Git transport");
        return static_cast<int>(status);
    }
};
#else
class FetchProcess {
    pid_t child;
public:
    FetchProcess(const fs::path& work, const std::vector<std::string>& args,
                 const std::vector<std::string>& environment) {
        std::vector<char*> argv, env;
        for (const auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
        for (const auto& entry : environment) env.push_back(const_cast<char*>(entry.c_str()));
        argv.push_back(nullptr);
        env.push_back(nullptr);
        struct rlimit size;
        if (getrlimit(RLIMIT_FSIZE, &size) < 0)
            throw std::system_error(errno, std::generic_category(), "Cannot inspect HTTPS Git file size limit");
        size.rlim_cur = std::min(size.rlim_cur, static_cast<rlim_t>(storage_limit));
        size.rlim_max = size.rlim_cur;
        child = fork();
        if (child < 0) throw std::system_error(errno, std::generic_category(), "Cannot fork HTTPS Git transport");
        if (child == 0) {
            const struct rlimit no_core{0, 0};
            if (setpgid(0, 0) < 0 || chdir(work.c_str()) < 0 ||
                setrlimit(RLIMIT_FSIZE, &size) < 0 || setrlimit(RLIMIT_CORE, &no_core) < 0)
                _exit(126);
            const int discard = open("/dev/null", O_RDWR);
            if (discard < 0 || dup2(discard, STDIN_FILENO) < 0 || dup2(discard, STDOUT_FILENO) < 0)
                _exit(126);
            if (discard > STDERR_FILENO) close(discard);
            const int errors = open("fetch-error", O_WRONLY | O_CREAT | O_EXCL, 0600);
            if (errors < 0 || dup2(errors, STDERR_FILENO) < 0) _exit(126);
            if (errors > STDERR_FILENO) close(errors);
            execve(argv.front(), argv.data(), env.data());
            _exit(126);
        }
    }

    ~FetchProcess() {
        // The leader is still waitable, so its PID cannot have been reused.
        kill(-child, SIGKILL);
        kill(child, SIGKILL);
        while (waitpid(child, nullptr, 0) < 0 && errno == EINTR) {}
    }

    std::optional<int> poll() const {
        siginfo_t info{};
        if (waitid(P_PID, static_cast<id_t>(child), &info, WEXITED | WNOHANG | WNOWAIT) < 0) {
            if (errno == EINTR) return {};
            throw std::system_error(errno, std::generic_category(), "Cannot wait for HTTPS Git transport");
        }
        if (!info.si_pid) return {};
        return info.si_code == CLD_EXITED ? info.si_status : -info.si_status;
    }
};
#endif
}

int run_git_fetch(const fs::path& work, const std::vector<std::string>& args,
                  const std::vector<std::string>& environment,
                  std::chrono::steady_clock::time_point deadline) {
    FetchProcess process(work, args, environment);
    for (;;) {
        if (std::chrono::steady_clock::now() >= deadline)
            throw std::runtime_error("HTTPS Git transport time limit exceeded.");
        check_storage(work);
        if (const auto status = process.poll()) {
            check_storage(work);
#ifndef _WIN32
            if (*status == -SIGXFSZ) storage_exceeded();
#endif
            if (*status == 126) throw std::runtime_error("Cannot start isolated HTTPS Git transport.");
            return *status;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}
}
