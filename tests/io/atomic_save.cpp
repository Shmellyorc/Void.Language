#include "void/backend.hpp"
#include "void/detail/filesystem_hooks.hpp"
#include "void/frontend.hpp"
#include "void/native_resource.hpp"
#include "void/path.hpp"
#include "../../src/runtime/host_internal.hpp"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <semaphore>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include "../../src/runtime/host_windows.hpp"
#include <winioctl.h>
#endif

namespace {
namespace fs = std::filesystem;
using namespace vd::host;
using vd::filesystem::testing::Hooks;
using vd::filesystem::testing::Observe;
using vd::filesystem::testing::SaveStep;
constexpr auto failure_code =
#ifdef _WIN32
    ERROR_WRITE_FAULT;
#else
    EIO;
#endif
constexpr auto cleanup_code =
#ifdef _WIN32
    ERROR_ACCESS_DENIED;
#else
    EACCES;
#endif

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::string contents(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    require(static_cast<bool>(file), "open committed fixture");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void ordinary_save(const char* source) {
    const auto analyzed = vd::analyze_project(source);
    if (!analyzed.succeeded()) throw vd::CompilationError(analyzed.diagnostics);
    vd::Jit jit;
    for (bool optimize : {false, true}) {
        const auto program = jit.compile(*analyzed.program, optimize);
        const auto destination = "progress.txt";
        constexpr auto original = "level=7\nplayer=old\n";
        std::ofstream(destination, std::ios::binary) << original;
        auto hooks = std::make_shared<vd::filesystem::testing::Hooks>();
        hooks->write_chunk = 3;
        hooks->write_failure_after = 8;
        {
            vd::filesystem::testing::Observe observe(hooks);
            require(program->invoke(0) == 7, "ordinary save did not report partial progress");
        }
        require(contents(destination) == "level=12", "ordinary save did not expose exact truncated prefix");
        require(program->invoke(0) == 0, "ordinary save success compatibility");
        std::filesystem::remove(destination);
        std::cout << (optimize ? "O2" : "JIT")
                  << ": ordinary write reported 8 bytes and replaced the 19-byte save with level=12\n";
    }
}

std::shared_ptr<Object> directory(const fs::path& path, bool create = true) {
    Request request{};
    request.operation = create ? dir_create : dir_admit;
    request.texts = {vd::path_utf8(fs::absolute(path))};
    auto result = directory_execute(request);
    if (result.kind || !request.acquired)
        throw std::runtime_error("private fixture admission failed: " + std::to_string(result.kind) +
                                 "/" + std::to_string(result.code));
    return request.acquired;
}

Result save(const std::shared_ptr<Object>& owner, std::string name, std::string bytes, bool replace = true) {
    Request request{};
    request.operation = dir_save;
    request.texts = {std::move(name)};
    request.bytes = {std::move(bytes)};
    request.numbers = {replace};
    request.objects = {owner};
    return directory_execute(request);
}

void committed(const Result& result) {
    if (!result.flag || result.detail || result.number != (result.kind == 0) || (result.kind && !result.code))
        throw std::runtime_error("invalid publication: kind=" + std::to_string(result.kind) +
            " native=" + std::to_string(result.code) + " cleanup=" + std::to_string(result.detail));
#ifndef _WIN32
    require(result.number == 1, "local Linux directory synchronization failed");
#endif
}

std::size_t handles() {
#ifdef _WIN32
    DWORD count = 0;
    require(GetProcessHandleCount(GetCurrentProcess(), &count), "count native handles");
    return count;
#else
    return static_cast<std::size_t>(std::distance(fs::directory_iterator("/proc/self/fd"), fs::directory_iterator{}));
#endif
}

#ifdef _WIN32
void junction(const fs::path& link, const fs::path& target) {
    require(fs::create_directory(link), "create junction fixture");
    win::Handle handle(CreateFileW(link.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                  FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    require(handle.value != INVALID_HANDLE_VALUE, "open junction fixture");
    const auto visible = fs::absolute(target).wstring();
    const auto substitute = L"\\??\\" + visible;
    const auto names = substitute + L'\0' + visible + L'\0';
    struct Mount { DWORD tag; WORD length, reserved, sub_offset, sub_length, print_offset, print_length; };
    std::vector<unsigned char> buffer(sizeof(Mount) + names.size() * sizeof(wchar_t));
    auto* mount = reinterpret_cast<Mount*>(buffer.data());
    mount->tag = IO_REPARSE_TAG_MOUNT_POINT;
    mount->length = static_cast<WORD>(buffer.size() - 8);
    mount->sub_length = static_cast<WORD>(substitute.size() * sizeof(wchar_t));
    mount->print_offset = static_cast<WORD>((substitute.size() + 1) * sizeof(wchar_t));
    mount->print_length = static_cast<WORD>(visible.size() * sizeof(wchar_t));
    std::memcpy(buffer.data() + sizeof(Mount), names.data(), names.size() * sizeof(wchar_t));
    DWORD returned = 0;
    require(DeviceIoControl(handle.value, FSCTL_SET_REPARSE_POINT, buffer.data(), static_cast<DWORD>(buffer.size()),
                            nullptr, 0, &returned, nullptr), "set directory junction");
}
#endif

void reader_controls(const std::shared_ptr<Object>& owner, const fs::path& target) {
    committed(save(owner, "progress.txt", "original"));
#ifdef _WIN32
    for (const DWORD sharing : {DWORD(FILE_SHARE_READ | FILE_SHARE_WRITE),
                               DWORD(FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE)}) {
        win::Handle reader(CreateFileW(target.c_str(), GENERIC_READ, sharing, nullptr, OPEN_EXISTING, 0, nullptr));
        require(reader.value != INVALID_HANDLE_VALUE, "open reader for sharing control");
        const auto result = save(owner, "progress.txt", "replacement");
        if (!(sharing & FILE_SHARE_DELETE)) {
            require(!result.flag && (result.code == ERROR_SHARING_VIOLATION || result.code == ERROR_ACCESS_DENIED),
                    "Windows reader that denies delete sharing did not block publication");
        } else if (result.flag) committed(result);
        else require(result.code == ERROR_SHARING_VIOLATION || result.code == ERROR_ACCESS_DENIED,
                     "unexpected Windows replace-while-open status");
        char original[32]{};
        DWORD count = 0;
        require(ReadFile(reader.value, original, sizeof(original), &count, nullptr) &&
                std::string_view(original, count) == "original", "held Windows reader observed a partial save");
        require(!reader.close(), "close Windows sharing fixture");
        require(contents(target) == (result.flag ? "replacement" : "original"), "Windows publication bytes");
        std::cout << "NTFS reader share-delete=" << bool(sharing & FILE_SHARE_DELETE)
                  << " published=" << result.flag << " native=" << result.code << '\n';
    }
#else
    std::ifstream reader(target, std::ios::binary);
    require(bool(reader), "open Linux reader");
    committed(save(owner, "progress.txt", "replacement"));
    const std::string original{std::istreambuf_iterator<char>(reader), std::istreambuf_iterator<char>()};
    require(original == "original" && contents(target) == "replacement", "Linux old/new reader identities");
    const std::string first(4096, 'a'), second(4096, 'b');
    committed(save(owner, "progress.txt", first));
    std::binary_semaphore observed{0};
    std::atomic<bool> done{false}, intact{true};
    auto concurrent = std::async(std::launch::async, [&] {
        unsigned reads = 0;
        while (!done) {
            auto current = contents(target);
            if (current != first && current != second) intact = false;
            if (!reads++) observed.release();
        }
        return reads;
    });
    try {
        require(observed.try_acquire_for(std::chrono::seconds(10)), "concurrent reader did not start");
        for (unsigned i = 0; i < 32; ++i) committed(save(owner, "progress.txt", i % 2 ? first : second));
    } catch (...) { done = true; throw; }
    done = true;
    require(concurrent.get() > 0 && intact, "reader observed partial content during replacement");
#endif
}

void native_save() {
    const auto root = fs::absolute("save-root");
    const auto target = root / "progress.txt";
    const auto stage = root / ".void-save-stage";
    const auto owner = directory(root);
#ifdef _WIN32
    // Windows lazily maps KernelBase.dll.mui when formatting its first native
    // error. Initialize the system message table before counting handles.
    require(!std::system_category().message(ERROR_FILE_NOT_FOUND).empty(), "initialize Windows error messages");
#endif
    const auto baseline = handles();
    const auto fresh = save(owner, "progress.txt", "original", false);
    committed(fresh);
    require(contents(target) == "original", "new publication bytes");
    std::cout << "Native directory synchronization: completed=" << fresh.number << " code=" << fresh.code << '\n';
    const std::string binary{"\0\xff\x80\n", 4};
    committed(save(owner, "progress.txt", binary));
    require(contents(target) == binary, "binary publication bytes");
    committed(save(owner, "progress.txt", {}));
    require(contents(target).empty(), "empty publication bytes");
    const std::string maximum(byte_snapshot_limit, '\xe9');
    committed(save(owner, "progress.txt", maximum));
    require(contents(target) == maximum, "maximum payload changed");
    const auto clean = directory(root / "oversize-root");
    require(save(clean, "never", maximum + 'x').kind == 12 && fs::is_empty(root / "oversize-root"),
            "oversized payload mutated directory");
    require(!clean->close(), "close oversize fixture");
    committed(save(owner, "progress.txt", "original"));
    require(!save(owner, "progress.txt", "unwanted", false).flag && contents(target) == "original",
            "exclusive collision changed destination");
#ifdef _WIN32
    const auto attributes = GetFileAttributesW(target.c_str());
    require(attributes != INVALID_FILE_ATTRIBUTES && SetFileAttributesW(target.c_str(), attributes | FILE_ATTRIBUTE_READONLY),
            "set read-only destination fixture");
    const auto denied = save(owner, "progress.txt", "unwanted");
    require(SetFileAttributesW(target.c_str(), attributes), "restore destination attributes");
    require(denied.code == ERROR_ACCESS_DENIED, "read-only destination did not report native permission failure");
#else
    fs::permissions(root, fs::perms::group_read, fs::perm_options::add);
    const auto denied = save(owner, "progress.txt", "unwanted");
    fs::permissions(root, fs::perms::group_read, fs::perm_options::remove);
    require(denied.kind == 2, "shared directory permissions were admitted through the held capability");
#endif
    require(!denied.flag && contents(target) == "original" && !fs::exists(stage),
            "permission failure changed destination or left a stage");
    for (const auto* invalid : {"../elsewhere", "relative/name", "D:/elsewhere", ".void-save-lock", ".void-save-stage"})
        require(save(owner, invalid, "unwanted").kind == 4, "destination accepted an unauthorized component");

    for (const auto step : {SaveStep::before_create, SaveStep::content_sync, SaveStep::before_publish}) {
        auto hooks = std::make_shared<Hooks>();
        hooks->save_error = [step](SaveStep point) { return point == step ? failure_code : 0; };
        Observe fault(hooks);
        const auto failed = save(owner, "progress.txt", "new payload");
        require(!failed.flag && !failed.number && failed.code == failure_code && !failed.detail &&
                contents(target) == "original" && !fs::exists(stage), "pre-publication failure lost old bytes or stage");
    }
    {
        auto hooks = std::make_shared<Hooks>();
        hooks->write_failure_after = 3;
        hooks->write_chunk = 2;
        Observe fault(hooks);
        const auto failed = save(owner, "progress.txt", "new payload");
        require(!failed.flag && !failed.number && failed.kind == 17 && !failed.detail &&
                contents(target) == "original" && !fs::exists(stage), "partial staged write altered old save");
    }
    {
        auto hooks = std::make_shared<Hooks>();
        hooks->save_error = [](SaveStep point) {
            if (point == SaveStep::before_publish) return failure_code;
            return point == SaveStep::remove_stage ? cleanup_code : 0;
        };
        Observe fault(hooks);
        const auto failed = save(owner, "progress.txt", "retained stage");
        require(!failed.flag && failed.code == failure_code && failed.detail == cleanup_code &&
                contents(target) == "original" && contents(stage) == "retained stage", "cleanup obscured primary failure");
    }
    const auto blocked = save(owner, "progress.txt", "unwanted");
    require(!blocked.flag && blocked.kind == 3 && contents(stage) == "retained stage" &&
            contents(target) == "original", "later save adopted abandoned staging name");
    require(fs::remove(stage), "explicitly remove fixture stage");

    {
        auto hooks = std::make_shared<Hooks>();
        hooks->save_error = [](SaveStep point) { return point == SaveStep::directory_sync ? failure_code : 0; };
        Observe fault(hooks);
        const auto failed = save(owner, "progress.txt", "published despite sync failure");
        require(failed.flag && !failed.number && failed.code == failure_code && !failed.detail &&
                contents(target) == "published despite sync failure" && !fs::exists(stage), "post-publication status or data lost");
    }
    {
        auto hooks = std::make_shared<Hooks>();
        hooks->save_error = [](SaveStep point) { return point == SaveStep::close_stage ? cleanup_code : 0; };
        Observe fault(hooks);
        const auto failed = save(owner, "progress.txt", "published despite close failure");
        require(failed.flag && failed.number == fresh.number && failed.detail == cleanup_code && failed.kind &&
                contents(target) == "published despite close failure" && !fs::exists(stage), "post-publication cleanup reset the outcome");
    }

    committed(save(owner, "progress.txt", "original"));
    const auto link = root / "linked.txt";
    fs::create_hard_link(target, link);
    require(!save(owner, "progress.txt", "unwanted").flag && contents(link) == "original" &&
            fs::equivalent(target, link), "hard-link identity was replaced");
    fs::remove(link);
    std::error_code symlink_error;
    fs::create_symlink(target, link, symlink_error);
    if (!symlink_error) {
        require(!save(owner, "linked.txt", "unwanted").flag && fs::is_symlink(link) && contents(target) == "original",
                "symlink destination was followed or replaced");
        fs::remove(link);
        std::cout << "Native file symlink refused\n";
    } else {
#ifdef _WIN32
        require(symlink_error.value() == ERROR_PRIVILEGE_NOT_HELD, "unexpected symlink fixture failure");
        const auto child = root / "junction-target";
        fs::create_directory(child);
        std::ofstream(child / "marker") << "intact";
        junction(link, child);
        require(!save(owner, "linked.txt", "unwanted").flag && contents(link / "marker") == "intact",
                "Windows destination reparse point changed");
        fs::remove(link);
        std::cout << "File symlink creation denied (1314). Native junction refused\n";
#else
        throw std::system_error(symlink_error);
#endif
    }

    {
        auto hooks = std::make_shared<Hooks>();
        hooks->save_error = [&](SaveStep point) {
            if (point == SaveStep::before_publish) std::ofstream(root / "collision.txt") << "other writer";
            return 0;
        };
        Observe fault(hooks);
        const auto collision = save(owner, "collision.txt", "unwanted");
        require(!collision.flag && collision.kind == 3 && contents(root / "collision.txt") == "other writer" &&
                !fs::exists(stage), "concurrent new destination bypassed no-replace rename");
    }
    {
        const auto moved = fs::absolute("moved-root");
        auto relocate = [&] {
            fs::rename(root, moved);
            auto replacement = directory(root);
            require(!replacement->close(), "close replacement parent");
        };
#ifdef _WIN32
        // NTFS can deny directory rename while a child handle is open.
        // Change the binding with only the admitted directory capability held.
        relocate();
#else
        auto hooks = std::make_shared<Hooks>();
        hooks->save_error = [&](SaveStep point) {
            if (point == SaveStep::before_publish) relocate();
            return 0;
        };
        Observe fault(hooks);
#endif
        const auto failed = save(owner, "progress.txt", "unwanted");
        require(!failed.flag && failed.kind && contents(moved / "progress.txt") == "original" &&
                fs::is_empty(root) && !fs::exists(moved / ".void-save-stage"), "parent identity change redirected publication");
        fs::remove(root);
        fs::rename(moved, root);
    }
    reader_controls(owner, target);

    {
        std::binary_semaphore entered{0}, release{0};
        auto first = std::async(std::launch::async, [&] {
            auto other = directory(root, false);
            auto hooks = std::make_shared<Hooks>();
            hooks->save_error = [&](SaveStep point) {
                if (point == SaveStep::before_publish) {
                    entered.release();
                    if (!release.try_acquire_for(std::chrono::seconds(15))) return static_cast<int>(failure_code);
                }
                return 0;
            };
            Observe fault(hooks);
            return save(other, "progress.txt", "first writer");
        });
        const auto started = entered.try_acquire_for(std::chrono::seconds(10));
        Result contended;
        if (started) contended = save(owner, "progress.txt", "second writer");
        release.release();
        committed(first.get());
        require(started && !contended.flag && contended.kind == 8 && contents(target) == "first writer",
                "cooperating writer bypassed the native directory lock");
        committed(save(owner, "progress.txt", "second writer"));
    }
    for (unsigned n = 0; n < 32; ++n) committed(save(owner, "progress.txt", "original"));
    require(handles() == baseline && !fs::exists(stage), "native save leaked file handles or a stage");
    require(!owner->close(), "close native test directory");
}

void public_faults(const char* source) {
    const auto analyzed = vd::analyze_project(source);
    if (!analyzed.succeeded()) throw vd::CompilationError(analyzed.diagnostics);
    const auto owner = directory("save-root", false);
    for (bool optimize : {false, true}) {
        vd::Jit jit;
        const auto code = jit.compile(*analyzed.program, optimize);
        const auto runtime = code->runtime_owner();
        for (const auto step : {SaveStep::before_publish, SaveStep::directory_sync, SaveStep::close_stage}) {
            committed(save(owner, "progress.txt", "original"));
            auto hooks = std::make_shared<Hooks>();
            hooks->save_error = [step](SaveStep point) { return point == step ? failure_code : 0; };
            Observe fault(hooks);
            const auto expected = step == SaveStep::before_publish ? 11 : step == SaveStep::directory_sync ? 13 : 14;
            require(code->invoke(0) == expected, "public save discarded a publication or cleanup flag");
            require(contents("save-root/progress.txt") == (expected == 11 ? "original" : "level=12\nplayer=新雪\n"),
                    "public save contents disagreed with its outcome");
            vd::Runtime::Attachment attached(*runtime);
            vd::Runtime::Managed managed(*runtime);
            require(runtime->collect() == vd::Runtime::Collection::completed && runtime->snapshot().objects == 0,
                    "public save retained a managed resource");
        }
    }
    require(!owner->close(), "close public fault fixture directory");
}
}

int main(int argc, char** argv) {
    try {
        require(argc == 3, "ordinary and atomic-save sources required");
        ordinary_save(argv[1]);
        native_save();
        public_faults(argv[2]);
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
