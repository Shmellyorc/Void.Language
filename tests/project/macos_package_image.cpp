#include "../../src/native_macos.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>

namespace {
using namespace llvm::MachO;
template<class T> void append(std::string& bytes, const T& value) {
    bytes.append(reinterpret_cast<const char*>(&value), sizeof value);
}
std::string specimen(std::string_view dependency = "/usr/lib/libSystem.B.dylib",
                     std::uint32_t platform = PLATFORM_MACOS, bool rpath = false) {
    std::string commands;
    segment_command_64 segment{};
    segment.cmd = LC_SEGMENT_64; segment.cmdsize = sizeof segment;
    std::memcpy(segment.segname, "__TEXT", 6);
    segment.vmaddr = 0x100000000; segment.vmsize = 0x4000;
    segment.maxprot = segment.initprot = VM_PROT_READ | VM_PROT_EXECUTE;
    append(commands, segment);
    append(commands, build_version_command{LC_BUILD_VERSION, sizeof(build_version_command), platform, 0x0b0000, 0x0b0000, 0});
    // The code byte is immediately after the commands; set its file offset below.
    append(commands, entry_point_command{LC_MAIN, sizeof(entry_point_command), 0, 0});
    auto string_command = [&](auto command, std::string_view name) {
        command.cmdsize = (sizeof command + name.size() + 1 + 7) & ~std::size_t(7);
        append(commands, command);
        commands.append(name);
        commands.append(command.cmdsize - sizeof command - name.size(), '\0');
    };
    string_command(dylinker_command{LC_LOAD_DYLINKER, 0, sizeof(dylinker_command)}, "/usr/lib/dyld");
    string_command(dylib_command{LC_LOAD_DYLIB, 0, {sizeof(dylib_command), 0, 0, 0}}, dependency);
    if (rpath) string_command(rpath_command{LC_RPATH, 0, sizeof(rpath_command)}, "@executable_path/native");
    const std::uint64_t entry = sizeof(mach_header_64) + commands.size();
    std::memcpy(commands.data() + sizeof(segment) + sizeof(build_version_command) + offsetof(entry_point_command, entryoff), &entry, sizeof entry);
    segment.filesize = entry + 4;
    std::memcpy(commands.data(), &segment, sizeof segment);
    std::string bytes;
    append(bytes, mach_header_64{MH_MAGIC_64, CPU_TYPE_ARM64, CPU_SUBTYPE_ARM64_ALL, MH_EXECUTE,
                               rpath ? 6u : 5u, static_cast<std::uint32_t>(commands.size()), MH_DYLDLINK | MH_PIE, 0});
    return bytes + commands + std::string(4, '\0');
}
void rejects(const std::string& bytes) {
    try { vd::native_macos::check_package_image(bytes); }
    catch (const std::runtime_error&) { return; }
    throw std::runtime_error("Unsafe Mach-O application package image was accepted");
}
}
int main(int argc, char** argv) { try {
    vd::native_macos::check_package_image(specimen());
    vd::native_macos::check_package_image(specimen("/System/Library/Frameworks/Foundation.framework/Versions/C/Foundation"));
    for (auto name : {"@rpath/libsecret.dylib", "@loader_path/libsecret.dylib", "/usr/local/lib/secret.dylib",
                      "/tmp/libsecret.dylib", "/usr/lib/../../tmp/secret.dylib", "/usr/lib//libSystem.B.dylib",
                      "/System/Library/Frameworks/../PrivateFrameworks/secret", "/usr/library/secret"})
        rejects(specimen(name));
    rejects(specimen("/usr/lib/libSystem.B.dylib", PLATFORM_IOS));
    rejects(specimen("/usr/lib/libSystem.B.dylib", PLATFORM_MACOS, true));
    auto wrong = specimen();
    const auto modify = [&](std::size_t offset, std::uint32_t value) {
        wrong = specimen(); std::memcpy(wrong.data() + offset, &value, sizeof value); rejects(wrong);
    };
    modify(offsetof(mach_header_64, cputype), CPU_TYPE_X86_64);
    modify(offsetof(mach_header_64, filetype), MH_DYLIB);
    modify(offsetof(mach_header_64, sizeofcmds), 0xffffffff);
    modify(sizeof(mach_header_64) + offsetof(segment_command_64, initprot), VM_PROT_READ);
    rejects(specimen().substr(0, 31));
    rejects("not a Mach-O executable");
    for (int i = 1; i < argc; ++i) {
        std::ifstream file(argv[i], std::ios::binary);
        if (!file) throw std::runtime_error("Cannot read native image fixture");
        const std::string bytes{std::istreambuf_iterator<char>(file), {}};
        vd::native_macos::check_package_image(bytes);
    }
    std::cout << "Mach-O package architecture, platform, loader and dependency checks passed\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; } }
