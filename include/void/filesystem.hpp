#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace vd::filesystem {
namespace testing { struct Hooks; }

// Native-only kernel shared by synchronous ABI calls and bounded native jobs.
// No member borrows a managed payload or carries generated source callbacks.
enum class ErrorKind : std::int64_t {
    success, not_found, permission_denied, already_exists, invalid_path,
    invalid_mode, invalid_range, closed, busy, unexpected_eof, invalid_utf8,
    unsupported, limit_exceeded, cross_device, not_directory, is_directory,
    not_empty, io_failure
};

enum class Operation : unsigned {
    path_join, path_normalize, path_filename, path_stem, path_extension,
    path_change_extension, path_parent, path_root, path_is_absolute,
    path_is_relative, path_is_rooted, path_absolute, path_relative, path_separator,
    open, read_bytes, read_exact, read_line, write_bytes, seek, position, length,
    set_length, flush, close, read_all, write_all, exists, metadata, copy_file,
    move_file, remove_file, create_directory, create_directories, remove_directory,
    remove_tree, entries, get_files, get_directories, copy_directory,
    move_directory, read_link, create_symlink
};

enum class EntryKind : std::int64_t { file, directory, symlink, other };
enum class OpenMode : std::int64_t {
    read, write, append, read_write, create, open_or_create, create_new
};

struct Entry {
    std::string path;
    std::string name;
    EntryKind kind = EntryKind::other;
    std::int64_t size = 0;
    std::int64_t modified_seconds = 0;
    std::int64_t modified_nanoseconds = 0;
};

struct Result {
    ErrorKind error = ErrorKind::success;
    std::int64_t os_code = 0;
    std::int64_t completed = 0;
    std::int64_t cleanup_code = 0;
    std::int64_t entries_processed = 0;
    std::int64_t number = 0;
    bool eof = false;
    std::string text;
    std::string bytes;
    std::vector<Entry> entries;
};

class File;
struct Request {
    Operation operation;
    std::string path;
    std::string target;
    std::string pattern;
    std::string bytes;
    std::int64_t first = 0;
    std::int64_t second = 0;
    std::int64_t third = 0;
    std::int64_t fourth = 0;
    std::shared_ptr<File> file;
    std::shared_ptr<const testing::Hooks> hooks;

    explicit Request(Operation selected) : operation(selected) {}
};

// Run only with exclusive file-operation authority. Cancellation is observed
// between native transfers/entries; an entered syscall is drained to completion.
Result run(const Request&, const std::atomic<bool>* cancellation = nullptr);
// Native configuration snapshots: regular files only, bounded before growth.
// POSIX nonblocking open prevents a FIFO/device from trapping a transport owner.
Result read_regular(const std::string&, std::int64_t max_bytes);
std::shared_ptr<File> open(const std::string&, OpenMode, Result&,
                           std::shared_ptr<const testing::Hooks> = {});
Result close(File&);
// Internal held-descriptor bridge for admitted directory capabilities only.
// adopt_descriptor consumes descriptor even if allocation fails.
std::shared_ptr<File> adopt_descriptor(int descriptor, bool writable);
int descriptor(const File&);
Result private_mode(File&, bool executable);
Result private_mode_matches(File&, bool executable);
// Exclusive FileBorrow required. Positioned access preserves the ordinary IO
// cursor and read buffer on every platform; never wraps a CRT fd around HANDLE.
Result read_at(File&, std::uint64_t offset, char* output, std::size_t count);
Result regular_length(File&);
#ifdef _WIN32
std::shared_ptr<File> adopt_native_handle(void* handle, bool writable);
void* native_handle(const File&);
#endif

} // namespace vd::filesystem
