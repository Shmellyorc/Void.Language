#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace vd::audit {
struct Finding { std::string code, detail; std::uint64_t offset = 0; };
struct Policy {
    unsigned revision = 1; // v2 permits required dynamic symbols, rejects static symbols
    std::string target; // exactly linux-x64, windows-x64 or macos-arm64
    bool reject_debug = true, reject_symbols = true, reject_loader_paths = true;
    std::vector<std::string> forbidden_strings;
};
struct Report {
    unsigned schema = 1;
    bool inspected = false; // structural inspection, never closure/protection approval
    std::string format, architecture, image_kind, sha256, interpreter;
    std::vector<std::string> dependencies, versions, loader_paths, exports;
    bool symbols = false, debug = false;
    bool static_symbols = false, dynamic_symbols = false, unwind = false;
    std::vector<Finding> findings;
};
inline constexpr std::size_t byte_limit = 64 * 1024 * 1024;
inline constexpr std::size_t record_limit = 4096;
// Owned report from caller-owned bytes. No file opening, loading, execution,
// subprocesses, environment access or mutation. ELF64LE, PE32+ and thin Mach-O64LE
// declarations only. Archives/universal images and other variants fail closed.
Report inspect(std::string_view bytes, const Policy&);
std::string json(const Report&);
// Remove LLD's CodeView and reproducibility records after LLVM strips DWARF.
// Changes only those data records and their PE directory declaration.
void remove_pe_debug_records(std::string&);
}
