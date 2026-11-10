#include "void/c_primitive.hpp"
#include "void/path.hpp"
#include "void/runtime.hpp"

#include <algorithm>
#include <atomic>
#include <stdexcept>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#elif defined(__linux__)
#include <dlfcn.h>
#include <unistd.h>
#endif

#if defined(__linux__) && defined(__GLIBC__) && defined(__x86_64__)
#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <linux/magic.h>
#include <sodium.h>
#include <sys/mman.h>
#include <sys/auxv.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/vfs.h>

#include <cerrno>
#include <cstring>
#include <cstdio>
#include <map>
#include <set>
#include <utility>
#endif

namespace vd {

namespace {
[[noreturn]] void denied(std::string_view stage, std::string_view reason) {
    throw std::runtime_error("C dynamic " + std::string(stage) + ": " + std::string(reason));
}

#if defined(_WIN32) || defined(__linux__)
class OsModule {
#ifdef _WIN32
    HMODULE handle_;
#else
    void* handle_;
#endif
public:
    explicit OsModule(const std::filesystem::path& path) {
        if (!std::filesystem::is_regular_file(path))
            denied("library", "missing or unreadable library " + path_utf8(path));
#ifdef _WIN32
        // Scope filesystem search to the DLL's directory and System32.
        // The ordinary loader still applies its loaded-module and redirection rules.
        // Return loader failures to VOID instead of opening a modal error dialog.
        DWORD previous_mode;
        if (!SetThreadErrorMode(GetThreadErrorMode() | SEM_FAILCRITICALERRORS, &previous_mode))
            denied("load", "cannot set thread error mode (Windows error " + std::to_string(GetLastError()) + ')');
        handle_ = LoadLibraryExW(path.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        const auto code = GetLastError();
        SetThreadErrorMode(previous_mode, nullptr);
        if (!handle_) {
            denied("load", path_utf8(path) + " (Windows error " + std::to_string(code) +
                "; check architecture, dependencies and initialization)");
        }
#else
        handle_ = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle_) {
            const auto* message = dlerror();
            denied("load", path_utf8(path) + ": " + (message ? message : "dlopen failed"));
        }
#endif
    }
    OsModule(const OsModule&) = delete;
    OsModule& operator=(const OsModule&) = delete;
    ~OsModule() {
#ifdef _WIN32
        FreeLibrary(handle_);
#else
        dlclose(handle_);
#endif
    }
    CPrimitiveThunk* find(const std::filesystem::path& path, const std::string& symbol) const {
#ifdef _WIN32
        auto address = GetProcAddress(handle_, symbol.c_str());
#else
        dlerror();
        auto address = dlsym(handle_, symbol.c_str());
        if (dlerror()) address = nullptr;
#endif
        if (!address) denied("export", path_utf8(path) + " does not provide " + symbol);
        return reinterpret_cast<CPrimitiveThunk*>(address);
    }
};
#endif

#if defined(__linux__) && defined(__GLIBC__) && defined(__x86_64__)
constexpr std::uint64_t page = 4096;
constexpr std::size_t image_limit = 16 * 1024 * 1024;
constexpr std::size_t symbol_limit = 512;
constexpr std::size_t relocation_limit = 4096;

struct File {
    int fd;
    explicit File(int value) : fd(value) {}
    File(File&& other) noexcept : fd(std::exchange(other.fd, -1)) {}
    File& operator=(File&& other) noexcept {
        if (fd >= 0) close(fd);
        fd = std::exchange(other.fd, -1);
        return *this;
    }
    ~File() { if (fd >= 0) close(fd); }
};
struct Module {
    void* handle;
    explicit Module(void* value) : handle(value) {}
    Module(Module&& other) noexcept : handle(std::exchange(other.handle, nullptr)) {}
    ~Module() { if (handle) dlclose(handle); }
};
struct Mapped {
    File file;
    struct stat identity;
    Module module;
    link_map* link;
};

File proc(const char* name) {
    File file(open(name, O_RDONLY | O_CLOEXEC));
    struct statfs fs{};
    if (file.fd < 0 || fstatfs(file.fd, &fs) || fs.f_type != PROC_SUPER_MAGIC)
        denied("namespace", "required process view is unavailable");
    return file;
}

std::string read_bounded(int fd, std::size_t limit) {
    std::string result;
    char buffer[8192];
    for (;;) {
        const auto n = read(fd, buffer, sizeof buffer);
        if (n == 0) return result;
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 || static_cast<std::size_t>(n) > limit - result.size())
            denied("read", "native input is unavailable or exceeds its limit");
        result.append(buffer, static_cast<std::size_t>(n));
    }
}
bool loader_setting(std::string_view setting) {
    return setting.starts_with("LD_") || setting.starts_with("GLIBC_TUNABLES=");
}
bool c_symbol(std::string_view name) {
    auto letter = [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; };
    return !name.empty() && letter(name.front()) &&
        std::all_of(name.begin(), name.end(), [&](char c) { return letter(c) || (c >= '0' && c <= '9'); });
}
void launch_policy() {
    errno = 0;
    const auto secure = getauxval(AT_SECURE);
    if (secure || errno || sysconf(_SC_PAGESIZE) != page)
        denied("namespace", "unsupported page geometry or secure execution mode");
    struct stat preload{};
    if (lstat("/etc/ld.so.preload", &preload) == 0 || errno != ENOENT)
        denied("namespace", "system loader preload configuration is unsupported");
    for (auto p = ::environ; p && *p; ++p)
        if (loader_setting(*p)) denied("namespace", "ambient loader environment");
    auto environment = proc("/proc/self/environ");
    auto startup = read_bounded(environment.fd, 4 * 1024 * 1024);
    std::string_view entries = startup;
    while (!entries.empty()) {
        auto end = entries.find('\0');
        if (end == entries.npos || loader_setting(entries.substr(0, end)))
            denied("namespace", "initial loader environment");
        entries.remove_prefix(end + 1);
    }
}

File root_directory(const std::filesystem::path& root) {
    if (!root.is_absolute() || root != root.lexically_normal() || root.native().find('\0') != std::string::npos ||
        root.native().size() > 4096)
        denied("path", "expected a bounded canonical absolute root");
    constexpr auto flags = O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC;
    File directory(open("/", flags));
    for (const auto& part : root.relative_path()) {
        if (directory.fd < 0 || part.empty() || part == "." || part == "..")
            denied("path", "invalid root component");
        directory = File(openat(directory.fd, part.c_str(), flags));
    }
    if (directory.fd < 0) denied("path", "root is unavailable or aliased");
    return directory;
}

std::string approved_bytes(int directory, const DynamicClosure::Image& image) {
    File file(openat(directory, image.name.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC));
    struct stat before{}, after{};
    if (file.fd < 0 || fstat(file.fd, &before) || !S_ISREG(before.st_mode) || before.st_nlink != 1 ||
        before.st_size <= 0 || static_cast<std::uint64_t>(before.st_size) > image_limit)
        denied("artifact", image.name + " is missing, aliased or outside file bounds");
    auto bytes = read_bounded(file.fd, image_limit);
    if (fstat(file.fd, &after) || after.st_nlink != 1 || before.st_size != after.st_size ||
        static_cast<std::uint64_t>(after.st_size) != bytes.size())
        denied("artifact", image.name + " changed while captured");
    unsigned char hash[crypto_hash_sha256_BYTES];
    char hex[crypto_hash_sha256_BYTES * 2 + 1];
    if (crypto_hash_sha256(hash, reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size()) != 0 ||
        sodium_bin2hex(hex, sizeof hex, hash, sizeof hash) == nullptr || image.sha256 != hex)
        denied("digest", image.name);
    return bytes;
}

template<class T>
T read_at(std::string_view bytes, std::uint64_t at) {
    if (at > bytes.size() || sizeof(T) > bytes.size() - at) denied("ELF", "truncated record");
    T value;
    std::memcpy(&value, bytes.data() + at, sizeof value);
    return value;
}

struct Elf {
    std::string_view bytes;
    std::vector<Elf64_Phdr> loads;
    std::vector<Elf64_Sym> symbols;
    std::vector<std::string> names;
    std::vector<Elf64_Rela> relocations;
    std::map<std::string, unsigned, std::less<>> definitions;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> metadata;

    const Elf64_Phdr* segment(std::uint64_t address, std::uint64_t size, unsigned flags, bool file) const {
        for (const auto& p : loads) {
            const auto extent = file ? p.p_filesz : p.p_memsz;
            if (address >= p.p_vaddr && address - p.p_vaddr <= extent &&
                size <= extent - (address - p.p_vaddr) && (p.p_flags & flags) == flags) return &p;
        }
        return nullptr;
    }
    std::uint64_t offset(std::uint64_t address, std::uint64_t size, bool table = true) {
        auto p = segment(address, size, PF_R, true);
        if (!p) denied("ELF", "dynamic table is outside a file-backed segment");
        if (table && size) metadata.emplace_back(address, address + size);
        return p->p_offset + (address - p->p_vaddr);
    }
    Elf(std::string_view image, const DynamicClosure::Image& expected) : bytes(image) {
        const auto header = read_at<Elf64_Ehdr>(bytes, 0);
        const auto* id = header.e_ident;
        if (std::memcmp(id, ELFMAG, SELFMAG) || id[EI_CLASS] != ELFCLASS64 || id[EI_DATA] != ELFDATA2LSB ||
            id[EI_VERSION] != EV_CURRENT || (id[EI_OSABI] != ELFOSABI_NONE && id[EI_OSABI] != ELFOSABI_GNU) ||
            id[EI_ABIVERSION] != 0 || header.e_type != ET_DYN || header.e_machine != EM_X86_64 ||
            header.e_version != EV_CURRENT || header.e_flags || header.e_entry || header.e_ehsize != sizeof header ||
            header.e_phentsize != sizeof(Elf64_Phdr) || header.e_phnum == 0 || header.e_phnum > 16 ||
            header.e_phoff != sizeof header)
            denied("ELF", "unsupported target or header");
        Elf64_Phdr dynamic{}, program_headers{}, relro{}, eh_frame{};
        bool stack = false;
        for (unsigned n = 0; n < header.e_phnum; ++n) {
            const auto p = read_at<Elf64_Phdr>(bytes, header.e_phoff + n * sizeof(Elf64_Phdr));
            if (p.p_offset > bytes.size() || p.p_filesz > bytes.size() - p.p_offset ||
                p.p_filesz > p.p_memsz || p.p_vaddr > image_limit || p.p_memsz > image_limit - p.p_vaddr)
                denied("ELF", "segment extent outside image bounds");
            switch (p.p_type) {
                case PT_LOAD:
                    if (!p.p_memsz || !(p.p_flags & PF_R) || (p.p_flags & ~(PF_R | PF_W | PF_X)) ||
                        ((p.p_flags & PF_X) && ((p.p_flags & PF_W) || p.p_filesz != p.p_memsz)) ||
                        p.p_align != page || p.p_offset % page != p.p_vaddr % page ||
                        (!loads.empty() && p.p_vaddr / page <
                            (loads.back().p_vaddr + loads.back().p_memsz + page - 1) / page))
                        denied("ELF", "unsupported load segment permissions or geometry");
                    loads.push_back(p);
                    break;
                case PT_DYNAMIC:
                    if (dynamic.p_type) denied("ELF", "duplicate dynamic segment");
                    dynamic = p;
                    break;
                case PT_GNU_STACK:
                    if (stack || p.p_flags != (PF_R | PF_W) || p.p_filesz || p.p_memsz)
                        denied("ELF", "unsupported stack");
                    stack = true;
                    break;
                case PT_PHDR:
                    if (program_headers.p_type) denied("ELF", "duplicate program-header mapping");
                    program_headers = p;
                    break;
                case PT_GNU_RELRO:
                    if (relro.p_type) denied("ELF", "duplicate RELRO");
                    relro = p;
                    break;
                case PT_GNU_EH_FRAME:
                    if (eh_frame.p_type) denied("ELF", "duplicate unwind header");
                    eh_frame = p;
                    break;
                default:
                    denied("ELF", "unsupported program header " + std::to_string(p.p_type));
            }
        }
        if (!stack || loads.empty() || loads.front().p_offset != 0 || loads.front().p_vaddr != 0 ||
            loads.front().p_filesz < header.e_phoff + header.e_phnum * sizeof(Elf64_Phdr) ||
            !dynamic.p_filesz || dynamic.p_filesz % sizeof(Elf64_Dyn) ||
            dynamic.p_filesz / sizeof(Elf64_Dyn) > 128 ||
            offset(dynamic.p_vaddr, dynamic.p_filesz) != dynamic.p_offset)
            denied("ELF", "invalid dynamic segment");
        const auto header_extent = header.e_phnum * sizeof(Elf64_Phdr);
        if (program_headers.p_type && (program_headers.p_offset != header.e_phoff ||
            program_headers.p_vaddr != header.e_phoff || program_headers.p_filesz != header_extent ||
            program_headers.p_memsz != header_extent || program_headers.p_flags != PF_R))
            denied("ELF", "invalid in-memory program-header table");
        if (relro.p_type && (relro.p_flags != PF_R || !relro.p_memsz ||
            !segment(relro.p_vaddr, relro.p_memsz, PF_W, false) ||
            offset(relro.p_vaddr, relro.p_filesz, false) != relro.p_offset))
            denied("ELF", "invalid RELRO mapping");
        if (eh_frame.p_type && (eh_frame.p_flags != PF_R ||
            offset(eh_frame.p_vaddr, eh_frame.p_filesz) != eh_frame.p_offset))
            denied("ELF", "invalid unwind header mapping");
        metadata.emplace_back(0, header.e_phoff + header.e_phnum * sizeof(Elf64_Phdr));
        std::map<Elf64_Sxword, std::uint64_t> tags;
        std::vector<std::uint64_t> needed;
        bool ended = false;
        for (std::uint64_t n = 0; n < dynamic.p_filesz; n += sizeof(Elf64_Dyn)) {
            const auto entry = read_at<Elf64_Dyn>(bytes, dynamic.p_offset + n);
            const auto value = entry.d_un.d_val;
            if (entry.d_tag == DT_NULL && value == 0) { ended = true; continue; }
            if (ended) denied("ELF", "data after dynamic terminator");
            switch (entry.d_tag) {
                case DT_NEEDED: needed.push_back(value); continue;
                case DT_SONAME: case DT_HASH: case DT_STRTAB: case DT_STRSZ: case DT_SYMTAB: case DT_SYMENT:
                case DT_PLTGOT: case DT_PLTREL: case DT_PLTRELSZ: case DT_JMPREL:
                case DT_RELA: case DT_RELASZ: case DT_RELAENT: case DT_FLAGS: case DT_FLAGS_1:
                    break;
                default: denied("ELF", "unsupported dynamic tag " + std::to_string(entry.d_tag));
            }
            if (!tags.emplace(entry.d_tag, value).second) denied("ELF", "duplicate dynamic tag");
        }
        auto get = [&](Elf64_Sxword tag) {
            auto it = tags.find(tag);
            if (it == tags.end()) denied("ELF", "missing dynamic tag");
            return it->second;
        };
        if (!ended || (tags.contains(DT_FLAGS) && (get(DT_FLAGS) & ~std::uint64_t(DF_BIND_NOW))) ||
            (tags.contains(DT_FLAGS_1) && (get(DT_FLAGS_1) & ~std::uint64_t(DF_1_NOW))))
            denied("ELF", "unsupported dynamic flags or terminator");
        const auto string_size = get(DT_STRSZ);
        if (!string_size || string_size > 65536) denied("ELF", "string table limit");
        auto strings = bytes.substr(offset(get(DT_STRTAB), string_size), string_size);
        if (strings.front() || strings.back()) denied("ELF", "invalid string table sentinels");
        auto name = [&](std::uint64_t n) {
            if (n >= strings.size()) denied("ELF", "string index outside table");
            auto text = strings.substr(n, strings.find('\0', n) - n);
            if (text.size() > 128) denied("ELF", "native name limit");
            return text;
        };
        if (name(get(DT_SONAME)) != expected.name || needed.size() != expected.needed.size())
            denied("dependency", expected.name + " SONAME or DT_NEEDED differs");
        for (std::size_t n = 0; n < needed.size(); ++n)
            if (name(needed[n]) != expected.needed[n]) denied("dependency", "unapproved DT_NEEDED in " + expected.name);

        const auto hash = get(DT_HASH);
        const auto hash_head = offset(hash, 8);
        const auto buckets = read_at<std::uint32_t>(bytes, hash_head);
        const auto count = read_at<std::uint32_t>(bytes, hash_head + 4);
        if (!buckets || buckets > symbol_limit || !count || count > symbol_limit || get(DT_SYMENT) != sizeof(Elf64_Sym))
            denied("ELF", "symbol/hash table limit");
        const auto hash_table = offset(hash, 8 + (buckets + count) * 4);
        auto index = [&](unsigned n) {
            const auto value = read_at<std::uint32_t>(bytes, hash_table + 8 + n * 4);
            if (value >= count) denied("ELF", "invalid hash link");
            return value;
        };
        for (unsigned n = 0; n < buckets + count; ++n) (void)index(n);
        for (unsigned n = 0; n < buckets; ++n) {
            unsigned traversed = 0;
            for (auto chain = index(n); chain; chain = index(buckets + chain))
                if (++traversed >= count) denied("ELF", "cyclic hash link");
        }
        const auto symtab = offset(get(DT_SYMTAB), count * sizeof(Elf64_Sym));
        for (unsigned n = 0; n < count; ++n) {
            auto sym = read_at<Elf64_Sym>(bytes, symtab + n * sizeof(Elf64_Sym));
            symbols.push_back(sym);
            names.emplace_back(name(sym.st_name));
            if (!n) {
                if (sym.st_name || sym.st_info || sym.st_other || sym.st_shndx || sym.st_value || sym.st_size)
                    denied("ELF", "invalid null symbol");
                continue;
            }
            const auto kind = ELF64_ST_TYPE(sym.st_info);
            if (!c_symbol(names.back()) || ELF64_ST_BIND(sym.st_info) != STB_GLOBAL || sym.st_other != STV_DEFAULT ||
                (kind != STT_FUNC && (kind != STT_NOTYPE || sym.st_shndx != SHN_UNDEF)))
                denied("ELF", "unsupported dynamic symbol (including weak, TLS and resolver symbols)");
            if (sym.st_shndx == SHN_UNDEF) {
                if (sym.st_value || sym.st_size) denied("ELF", "invalid undefined function");
            } else if (sym.st_shndx >= SHN_LORESERVE || !sym.st_size ||
                !segment(sym.st_value, sym.st_size, PF_X, true) || !definitions.emplace(names.back(), n).second)
                denied("ELF", "invalid or conflicting function definition");
        }
        auto table = [&](std::uint64_t address, std::uint64_t size, unsigned kind) {
            if (!size || size % sizeof(Elf64_Rela) || size / sizeof(Elf64_Rela) > relocation_limit)
                denied("ELF", "invalid relocation extent");
            auto start = offset(address, size);
            for (std::uint64_t n = 0; n < size; n += sizeof(Elf64_Rela)) {
                const auto entry = read_at<Elf64_Rela>(bytes, start + n);
                if (ELF64_R_TYPE(entry.r_info) != kind || !ELF64_R_SYM(entry.r_info) ||
                    ELF64_R_SYM(entry.r_info) >= count || entry.r_addend || entry.r_offset % 8 ||
                    !segment(entry.r_offset, 8, PF_W, false))
                    denied("ELF", "unsupported relocation or target");
                relocations.push_back(entry);
            }
        };
        if (tags.contains(DT_RELA) || tags.contains(DT_RELASZ) || tags.contains(DT_RELAENT)) {
            if (get(DT_RELAENT) != sizeof(Elf64_Rela)) denied("ELF", "unsupported RELA entry");
            table(get(DT_RELA), get(DT_RELASZ), R_X86_64_GLOB_DAT);
        }
        if (tags.contains(DT_JMPREL) || tags.contains(DT_PLTREL) || tags.contains(DT_PLTRELSZ) || tags.contains(DT_PLTGOT)) {
            if (get(DT_PLTREL) != DT_RELA || !segment(get(DT_PLTGOT), 24, PF_W, true))
                denied("ELF", "unsupported PLT");
            table(get(DT_JMPREL), get(DT_PLTRELSZ), R_X86_64_JUMP_SLOT);
        }
        std::set<std::uint64_t> targets;
        for (const auto& entry : relocations) {
            auto address = entry.r_offset;
            if (!targets.insert(address).second || std::any_of(metadata.begin(), metadata.end(), [&](auto range) {
                return address < range.second && range.first < address + 8;
            })) denied("ELF", "relocation overlaps loader metadata or another relocation");
        }
    }
};

File sealed_image(std::string_view bytes) {
    File file(memfd_create("void-admitted-native", MFD_CLOEXEC | MFD_ALLOW_SEALING));
    if (file.fd < 0) denied("seal", "memfd unavailable");
    while (!bytes.empty()) {
        auto written = write(file.fd, bytes.data(), bytes.size());
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) denied("seal", "failed to write sealed image");
        bytes.remove_prefix(static_cast<std::size_t>(written));
    }
    constexpr int seals = F_SEAL_WRITE | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL;
    if (fcntl(file.fd, F_ADD_SEALS, seals) || fcntl(file.fd, F_GET_SEALS) != seals)
        denied("seal", "unable to verify image seals");
    return file;
}

void mapping(std::string_view maps, const Mapped& image, const Elf64_Phdr& segment) {
    const auto lo = image.link->l_addr + segment.p_vaddr / page * page;
    const auto end = image.link->l_addr + segment.p_vaddr + segment.p_filesz;
    auto current = lo;
    while (!maps.empty() && current < end) {
        auto line = maps.substr(0, maps.find('\n'));
        maps.remove_prefix(std::min(maps.size(), line.size() + 1));
        unsigned long low, high, offset, inode;
        unsigned device_major, device_minor;
        char permissions[5];
        // This is a trusted procfs line, bounded by the captured mapping view.
        std::string row(line);
        if (std::sscanf(row.c_str(), "%lx-%lx %4s %lx %x:%x %lu",
                        &low, &high, permissions, &offset, &device_major, &device_minor, &inode) != 7 ||
            current < low || current >= high) continue;
        if (permissions[2] != 'x' || permissions[1] == 'w' || inode != image.identity.st_ino ||
            device_major != major(image.identity.st_dev) || device_minor != minor(image.identity.st_dev) ||
            offset + (current - low) != segment.p_offset / page * page + (current - lo))
            denied("mapping", "executable region differs from sealed image");
        current = high;
    }
    if (current < end) denied("mapping", "incomplete executable mapping");
}

void map_images(std::vector<Mapped>& result, const std::vector<std::string>& bytes, const std::vector<Elf>& elf) {
    struct Staged { File file; struct stat identity; std::string path; };
    std::vector<Staged> staged;
    staged.reserve(bytes.size());
    result.reserve(bytes.size());
    for (const auto& data : bytes) {
        auto file = sealed_image(data);
        struct stat identity{}, via_proc{};
        auto path = "/proc/self/fd/" + std::to_string(file.fd);
        if (fstat(file.fd, &identity) || stat(path.c_str(), &via_proc) ||
            identity.st_ino != via_proc.st_ino || identity.st_dev != via_proc.st_dev)
            denied("mapping", "proc descriptor identity mismatch");
        staged.push_back({std::move(file), identity, std::move(path)});
    }
    Lmid_t ns = LM_ID_NEWLM;
    for (auto& item : staged) {
        Module module(dlmopen(ns, item.path.c_str(), RTLD_NOW | RTLD_LOCAL));
        if (!module.handle) {
            const char* message = dlerror();
            denied("load", message ? message : "dlmopen failed");
        }
        link_map* link = nullptr;
        Lmid_t actual;
        if (dlinfo(module.handle, RTLD_DI_LMID, &actual) || actual == LM_ID_BASE ||
            (ns != LM_ID_NEWLM && actual != ns) ||
            dlinfo(module.handle, RTLD_DI_LINKMAP, &link) || !link)
            denied("mapping", "wrong loader namespace or link map");
        ns = actual;
        result.push_back({std::move(item.file), item.identity, std::move(module), link});
        while (link->l_prev) link = link->l_prev;
        for (const auto& admitted : result) {
            if (link != admitted.link) denied("mapping", "unexpected member or order in native namespace");
            link = link->l_next;
        }
        if (link) denied("mapping", "unexpected native namespace member");
    }
    auto file = proc("/proc/self/maps");
    auto maps = read_bounded(file.fd, 4 * 1024 * 1024);
    for (std::size_t n = 0; n < result.size(); ++n)
        for (const auto& p : elf[n].loads)
            if (p.p_flags & PF_X) mapping(maps, result[n], p);
}
#endif
} // namespace

class DynamicBinding final {
public:
    DynamicClosure::Owner plan;
    std::filesystem::path root;
    CPrimitiveThunk* thunk = nullptr;
    mutable std::atomic<bool> claimed{false};
#if defined(_WIN32) || defined(__linux__)
    // Destroy the adapter first, while its provider still has a reference.
    std::unique_ptr<OsModule> provider, adapter;
#endif
#if defined(__linux__) && defined(__GLIBC__) && defined(__x86_64__)
    std::vector<Mapped> modules;
    ~DynamicBinding() { while (!modules.empty()) modules.pop_back(); }
#endif
};

bool matches_dynamic(const DynamicBinding& binding, const DynamicClosure& plan, const std::filesystem::path& root) {
    return binding.plan->bytes() == plan.bytes() && binding.root == root;
}

std::shared_ptr<const DynamicBinding> load_dynamic(DynamicClosure::Owner plan, const std::filesystem::path& root) {
    if (!plan) denied("authority", "missing host closure");
    if (auto* runtime = Runtime::attached(); runtime && runtime->is_managed())
        denied("authority", "loader admission requires NativeSafe state");
    if (plan->conventional()) {
#if (defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))) || \
    (defined(__linux__) && defined(__GLIBC__) && defined(__x86_64__))
#ifdef _WIN32
        constexpr auto target = "x86_64-pc-windows-msvc";
#else
        constexpr auto target = "x86_64-unknown-linux-gnu";
#endif
        if (plan->target() != target) denied("target", "conventional descriptor does not match this host");
        if (!root.is_absolute() || root != root.lexically_normal())
            denied("path", "conventional loading requires an absolute native directory");
        const auto library = root / utf8_path(plan->library());
        const auto adapter = root / utf8_path(plan->adapter());
        auto result = std::make_shared<DynamicBinding>();
        result->plan = std::move(plan);
        result->root = root;
        result->provider = std::make_unique<OsModule>(library);
        for (const auto& item : result->plan->exports()) result->provider->find(library, item.symbol);
        auto* transport = result->provider.get();
        if (adapter != library) {
            result->adapter = std::make_unique<OsModule>(adapter);
            transport = result->adapter.get();
        }
        result->thunk = transport->find(adapter, result->plan->dispatch());
        const auto& descriptor = result->plan->descriptor();
        if (result->thunk(descriptor.data(), static_cast<std::uint32_t>(descriptor.size()),
                          c_primitive_probe, nullptr, 0, nullptr))
            denied("transport", "adapter rejected the exact descriptor or thunk identity: " + path_utf8(adapter));
        return result;
#else
        denied("target", "conventional loading requires Windows x64 or Linux x86-64 GNU");
#endif
    }
#if defined(__linux__) && defined(__GLIBC__) && defined(__x86_64__)
    launch_policy();
    if (sodium_init() < 0) denied("digest", "SHA-256 provider initialization failed");
    auto directory = root_directory(root);
    const auto& images = plan->images();
    std::vector<std::string> bytes;
    std::vector<Elf> elf;
    bytes.reserve(images.size());
    elf.reserve(images.size());
    for (const auto& image : images) {
        bytes.push_back(approved_bytes(directory.fd, image));
        elf.emplace_back(bytes.back(), image);
    }

    struct Definition { std::size_t image; std::uint64_t value; };
    std::map<std::string, Definition, std::less<>> definitions;
    for (std::size_t n = 0; n < elf.size(); ++n)
        for (const auto& [name, index] : elf[n].definitions)
            if (!definitions.emplace(name, Definition{n, elf[n].symbols[index].st_value}).second)
                denied("symbol", "conflicting closure definition " + name);
    for (std::size_t n = 0; n < elf.size(); ++n)
        for (std::size_t k = 1; k < elf[n].symbols.size(); ++k) {
            if (elf[n].symbols[k].st_shndx != SHN_UNDEF) continue;
            auto it = definitions.find(elf[n].names[k]);
            if (it == definitions.end() || std::find(images[n].needed.begin(), images[n].needed.end(),
                    images[it->second.image].name) == images[n].needed.end())
                denied("dependency", "unapproved symbol import " + elf[n].names[k]);
        }
    for (const auto& e : plan->exports()) {
        auto it = definitions.find(e.symbol);
        if (it == definitions.end() || images[it->second.image].name != e.image)
            denied("symbol", "missing exact owned export " + e.symbol);
    }

    auto result = std::make_shared<DynamicBinding>();
    result->plan = std::move(plan);
    result->root = root;
    map_images(result->modules, bytes, elf);
    const auto& loaded = result->modules;
    for (std::size_t n = 0; n < elf.size(); ++n)
        for (const auto& relocation : elf[n].relocations) {
            const auto& target = definitions.at(elf[n].names[ELF64_R_SYM(relocation.r_info)]);
            std::uintptr_t actual;
            std::memcpy(&actual, reinterpret_cast<const void*>(loaded[n].link->l_addr + relocation.r_offset), sizeof actual);
            if (actual != loaded[target.image].link->l_addr + target.value)
                denied("symbol", "relocation resolved outside its admitted owner");
        }
    for (const auto& e : result->plan->exports()) {
        const auto& target = definitions.at(e.symbol);
        const auto& owner = loaded[target.image];
        dlerror();
        void* address = dlsym(owner.module.handle, e.symbol.c_str());
        Dl_info info{};
        void* link = nullptr;
        if (dlerror() || !address ||
            reinterpret_cast<std::uintptr_t>(address) != owner.link->l_addr + target.value ||
            !dladdr1(address, &info, &link, RTLD_DL_LINKMAP) || link != owner.link)
            denied("symbol", "mapped export belongs to another image: " + e.symbol);
        if (e.symbol == DynamicClosure::transport) result->thunk = reinterpret_cast<CPrimitiveThunk*>(address);
    }
    const auto& descriptor = result->plan->descriptor();
    if (!result->thunk || result->thunk(descriptor.data(), static_cast<std::uint32_t>(descriptor.size()),
                                      c_primitive_probe, nullptr, 0, nullptr))
        denied("transport", "provider rejected the exact descriptor or thunk identity");
    return result;
#else
    (void)plan;
    (void)root;
    denied("target", "verified loading requires Linux x86-64 GNU");
#endif
}

std::filesystem::path installed_dynamic_root() {
#ifdef _WIN32
    std::wstring path(32768, L'\0');
    const auto count = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!count || count >= path.size()) denied("path", "executable location is unavailable");
    path.resize(count);
    return std::filesystem::path(path).parent_path() / "native";
#elif defined(__linux__) && defined(__GLIBC__) && defined(__x86_64__)
    char path[4097];
    auto count = readlink("/proc/self/exe", path, sizeof path);
    if (count <= 0 || count >= static_cast<ssize_t>(sizeof path))
        denied("path", "executable location is unavailable");
    std::string_view name(path, static_cast<std::size_t>(count));
    if (!name.starts_with('/') || name.ends_with(" (deleted)"))
        denied("path", "executable location is not stable");
    return std::filesystem::path(name).parent_path() / "native";
#else
    denied("target", "verified loading requires Linux x86-64 GNU");
#endif
}

std::shared_ptr<const DynamicBinding> Runtime::dynamic_binding() const {
    std::lock_guard lock(mutex_);
    return native_dynamic_;
}

void Runtime::bind_dynamic(std::shared_ptr<const DynamicBinding> binding) {
    std::lock_guard lock(mutex_);
    if (closing_ || !binding || (native_dynamic_ && native_dynamic_ != binding))
        denied("authority", "Runtime already has a different binding or is closing");
    if (!native_dynamic_ && binding->claimed.exchange(true))
        denied("authority", "native binding is already owned by a Runtime");
    native_dynamic_ = std::move(binding);
}

extern "C" CPrimitiveThunk* vd_c_dynamic_resolve_v1(const char* bytes, std::uint32_t size) noexcept {
    try {
        auto& runtime = Runtime::current();
        if (!runtime.is_managed() || !bytes || !size || size > 32768) return nullptr;
        auto binding = runtime.dynamic_binding();
        if (binding && binding->plan->bytes() == std::string_view(bytes, size)) return binding->thunk;
    } catch (const std::exception&) {}
    return nullptr;
}

extern "C" std::uint32_t vd_c_dynamic_call_v1(const char* bytes, std::uint32_t size, std::uint32_t ordinal,
    const std::uint64_t* words, std::uint32_t count, std::uint64_t* result,
    const CSpanRequest* spans, std::uint32_t span_count) noexcept {
    try {
        auto& runtime = Runtime::current();
        if (!runtime.is_managed() || !bytes || !size || size > 32768 || ordinal >= 64 ||
            count > c_primitive_max_arguments || (count && !words)) return 7;
        auto binding = runtime.dynamic_binding();
        if (!binding || binding->plan->bytes() != std::string_view(bytes, size)) return 7;
        runtime.begin_native_job();
        struct Job { Runtime& runtime; ~Job() { runtime.end_native_job(); } } job{runtime};
        const auto& descriptor = binding->plan->descriptor();
        if (span_count)
            return vd_c_span_dispatch_v2(binding->thunk, descriptor.data(), descriptor.size(),
                                        ordinal, words, count, result, spans, span_count);
        Runtime::Native native(runtime);
        return binding->thunk(descriptor.data(), descriptor.size(), ordinal, words, count, result);
    } catch (const std::exception&) { return 7; }
}
} // namespace vd
