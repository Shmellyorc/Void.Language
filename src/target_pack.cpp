#include "target_pack.hpp"
#include "frontend/project_digest.hpp"
#include "frontend/project_files.hpp"
#include "void/path.hpp"
#include "void/project_creation.hpp"
#include <llvm/BinaryFormat/Magic.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Object/Archive.h>
#include <llvm/Object/COFF.h>
#include <llvm/Object/COFFImportFile.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Process.h>
#include <llvm/Support/Endian.h>
#include <algorithm>
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
        if (target.architecture == Architecture::arm64) result.emplace(windows_arm64_synchronization);
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
llvm::Triple::ArchType llvm_architecture(const Target& target) {
    return target.architecture == Architecture::arm64 ? llvm::Triple::aarch64 : llvm::Triple::x86_64;
}
void check_archive(std::string_view name, std::string_view bytes, const Target& target) {
    auto archive = llvm::object::Archive::create({bytes, name});
    std::string member_name;
    const auto reject = [&](std::string_view reason) {
        auto message = "Target pack contains a malformed or incompatible archive: " + std::string(name);
        if (!member_name.empty()) message += " (" + member_name + ")";
        throw std::runtime_error(message + ": " + std::string(reason));
    };
    if (!archive) {
        llvm::consumeError(archive.takeError());
        reject("invalid archive structure");
    }
    const bool windows = target.system == System::windows_msvc;
    const bool msvc_sdk = windows && (name == windows_arm64_synchronization ||
        std::ranges::find(windows_libraries, name) != windows_libraries.end());
    const auto expected = llvm_architecture(target);
    const auto machine = expected == llvm::Triple::aarch64 ? llvm::COFF::IMAGE_FILE_MACHINE_ARM64 :
        llvm::COFF::IMAGE_FILE_MACHINE_AMD64;
    const bool arm_sdk = msvc_sdk && machine == llvm::COFF::IMAGE_FILE_MACHINE_ARM64;
    bool alternate_abi = false;
    const auto matches_machine = [&](std::uint16_t actual) {
        // Native ARM64 SDK archives can also hold ARM64EC and x64 code. The
        // explicitly targeted LLD driver checks every member used by the link.
        if (arm_sdk && (actual == llvm::COFF::IMAGE_FILE_MACHINE_ARM64EC || actual == llvm::COFF::IMAGE_FILE_MACHINE_AMD64)) {
            alternate_abi = true;
            return false;
        }
        if (msvc_sdk && actual == llvm::COFF::IMAGE_FILE_MACHINE_UNKNOWN) return false;
        if (actual != machine && !(arm_sdk && actual == llvm::COFF::IMAGE_FILE_MACHINE_ARM64X))
            reject("COFF machine differs from the target (" + std::to_string(actual) + ")");
        return true;
    };
    auto iteration_error = llvm::Error::success();
    std::size_t count = 0, compatible = 0;
    bool neutral_sdk_symbols = false;
    for (const auto& member : (*archive)->children(iteration_error)) {
        auto buffer = member.getMemoryBufferRef();
        if (!buffer) {
            llvm::consumeError(buffer.takeError());
            reject("invalid member extent");
        }
        member_name = std::string(llvm::sys::path::filename(buffer->getBufferIdentifier()));
        if (++count > 100000) reject("member count exceeds limit");
        const auto kind = llvm::identify_magic(buffer->getBuffer());
        if ((windows && kind == llvm::file_magic::windows_resource) ||
            (msvc_sdk && kind == llvm::file_magic::unknown)) {
            // These vendor records carry no readable machine identity. Keep
            // checking all native members and let LLD reject any unsupported
            // record that the application actually selects from the archive.
            continue;
        }
        if (msvc_sdk && kind == llvm::file_magic::coff_cl_gl_object) {
            // MSVC libraries can carry lazy LTCG members alongside ordinary
            // COFF. Validate the anonymous header's machine. LLD decides
            // whether the application actually requires such a member.
            const auto contents = buffer->getBuffer();
            if (contents.size() < 8) reject("truncated MSVC LTCG member");
            matches_machine(llvm::support::endian::read16le(contents.data() + 6));
            continue;
        }
        if (kind == llvm::file_magic::bitcode) {
            auto triple = llvm::getBitcodeTargetTriple(*buffer);
            if (!triple) {
                llvm::consumeError(triple.takeError());
                reject("invalid LLVM bitcode");
            }
            const llvm::Triple value{llvm::StringRef(*triple)};
            if (value.getArch() != expected || (windows ? !value.isWindowsMSVCEnvironment() : !value.isOSLinux()))
                reject("LLVM triple differs from the target");
        } else if (windows && kind == llvm::file_magic::coff_import_library) {
            if (buffer->getBufferSize() < sizeof(llvm::object::coff_import_header))
                reject("truncated import header");
            const llvm::object::COFFImportFile object(*buffer);
            if (!matches_machine(object.getMachine())) continue;
        } else if (windows && kind == llvm::file_magic::coff_object) {
            auto object = llvm::object::COFFObjectFile::create(*buffer);
            if (!object) {
                llvm::consumeError(object.takeError());
                reject("invalid COFF object");
            }
            if (!(*object)->isRelocatableObject()) reject("COFF member is not a relocatable object");
            if (!matches_machine((*object)->getMachine())) {
                // Import and alias archives such as oldnames.lib can have
                // only machine-neutral weak symbol records.
                if ((*object)->getMachine() == llvm::COFF::IMAGE_FILE_MACHINE_UNKNOWN &&
                    !(*object)->symbols().empty()) {
                    bool neutral = true;
                    for (const auto& section : (*object)->sections())
                        if ((section.isText() && section.getSize()) || !section.relocations().empty()) {
                            neutral = false;
                            break;
                        }
                    neutral_sdk_symbols |= neutral;
                }
                continue;
            }
        } else {
            if (windows || kind != llvm::file_magic::elf_relocatable)
                reject("unexpected member format " + std::to_string(static_cast<int>(kind)));
            auto object = llvm::object::ObjectFile::createObjectFile(*buffer);
            if (!object) {
                llvm::consumeError(object.takeError());
                reject("invalid relocatable object");
            }
            if (!llvm::isa<llvm::object::ELF64LEObjectFile>(object->get()) || (*object)->getArch() != expected)
                reject("relocatable object has the wrong architecture");
        }
        ++compatible;
    }
    if (iteration_error) {
        llvm::consumeError(std::move(iteration_error));
        reject("invalid member table");
    }
    if (!compatible && (!neutral_sdk_symbols || alternate_abi)) reject("no compatible linkable object");
}
void check_link_file(std::string_view name, std::string_view bytes, const Target& target) {
    const auto kind = llvm::identify_magic(bytes);
    if (name.ends_with(".a") || name.ends_with(".lib")) {
        // A thin archive can reference arbitrary physical files at link time.
        if (kind != llvm::file_magic::archive || !bytes.starts_with("!<arch>\n"))
            throw std::runtime_error("Expected a self-contained archive in target pack: " + std::string(name));
        check_archive(name, bytes, target);
        return;
    }
    if (kind != (name.ends_with(".o") ? llvm::file_magic::elf_relocatable : llvm::file_magic::elf_shared_object))
        throw std::runtime_error("Expected a target ELF file in target pack: " + std::string(name));
    auto file = llvm::object::ObjectFile::createObjectFile({bytes, name});
    if (!file) throw std::runtime_error("Invalid target ELF file: " + llvm::toString(file.takeError()));
    if (!llvm::isa<llvm::object::ELF64LEObjectFile>(file->get()) || (*file)->getArch() != llvm_architecture(target))
        throw std::runtime_error("Target pack link input has the wrong architecture: " + std::string(name));
}
bool notice_path(std::string_view path) {
    unsigned depth = 0;
    while (!path.empty()) {
        const auto slash = path.find('/');
        const auto component = path.substr(0, slash);
        if (++depth > 3 || component.size() > 128 || !portable_path_component(component))
            return false;
        if (slash == path.npos) return true;
        path.remove_prefix(slash + 1);
    }
    return false;
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

static Pack read_pack(const fs::path& requested, const project::ProjectDirectoryReader& reader,
                      std::string_view expected_manifest = {}) {
    const auto root = reader.inspect(requested).path;
    const auto manifest = reader.read(root / pack_manifest, manifest_limit);
    const auto manifest_digest = digest(manifest);
    if (!expected_manifest.empty() && expected_manifest != manifest_digest)
        throw std::runtime_error("Target pack manifest differs from the authenticated digest.");
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
    Pack pack{target, std::move(id), std::move(revision), std::move(version), std::move(minimum), manifest_digest, {}};
    std::set<std::string> names;
    std::map<std::string, std::string> directories{{"licenses", "licenses"}, {"link", "link"}};
    std::size_t budget = pack_limit;
    std::size_t sources = 0;
    while (std::getline(rows, line)) {
        const auto tab = line.find('\t');
        if (tab != 64 || !hex(std::string_view(line).substr(0, tab), 64) || names.size() == 96)
            throw std::runtime_error("Invalid target pack inventory record.");
        const auto name = line.substr(tab + 1);
        const auto slash = name.find('/');
        if (slash == name.npos) throw std::runtime_error("Target pack requires a link/, licenses/ or source/ member.");
        const auto role = name.substr(0, slash);
        const auto leaf = name.substr(slash + 1);
        const bool source = role == "source" && target->system == System::linux_gnu &&
            std::ranges::find(linux_http_sources, leaf) != linux_http_sources.end();
        const auto folded = project::fold_project_name(name);
        const bool reserved_notice = folded == "licenses/application-license.txt" ||
            folded == "licenses/gnu-http-libraries.txt" ||
            std::ranges::any_of(linux_http_sources, [&](std::string_view source_name) {
                return folded == "licenses/" + std::string(source_name);
            });
        const bool valid_name = role == "licenses" ? notice_path(leaf) && !reserved_notice :
            leaf.size() <= 128 && leaf.find('/') == leaf.npos && portable_path_component(leaf);
        if (!valid_name || (role != "link" && role != "licenses" && !source) ||
            directories.contains(folded) || !names.insert(folded).second)
            throw std::runtime_error("Invalid or duplicate target pack member: " + name);
        for (auto parent = name.find('/'); parent != name.npos; parent = name.find('/', parent + 1)) {
            const auto directory = name.substr(0, parent);
            const auto key = project::fold_project_name(directory);
            const auto& [item, inserted] = directories.emplace(key, directory);
            if ((!inserted && item->second != directory) || names.contains(key))
                throw std::runtime_error("Conflicting target pack directory: " + directory);
        }
        if (role == "link" && !expected.erase(leaf)) throw std::runtime_error("Unexpected target pack link input: " + name);
        const std::size_t member_limit = role == "link" ? budget : (source ? 8 : 1) * 1024 * 1024;
        auto bytes = reader.read(root / utf8_path(name), std::min(budget, member_limit));
        if (bytes.empty() || digest(bytes) != line.substr(0, tab))
            throw std::runtime_error("Target pack checksum mismatch: " + name);
        budget -= bytes.size();
        if (role == "link") check_link_file(leaf, bytes, *target);
        if (source) ++sources;
        pack.files.emplace(name, std::move(bytes));
    }
    if (!expected.empty()) throw std::runtime_error("Incomplete target pack. Missing link/" + *expected.begin());
    if (sources && sources != linux_http_sources.size())
        throw std::runtime_error("Target pack requires both GNU HTTP provider source archives.");
    if (!pack.files.contains("licenses/VOID-MIT-LICENSE.txt"))
        throw std::runtime_error("Target pack is missing its VOID license.");
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        const auto name = path_utf8(entry.path().lexically_relative(root));
        if (name == pack_manifest) continue;
        const auto kind = entry.symlink_status();
        if (fs::is_directory(kind)) {
            const auto directory = directories.find(project::fold_project_name(name));
            if (directory != directories.end() && directory->second == name) continue;
        } else if (fs::is_regular_file(kind) && pack.files.contains(name)) continue;
        throw std::runtime_error("Uninventoried target pack member: " + name);
    }
    pack.files.emplace(pack_manifest, manifest);
    return pack;
}

Pack read_pack(const fs::path& requested) { return read_pack(requested, {}); }

namespace {
struct ManagedPack {
    std::string inventory;
    std::string manifest;
};

// The managed launcher supplies only selections it authenticated with the retained
// release keys. The compiler binds its independent input snapshot to that selection.
std::map<std::string, ManagedPack> managed_packs() {
    const auto selected = llvm::sys::Process::GetEnv("VOIDUP_TARGET_PACKS");
    std::map<std::string, ManagedPack> packs;
    if (!selected || selected->empty()) return packs;
    if (selected->size() > 320)
        throw std::runtime_error("Invalid managed target-pack selection.");
    std::istringstream rows(*selected);
    std::string row;
    while (std::getline(rows, row, ',')) {
        const auto id_end = row.find(':');
        const auto inventory_end = row.find(':', id_end == row.npos ? id_end : id_end + 1);
        const auto id = std::string_view(row).substr(0, id_end);
        const auto* target = find_target(id);
        if (!target || target->system != System::linux_gnu || inventory_end == row.npos ||
            !hex(std::string_view(row).substr(id_end + 1, inventory_end - id_end - 1), 64) ||
            !hex(std::string_view(row).substr(inventory_end + 1), 64) ||
            !packs.emplace(id, ManagedPack{row.substr(id_end + 1, 64), row.substr(inventory_end + 1)}).second)
            throw std::runtime_error("Invalid managed target-pack selection.");
    }
    return packs;
}
}

Pack load_pack(std::string_view id) {
    if (!target_name(id)) throw std::invalid_argument("Invalid target pack ID.");
    auto path = pack_store() / utf8_path(id);
    std::string expected_manifest;
    if (!custom_target(id)) {
        const auto selections = managed_packs();
        const auto found = selections.find(std::string(id));
        if (found == selections.end()) {
            if (find_target(id)->system == System::macos)
                throw std::runtime_error("Offline macOS cross-linking requires independently redistributable libSystem and framework inputs. Use the matching native macOS toolchain.");
            throw std::runtime_error("No signed official pack is enabled for " + std::string(id) +
                ". Use voidup target list --available to check for an official pack or select a registered custom pack.");
        }
        path = pack_store().parent_path() / "official-target-packs" / ("p-" + found->second.inventory);
        expected_manifest = found->second.manifest;
    }
    if (!fs::exists(fs::symlink_status(path)))
        throw std::runtime_error("Target pack " + std::string(id) +
            " is not installed. Use voidup target install ID or voidup target add DIRECTORY.");
    auto pack = read_pack(path, {}, expected_manifest);
    if (pack.id != id) throw std::runtime_error("Installed target pack identity differs from its registration.");
    return pack;
}

int pack_command(std::span<const std::string_view> args) {
    if (args.empty()) throw std::invalid_argument("Use target list, add DIRECTORY, inspect ID, verify ID or remove ID. Use VOIDUP to acquire official packs.");
    const auto verb = args.front();
    if (verb == "compatibility" && args.size() == 1) {
        std::cout << "VOID target pack 1|" VOID_COMPILER_VERSION "|" LLVM_VERSION_STRING "|void-aot-64-v1\n";
        return 0;
    }
    if (verb == "check-payload" && args.size() == 6) {
        if (!hex(args[5], 64)) throw std::invalid_argument("Expected the signed pack manifest digest.");
        const auto pack = read_pack(utf8_path(args[1]), {}, args[5]);
        if (pack.id != args[2] || pack.version != args[3] || pack.revision != args[4])
            throw std::runtime_error("Target pack identity differs from the signed catalog.");
        return 0;
    }
    if (verb == "list" && args.size() == 2 && args[1] == "--available") {
        std::cout << "No signed official target packs are enabled in standalone voidc. Use voidup target list --available.\n";
        return 0;
    }
    if (verb == "list" && args.size() == 1) {
        const auto root = pack_store();
        std::set<std::string> ids;
        for (const auto& [id, selection] : managed_packs()) ids.insert(id);
        if (fs::exists(fs::symlink_status(root))) {
            for (const auto& entry : fs::directory_iterator(project::inspect_creation_root(root).path)) {
                const auto id = path_utf8(entry.path().filename());
                if (custom_target(id)) ids.insert(id);
            }
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
            std::map<std::string, std::string_view> files{{"licenses/", ""}, {"link/", ""}};
            if (pack.files.contains(std::string("source/") + linux_http_sources.front()))
                files.emplace("source/", "");
            for (const auto& [name, bytes] : pack.files) {
                for (auto slash = name.find('/'); slash != name.npos; slash = name.find('/', slash + 1))
                    files.emplace(name.substr(0, slash + 1), "");
                files.emplace(name, bytes);
            }
            project::publish_project_directory(store, pack.id, files, [&](const auto& candidate, const auto& reader) {
                if (read_pack(candidate, reader).digest != pack.digest) throw std::runtime_error("Target pack changed during registration.");
            });
        }, true);
        print_pack(pack);
    } else if (verb == "inspect" || verb == "verify") {
        print_pack(load_pack(args[1]));
    } else if (verb == "remove") {
        if (!target_name(args[1])) throw std::invalid_argument("Invalid target pack ID.");
        if (!custom_target(args[1])) throw std::invalid_argument("Use VOIDUP to remove official target packs.");
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
