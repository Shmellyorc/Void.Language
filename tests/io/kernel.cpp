#include "void/filesystem.hpp"
#include "void/detail/filesystem_hooks.hpp"
#include "void/path.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
namespace kernel = vd::filesystem;
namespace fs = std::filesystem;
using kernel::Operation;
using kernel::ErrorKind;
using kernel::Request;
using kernel::Result;

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

Result perform(Operation operation, const fs::path& path = {}, const fs::path& target = {},
               std::int64_t first = 0, std::int64_t second = 0,
               std::int64_t third = 0, std::string bytes = {}, std::string pattern = "*") {
    Request request(operation);
    request.path = vd::path_utf8(path);
    request.target = vd::path_utf8(target);
    request.first = first;
    request.second = second;
    request.third = third;
    request.bytes = std::move(bytes);
    request.pattern = std::move(pattern);
    request.hooks = kernel::testing::current();
    return kernel::run(request);
}

void okay(const Result& result, const char* message) {
    if (result.error != ErrorKind::success) {
        throw std::runtime_error(std::string(message) + ": kind=" +
                                 std::to_string(static_cast<int>(result.error)) +
                                 " native=" + std::to_string(result.os_code));
    }
}

Result write(const fs::path& path, std::string bytes, kernel::OpenMode mode = kernel::OpenMode::write) {
    return perform(Operation::write_all, path, {}, static_cast<std::int64_t>(mode), 0, 0, std::move(bytes));
}

Result read(const fs::path& path, std::int64_t maximum = 67108864) {
    return perform(Operation::read_all, path, {}, maximum);
}

Result file(const std::shared_ptr<kernel::File>& resource, Operation operation,
            std::int64_t first = 0, std::int64_t second = 0, std::string bytes = {}) {
    Request request(operation);
    request.file = resource;
    request.first = first;
    request.second = second;
    request.bytes = std::move(bytes);
    return kernel::run(request);
}

void path_cases() {
    require(perform(Operation::path_normalize, "a/./b/../c").text == "a/c", "path normalization");
    require(perform(Operation::path_filename, "a/file.tar.gz").text == "file.tar.gz", "filename");
    require(perform(Operation::path_filename, "a/").text.empty(), "trailing separator filename");
    require(perform(Operation::path_stem, "a/file.tar.gz").text == "file.tar", "stem");
    require(perform(Operation::path_extension, ".config").text.empty(), "dotfile extension");
    require(perform(Operation::path_change_extension, "file.txt", "bin").text == "file.bin", "change extension");
    require(perform(Operation::path_change_extension, "file.txt").text == "file", "remove extension");
    require(perform(Operation::path_is_relative, "a").number == 1, "relative path predicate");
    auto base = fs::current_path();
    auto full = perform(Operation::path_absolute, "a/../b", base);
    okay(full, "absolute path");
    require(full.text == vd::path_utf8(base / "b"), "explicit absolute base");
    auto relative = perform(Operation::path_relative, base / "a/b", base);
    okay(relative, "relative path");
    require(relative.text == "a/b", "relative path result");
    Request invalid(Operation::path_normalize);
    invalid.path = std::string("a\0b", 3);
    require(kernel::run(invalid).error == ErrorKind::invalid_path, "path NUL rejection");
    invalid.path = std::string("\xc0\xaf", 2);
    require(kernel::run(invalid).error == ErrorKind::invalid_path, "path UTF-8 rejection");
#ifdef _WIN32
    require(perform(Operation::path_is_rooted, "C:relative").number == 1, "Windows rooted drive path");
    require(perform(Operation::path_is_absolute, "C:relative").number == 0, "Windows drive-relative path");
    require(perform(Operation::path_is_absolute, "C:/absolute").number == 1, "Windows absolute drive path");
    require(perform(Operation::path_root, "//server/share/a").text == "//server/share/", "Windows UNC root");
    require(perform(Operation::path_normalize, "//server/share/a/../../b").text == "//server/share/b",
            "Windows UNC share bounds normalization");
    require(perform(Operation::path_parent, "//server/share").text == "//server/share/",
            "Windows UNC root retains its share");
    require(perform(Operation::path_relative, "C:/Base/a", "c:/base").text == "a",
            "Windows ordinal-insensitive relative base");
    require(perform(Operation::path_relative, "//server/one/a", "//server/two").error == ErrorKind::invalid_path,
            "Windows incompatible UNC shares");
    require(perform(Operation::path_separator).text == "\\", "Windows preferred separator");
#else
    require(perform(Operation::path_root, "/a/b").text == "/", "POSIX root");
    require(perform(Operation::path_join, "/a", "/b").text == "/b", "rooted join replacement");
#endif
}

void stream_cases(const fs::path& root) {
    auto path = root / vd::utf8_path("stream 雪.bin");
    for (auto mode : {kernel::OpenMode::read, kernel::OpenMode::read_write}) {
        Result status;
        auto resource = kernel::open(vd::path_utf8(path), mode, status);
        require(!resource && status.error == ErrorKind::not_found, "existing-only modes");
    }
    okay(write(path, "abcdef"), "initial write");
    Result status;
    auto resource = kernel::open(vd::path_utf8(path), kernel::OpenMode::read_write, status);
    okay(status, "read/write open");
    require(kernel::testing::noninheritable(resource), "native file handle is inheritable");
    require(file(resource, Operation::read_bytes, 1).bytes == "a", "short byte read");
    require(file(resource, Operation::position).number == 1, "logical read-ahead position");
    okay(file(resource, Operation::write_bytes, 0, 0, "X"), "write after read-ahead");
    require(file(resource, Operation::position).number == 2, "write cursor");
    require(read(path).bytes == "aXcdef", "read-ahead overwrite alignment");
    require(file(resource, Operation::seek, -1, 2).number == 5, "end-relative seek");
    require(file(resource, Operation::read_exact, 2).error == ErrorKind::unexpected_eof, "exact EOF error");
    require(file(resource, Operation::position).number == 6, "exact EOF advances actual bytes");
    require(file(resource, Operation::read_bytes, 0).bytes.empty(), "zero read at EOF");
    require(file(resource, Operation::seek, -7, 1).error == ErrorKind::invalid_range, "negative seek");
    require(file(resource, Operation::position).number == 6, "invalid seek cursor recovery");
    okay(file(resource, Operation::set_length, 2), "truncate");
    require(file(resource, Operation::position).number == 6, "truncate preserves cursor");
    okay(file(resource, Operation::set_length, 8), "extend");
    require(file(resource, Operation::length).number == 8, "extended length");
    require(read(path).bytes == std::string("aX\0\0\0\0\0\0", 8), "zero extension");
    okay(file(resource, Operation::flush), "flush");
    okay(file(resource, Operation::close), "close");
    okay(file(resource, Operation::close), "double close");
    require(file(resource, Operation::length).error == ErrorKind::closed, "closed alias invalidation");
    for (auto mode : {kernel::OpenMode::append, kernel::OpenMode::open_or_create}) {
        status = {};
        resource = kernel::open(vd::path_utf8(path), mode, status);
        okay(status, "append/open-or-create open");
        require(file(resource, Operation::length).number == 8, "nontruncating open");
        okay(file(resource, Operation::seek, 0), "append seek");
        okay(file(resource, Operation::write_bytes, 0, 0, "Z"), "append/overwrite write");
        require(file(resource, Operation::position).number == (mode == kernel::OpenMode::append ? 9 : 1),
                "append position selects EOF");
        okay(file(resource, Operation::close), "mode close");
        okay(write(path, "12345678"), "reset modes");
    }
    status = {};
    require(!kernel::open(vd::path_utf8(path), kernel::OpenMode::create_new, status) &&
            status.error == ErrorKind::already_exists, "exclusive creation");
    okay(write(path, ""), "truncate mode");
    require(read(path).bytes.empty(), "write truncates existing data");
    okay(write(path, std::string(65535, 'x') + "\r\n\nlast\r"), "line fixture");
    status = {};
    resource = kernel::open(vd::path_utf8(path), kernel::OpenMode::read, status);
    okay(status, "line open");
    auto line = file(resource, Operation::read_line, 70000);
    require(line.bytes == std::string(65535, 'x') && line.completed == 65537 && !line.eof,
            "CRLF crossing native buffer boundary");
    line = file(resource, Operation::read_line, 10);
    require(line.bytes.empty() && !line.eof, "empty line differs from EOF");
    require(file(resource, Operation::read_line, 10).bytes == "last", "final CR line");
    require(file(resource, Operation::read_line, 10).eof, "line EOF");
    okay(file(resource, Operation::seek, 0), "line recovery seek");
    line = file(resource, Operation::read_line, 2);
    require(line.error == ErrorKind::limit_exceeded && line.bytes == "xx" && line.completed == 2,
            "bounded line preserves partial bytes and cursor");
    require(file(resource, Operation::write_bytes, 0, 0, "x").error == ErrorKind::invalid_mode,
            "read-only native mode");
    okay(file(resource, Operation::close), "line close");
}

void transfer_failures(const fs::path& root) {
    auto path = root / "transfer failures.bin";
    {
        auto hooks = std::make_shared<kernel::testing::Hooks>();
        hooks->read_chunk = 1;
        hooks->write_chunk = 1;
        kernel::testing::Observe observe(hooks);
        okay(write(path, "short transfer"), "short write loop");
        require(read(path).bytes == "short transfer", "short read loop");
    }
    {
        auto hooks = std::make_shared<kernel::testing::Hooks>();
        hooks->zero_write = true;
        kernel::testing::Observe observe(hooks);
        auto result = write(path, "never written");
        require(result.error == ErrorKind::io_failure && result.completed == 0,
                "zero write progress must fail instead of looping");
    }
    unsigned closes = 0;
    {
        auto hooks = std::make_shared<kernel::testing::Hooks>();
        hooks->write_chunk = 1;
        hooks->write_failure_after = 3;
        hooks->close_error = EIO;
        hooks->before = [&](Operation operation) {
            if (operation == Operation::close) ++closes;
        };
        kernel::testing::Observe observe(hooks);
        auto result = write(path, "abcdef");
        require(result.error == ErrorKind::io_failure && result.completed == 3 &&
                result.os_code == EIO && result.cleanup_code == EIO,
                "partial write or accompanying close diagnostic lost");
    }
    require(closes == 1 && read(path).bytes == "abc", "native close must retire once after partial failure");
    {
        auto hooks = std::make_shared<kernel::testing::Hooks>();
        hooks->close_error = EIO;
        kernel::testing::Observe observe(hooks);
        auto result = write(path, "complete");
        require(result.error == ErrorKind::io_failure && result.os_code == EIO && result.completed == 8,
                "successful transfer must report its later close failure");
    }
}

void directory_cases(const fs::path& root) {
    auto source = root / "tree";
    auto target = root / "copy";
    okay(perform(Operation::create_directories, source / "nested"), "recursive directories");
    okay(write(source / "a.txt", "alpha"), "tree file");
    okay(write(source / "nested" / "b.txt", "beta"), "nested file");
    okay(write(source / vd::utf8_path("雪.txt"), "unicode"), "Unicode file");
    okay(write(source / "other.bin", "binary"), "unmatched file");
    require(perform(Operation::exists, source / "missing").number == 0, "missing existence");
    auto selected = perform(Operation::get_files, source, {}, 1, 100, 0, {}, "?.txt");
    okay(selected, "recursive filtered files");
    require(selected.entries.size() == 3, "Unicode scalar wildcard or recursive filtering");
    auto directories = perform(Operation::get_directories, source, {}, 1, 100, 0, {}, "nest*");
    okay(directories, "filtered directories");
    require(directories.entries.size() == 1, "directory name pattern");
    require(perform(Operation::entries, source, {}, 1, 1).error == ErrorKind::limit_exceeded,
            "enumeration total-entry budget");
    require(perform(Operation::remove_directory, source).error == ErrorKind::not_empty,
            "empty-only directory removal");
    require(perform(Operation::remove_file, source).error == ErrorKind::is_directory,
            "file removal rejects directory");
    require(perform(Operation::copy_directory, source, source / "inside", 1, 0, 100).error == ErrorKind::invalid_path,
            "copy into descendant rejected");
    require(perform(Operation::copy_directory, source, target, 0, 0, 100).error != ErrorKind::success &&
            !fs::exists(target), "nonrecursive copy cannot silently omit children");
    okay(perform(Operation::copy_directory, source, target, 1, 0, 100), "recursive directory copy");
    require(read(target / "nested" / "b.txt").bytes == "beta", "copied descendant contents");
    require(perform(Operation::copy_directory, source, target, 1, 0, 100).error == ErrorKind::already_exists,
            "directory collision policy");
    okay(write(target / "keep.txt", "keep"), "merge unrelated entry");
    okay(perform(Operation::copy_directory, source, target, 1, 1, 100), "directory merge");
    require(read(target / "keep.txt").bytes == "keep", "merge preserves unrelated entries");
    okay(perform(Operation::copy_file, source / "a.txt", root / "one.txt"), "file copy");
    require(perform(Operation::copy_file, source / "a.txt", root / "one.txt").error == ErrorKind::already_exists,
            "file copy exclusive collision");
    require(perform(Operation::copy_file, source / "a.txt", source / "a.txt", 1).error == ErrorKind::invalid_path &&
            read(source / "a.txt").bytes == "alpha", "copy onto itself cannot truncate");
    okay(write(root / "occupied.txt", "occupied"), "rename collision fixture");
    require(perform(Operation::move_file, root / "one.txt", root / "occupied.txt").error == ErrorKind::already_exists,
            "native no-replace rename");
    require(read(root / "occupied.txt").bytes == "occupied", "exclusive rename preserves target");
    okay(perform(Operation::move_file, root / "one.txt", root / "occupied.txt", 1), "replacement rename");
    require(read(root / "occupied.txt").bytes == "alpha", "replacement rename contents");
    okay(perform(Operation::move_directory, target, root / "moved"), "directory rename");
    require(perform(Operation::remove_tree, root / "moved", {}, 0, 2).error == ErrorKind::limit_exceeded &&
            fs::exists(root / "moved" / "keep.txt"), "recursive removal budget before mutation");
    okay(perform(Operation::remove_tree, root / "moved", {}, 0, 100), "explicit recursive removal");
    require(!fs::exists(root / "moved"), "removed tree");
    auto metadata = perform(Operation::metadata, source / "a.txt");
    okay(metadata, "metadata");
    require(metadata.entries.size() == 1 && metadata.entries[0].size == 5 &&
            metadata.entries[0].kind == kernel::EntryKind::file &&
            metadata.entries[0].modified_nanoseconds >= 0 &&
            metadata.entries[0].modified_nanoseconds < 1000000000, "file metadata values");
    okay(perform(Operation::remove_file, root / "occupied.txt"), "file deletion");
    okay(perform(Operation::remove_file, root / "occupied.txt", {}, 1), "missing-ok deletion");
#ifndef _WIN32
    auto outside = root / "outside";
    okay(perform(Operation::create_directory, outside), "outside directory");
    okay(write(outside / "retained", "safe"), "outside file");
    okay(perform(Operation::create_symlink, source / "link", outside, 1), "directory symlink");
    auto entries = perform(Operation::entries, source, {}, 1, 100);
    okay(entries, "nofollow enumeration");
    require(std::none_of(entries.entries.begin(), entries.entries.end(), [](const auto& entry) {
        return entry.name == "retained";
    }), "enumeration followed symlink");
    require(perform(Operation::metadata, source / "link").entries[0].kind == kernel::EntryKind::symlink,
            "link metadata");
    require(perform(Operation::metadata, source / "link", {}, 1).entries[0].kind == kernel::EntryKind::directory,
            "explicit followed metadata");
    require(perform(Operation::read_link, source / "link").text == vd::path_utf8(outside), "read link");
    okay(perform(Operation::copy_directory, source, target, 1, 0, 100), "copy tree preserves link");
    require(fs::is_symlink(target / "link"), "copied symbolic link");
    okay(perform(Operation::remove_tree, target, {}, 0, 100), "remove copied linked tree");
    okay(perform(Operation::remove_tree, source, {}, 0, 100), "remove linked tree");
    require(read(outside / "retained").bytes == "safe", "recursive removal followed link target");
#endif
}

#ifndef _WIN32
void short_read_case(const fs::path& root) {
    auto pipe = root / "short.fifo";
    require(::mkfifo(pipe.c_str(), 0600) == 0, "create FIFO fixture");
    std::thread writer([&] {
        auto descriptor = ::open(pipe.c_str(), O_WRONLY | O_CLOEXEC);
        if (descriptor < 0) return;
        for (char byte : std::string("short reads")) {
            if (::write(descriptor, &byte, 1) != 1) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        ::close(descriptor);
    });
    auto result = read(pipe, 100);
    writer.join();
    okay(result, "short FIFO reads");
    require(result.bytes == "short reads", "whole-file read stopped before actual EOF");
}

void native_failure_cases(const fs::path& root) {
    auto previous = fs::current_path();
    auto deleted_cwd = root / "deleted cwd";
    fs::create_directory(deleted_cwd);
    fs::current_path(deleted_cwd);
    fs::remove(deleted_cwd);
    auto explicit_absolute = perform(Operation::path_absolute, "asset.bin", root);
    auto explicit_relative = perform(Operation::path_relative, root / "asset.bin", root);
    auto implicit_absolute = perform(Operation::path_absolute, "asset.bin");
    fs::current_path(previous);
    require(explicit_absolute.text == vd::path_utf8(root / "asset.bin") &&
            explicit_absolute.error == ErrorKind::success &&
            explicit_relative.text == "asset.bin" && explicit_relative.error == ErrorKind::success,
            "explicit-base lexical paths unnecessarily require a working directory");
    require(implicit_absolute.error == ErrorKind::not_found,
            "missing implicit working directory must remain recoverable");

    auto protected_file = root / "permission.bin";
    okay(write(protected_file, "private"), "permission fixture");
    require(::chmod(protected_file.c_str(), 0) == 0, "remove file permissions");
    auto denied = read(protected_file);
    require(::chmod(protected_file.c_str(), 0600) == 0, "restore file permissions");
    require(denied.error == ErrorKind::permission_denied && denied.os_code != 0,
            "permission denial collapsed into absence or success");
    auto protected_directory = root / "permission directory";
    fs::create_directory(protected_directory);
    require(::chmod(protected_directory.c_str(), 0) == 0, "remove directory permissions");
    auto listed = perform(Operation::entries, protected_directory, {}, 0, 100);
    require(::chmod(protected_directory.c_str(), 0700) == 0, "restore directory permissions");
    require(listed.error == ErrorKind::permission_denied, "enumeration ignored permission failure");

    auto original = root / "original.bin";
    auto alias = root / "hard link.bin";
    okay(write(original, "preserved"), "hard-link fixture");
    fs::create_hard_link(original, alias);
    require(perform(Operation::copy_file, original, alias, 1).error == ErrorKind::invalid_path &&
            read(original).bytes == "preserved", "hard-link replacement truncated source");
    auto link = root / "destination link.bin";
    fs::create_symlink(original, link);
    require(perform(Operation::copy_file, protected_file, link, 1).error != ErrorKind::success &&
            read(original).bytes == "preserved", "replacement copy followed destination link");

    auto before_invalid = perform(Operation::entries, root, {}, 0, 100);
    okay(before_invalid, "enumerate before invalid filename creation");
    auto invalid = root / std::string("invalid-\xff", 9);
    auto descriptor = ::open(invalid.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    auto creation_error = descriptor < 0 ? errno : 0;
    std::cout << "invalid native filename fixture: created=" << (descriptor >= 0)
              << " errno=" << creation_error << '\n';
    if (descriptor >= 0) {
        require(::close(descriptor) == 0, "close invalid filename fixture");
        struct stat actual{};
        require(::lstat(invalid.c_str(), &actual) == 0 && S_ISREG(actual.st_mode),
                "invalid filename fixture was not created with its exact native bytes");
        require(perform(Operation::entries, root, {}, 0, 100).error == ErrorKind::invalid_path,
                "invalid native filename escaped UTF-8 validation");
        require(::unlink(invalid.c_str()) == 0, "remove invalid filename fixture");
    } else {
#ifdef __APPLE__
        // APFS can reject malformed UTF-8 at creation. There is then no invalid
        // entry to enumerate. Lookup may report absence instead of rechecking
        // encoding; neither successful lookup nor an unrelated error is valid.
        require(creation_error == EILSEQ, "unexpected native invalid-filename creation failure");
        struct stat absent{};
        errno = 0;
        auto lookup_status = ::lstat(invalid.c_str(), &absent);
        auto lookup_error = errno;
        std::cout << "invalid native filename lookup: status=" << lookup_status
                  << " errno=" << lookup_error << '\n';
        require(lookup_status < 0 && (lookup_error == ENOENT || lookup_error == EILSEQ),
                "invalid native filename lookup must report absence or invalid encoding");
        auto after_invalid = perform(Operation::entries, root, {}, 0, 100);
        okay(after_invalid, "enumerate after native filename rejection");
        std::vector<std::string> before_names, after_names;
        for (const auto& entry : before_invalid.entries) before_names.push_back(entry.name);
        for (const auto& entry : after_invalid.entries) after_names.push_back(entry.name);
        std::sort(before_names.begin(), before_names.end());
        std::sort(after_names.begin(), after_names.end());
        require(before_names == after_names, "rejected filename creation changed directory entries");
#else
        require(false, "could not create invalid native filename fixture");
#endif
    }
    // VOID's own validation must reject malformed input before any OS lookup,
    // on both byte-preserving filesystems and filesystems which reject creation.
    require(perform(Operation::metadata, invalid).error == ErrorKind::invalid_path,
            "invalid input path escaped UTF-8 validation");
}
#endif
}

int main(int argc, char** argv) {
    try {
        require(argc == 2, "fixture directory required");
        auto root = fs::absolute(vd::utf8_path(argv[1]));
        fs::remove_all(root);
        fs::create_directories(root);
        path_cases();
        stream_cases(root);
        transfer_failures(root);
        directory_cases(root);
        auto large = std::string(3 * 1024 * 1024 + 17, 'x');
        for (std::size_t n = 0; n < large.size(); ++n) large[n] = static_cast<char>(n % 251);
        okay(write(root / "large.bin", large), "large streamed write");
        require(read(root / "large.bin").bytes == large, "large streamed byte oracle");
        auto bounded = read(root / "large.bin", 7);
        require(bounded.error == ErrorKind::limit_exceeded && bounded.bytes == large.substr(0, 7) &&
                bounded.completed == 7, "whole-file safe budget and partial progress");
#ifndef _WIN32
        short_read_case(root);
        native_failure_cases(root);
#endif
        fs::remove_all(root);
        std::cout << "native filesystem kernel: paths, modes, cursor/read-ahead, EOF, lines, "
                     "growth, metadata, filtered recursion, copy/move/delete, symlinks, budgets "
                     "and 3 MiB streaming passed\n";
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
