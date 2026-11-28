#include "target_pack.hpp"
#include "frontend/project_digest.hpp"
#include "frontend/project_files.hpp"
#include "void/path.hpp"
#include "void/project_creation.hpp"
#include <llvm/BinaryFormat/Magic.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Support/Process.h>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace vd::publish {
namespace {
namespace fs = std::filesystem;
constexpr std::size_t manifest_limit = 32 * 1024;
constexpr std::size_t pack_limit = 256 * 1024 * 1024;

bool hex(std::string_view text, std::size_t length) {
    return text.size() == length && text.find_first_not_of("0123456789abcdef") == text.npos;
}
bool numeric_version(std::string_view text) {
    return !text.empty() && text.size() <= 24 && text.front() != '.' && text.back() != '.' &&
        text.find("..") == text.npos && text.find_first_not_of("0123456789.") == text.npos;
}
std::string digest(std::string_view bytes) {
    project::Digest hash;
    hash.bytes(bytes);
    return hash.finish();
}
std::set<std::string> link_names(const Target& target) {
    if (target.system == System::windows_msvc) {
        std::set<std::string> result(windows_archives.begin(), windows_archives.end());
        result.insert(windows_libraries.begin(), windows_libraries.end());
        return result;
    }
    if (target.system != System::linux_gnu)
        throw std::runtime_error("A macOS pack requires independently redistributable libSystem and framework linking inputs. Use the matching native macOS toolchain for publishing.");
    std::set<std::string> result{linux_loader(target)};
    for (auto name : linux_start) result.emplace(name);
    for (auto name : linux_archives) result.emplace(name);
    for (auto name : linux_libraries) result.emplace(name);
    for (auto name : linux_end) result.emplace(name);
    return result;
}
void check_link_file(std::string_view name, std::string_view bytes, const Target& target) {
    const auto kind = llvm::identify_magic(bytes);
    if (name.ends_with(".a") || name.ends_with(".lib")) {
        // A thin archive can reference arbitrary physical files at link time.
        if (kind != llvm::file_magic::archive || !bytes.starts_with("!<arch>\n"))
            throw std::runtime_error("Expected a self-contained archive in target pack: " + std::string(name));
        return;
    }
    if (kind != (name.ends_with(".o") ? llvm::file_magic::elf_relocatable : llvm::file_magic::elf_shared_object))
        throw std::runtime_error("Expected a target ELF file in target pack: " + std::string(name));
    auto file = llvm::object::ObjectFile::createObjectFile({bytes, name});
    if (!file) throw std::runtime_error("Invalid target ELF file: " + llvm::toString(file.takeError()));
    const auto expected = target.architecture == Architecture::arm64 ? llvm::Triple::aarch64 : llvm::Triple::x86_64;
    if (!llvm::isa<llvm::object::ELF64LEObjectFile>(file->get()) || (*file)->getArch() != expected)
        throw std::runtime_error("Target pack link input has the wrong architecture: " + std::string(name));
}
void print_pack(const Pack& pack) {
    std::size_t bytes = 0;
    for (const auto& [name, data] : pack.files) bytes += data.size();
    std::cout << pack.id << " " << pack.version << " " << pack.target->triple << " " << pack.minimum
              << " source=" << pack.revision << " bytes=" << bytes << " sha256=" << pack.digest << '\n';
}
}

fs::path pack_store() {
    if (const auto selected = llvm::sys::Process::GetEnv("VOID_TARGET_PACK_ROOT")) {
        auto path = utf8_path(*selected);
        if (!path.is_absolute() || path != path.lexically_normal())
            throw std::runtime_error("VOID_TARGET_PACK_ROOT must be an absolute normalized path.");
        return path;
    }
    const auto data = project::user_data_directory();
    if (data.empty()) throw std::runtime_error("Cannot locate target-pack storage. Set VOID_TARGET_PACK_ROOT.");
    return data / "target-packs";
}

static Pack read_pack(const fs::path& requested, const project::ProjectDirectoryReader& reader) {
    const auto root = reader.inspect(requested).path;
    const auto manifest = reader.read(root / pack_manifest, manifest_limit);
    if (!manifest.ends_with('\n') || manifest.find_first_of("\r\0", 0, 2) != manifest.npos)
        throw std::runtime_error("Target pack manifest requires LF-terminated text.");
    std::istringstream rows(manifest);
    std::string line;
    auto field = [&](std::string_view name) {
        if (!std::getline(rows, line) || !line.starts_with(name) || line.size() <= name.size())
            throw std::runtime_error("Target pack manifest is missing " + std::string(name));
        return line.substr(name.size());
    };
    if (!std::getline(rows, line) || line != "VOID target pack 1")
        throw std::runtime_error("Unsupported target pack format. Expected VOID target pack 1.");
    auto id = field("id ");
    auto version = field("version ");
    const auto target_id = field("target ");
    const auto* target = find_target(target_id);
    if (!target || !(id == target_id || custom_target(id)) || !numeric_version(version))
        throw std::runtime_error("Invalid target pack ID, platform or version.");
    if (field("compiler ") != VOID_COMPILER_VERSION || field("llvm ") != LLVM_VERSION_STRING ||
        field("runtime ") != "void-aot-64-v1")
        throw std::runtime_error("Incompatible target pack. Use this VOID compiler version, LLVM " LLVM_VERSION_STRING " and Runtime ABI void-aot-64-v1.");
    auto revision = field("source ");
    if (!hex(revision, 40)) throw std::runtime_error("Target pack requires a source revision.");
    auto minimum = field("minimum ");
    auto expected = link_names(*target);
    if (target->system == System::linux_gnu &&
        (!minimum.starts_with("glibc-") || !numeric_version(std::string_view(minimum).substr(6))))
        throw std::runtime_error("Linux target pack requires a glibc deployment floor.");
    if (target->system == System::windows_msvc &&
        minimum != (target->architecture == Architecture::arm64 ? "windows-11" : "windows-10"))
        throw std::runtime_error("Windows target pack requires a Windows 10 x64 or Windows 11 ARM64 deployment floor.");
    Pack pack{target, std::move(id), std::move(revision), std::move(version), std::move(minimum), digest(manifest), {}};
    std::set<std::string> names;
    std::size_t budget = pack_limit;
    while (std::getline(rows, line)) {
        const auto tab = line.find('\t');
        if (tab != 64 || !hex(std::string_view(line).substr(0, tab), 64) || names.size() == 96)
            throw std::runtime_error("Invalid target pack inventory record.");
        const auto name = line.substr(tab + 1);
        const auto slash = name.find('/');
        if (slash == name.npos) throw std::runtime_error("Target pack requires a link/ or licenses/ member.");
        const auto role = name.substr(0, slash);
        const auto leaf = name.substr(slash + 1);
        if (!portable_path_component(leaf) || leaf.size() > 128 || leaf.find('/') != leaf.npos ||
            (role != "link" && role != "licenses") || !names.insert(project::fold_project_name(name)).second)
            throw std::runtime_error("Invalid or duplicate target pack member: " + name);
        if (role == "link" && !expected.erase(leaf)) throw std::runtime_error("Unexpected target pack link input: " + name);
        auto bytes = reader.read(root / utf8_path(name), role == "link" ? budget : std::min(budget, std::size_t{1024 * 1024}));
        if (bytes.empty() || digest(bytes) != line.substr(0, tab))
            throw std::runtime_error("Target pack checksum mismatch: " + name);
        budget -= bytes.size();
        if (role == "link") check_link_file(leaf, bytes, *target);
        pack.files.emplace(name, std::move(bytes));
    }
    if (!expected.empty()) throw std::runtime_error("Incomplete target pack. Missing link/" + *expected.begin());
    if (!pack.files.contains("licenses/VOID-MIT-LICENSE.txt"))
        throw std::runtime_error("Target pack is missing its VOID license.");
    for (const auto& entry : fs::directory_iterator(root)) {
        const auto name = path_utf8(entry.path().filename());
        if (name == pack_manifest) continue;
        if ((name != "link" && name != "licenses") || !fs::is_directory(entry.symlink_status()))
            throw std::runtime_error("Unexpected target pack entry: " + name);
        for (const auto& member : fs::directory_iterator(entry))
            if (!pack.files.contains(name + "/" + path_utf8(member.path().filename())))
                throw std::runtime_error("Uninventoried target pack member: " + path_utf8(member.path()));
    }
    pack.files.emplace(pack_manifest, manifest);
    return pack;
}

Pack read_pack(const fs::path& requested) { return read_pack(requested, {}); }

Pack load_pack(std::string_view id) {
    if (!target_name(id)) throw std::invalid_argument("Invalid target pack ID.");
    const auto path = pack_store() / utf8_path(id);
    if (!fs::exists(fs::symlink_status(path)))
        throw std::runtime_error("Target pack " + std::string(id) +
            " is not installed. Use voidup target list or voidup target add DIRECTORY. Publishing without a pack requires its matching native host.");
    auto pack = read_pack(path);
    if (pack.id != id) throw std::runtime_error("Installed target pack identity differs from its registration.");
    return pack;
}

int pack_command(std::span<const std::string_view> args) {
    if (args.empty()) throw std::invalid_argument("Use target list [--available], add DIRECTORY, inspect ID, verify ID or remove ID.");
    const auto verb = args.front();
    if (verb == "list" && args.size() == 2 && args[1] == "--available") {
        std::cout << "No signed official target packs are enabled on this release channel.\n";
        return 0;
    }
    if (verb == "list" && args.size() == 1) {
        const auto root = pack_store();
        if (!fs::exists(fs::symlink_status(root))) return 0;
        std::set<std::string> ids;
        for (const auto& entry : fs::directory_iterator(project::inspect_creation_root(root).path)) {
            const auto id = path_utf8(entry.path().filename());
            if (target_name(id)) ids.insert(id);
        }
        for (const auto& id : ids) std::cout << id << '\n';
        return 0;
    }
    if (args.size() != 2) throw std::invalid_argument("Target command requires one pack ID or directory.");
    if (verb == "add") {
        const auto pack = read_pack(utf8_path(args[1]));
        if (!custom_target(pack.id))
            throw std::runtime_error("Local target packs require a custom-* ID. Official IDs are reserved for authenticated acquisition.");
        project::with_user_package_store(pack_store(), true, [&](const project::CreationRoot& store) {
            const auto current = store.path / utf8_path(pack.id);
            if (fs::exists(fs::symlink_status(current))) {
                if (read_pack(current).digest != pack.digest)
                    throw std::runtime_error("A different pack already uses this ID. Register under a new custom ID.");
                return;
            }
            const std::map<std::string, std::string_view> directories{{"licenses/", ""}, {"link/", ""}};
            std::map<std::string, std::string_view> files = directories;
            for (const auto& [name, bytes] : pack.files) files.emplace(name, bytes);
            project::publish_project_directory(store, pack.id, files, [&](const auto& candidate, const auto& reader) {
                if (read_pack(candidate, reader).digest != pack.digest) throw std::runtime_error("Target pack changed during registration.");
            });
        }, true);
        print_pack(pack);
    } else if (verb == "inspect" || verb == "verify") {
        print_pack(load_pack(args[1]));
    } else if (verb == "remove") {
        if (!target_name(args[1])) throw std::invalid_argument("Invalid target pack ID.");
        bool removed = false;
        project::with_user_package_store(pack_store(), false, [&](const project::CreationRoot& store) {
            const auto id = std::string(args[1]);
            if (!fs::is_directory(fs::symlink_status(store.path / utf8_path(id)))) return;
            project::remove_user_registration(store, id);
            removed = true;
        });
        if (!removed) throw std::runtime_error("Target pack is not installed.");
    } else if (verb == "install" || verb == "update") {
        throw std::runtime_error("No signed official target packs are enabled on this release channel.");
    } else throw std::invalid_argument("Unknown target pack command.");
    return 0;
}
}
