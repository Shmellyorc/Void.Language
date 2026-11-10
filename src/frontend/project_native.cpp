#include "project_native.hpp"
#include "project_files.hpp"
#include "void/path.hpp"
#include <llvm/BinaryFormat/COFF.h>
#include <llvm/BinaryFormat/ELF.h>
#include <llvm/Object/Archive.h>
#include <llvm/Object/COFF.h>
#include <llvm/Object/COFFImportFile.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Support/Process.h>
#include <llvm/Support/SHA256.h>
#include <set>

namespace vd::project {
namespace {
namespace fs = std::filesystem;

std::string verified_file(const fs::path& directory, const std::string& name, std::string_view expected_hash,
                          std::uint64_t size) {
    auto bytes = read_project_input(directory / utf8_path(name), static_cast<std::size_t>(size));
    const auto digest = llvm::SHA256::hash({reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()});
    std::string hash;
    constexpr char hex[] = "0123456789abcdef";
    for (auto byte : digest) { hash += hex[byte >> 4]; hash += hex[byte & 15]; }
    if (bytes.size() != size || hash != expected_hash)
        throw std::runtime_error("Managed native artifact identity mismatch: " + name);
    return bytes;
}

std::string verified_file(const fs::path& directory, const NativeArtifact& file) {
    return verified_file(directory, file.name, file.sha256, file.size);
}

void verify_image(std::string_view bytes, const std::string& name, bool windows) {
    const llvm::StringRef data(bytes.data(), bytes.size());
    auto file = llvm::object::ObjectFile::createObjectFile({data, name});
    if (!file) llvm::consumeError(file.takeError());
    else if ((*file)->getArch() == llvm::Triple::x86_64) {
        if (windows) {
            const auto* image = llvm::dyn_cast<llvm::object::COFFObjectFile>(file->get());
            if (image && image->getPE32PlusHeader() && (image->getCharacteristics() & llvm::COFF::IMAGE_FILE_DLL)) return;
        } else if (const auto* image = llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(file->get())) {
            const auto& header = image->getELFFile().getHeader();
            const auto os = header.e_ident[llvm::ELF::EI_OSABI];
            if (header.e_type == llvm::ELF::ET_DYN &&
                (os == llvm::ELF::ELFOSABI_NONE || os == llvm::ELF::ELFOSABI_GNU)) return;
        }
    }
    throw std::runtime_error("Managed native artifact has the wrong target or image type: " + name);
}

bool compatible_link_object(const llvm::object::Binary& binary, bool windows) {
    using namespace llvm::object;
    if (windows) {
        if (const auto* import = llvm::dyn_cast<COFFImportFile>(&binary))
            return import->getData().size() >= sizeof(coff_import_header) &&
                import->getMachine() == llvm::COFF::IMAGE_FILE_MACHINE_AMD64;
        if (!llvm::isa<COFFObjectFile>(binary)) return false;
    } else if (!llvm::isa<ELF64LEObjectFile>(binary)) return false;

    const auto& object = llvm::cast<ObjectFile>(binary);
    if (!object.isRelocatableObject() || object.getArch() != llvm::Triple::x86_64) return false;
    // The selected input set is explicit. Keep object directives from adding files.
    for (const auto& section : object.sections()) {
        if (!windows) {
            const auto type = ELFSectionRef(section).getType();
            if (type == llvm::ELF::SHT_LLVM_DEPENDENT_LIBRARIES || type == llvm::ELF::SHT_LLVM_LINKER_OPTIONS)
                return false;
            continue;
        }
        auto name = section.getName();
        if (!name) {
            llvm::consumeError(name.takeError());
            return false;
        }
        if (*name == ".drectve") return false;
    }
    return true;
}

bool compatible_link_input(llvm::MemoryBufferRef data, bool windows) {
    using namespace llvm::object;
    auto binary = createBinary(data);
    if (!binary) {
        llvm::consumeError(binary.takeError());
        return false;
    }
    const auto* archive = llvm::dyn_cast<Archive>(binary->get());
    if (!archive) return compatible_link_object(**binary, windows);
    if (archive->isThin()) return false;
    auto error = llvm::Error::success();
    bool has_members = false, compatible = true;
    for (const auto& child : archive->children(error)) {
        has_members = true;
        auto member = child.getAsBinary();
        if (!member) {
            llvm::consumeError(member.takeError());
            compatible = false;
            break;
        }
        if (!compatible_link_object(**member, windows)) {
            compatible = false;
            break;
        }
    }
    if (error) {
        llvm::consumeError(std::move(error));
        return false;
    }
    return has_members && compatible;
}

}

SelectedNative select_managed_native(const NativeRequirement& requirement) {
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
    constexpr std::string_view target = "x86_64-pc-windows-msvc";
#elif defined(__linux__) && defined(__GLIBC__) && defined(__x86_64__)
    constexpr std::string_view target = "x86_64-unknown-linux-gnu";
#else
    constexpr std::string_view target;
#endif
    if (target.empty()) throw std::runtime_error("Managed native providers are unsupported on this host.");
    const auto selected = llvm::sys::Process::GetEnv("VOID_NATIVE_CATALOG");
    if (!selected || !utf8_path(*selected).is_absolute())
        throw std::runtime_error("Select an approved absolute directory with VOID_NATIVE_CATALOG.");
    const auto directory = inspect_creation_root(utf8_path(*selected)).path;
    auto plan = parse_native_catalog(read_project_input(directory / "void.native.toml", native_catalog_byte_limit));
    if (plan.target != target)
        throw std::runtime_error("Managed native catalog target '" + plan.target +
            "' does not match this host ('" + std::string(target) + "').");
    if (plan.id != requirement.id)
        throw std::runtime_error("Managed native catalog provider ID '" + plan.id +
            "' does not match required ID '" + requirement.id + "'.");
    if (plan.version != requirement.version)
        throw std::runtime_error("Managed native catalog version '" + plan.version.text() +
            "' does not match required exact version '" + requirement.version.text() + "'.");
    if (plan.abi != requirement.abi)
        throw std::runtime_error("Managed native catalog ABI " + std::to_string(plan.abi) +
            " does not match required ABI " + std::to_string(requirement.abi) + ".");

    std::set<std::string> names{"void.native.toml", plan.descriptor.name};
    for (const auto* role : {&plan.runtime, &plan.link, &plan.notices})
        for (const auto& file : *role) names.insert(file.name);
    for (const auto& entry : fs::directory_iterator(directory)) {
        const auto name = path_utf8(entry.path().filename());
        if (!names.erase(name)) throw std::runtime_error("Unexpected managed native catalog entry: " + name);
    }
    if (!names.empty()) throw std::runtime_error("Missing managed native artifact: " + *names.begin());

    const auto descriptor = verified_file(directory, plan.descriptor);
    const bool windows = target == "x86_64-pc-windows-msvc";
    for (const auto& file : plan.runtime) {
        if (windows ? !file.name.ends_with(".dll") : file.name.find(".so") == file.name.npos)
            throw std::runtime_error("Invalid managed native runtime filename: " + file.name);
        verify_image(verified_file(directory, file), file.name, windows);
    }
    for (const auto* role : {&plan.link, &plan.notices})
        for (const auto& file : *role) (void)verified_file(directory, file);
    std::vector<DynamicClosure::Payload> payload;
    for (const auto* role : {&plan.runtime, &plan.notices})
        for (const auto& file : *role) payload.push_back({file.name, file.sha256, file.size});
    auto admission = CPrimitiveAdmission::admit_conventional(descriptor, directory, plan.library, plan.adapter, payload);
    if (admission->provider() != plan.id || admission->target() != plan.target || admission->revision() != plan.abi)
        throw std::runtime_error("Managed native descriptor does not match the approved provider, target or ABI.");

    return {std::move(plan), std::move(admission)};
}

std::map<std::string, std::string> read_managed_payload(const CPrimitiveAdmission& native) {
    std::map<std::string, std::string> files;
    for (const auto& file : native.dynamic()->payload())
        files.emplace(file.name, verified_file(native.dynamic_root(), file.name, file.sha256, file.size));
    return files;
}

std::map<std::string, std::string> read_managed_link_inputs(const CPrimitiveAdmission& native,
                                                          const std::vector<NativeArtifact>& inputs) {
    std::map<std::string, std::string> files;
    const bool windows = native.target() == "x86_64-pc-windows-msvc";
    for (const auto& file : inputs) {
        auto bytes = verified_file(native.dynamic_root(), file);
        if (!compatible_link_input({llvm::StringRef(bytes), file.name}, windows))
            throw std::runtime_error("Incompatible managed native link input for " + std::string(native.target()) +
                ": " + file.name + " (expected a self-contained native object or archive).");
        files.emplace(file.name, std::move(bytes));
    }
    return files;
}
}
