#pragma once
#include <llvm/Object/MachO.h>
#include <stdexcept>
#include <string_view>

namespace vd::native_macos {
// Inspect load commands rather than probing system files: modern macOS keeps
// many OS libraries in the dyld shared cache, without a standalone dylib file.
// The first package layout uses static third-party archives and OS libraries.
// Caller-controlled paths and unresolved private dependencies must fail closed.
inline void check_package_image(std::string_view bytes) {
    auto image = llvm::object::ObjectFile::createObjectFile({llvm::StringRef(bytes), "app"});
    if (!image)
        throw std::runtime_error("Invalid application package executable: " + llvm::toString(image.takeError()));
    const auto* macho = llvm::dyn_cast<llvm::object::MachOObjectFile>(image->get());
    if (!macho || !macho->is64Bit() || !macho->isLittleEndian() ||
        macho->getArch() != llvm::Triple::aarch64 ||
        macho->getHeader64().cpusubtype != llvm::MachO::CPU_SUBTYPE_ARM64_ALL ||
        macho->getHeader64().filetype != llvm::MachO::MH_EXECUTE)
        throw std::runtime_error("Application package requires a macOS ARM64 executable.");
    auto text = [](const auto& load, std::uint32_t offset) {
        if (offset >= load.C.cmdsize)
            throw std::runtime_error("Invalid application package executable load-command string.");
        const std::string_view storage(load.Ptr + offset, load.C.cmdsize - offset);
        const auto end = storage.find('\0');
        if (end == storage.npos || !end)
            throw std::runtime_error("Invalid application package executable load-command string.");
        return storage.substr(0, end);
    };
    bool macos = false, loader = false;
    std::uint64_t entry = 0;
    for (const auto& load : macho->load_commands()) {
        switch (load.C.cmd) {
        case llvm::MachO::LC_RPATH:
        case llvm::MachO::LC_DYLD_ENVIRONMENT:
            throw std::runtime_error("Application package executable contains an embedded library search path.");
        case llvm::MachO::LC_LOAD_DYLIB:
        case llvm::MachO::LC_LOAD_WEAK_DYLIB:
        case llvm::MachO::LC_REEXPORT_DYLIB:
        case llvm::MachO::LC_LAZY_LOAD_DYLIB:
        case llvm::MachO::LC_LOAD_UPWARD_DYLIB: {
            const auto command = macho->getDylibIDLoadCommand(load);
            const auto name = text(load, command.dylib.name);
            // These are the OS-owned shared-cache namespaces, not /usr/local
            // or arbitrary absolute paths in a compiler/build installation.
            bool system = name.starts_with("/usr/lib/") || name.starts_with("/System/Library/Frameworks/");
            auto parts = name.substr(1);
            while (!parts.empty()) {
                const auto slash = parts.find('/');
                const auto part = parts.substr(0, slash);
                if (part.empty() || part == "." || part == "..") system = false;
                if (slash == parts.npos) break;
                parts.remove_prefix(slash + 1);
                if (parts.empty()) system = false;
            }
            if (!system)
                throw std::runtime_error("Application package executable has an unresolved non-system dependency: " + std::string(name));
            break;
        }
        case llvm::MachO::LC_BUILD_VERSION:
            if (macho->getBuildVersionLoadCommand(load).platform != llvm::MachO::PLATFORM_MACOS)
                throw std::runtime_error("Application package executable targets a non-macOS platform.");
            macos = true;
            break;
        case llvm::MachO::LC_VERSION_MIN_MACOSX:
            macos = true;
            break;
        case llvm::MachO::LC_MAIN: {
            const auto command = macho->getEntryPointCommand(load);
            if (!command.entryoff || command.entryoff >= bytes.size())
                throw std::runtime_error("Invalid application package executable entry point.");
            entry = command.entryoff;
            break;
        }
        case llvm::MachO::LC_LOAD_DYLINKER:
            if (text(load, macho->getDylinkerCommand(load).name) != "/usr/lib/dyld")
                throw std::runtime_error("Invalid application package executable dynamic loader.");
            loader = true;
            break;
        }
    }
    bool executable_entry = false;
    for (const auto& load : macho->load_commands()) {
        if (load.C.cmd != llvm::MachO::LC_SEGMENT_64) continue;
        const auto segment = macho->getSegment64LoadCommand(load);
        if ((segment.initprot & llvm::MachO::VM_PROT_EXECUTE) && entry >= segment.fileoff &&
            entry - segment.fileoff < segment.filesize && segment.filesize - (entry - segment.fileoff) >= 4)
            executable_entry = true;
    }
    if (!macos || !entry || entry % 4 || !executable_entry || !loader)
        throw std::runtime_error("Application package executable lacks macOS launch metadata.");
}
}
