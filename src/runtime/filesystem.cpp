#include "void/filesystem.hpp"
#include "void/detail/filesystem_hooks.hpp"
#include "void/native.hpp"
#include "void/path.hpp"
#include "void/utf8.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <limits>
#include <set>
#include <stdexcept>
#include <system_error>
#include <utility>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/syscall.h>
#include <linux/fs.h>
#endif
#ifdef __APPLE__
#include <stdio.h>
#endif
#endif

namespace vd::filesystem {
namespace fs = std::filesystem;
namespace testing {
namespace {
thread_local std::shared_ptr<const Hooks> selected;
}
std::shared_ptr<const Hooks> current() {
    return selected;
}
Observe::Observe(std::shared_ptr<const Hooks> hooks) : previous_(std::move(selected)) {
    selected = std::move(hooks);
}
Observe::~Observe() {
    selected = std::move(previous_);
}
}

namespace {
constexpr std::size_t transfer_size = 65536;
constexpr auto maximum = std::numeric_limits<std::int64_t>::max();

ErrorKind classify(const std::error_code& error) {
    if (!error) return ErrorKind::success;
    if (error == std::errc::no_such_file_or_directory) return ErrorKind::not_found;
    if (error == std::errc::permission_denied || error == std::errc::operation_not_permitted) {
        return ErrorKind::permission_denied;
    }
    if (error == std::errc::file_exists) return ErrorKind::already_exists;
    if (error == std::errc::cross_device_link) return ErrorKind::cross_device;
    if (error == std::errc::not_a_directory) return ErrorKind::not_directory;
    if (error == std::errc::is_a_directory) return ErrorKind::is_directory;
    if (error == std::errc::directory_not_empty) return ErrorKind::not_empty;
    if (error == std::errc::invalid_argument || error == std::errc::filename_too_long) {
        return ErrorKind::invalid_path;
    }
    if (error == std::errc::operation_not_supported || error == std::errc::function_not_supported) {
        return ErrorKind::unsupported;
    }
    return ErrorKind::io_failure;
}

void fail(Result& result, ErrorKind kind, std::int64_t code = 0) {
    if (result.error == ErrorKind::success) {
        result.error = kind;
        result.os_code = code;
    }
}

void fail(Result& result, const std::error_code& error) {
    if (error) fail(result, classify(error), error.value());
}

std::error_code native_error() {
#ifdef _WIN32
    return {static_cast<int>(GetLastError()), std::system_category()};
#else
    return {errno, std::generic_category()};
#endif
}

bool valid_path(std::string_view text, bool empty = false) {
    return (empty || !text.empty()) && text.find('\0') == std::string_view::npos && vd::valid_utf8(text);
}

bool cancelled(const std::atomic<bool>* flag) {
    return flag && flag->load(std::memory_order_relaxed);
}

std::int64_t checked(std::uintmax_t value) {
    if (value > static_cast<std::uintmax_t>(maximum)) throw std::length_error("Filesystem size exceeds i64");
    return static_cast<std::int64_t>(value);
}

std::string output_path(const fs::path& path) {
    auto text = vd::path_utf8(path);
    if (!valid_path(text, true)) throw std::filesystem::filesystem_error(
        "Native filename is not UTF-8", path, std::make_error_code(std::errc::illegal_byte_sequence));
    return text;
}

fs::path host_root(const fs::path& value) {
#ifdef _WIN32
    auto native = value;
    native.make_preferred();
    const auto& text = native.native();
    std::size_t server = 2;
    if (text.starts_with(L"\\\\?\\UNC\\")) server = 8;
    else if (!text.starts_with(L"\\\\") || text.starts_with(L"\\\\?\\") || text.starts_with(L"\\\\.\\")) {
        return native.root_path();
    }
    auto separator = text.find(L'\\', server);
    if (separator == std::wstring::npos) return native.root_path();
    auto share = text.find_first_not_of(L'\\', separator);
    if (share == std::wstring::npos) return native.root_path();
    auto end = text.find(L'\\', share);
    if (end == std::wstring::npos) return fs::path(text + L"\\");
    return fs::path(text.substr(0, end + 1));
#else
    return value.root_path();
#endif
}

fs::path normalize_host(const fs::path& value) {
#ifdef _WIN32
    auto native = value;
    native.make_preferred();
    auto root = host_root(native);
    if (root.native().size() > native.root_path().native().size()) {
        // The standard path parser treats only the UNC server as root_name.
        // Anchor the share separately so lexical parents cannot escape it.
        if (native.native().size() <= root.native().size()) return root;
        auto remainder = fs::path(native.native().substr(root.native().size()));
        auto anchored = (fs::path(L"\\") / remainder).lexically_normal();
        return root / anchored.relative_path();
    }
#endif
    return value.lexically_normal();
}

fs::path relative_host(const fs::path& target, const fs::path& base) {
#ifdef _WIN32
    auto left = target;
    auto right = base;
    left.make_preferred();
    right.make_preferred();
    auto left_root = host_root(left).native();
    auto right_root = host_root(right).native();
    auto equal = [](const std::wstring& a, const std::wstring& b) {
        return CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(),
                                    static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
    };
    if (!equal(left_root, right_root)) return {};
    auto a = fs::path(left.native().substr(left_root.size()));
    auto b = fs::path(right.native().substr(right_root.size()));
    auto first = a.begin();
    auto second = b.begin();
    while (first != a.end() && second != b.end() && equal(first->native(), second->native())) {
        ++first;
        ++second;
    }
    fs::path result;
    for (; second != b.end(); ++second) {
        if (!second->empty() && *second != L".") result /= L"..";
    }
    for (; first != a.end(); ++first) result /= *first;
    return result.empty() ? fs::path(L".") : result;
#else
    return target.lexically_relative(base);
#endif
}

#ifdef _WIN32
std::wstring native_path(const std::string& text) {
    auto path = vd::utf8_path(text);
    // Extended paths preserve Unicode and long names. Resolve once without
    // changing cwd; preserve drive-relative/native root semantics first.
    auto full = normalize_host(fs::absolute(path)).make_preferred().native();
    if (full.starts_with(L"\\\\?\\")) return full;
    if (full.starts_with(L"\\\\")) return L"\\\\?\\UNC\\" + full.substr(2);
    return L"\\\\?\\" + full;
}
using NativeHandle = HANDLE;
const NativeHandle invalid_handle = INVALID_HANDLE_VALUE;
#else
using NativeHandle = int;
constexpr NativeHandle invalid_handle = -1;
#endif
}

class File {
public:
    NativeHandle handle = invalid_handle;
    OpenMode mode;
    std::shared_ptr<const testing::Hooks> hooks;
    std::int64_t written = 0;
    std::vector<char> input = std::vector<char>(transfer_size);
    std::size_t first = 0;
    std::size_t last = 0;
    std::int64_t logical = 0;

    explicit File(OpenMode selected, std::shared_ptr<const testing::Hooks> observation = {})
        : mode(selected), hooks(std::move(observation)) {}
    ~File() { close(*this); }
    File(const File&) = delete;
    File& operator=(const File&) = delete;
};

namespace testing {
bool noninheritable(const std::shared_ptr<File>& file) {
    if (!file || file->handle == invalid_handle) return false;
#ifdef _WIN32
    DWORD flags = 0;
    return GetHandleInformation(file->handle, &flags) && !(flags & HANDLE_FLAG_INHERIT);
#else
    auto flags = ::fcntl(file->handle, F_GETFD);
    return flags >= 0 && (flags & FD_CLOEXEC);
#endif
}
}

namespace {
bool readable(const File& file) {
    return file.mode != OpenMode::write && file.mode != OpenMode::append;
}

bool writable(const File& file) { return file.mode != OpenMode::read; }

bool seek_native(File& file, std::int64_t offset, int origin, Result& result) {
#ifdef _WIN32
    LARGE_INTEGER distance;
    distance.QuadPart = offset;
    LARGE_INTEGER position;
    if (!SetFilePointerEx(file.handle, distance, &position, origin)) {
        fail(result, native_error());
        return false;
    }
    result.number = position.QuadPart;
#else
    static_assert(sizeof(off_t) >= sizeof(std::int64_t), "64-bit file offsets are required");
    auto position = ::lseek(file.handle, static_cast<off_t>(offset), origin);
    if (position == static_cast<off_t>(-1)) {
        if (errno == ESPIPE) fail(result, ErrorKind::unsupported, errno);
        else fail(result, native_error());
        return false;
    }
    result.number = static_cast<std::int64_t>(position);
#endif
    return true;
}

std::size_t read_native(File& file, char* bytes, std::size_t count, Result& result) {
    if (file.hooks) {
        count = std::min(count, file.hooks->read_chunk);
        if (file.hooks->before) file.hooks->before(Operation::read_bytes);
    }
    if (count == 0) throw std::logic_error("Test read chunk cannot be zero");
    for (;;) {
#ifdef _WIN32
        DWORD received = 0;
        if (!ReadFile(file.handle, bytes, static_cast<DWORD>(std::min(count, transfer_size)), &received, nullptr)) {
            if (GetLastError() == ERROR_BROKEN_PIPE || GetLastError() == ERROR_HANDLE_EOF) result.eof = true;
            else fail(result, native_error());
            return 0;
        }
#else
        auto received = ::read(file.handle, bytes, std::min(count, transfer_size));
        if (received < 0) {
            if (errno == EINTR) continue;
            fail(result, native_error());
            return 0;
        }
#endif
        if (received == 0) result.eof = true;
        return static_cast<std::size_t>(received);
    }
}

bool advance(File& file, std::size_t count, Result& result) {
    if (count > static_cast<std::uint64_t>(maximum - file.logical)) {
        fail(result, ErrorKind::invalid_range);
        return false;
    }
    file.logical += static_cast<std::int64_t>(count);
    result.completed += static_cast<std::int64_t>(count);
    return true;
}

bool fill(File& file, Result& result) {
    if (file.first != file.last) return true;
    file.first = 0;
    file.last = read_native(file, file.input.data(), file.input.size(), result);
    return file.last != 0;
}

void read(File& file, std::int64_t count, bool exact, Result& result,
          const std::atomic<bool>* cancellation) {
    if (count < 0) {
        fail(result, ErrorKind::invalid_range);
        return;
    }
    if (!readable(file)) {
        fail(result, ErrorKind::invalid_mode);
        return;
    }
    auto wanted = static_cast<std::size_t>(count);
    if (static_cast<std::uint64_t>(wanted) != static_cast<std::uint64_t>(count)) {
        throw std::length_error("Read extent exceeds address space");
    }
    result.bytes.reserve(exact ? wanted : std::min(wanted, transfer_size));
    while (result.bytes.size() < wanted && !cancelled(cancellation)) {
        if (!fill(file, result)) break;
        auto amount = std::min(wanted - result.bytes.size(), file.last - file.first);
        result.bytes.append(file.input.data() + file.first, amount);
        file.first += amount;
        if (!advance(file, amount, result)) return;
        if (!exact) break;
    }
    if (exact && result.error == ErrorKind::success && result.bytes.size() != wanted &&
        !cancelled(cancellation)) fail(result, ErrorKind::unexpected_eof);
}

void read_line(File& file, std::int64_t limit, Result& result,
               const std::atomic<bool>* cancellation) {
    if (limit < 0) {
        fail(result, ErrorKind::invalid_range);
        return;
    }
    if (!readable(file)) {
        fail(result, ErrorKind::invalid_mode);
        return;
    }
    bool saw_data = false;
    while (!cancelled(cancellation) && fill(file, result)) {
        char byte = file.input[file.first];
        if (byte != '\r' && byte != '\n' && checked(result.bytes.size()) == limit) {
            fail(result, ErrorKind::limit_exceeded);
            break;
        }
        ++file.first;
        if (!advance(file, 1, result)) return;
        saw_data = true;
        if (byte == '\n') break;
        if (byte == '\r') {
            if (fill(file, result) && file.input[file.first] == '\n') {
                ++file.first;
                advance(file, 1, result);
            }
            break;
        }
        result.bytes.push_back(byte);
    }
    // EOF is the empty end-of-stream variant, not the empty-line variant.
    result.eof = result.eof && !saw_data;
}

bool align_for_write(File& file, Result& result) {
    if (file.first != file.last) {
        if (!seek_native(file, file.logical, 0, result)) return false;
        file.first = file.last = 0;
    }
    return true;
}

void write(File& file, const std::string& bytes, Result& result,
           const std::atomic<bool>* cancellation) {
    if (!writable(file)) {
        fail(result, ErrorKind::invalid_mode);
        return;
    }
    if (checked(bytes.size()) > maximum - file.logical) {
        fail(result, ErrorKind::invalid_range);
        return;
    }
    if (!align_for_write(file, result)) return;
    while (static_cast<std::size_t>(result.completed) < bytes.size() && !cancelled(cancellation)) {
        auto amount = std::min(bytes.size() - static_cast<std::size_t>(result.completed), transfer_size);
        if (file.hooks) {
            if (file.hooks->before) file.hooks->before(Operation::write_bytes);
            amount = std::min(amount, file.hooks->write_chunk);
            if (file.hooks->write_failure_after >= 0) {
                if (file.written >= file.hooks->write_failure_after) {
                    fail(result, ErrorKind::io_failure, EIO);
                    break;
                }
                amount = std::min(amount, static_cast<std::size_t>(file.hooks->write_failure_after - file.written));
            }
            if (file.hooks->zero_write || amount == 0) {
                fail(result, ErrorKind::io_failure);
                break;
            }
        }
        auto* data = bytes.data() + result.completed;
#ifdef _WIN32
        DWORD sent = 0;
        OVERLAPPED append{};
        append.Offset = MAXDWORD;
        append.OffsetHigh = MAXDWORD;
        auto* positioning = file.mode == OpenMode::append ? &append : nullptr;
        if (!WriteFile(file.handle, data, static_cast<DWORD>(amount), &sent, positioning)) {
            fail(result, native_error());
            break;
        }
#else
        auto sent = ::write(file.handle, data, amount);
        if (sent < 0) {
            if (errno == EINTR) continue;
            fail(result, native_error());
            break;
        }
#endif
        if (sent == 0) {
            fail(result, ErrorKind::io_failure);
            break;
        }
        if (static_cast<std::uint64_t>(sent) > static_cast<std::uint64_t>(maximum - file.written)) {
            file.written = maximum;
        } else file.written += static_cast<std::int64_t>(sent);
        if (!advance(file, static_cast<std::size_t>(sent), result)) break;
    }
    if (file.mode == OpenMode::append) {
        Result position;
        if (seek_native(file, 0, 1, position)) file.logical = position.number;
    }
}

void length(File& file, Result& result) {
#ifdef _WIN32
    LARGE_INTEGER size;
    if (!GetFileSizeEx(file.handle, &size)) fail(result, native_error());
    else result.number = size.QuadPart;
#else
    struct stat status{};
    if (::fstat(file.handle, &status) != 0) fail(result, native_error());
    else if (status.st_size < 0) fail(result, ErrorKind::invalid_range);
    else result.number = static_cast<std::int64_t>(status.st_size);
#endif
}

void set_length(File& file, std::int64_t count, Result& result) {
    if (count < 0) {
        fail(result, ErrorKind::invalid_range);
        return;
    }
    if (!writable(file)) {
        fail(result, ErrorKind::invalid_mode);
        return;
    }
    if (!align_for_write(file, result)) return;
#ifdef _WIN32
    auto saved = file.logical;
    if (!seek_native(file, count, 0, result)) return;
    if (!SetEndOfFile(file.handle)) fail(result, native_error());
    Result restored;
    if (!seek_native(file, saved, 0, restored)) {
        if (result.error == ErrorKind::success) result = restored;
        else result.cleanup_code = restored.os_code;
    }
#else
    for (;;) {
        if (::ftruncate(file.handle, static_cast<off_t>(count)) == 0) break;
        if (errno == EINTR) continue;
        fail(result, native_error());
        break;
    }
#endif
    result.number = count;
}

void file_operation(const Request& request, Result& result, const std::atomic<bool>* cancellation) {
    auto& file = *request.file;
    // Complete the existing test barrier at native operation boundaries. Transfer
    // chunks and close retain their established per-file hooks below.
    if (file.hooks && file.hooks->before && request.operation >= Operation::seek &&
        request.operation < Operation::close) file.hooks->before(request.operation);
    if (request.operation == Operation::close) {
        result = close(file);
        return;
    }
    if (file.handle == invalid_handle) {
        fail(result, ErrorKind::closed);
        return;
    }
    switch (request.operation) {
    case Operation::read_bytes:
    case Operation::read_exact:
        read(file, request.first, request.operation == Operation::read_exact, result, cancellation);
        break;
    case Operation::read_line:
        read_line(file, request.first, result, cancellation);
        break;
    case Operation::write_bytes:
        write(file, request.bytes, result, cancellation);
        break;
    case Operation::seek: {
        if (request.second < 0 || request.second > 2) {
            fail(result, ErrorKind::invalid_range);
            break;
        }
        std::int64_t base = 0;
        if (request.second == 1) base = file.logical;
        if (request.second == 2) {
            length(file, result);
            if (result.error != ErrorKind::success) break;
            base = result.number;
        }
        if ((request.first < 0 && request.first < -base) ||
            (request.first >= 0 && request.first > maximum - base)) {
            fail(result, ErrorKind::invalid_range);
            break;
        }
        if (seek_native(file, base + request.first, 0, result)) {
            file.logical = result.number;
            file.first = file.last = 0;
        }
        break;
    }
    case Operation::position:
        result.number = file.logical;
        break;
    case Operation::length:
        length(file, result);
        break;
    case Operation::set_length:
        set_length(file, request.first, result);
        break;
    case Operation::flush:
#ifdef _WIN32
        if (!FlushFileBuffers(file.handle)) fail(result, native_error());
#else
        while (::fsync(file.handle) != 0) {
            if (errno == EINTR) continue;
            fail(result, native_error());
            break;
        }
#endif
        break;
    default:
        throw std::logic_error("Invalid native file operation");
    }
}
}

std::shared_ptr<File> open(const std::string& path, OpenMode mode, Result& result,
                           std::shared_ptr<const testing::Hooks> hooks) {
    if (!valid_path(path)) {
        fail(result, ErrorKind::invalid_path);
        return {};
    }
    if (mode < OpenMode::read || mode > OpenMode::create_new) {
        fail(result, ErrorKind::invalid_mode);
        return {};
    }
    if (!hooks) hooks = testing::current();
    auto file = std::make_shared<File>(mode, std::move(hooks));
    if (file->hooks && file->hooks->before) file->hooks->before(Operation::open);
#ifdef _WIN32
    DWORD access = GENERIC_READ;
    DWORD creation = OPEN_EXISTING;
    switch (mode) {
    case OpenMode::read: break;
    case OpenMode::write: access = GENERIC_WRITE; creation = CREATE_ALWAYS; break;
    case OpenMode::append: access = GENERIC_WRITE; creation = OPEN_ALWAYS; break;
    case OpenMode::read_write: access = GENERIC_READ | GENERIC_WRITE; break;
    case OpenMode::create: access = GENERIC_READ | GENERIC_WRITE; creation = CREATE_ALWAYS; break;
    case OpenMode::open_or_create: access = GENERIC_READ | GENERIC_WRITE; creation = OPEN_ALWAYS; break;
    case OpenMode::create_new: access = GENERIC_READ | GENERIC_WRITE; creation = CREATE_NEW; break;
    }
    file->handle = CreateFileW(native_path(path).c_str(), access,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               nullptr, creation, FILE_ATTRIBUTE_NORMAL, nullptr);
#else
    int flags = O_RDONLY | O_CLOEXEC;
    switch (mode) {
    case OpenMode::read: break;
    case OpenMode::write: flags = O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC; break;
    case OpenMode::append: flags = O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC; break;
    case OpenMode::read_write: flags = O_RDWR | O_CLOEXEC; break;
    case OpenMode::create: flags = O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC; break;
    case OpenMode::open_or_create: flags = O_RDWR | O_CREAT | O_CLOEXEC; break;
    case OpenMode::create_new: flags = O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC; break;
    }
    do {
        file->handle = ::open(path.c_str(), flags, 0666);
    } while (file->handle < 0 && errno == EINTR);
#endif
    if (file->handle == invalid_handle) {
        fail(result, native_error());
        return {};
    }
#ifdef _WIN32
    BY_HANDLE_FILE_INFORMATION information{};
    if (!GetFileInformationByHandle(file->handle, &information)) fail(result, native_error());
    else if (information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) fail(result, ErrorKind::is_directory);
#else
    struct stat information{};
    if (::fstat(file->handle, &information) != 0) fail(result, native_error());
    else if (S_ISDIR(information.st_mode)) fail(result, ErrorKind::is_directory);
#endif
    if (result.error != ErrorKind::success) {
        auto cleanup = close(*file);
        result.cleanup_code = cleanup.os_code;
        return {};
    }
    return file;
}

Result close(File& file) {
    Result result;
    auto handle = std::exchange(file.handle, invalid_handle);
    if (handle == invalid_handle) return result;
    if (file.hooks && file.hooks->before) file.hooks->before(Operation::close);
#ifdef _WIN32
    if (!CloseHandle(handle)) fail(result, native_error());
#else
    // Retrying close after EINTR can close a reused descriptor on Linux.
    // Retire identity once and preserve the native diagnostic.
    if (::close(handle) != 0) fail(result, native_error());
#endif
    file.first = file.last = 0;
    if (file.hooks && file.hooks->close_error && result.error == ErrorKind::success) {
        fail(result, ErrorKind::io_failure, file.hooks->close_error);
    }
    return result;
}

namespace {
void path_operation(const Request& request, Result& result) {
    if (!valid_path(request.path, true) || !valid_path(request.target, true)) {
        fail(result, ErrorKind::invalid_path);
        return;
    }
    auto value = vd::utf8_path(request.path);
    auto other = vd::utf8_path(request.target);
    switch (request.operation) {
    case Operation::path_join: result.text = output_path(value / other); break;
    case Operation::path_normalize: result.text = output_path(normalize_host(value)); break;
    case Operation::path_filename: result.text = output_path(value.filename()); break;
    case Operation::path_stem: result.text = output_path(value.stem()); break;
    case Operation::path_extension: result.text = output_path(value.extension()); break;
    case Operation::path_change_extension:
        if (other.has_parent_path() || other.has_root_path()) fail(result, ErrorKind::invalid_path);
        else result.text = output_path(value.replace_extension(other));
        break;
    case Operation::path_parent: {
        auto parent = value.parent_path();
        auto root = host_root(value);
#ifdef _WIN32
        if (!root.empty() && value.make_preferred().native().size() <= root.native().size()) parent = root;
#endif
        result.text = output_path(parent);
        break;
    }
    case Operation::path_root: result.text = output_path(host_root(value)); break;
    case Operation::path_is_absolute: result.number = value.is_absolute(); break;
    case Operation::path_is_relative: result.number = value.is_relative(); break;
    case Operation::path_is_rooted: result.number = value.has_root_path(); break;
    case Operation::path_separator:
        result.text.assign(1, static_cast<char>(fs::path::preferred_separator));
        break;
    case Operation::path_absolute:
    case Operation::path_relative: {
        std::error_code error;
        if (other.empty()) {
            other = fs::current_path(error);
            if (error) {
                fail(result, error);
                break;
            }
        }
        if (!other.is_absolute()) {
            fail(result, ErrorKind::invalid_path);
            break;
        }
        auto resolved = normalize_host(value.is_absolute() ? value : other / value);
        if (!resolved.is_absolute()) {
            fail(result, ErrorKind::invalid_path);
            break;
        }
        if (request.operation == Operation::path_absolute) result.text = output_path(resolved);
        else {
            auto relative = relative_host(resolved, normalize_host(other));
            if (relative.empty()) fail(result, ErrorKind::invalid_path);
            else result.text = output_path(relative);
        }
        break;
    }
    default: throw std::logic_error("Invalid path operation");
    }
}
}


namespace {
void finish_close(File& file, Result& result) {
    auto cleanup = close(file);
    if (cleanup.error == ErrorKind::success) return;
    if (result.error == ErrorKind::success) {
        result.error = cleanup.error;
        result.os_code = cleanup.os_code;
    } else result.cleanup_code = cleanup.os_code;
}

void whole_file(const Request& request, Result& result, const std::atomic<bool>* cancellation) {
    bool reading = request.operation == Operation::read_all;
    if (reading && request.first < 0) {
        fail(result, ErrorKind::invalid_range);
        return;
    }
    auto mode = reading ? OpenMode::read : static_cast<OpenMode>(request.first);
    if (!reading && mode != OpenMode::write && mode != OpenMode::append &&
        mode != OpenMode::create_new && mode != OpenMode::create &&
        mode != OpenMode::open_or_create && mode != OpenMode::read_write) {
        fail(result, ErrorKind::invalid_mode);
        return;
    }
    auto file = open(request.path, mode, result, request.hooks);
    if (!file) return;
    if (reading) {
        while (!cancelled(cancellation) && fill(*file, result)) {
            auto available = file->last - file->first;
            auto remaining = request.first - checked(result.bytes.size());
            auto amount = std::min(available, static_cast<std::size_t>(remaining));
            result.bytes.append(file->input.data() + file->first, amount);
            file->first += amount;
            advance(*file, amount, result);
            if (amount != available) {
                fail(result, ErrorKind::limit_exceeded);
                break;
            }
        }
    } else write(*file, request.bytes, result, cancellation);
    finish_close(*file, result);
}

Entry metadata(const fs::path& path, bool follow, Result& result) {
    Entry entry;
    entry.path = output_path(path);
    entry.name = output_path(path.filename());
#ifdef _WIN32
    auto handle = CreateFileW(native_path(entry.path).c_str(), FILE_READ_ATTRIBUTES,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS |
                              (follow ? 0 : FILE_FLAG_OPEN_REPARSE_POINT), nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        fail(result, native_error());
        return entry;
    }
    BY_HANDLE_FILE_INFORMATION information{};
    if (!GetFileInformationByHandle(handle, &information)) fail(result, native_error());
    if (!CloseHandle(handle)) {
        auto error = native_error();
        if (result.error == ErrorKind::success) fail(result, error);
        else result.cleanup_code = error.value();
    }
    if (result.error != ErrorKind::success) return entry;
    if (!follow && (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) entry.kind = EntryKind::symlink;
    else if (information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) entry.kind = EntryKind::directory;
    else entry.kind = EntryKind::file;
    entry.size = checked((static_cast<std::uint64_t>(information.nFileSizeHigh) << 32) |
                         information.nFileSizeLow);
    auto ticks = (static_cast<std::uint64_t>(information.ftLastWriteTime.dwHighDateTime) << 32) |
                  information.ftLastWriteTime.dwLowDateTime;
    constexpr std::uint64_t epoch_ticks = 116444736000000000ULL;
    if (ticks >= epoch_ticks) {
        ticks -= epoch_ticks;
        entry.modified_seconds = static_cast<std::int64_t>(ticks / 10000000);
        entry.modified_nanoseconds = static_cast<std::int64_t>(ticks % 10000000) * 100;
    } else {
        auto before = epoch_ticks - ticks;
        entry.modified_seconds = -static_cast<std::int64_t>((before + 9999999) / 10000000);
        entry.modified_nanoseconds = static_cast<std::int64_t>((10000000 - before % 10000000) % 10000000) * 100;
    }
#else
    struct stat information{};
    auto status = follow ? ::stat(entry.path.c_str(), &information) : ::lstat(entry.path.c_str(), &information);
    if (status != 0) {
        fail(result, native_error());
        return entry;
    }
    if (S_ISREG(information.st_mode)) entry.kind = EntryKind::file;
    else if (S_ISDIR(information.st_mode)) entry.kind = EntryKind::directory;
    else if (S_ISLNK(information.st_mode)) entry.kind = EntryKind::symlink;
    if (information.st_size < 0) fail(result, ErrorKind::invalid_range);
    else entry.size = static_cast<std::int64_t>(information.st_size);
#ifdef __APPLE__
    entry.modified_seconds = information.st_mtimespec.tv_sec;
    entry.modified_nanoseconds = information.st_mtimespec.tv_nsec;
#else
    entry.modified_seconds = information.st_mtim.tv_sec;
    entry.modified_nanoseconds = information.st_mtim.tv_nsec;
#endif
#endif
    return entry;
}

bool scalar_equal(std::string_view left, std::string_view right) {
#ifdef _WIN32
    auto a = vd::utf8_path(left).native();
    auto b = vd::utf8_path(right).native();
    return CompareStringOrdinal(a.data(), static_cast<int>(a.size()),
                                b.data(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
#else
    return left == right;
#endif
}

bool matches(std::string_view pattern, std::string_view name) {
    std::size_t p = 0;
    std::size_t n = 0;
    std::size_t star = std::string_view::npos;
    std::size_t retry = 0;
    while (n != name.size()) {
        if (p != pattern.size() && pattern[p] == '*') {
            star = ++p;
            retry = n;
        } else if (p != pattern.size()) {
            auto pw = vd::utf8_scalar_bytes(pattern, p);
            auto nw = vd::utf8_scalar_bytes(name, n);
            if (pattern[p] == '?' || scalar_equal(pattern.substr(p, pw), name.substr(n, nw))) {
                p += pw;
                n += nw;
                continue;
            }
            if (star == std::string_view::npos) return false;
            p = star;
            retry += vd::utf8_scalar_bytes(name, retry);
            n = retry;
        } else {
            if (star == std::string_view::npos) return false;
            p = star;
            retry += vd::utf8_scalar_bytes(name, retry);
            n = retry;
        }
    }
    while (p != pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
}

using Identity = std::pair<std::uint64_t, std::uint64_t>;
Identity directory_identity(const fs::path& path, Result& result) {
#ifdef _WIN32
    auto handle = CreateFileW(native_path(output_path(path)).c_str(), FILE_READ_ATTRIBUTES,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING,
                              FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        fail(result, native_error());
        return {};
    }
    BY_HANDLE_FILE_INFORMATION information{};
    if (!GetFileInformationByHandle(handle, &information)) fail(result, native_error());
    else if (!(information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
             (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) fail(result, ErrorKind::not_directory);
    if (!CloseHandle(handle)) fail(result, native_error());
    return {information.dwVolumeSerialNumber,
            (static_cast<std::uint64_t>(information.nFileIndexHigh) << 32) | information.nFileIndexLow};
#else
    struct stat information{};
    if (::lstat(path.c_str(), &information) != 0) fail(result, native_error());
    else if (!S_ISDIR(information.st_mode)) fail(result, ErrorKind::not_directory);
    return {information.st_dev, information.st_ino};
#endif
}

bool valid_pattern(std::string_view pattern) {
    if (!valid_path(pattern)) return false;
#ifdef _WIN32
    return pattern.find_first_of("/\\") == std::string_view::npos;
#else
    return pattern.find('/') == std::string_view::npos;
#endif
}

void enumerate(const Request& request, Result& result, const std::atomic<bool>* cancellation) {
    if (request.second < 0) {
        fail(result, ErrorKind::invalid_range);
        return;
    }
    if (!valid_pattern(request.pattern)) {
        fail(result, ErrorKind::invalid_path);
        return;
    }
    std::vector<fs::path> pending{vd::utf8_path(request.path)};
    std::set<Identity> visited;
    while (!pending.empty() && !cancelled(cancellation)) {
        auto directory = std::move(pending.back());
        pending.pop_back();
        auto identity = directory_identity(directory, result);
        if (result.error != ErrorKind::success) return;
        if (!visited.insert(identity).second) continue;
        std::error_code error;
        fs::directory_iterator cursor(directory, fs::directory_options::none, error);
        if (error) {
            fail(result, error);
            return;
        }
        const fs::directory_iterator end;
        while (cursor != end && !cancelled(cancellation)) {
            if (result.entries_processed == request.second) {
                fail(result, ErrorKind::limit_exceeded);
                return;
            }
            auto entry = metadata(cursor->path(), false, result);
            if (result.error != ErrorKind::success) return;
            ++result.entries_processed;
            if (entry.kind == EntryKind::directory && request.first) pending.push_back(cursor->path());
            bool selected = request.operation == Operation::entries ||
                            (request.operation == Operation::get_files && entry.kind == EntryKind::file) ||
                            (request.operation == Operation::get_directories && entry.kind == EntryKind::directory);
            if (selected && matches(request.pattern, entry.name)) result.entries.push_back(std::move(entry));
            cursor.increment(error);
            if (error) {
                fail(result, error);
                return;
            }
        }
    }
}

bool real_directory(const fs::path& path, Result& result) {
    auto entry = metadata(path, false, result);
    if (result.error == ErrorKind::success && entry.kind != EntryKind::directory) {
        fail(result, ErrorKind::not_directory);
    }
    return result.error == ErrorKind::success;
}

void rename(const Request& request, bool directory, Result& result) {
    auto source = vd::utf8_path(request.path);
    auto entry = metadata(source, false, result);
    if (result.error != ErrorKind::success) return;
    if (directory ? entry.kind != EntryKind::directory : entry.kind == EntryKind::directory) {
        fail(result, directory ? ErrorKind::not_directory : ErrorKind::is_directory);
        return;
    }
#ifdef _WIN32
    if (!MoveFileExW(native_path(request.path).c_str(), native_path(request.target).c_str(),
                      request.first ? MOVEFILE_REPLACE_EXISTING : 0)) fail(result, native_error());
#else
    int status;
    if (request.first) status = ::rename(request.path.c_str(), request.target.c_str());
    else {
#ifdef __linux__
        status = static_cast<int>(::syscall(SYS_renameat2, AT_FDCWD, request.path.c_str(),
                                            AT_FDCWD, request.target.c_str(), RENAME_NOREPLACE));
#elif defined(__APPLE__)
        status = ::renamex_np(request.path.c_str(), request.target.c_str(), RENAME_EXCL);
#else
        fail(result, ErrorKind::unsupported);
        return;
#endif
    }
    if (status != 0) fail(result, native_error());
#endif
    if (result.error == ErrorKind::success) result.entries_processed = 1;
}

void remove_one(const fs::path& path, bool directory, bool missing_ok, Result& result) {
    auto entry = metadata(path, false, result);
    if (missing_ok && result.error == ErrorKind::not_found) {
        result.error = ErrorKind::success;
        result.os_code = 0;
        return;
    }
    if (result.error != ErrorKind::success) return;
    if (directory ? entry.kind != EntryKind::directory : entry.kind == EntryKind::directory) {
        fail(result, directory ? ErrorKind::not_directory : ErrorKind::is_directory);
        return;
    }
    // The native operation enforces the requested type at mutation time. An
    // earlier metadata observation cannot authorize removal of a replacement.
#ifdef _WIN32
    auto native = native_path(output_path(path));
    auto attributes = GetFileAttributesW(native.c_str());
    bool directory_link = entry.kind == EntryKind::symlink &&
                          attributes != INVALID_FILE_ATTRIBUTES &&
                          (attributes & FILE_ATTRIBUTE_DIRECTORY);
    bool removed = directory || directory_link ? RemoveDirectoryW(native.c_str()) : DeleteFileW(native.c_str());
    if (!removed) fail(result, native_error());
#else
    bool removed = (directory ? ::rmdir(path.c_str()) : ::unlink(path.c_str())) == 0;
    if (!removed) fail(result, native_error());
#endif
    if (removed) ++result.entries_processed;
}

void remove_tree(const Request& request, Result& result, const std::atomic<bool>* cancellation) {
    if (request.second < 1) {
        fail(result, ErrorKind::invalid_range);
        return;
    }
    auto root = vd::utf8_path(request.path);
    auto entry = metadata(root, false, result);
    if (request.first && result.error == ErrorKind::not_found) {
        result.error = ErrorKind::success;
        result.os_code = 0;
        return;
    }
    if (result.error != ErrorKind::success) return;
    if (entry.kind != EntryKind::directory) {
        fail(result, ErrorKind::not_directory);
        return;
    }
    Request traversal{Operation::entries};
    traversal.path = request.path;
    traversal.pattern = "*";
    traversal.first = 1;
    traversal.second = request.second - 1;
    Result snapshot;
    enumerate(traversal, snapshot, cancellation);
    if (snapshot.error != ErrorKind::success) {
        result.error = snapshot.error;
        result.os_code = snapshot.os_code;
        return;
    }
    // A bounded complete snapshot precedes deletion. Reverse DFS discovery puts
    // every child before its parent without recursion on the native stack.
    for (auto it = snapshot.entries.rbegin(); it != snapshot.entries.rend(); ++it) {
        if (cancelled(cancellation)) return;
        remove_one(vd::utf8_path(it->path), it->kind == EntryKind::directory, false, result);
        if (result.error != ErrorKind::success) return;
    }
    if (!cancelled(cancellation)) remove_one(root, true, false, result);
}

std::shared_ptr<File> copy_destination(const Request& request, Result& result) {
    if (!request.first) return open(request.target, OpenMode::create_new, result, request.hooks);
    auto file = std::make_shared<File>(OpenMode::read_write, request.hooks);
#ifdef _WIN32
    file->handle = CreateFileW(native_path(request.target).c_str(), GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               nullptr, OPEN_ALWAYS, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
#else
    do {
        file->handle = ::open(request.target.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0666);
    } while (file->handle < 0 && errno == EINTR);
#endif
    if (file->handle == invalid_handle) {
        fail(result, native_error());
        return {};
    }
    return file;
}

bool same_regular_files(File& source, File& target, Result& result) {
#ifdef _WIN32
    BY_HANDLE_FILE_INFORMATION a{}, b{};
    if (!GetFileInformationByHandle(source.handle, &a) || !GetFileInformationByHandle(target.handle, &b)) {
        fail(result, native_error());
        return false;
    }
    if ((a.dwFileAttributes | b.dwFileAttributes) & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) {
        fail(result, ErrorKind::invalid_path);
        return false;
    }
    return a.dwVolumeSerialNumber == b.dwVolumeSerialNumber &&
           a.nFileIndexHigh == b.nFileIndexHigh && a.nFileIndexLow == b.nFileIndexLow;
#else
    struct stat a{}, b{};
    if (::fstat(source.handle, &a) != 0 || ::fstat(target.handle, &b) != 0) {
        fail(result, native_error());
        return false;
    }
    if (!S_ISREG(a.st_mode) || !S_ISREG(b.st_mode)) {
        fail(result, ErrorKind::invalid_path);
        return false;
    }
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino;
#endif
}

void copy_file(const Request& request, Result& result, const std::atomic<bool>* cancellation) {
    auto source = open(request.path, OpenMode::read, result, request.hooks);
    if (!source) return;
    auto target = copy_destination(request, result);
    if (target) {
        if (same_regular_files(*source, *target, result)) fail(result, ErrorKind::invalid_path);
        if (result.error == ErrorKind::success) set_length(*target, 0, result);
        while (result.error == ErrorKind::success && !cancelled(cancellation)) {
            Result chunk;
            read(*source, transfer_size, false, chunk, cancellation);
            if (!chunk.bytes.empty()) {
                Result sent;
                write(*target, chunk.bytes, sent, cancellation);
                result.completed += sent.completed;
                fail(result, sent.error, sent.os_code);
            }
            fail(result, chunk.error, chunk.os_code);
            if (chunk.eof) break;
        }
        finish_close(*target, result);
    }
    finish_close(*source, result);
    if (result.error == ErrorKind::success && !cancelled(cancellation)) result.entries_processed = 1;
}

bool descendant(const fs::path& source, const fs::path& target, Result& result) {
    std::error_code error;
    auto a = fs::weakly_canonical(source, error);
    if (error) {
        fail(result, error);
        return true;
    }
    auto b = fs::weakly_canonical(target, error);
    if (error) {
        fail(result, error);
        return true;
    }
    auto left = a.begin();
    auto right = b.begin();
    while (left != a.end() && right != b.end() && scalar_equal(output_path(*left), output_path(*right))) {
        ++left;
        ++right;
    }
    return left == a.end();
}

void copy_directory(const Request& request, Result& result, const std::atomic<bool>* cancellation) {
    if (request.third < 1) {
        fail(result, ErrorKind::invalid_range);
        return;
    }
    auto source = vd::utf8_path(request.path);
    auto target = vd::utf8_path(request.target);
    if (!real_directory(source, result)) return;
    if (descendant(source, target, result)) {
        fail(result, ErrorKind::invalid_path);
        return;
    }
    Request traversal{Operation::entries};
    traversal.path = request.path;
    traversal.pattern = "*";
    traversal.first = request.first;
    traversal.second = request.third - 1;
    Result snapshot;
    enumerate(traversal, snapshot, cancellation);
    if (snapshot.error != ErrorKind::success) {
        result.error = snapshot.error;
        result.os_code = snapshot.os_code;
        return;
    }
    if (!request.first && std::any_of(snapshot.entries.begin(), snapshot.entries.end(),
                                     [](const auto& entry) { return entry.kind == EntryKind::directory; })) {
        fail(result, ErrorKind::not_directory);
        return;
    }
    auto create = [&](const fs::path& directory) {
        std::error_code error;
        if (fs::create_directory(directory, error)) {
            ++result.entries_processed;
            return true;
        }
        if (!error && !request.second) fail(result, ErrorKind::already_exists);
        else fail(result, error);
        return result.error == ErrorKind::success && real_directory(directory, result);
    };
    if (cancelled(cancellation) || !create(target)) return;
    for (const auto& entry : snapshot.entries) {
        if (cancelled(cancellation)) return;
        auto relative = vd::utf8_path(entry.path).lexically_relative(source);
        auto destination = target / relative;
        if (entry.kind == EntryKind::directory) {
            if (!create(destination)) return;
        } else if (entry.kind == EntryKind::file) {
            Request copy{Operation::copy_file};
            copy.path = entry.path;
            copy.target = output_path(destination);
            copy.first = request.second;
            copy.hooks = request.hooks;
            Result copied;
            copy_file(copy, copied, cancellation);
            result.completed += copied.completed;
            result.entries_processed += copied.entries_processed;
            fail(result, copied.error, copied.os_code);
            if (copied.cleanup_code) result.cleanup_code = copied.cleanup_code;
        } else if (entry.kind == EntryKind::symlink) {
            std::error_code error;
            auto link = fs::read_symlink(vd::utf8_path(entry.path), error);
            if (!error) {
#ifdef _WIN32
                auto attributes = GetFileAttributesW(native_path(entry.path).c_str());
                if (attributes == INVALID_FILE_ATTRIBUTES) error = native_error();
                else if (attributes & FILE_ATTRIBUTE_DIRECTORY) fs::create_directory_symlink(link, destination, error);
                else fs::create_symlink(link, destination, error);
#else
                fs::create_symlink(link, destination, error);
#endif
            }
            fail(result, error);
            if (!error) ++result.entries_processed;
        } else fail(result, ErrorKind::unsupported);
        if (result.error != ErrorKind::success) return;
    }
}

void create_parents(const fs::path& value, Result& result,
                    const std::atomic<bool>* cancellation) {
    auto current = value.root_path();
    for (const auto& component : value.relative_path()) {
        if (cancelled(cancellation)) return;
        current /= component;
        std::error_code error;
        if (fs::create_directory(current, error)) ++result.entries_processed;
        else if (error) {
            fail(result, error);
            return;
        } else if (!fs::is_directory(current, error)) {
            if (error) fail(result, error);
            else fail(result, ErrorKind::not_directory);
            return;
        }
    }
    if (!cancelled(cancellation)) real_directory(value, result);
}

void filesystem_operation(const Request& request, Result& result,
                          const std::atomic<bool>* cancellation) {
    if (request.hooks && request.hooks->before) request.hooks->before(request.operation);
    if (!valid_path(request.path) ||
        (!request.target.empty() && !valid_path(request.target))) {
        fail(result, ErrorKind::invalid_path);
        return;
    }
    auto path = vd::utf8_path(request.path);
    std::error_code error;
    switch (request.operation) {
    case Operation::read_all:
    case Operation::write_all:
        whole_file(request, result, cancellation);
        break;
    case Operation::exists: {
        auto status = fs::symlink_status(path, error);
        if (error == std::errc::no_such_file_or_directory) error.clear();
        result.number = fs::exists(status);
        fail(result, error);
        break;
    }
    case Operation::metadata:
        result.entries.push_back(metadata(path, request.first != 0, result));
        break;
    case Operation::copy_file:
        if (!valid_path(request.target)) fail(result, ErrorKind::invalid_path);
        else copy_file(request, result, cancellation);
        break;
    case Operation::move_file:
    case Operation::move_directory:
        if (!valid_path(request.target)) fail(result, ErrorKind::invalid_path);
        else rename(request, request.operation == Operation::move_directory, result);
        break;
    case Operation::remove_file:
        remove_one(path, false, request.first != 0, result);
        break;
    case Operation::create_directory: {
        bool created = fs::create_directory(path, error);
        fail(result, error);
        if (result.error == ErrorKind::success && !created) real_directory(path, result);
        result.entries_processed = created ? 1 : 0;
        break;
    }
    case Operation::create_directories:
        create_parents(path, result, cancellation);
        break;
    case Operation::remove_directory:
        remove_one(path, true, request.first != 0, result);
        break;
    case Operation::remove_tree:
        remove_tree(request, result, cancellation);
        break;
    case Operation::entries:
    case Operation::get_files:
    case Operation::get_directories:
        enumerate(request, result, cancellation);
        break;
    case Operation::copy_directory:
        if (!valid_path(request.target)) fail(result, ErrorKind::invalid_path);
        else copy_directory(request, result, cancellation);
        break;
    case Operation::read_link:
        result.text = output_path(fs::read_symlink(path, error));
        fail(result, error);
        break;
    case Operation::create_symlink:
        if (!valid_path(request.target)) fail(result, ErrorKind::invalid_path);
        else {
            if (request.first) fs::create_directory_symlink(vd::utf8_path(request.target), path, error);
            else fs::create_symlink(vd::utf8_path(request.target), path, error);
            fail(result, error);
            if (!error) result.entries_processed = 1;
        }
        break;
    default: throw std::logic_error("Invalid filesystem operation");
    }
}
}

Result run(const Request& request, const std::atomic<bool>* cancellation) {
    Result result;
    if (cancelled(cancellation)) return result;
    try {
        if (request.operation <= Operation::path_separator) {
            path_operation(request, result);
        } else if (request.operation >= Operation::read_bytes && request.operation <= Operation::close) {
            if (!request.file) fail(result, ErrorKind::closed);
            else file_operation(request, result, cancellation);
        } else {
            filesystem_operation(request, result, cancellation);
        }
    } catch (const fs::filesystem_error& error) {
        if (error.code() == std::errc::illegal_byte_sequence) fail(result, ErrorKind::invalid_path, error.code().value());
        else fail(result, error.code());
    }
    return result;
}

} // namespace vd::filesystem
