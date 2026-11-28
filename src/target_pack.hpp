#pragma once
#include "void/publish_target.hpp"
#include <filesystem>
#include <map>
#include <span>
#include <string>

namespace vd::publish {
inline constexpr std::string_view pack_manifest = "target-pack.txt";
inline constexpr std::array linux_start{"Scrt1.o", "crti.o", "crtbeginS.o"};
inline constexpr std::array linux_archives{"libvoid_runtime.a", "libvoid_native_fixture.a", "libarchive.a",
    "libsodium.a", "libcurl.a", "libpsl.a", "libnghttp2.a", "libssl.a", "libcrypto.a", "libcares.a", "libz.a"};
inline constexpr std::array linux_libraries{"libidn2.so.0", "libunistring.so.5", "libstdc++.so.6", "libm.so.6",
    "libgcc.a", "libgcc_s.so.1", "libc.so.6"};
inline constexpr std::array linux_end{"crtendS.o", "crtn.o"};

inline constexpr std::array windows_archives{"void_runtime.lib", "void_native_fixture.lib", "archive.lib",
    "libsodium.lib", "curl.lib", "psl.lib", "nghttp2.lib", "cares.lib", "zs.lib"};
inline constexpr std::array windows_libraries{"libcmt.lib", "libvcruntime.lib", "libcpmt.lib", "libucrt.lib",
    "oldnames.lib", "legacy_stdio_definitions.lib", "kernel32.lib", "shell32.lib", "ole32.lib", "uuid.lib",
    "ws2_32.lib", "crypt32.lib", "secur32.lib", "normaliz.lib", "iphlpapi.lib", "advapi32.lib", "bcrypt.lib"};

inline constexpr const char* linux_loader(const Target& target) {
    return target.architecture == Architecture::arm64 ? "ld-linux-aarch64.so.1" : "ld-linux-x86-64.so.2";
}
struct Pack {
    const Target* target;
    std::string id, revision, version, minimum, digest;
    // One verified byte snapshot. Do not reopen link inputs by their original paths.
    std::map<std::string, std::string> files;
};
std::filesystem::path pack_store();
Pack read_pack(const std::filesystem::path&);
Pack load_pack(std::string_view id);
int pack_command(std::span<const std::string_view>);
}
