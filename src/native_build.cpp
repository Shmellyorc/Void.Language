#include "void/native_build.hpp"
#include "void/backend.hpp"
#include "void/cache.hpp"
#include "void/c_primitive.hpp"
#include "void/path.hpp"
#include "void/utf8.hpp"
#include "void/project_resolution.hpp"
#include "frontend/project_files.hpp"
#include "frontend/project_native.hpp"
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/SHA256.h>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <sstream>
#include <set>
#ifdef _WIN32
#include "native_windows.hpp"
#endif
namespace vd {
namespace {
namespace fs = std::filesystem;
constexpr auto managed_payload_directory = "native";
// Physical paths, not VFS identities. Match each component before advancing;
// never canonicalize the destination and erase descendant spelling/alias evidence.
void exact_path(const fs::path& path) {
    auto current=path.root_path();
    for(const auto& component:path.relative_path()) {
        fs::path next;
        for(const auto& child:fs::directory_iterator(current))
            if(child.path().filename()==component){next=child.path();break;}
#ifdef _WIN32
        if(next.empty()) {
            // Like discover_project's Windows prefix walk, expand only a checked
            // existing directory ancestor. Unlike GetLongPathNameW alone, the
            // registered alternate name proves this is an exact OS 8.3 spelling,
            // rather than a case-insensitive match of an ordinary long name.
            WIN32_FIND_DATAW entry{};
            const auto candidate=current/component;
            auto handle=FindFirstFileW(candidate.c_str(),&entry);
            if(handle!=INVALID_HANDLE_VALUE) {
                FindClose(handle);
                if(entry.cAlternateFileName[0] && component.native()==entry.cAlternateFileName &&
                   (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    if(entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
                        throw std::runtime_error("Output path contains a reparse point: "+path_utf8(candidate));
                    next=current/entry.cFileName;
                }
            }
        }
        if(!next.empty()) {
            const auto attributes=GetFileAttributesW(next.c_str());
            if(attributes==INVALID_FILE_ATTRIBUTES)
                throw std::system_error(GetLastError(),std::system_category(),"Cannot inspect physical path");
            if((attributes & FILE_ATTRIBUTE_DIRECTORY) && (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
                throw std::runtime_error("Output path contains a reparse point: "+path_utf8(next));
        }
#endif
        if(next.empty())throw std::runtime_error("Output path has incorrect spelling: "+path_utf8(current/component));
        current=std::move(next);
    }
}
fs::path output_path(const ValidatedAnalysis& analysis,const fs::path& requested,bool managed=false) {
    auto path=fs::absolute(requested).lexically_normal();
    if(path.extension()==".void")throw std::runtime_error("Unsafe source/output collision: output has a source extension: "+path_utf8(path));
    if(path.filename().empty())throw std::runtime_error("Output must name a file.");
    if (managed) {
        auto leaf = path_utf8(path.filename());
#ifdef _WIN32
        leaf.resize(leaf.find_last_not_of(" .") + 1);
#endif
        if (project::fold_project_name(leaf) == managed_payload_directory)
            throw std::runtime_error("Managed native executable output conflicts with the reserved native payload directory: " + path_utf8(path));
    }
    // Check before directory creation, so a failed collision never touches input.
    for(const auto& module:analysis.modules()) {
        auto source=utf8_path(module.source.path());
        if(path==source || (fs::exists(path) && fs::exists(source) && fs::equivalent(path,source)))
            throw std::runtime_error("Unsafe source/output collision: "+path_utf8(path));
    }
    fs::create_directories(path.parent_path());
    exact_path(path.parent_path());
    auto status=fs::symlink_status(path);
    if(fs::exists(status)) {
        exact_path(path);
        if(!fs::is_regular_file(status))throw std::runtime_error("Output must be an ordinary file: "+path_utf8(path));
    }
    return path;
}
#ifdef _WIN32
struct Staging : native_windows::Directory {
    explicit Staging(const fs::path& output):Directory(output.parent_path(),false,native_windows::staging_leaf){}
};
#else
struct Staging {
    fs::path directory;
    explicit Staging(const fs::path& output) {
        for(unsigned attempt=0;attempt<32;++attempt) {
            llvm::SmallString<64> name;
            // LLVM's unique-directory API substitutes '%' across the whole
            // path. Generate only the leaf, keeping physical parents literal.
            llvm::sys::fs::createUniquePath(".void-build-%%%%%%%%%%%%%%%%",name,true);
            directory=output.parent_path()/utf8_path(name.str().str()).filename();
            std::error_code error;
            if(fs::create_directory(directory,error)) {
                fs::permissions(directory,fs::perms::owner_all,fs::perm_options::replace,error);
                if(error){fs::remove(directory);throw std::runtime_error("Cannot secure native staging directory: "+error.message());}
                return;
            }
            if(error)throw std::runtime_error("Cannot create native staging directory: "+error.message());
        }
        throw std::runtime_error("Cannot reserve a unique native staging directory.");
    }
    ~Staging(){std::error_code ignored;fs::remove_all(directory,ignored);}
};
#endif
void publish(const fs::path& staged,const fs::path& output) {
    // POSIX rename and Windows MoveFileEx replace in one operation. Never remove
    // a good executable first; an executing/locked Windows file leaves it intact.
#ifdef _WIN32
    if(!MoveFileExW(staged.c_str(),output.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot publish native output (Windows error "+std::to_string(GetLastError())+"): "+path_utf8(output));
#else
    fs::rename(staged,output);
#endif
}
struct Resources {
    fs::path driver,linker,runtime,fixture;
    std::vector<std::string> flags;
    std::string verified_digests;
};
void verify_resources(const fs::path& config, Resources& resources) {
    const auto inventory = config.parent_path()/"void-native-resources.txt";
    std::ifstream file(inventory);
    std::string line;
    if(!std::getline(file,line) || line!="VOID native resources 1")
        throw std::runtime_error("Native resource inventory missing or invalid: "+path_utf8(inventory));
    std::vector<fs::path> checked;
    while(std::getline(file,line)) {
        if(!line.empty() && line.back()=='\r')line.pop_back();
        if(line.size()<66 || line[64]!='\t' || checked.size()>=64)
            throw std::runtime_error("Invalid native resource inventory entry");
        auto name=line.substr(65);
        const std::string marker="@VOID_NATIVE_DIR@";
        if(name.starts_with(marker))name.replace(0,marker.size(),path_utf8(config.parent_path()));
        const auto path=utf8_path(name).lexically_normal();
        std::ifstream input(path,std::ios::binary);
        if(!input || !fs::is_regular_file(path))
            throw std::runtime_error("Native link resource missing: "+path_utf8(path));
        llvm::SHA256 digest;
        std::array<char,65536> buffer;
        while(input.read(buffer.data(),buffer.size()) || input.gcount())
            digest.update(llvm::StringRef(buffer.data(),static_cast<std::size_t>(input.gcount())));
        if(!input.eof())throw std::runtime_error("Cannot read native link resource: "+path_utf8(path));
        constexpr char hex[]="0123456789abcdef";
        std::string actual;
        for(auto byte:digest.final()){actual+=hex[byte>>4];actual+=hex[byte&15];}
        if(actual!=line.substr(0,64))
            throw std::runtime_error("Native link resource checksum mismatch: "+path_utf8(path));
        resources.verified_digests += actual;
        if(std::find(checked.begin(),checked.end(),path)!=checked.end())
            throw std::runtime_error("Duplicate native resource inventory entry");
        checked.push_back(path);
    }
    if(!file.eof())throw std::runtime_error("Cannot read native resource inventory");
    auto require=[&](const fs::path& path){
        if(std::find(checked.begin(),checked.end(),path.lexically_normal())==checked.end())
            throw std::runtime_error("Native link resource absent from inventory: "+path_utf8(path));
    };
    require(resources.runtime);require(resources.fixture);
    for(const auto& flag:resources.flags) {
        const auto path=utf8_path(flag);
        const auto extension=path.extension();
        if(path.is_absolute() && (extension==".a" || extension==".lib" || extension==".so" || extension==".dylib"))require(path);
    }
}
Resources resources(const char* argv0) {
    auto executable=utf8_path(llvm::sys::fs::getMainExecutable(argv0,reinterpret_cast<void*>(&resources)));
    const auto config=executable.parent_path()/"void-native.txt";
    std::ifstream file(config);std::vector<std::string> lines;std::string line;
    while(std::getline(file,line)){if(!line.empty() && line.back()=='\r')line.pop_back();lines.push_back(line);}
    if(!file.eof() || lines.size()<5 || (lines[0]!="VOID native 23.1.4 v1" && lines[0]!="VOID native 23.1.4 v2"))
        throw std::runtime_error("Native toolchain resources missing or invalid: "+path_utf8(config));
    auto path=[&](const std::string& text){auto p=utf8_path(text);return p.is_absolute()?p:(config.parent_path()/p).lexically_normal();};
    Resources result{path(lines[1]),lines[2].empty()?fs::path{}:path(lines[2]),path(lines[3]),path(lines[4]),{},{}};
    for(const auto& p:{result.driver,result.runtime,result.fixture}) {
        if(!fs::is_regular_file(p))throw std::runtime_error("Native toolchain resource missing: "+path_utf8(p));
        exact_path(p);
    }
    if(!result.linker.empty()) {
        if(!fs::is_regular_file(result.linker))throw std::runtime_error("Native linker resource missing: "+path_utf8(result.linker));
        exact_path(result.linker);
    }
    if(!llvm::sys::fs::can_execute(path_utf8(result.driver)))throw std::runtime_error("Native Clang driver is not executable: "+path_utf8(result.driver));
    result.flags.assign(lines.begin()+5,lines.end());
    if(lines[0]=="VOID native 23.1.4 v2") {
        const std::string marker="@VOID_NATIVE_DIR@";
        for(auto& flag:result.flags) {
            auto position=flag.find(marker);
            if(position!=std::string::npos) {
                flag.replace(position,marker.size(),path_utf8(config.parent_path()));
            }
        }
        verify_resources(config,result);
    }
    return result;
}
int execute(const std::vector<std::string>& arguments,const fs::path& capture={}) {
    std::vector<llvm::StringRef> refs(arguments.begin(),arguments.end());
    std::string error;bool failed=false;
    std::vector<std::optional<llvm::StringRef>> redirects;
    const auto capture_text=path_utf8(capture);
    if(!capture.empty())redirects={std::nullopt,capture_text,std::nullopt};
    int status=llvm::sys::ExecuteAndWait(arguments[0],refs,std::nullopt,redirects,0,0,&error,&failed);
    if(failed || status<0)throw std::runtime_error("Native toolchain execution failed: "+error);
    return status;
}
void version(const fs::path& tool,const char* expected,const fs::path& capture) {
    if(execute({path_utf8(tool),"--version"},capture)!=0)throw std::runtime_error("Native toolchain version check failed: "+path_utf8(tool));
    std::ifstream file(capture);std::string text((std::istreambuf_iterator<char>(file)),{});
    const auto position=text.find(expected);
    if(position==std::string::npos || (position+std::char_traits<char>::length(expected)<text.size() && std::isdigit(static_cast<unsigned char>(text[position+std::char_traits<char>::length(expected)]))))
        throw std::runtime_error("Native toolchain requires matching LLVM/Clang/LLD 23.1.4: "+path_utf8(tool));
}
}
void publish_object(const ValidatedAnalysis& analysis,const fs::path& requested,bool optimize,std::string_view profile) {
    auto output=output_path(analysis,requested);Staging stage(output);
    auto object=stage.directory/"program.o";
    emit_object(analysis.lower(),object,optimize,true,std::string(compiler_build_identity())+":"+std::string(profile));
    output_path(analysis,output);publish(object,output);
}
static std::string link_executable(const ValidatedAnalysis& analysis, const fs::path& requested, bool optimize,
                                   const char* argv0, std::string_view profile, bool package) {
    if (analysis.c_primitive_admission() && !analysis.c_primitive_admission()->dynamic())
        throw std::invalid_argument("Host-admitted C primitives require an explicitly linked AOT fixture");
    auto config=[&] {
        try {return resources(argv0);}
        catch(const std::exception& error) {
            throw std::runtime_error(std::string(error.what())+
              "\nReinstall the matching CMake compiler component; keep bin/void-native*.txt, lib/void, "
              "and the original LLVM/Clang/LLD 23.1.4 SDK together. See docs/install/README.md.");
        }
    }();
    if (package) {
        if (config.verified_digests.empty()) throw std::runtime_error("Application packaging requires a verified v2 native resource inventory.");
        // NativeBuild.cmake emits this group to locate the development IDN
        // libraries. Package executables use their OS-provided SONAMEs.
        const std::array<std::string,3> prefix{"-Xlinker", "-rpath", "-Xlinker"};
        auto found = std::search(config.flags.begin(), config.flags.end(), prefix.begin(), prefix.end());
        if (found != config.flags.end() && config.flags.end() - found >= 4) config.flags.erase(found, found + 4);
    }
    const auto& native = analysis.c_primitive_admission();
    const auto managed = native && native->dynamic() && !native->dynamic()->payload().empty();
    auto output=output_path(analysis,requested,managed);Staging stage(output);
    const auto payload = managed ? project::read_managed_payload(*native) : std::map<std::string, std::string>{};
    const auto destination = managed ? std::optional(project::inspect_creation_root(output.parent_path())) : std::nullopt;
#ifdef _WIN32
    auto root=native_windows::temporary_root();exact_path(root);
    native_windows::Directory linking(root,true);
    const auto& work=linking.directory;
    native_windows::copy_checked(config.runtime,work/"runtime.lib");
    native_windows::copy_checked(config.fixture,work/"fixture.lib");
    config.runtime="runtime.lib";config.fixture="fixture.lib";
    const auto executable=work/"program.exe";
#else
    const auto& work=stage.directory;
    const auto executable=work/"program";
#endif
    version(config.driver,"clang version 23.1.4",work/"version.txt");
    if(!config.linker.empty())version(config.linker,"LLD 23.1.4",work/"version.txt");
    const auto object=work/"program.o";
    std::string provenance=std::string(compiler_build_identity())+":"+std::to_string(profile.size())+":"+std::string(profile)+":"+config.verified_digests;
    for(const auto& flag:config.flags)provenance+=std::to_string(flag.size())+":"+flag;
    std::vector<std::string> args{path_utf8(config.driver),"--driver-mode=g++"};
    if(!config.linker.empty())args.push_back("--ld-path="+path_utf8(config.linker));
    // LLD interprets '%' in absolute output paths. The driver's child working
    // directory gives the linker a relative name. Windows uses a short private
    // workspace because Clang cannot chdir to a genuine long project path.
#ifdef _WIN32
    const auto object_argument=object.filename();
#else
    const auto& object_argument=object;
#endif
    args.insert(args.end(),{"-working-directory",path_utf8(work),path_utf8(object_argument),path_utf8(config.runtime),path_utf8(config.fixture),"-o",path_utf8(executable.filename())});
    const auto& locked = analysis.locked_dependencies();
    if (managed && locked && !locked->native_link_inputs().empty()) {
        const auto inputs = project::read_managed_link_inputs(*native, locked->native_link_inputs());
        const fs::path input_directory = "catalog-link";
        fs::create_directory(work / input_directory);
        for (const auto& [name, bytes] : inputs) {
            const auto relative = input_directory / utf8_path(name);
            std::ofstream file(work / relative, std::ios::binary);
            file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            file.close();
            if (!file) throw std::runtime_error("Cannot stage managed native link input: " + name);
            args.insert(args.end(), {"-Xlinker", path_utf8(relative)});
        }
    }
    // Dependency archives follow the program/runtime objects that reference
    // them. Driver options in this list apply to the whole invocation.
    args.insert(args.end(),config.flags.begin(),config.flags.end());
#if defined(__linux__)
    args.push_back("-ldl"); // Optional system terminfo binding in the Runtime archive.
#endif
    emit_object(analysis.lower(ir::Emission::entry),object,optimize,true,provenance);
    auto status=execute(args);
    if(status!=0)throw std::runtime_error("Native linker failed (status "+std::to_string(status)+
      "). Driver: "+path_utf8(config.driver)+"\nCheck the linker diagnostics above and host C/C++ development libraries. See docs/install/README.md.");
    if(!fs::is_regular_file(executable) || fs::file_size(executable)==0 || !llvm::sys::fs::can_execute(path_utf8(executable)))
        throw std::runtime_error("Native linker did not produce a usable executable.");
    if (managed) {
        std::map<std::string, std::string_view> files(payload.begin(), payload.end());
        project::publish_project_directory(*destination, managed_payload_directory, files, [&](const fs::path& directory) {
            if (project::inspect_creation_root(directory).path != directory)
                throw std::runtime_error("Managed native output directory spelling changed.");
            std::size_t count = 0;
            for (const auto& entry : fs::directory_iterator(directory)) {
                const auto file = payload.find(path_utf8(entry.path().filename()));
                if (file == payload.end() || project::read_project_input(entry.path(), file->second.size()) != file->second)
                    throw std::runtime_error("Managed native output has a different payload; select a fresh output directory.");
                ++count;
            }
            if (count != payload.size()) throw std::runtime_error("Managed native output payload is incomplete.");
        });
    }
#ifdef _WIN32
    // Check PE structure through LLVM, not merely child status or nonempty bytes.
    // Keep the original pinned, with writes/deletion denied, through transfer.
    auto pinned=native_windows::verified_executable(executable);
    const auto staged=stage.directory/"program.exe";
    native_windows::copy_checked(executable,staged,true);
    output_path(analysis,output);publish(staged,output);
#else
    output_path(analysis,output);publish(executable,output);
#endif
    return std::move(config.verified_digests);
}

void build_executable(const ValidatedAnalysis& analysis, const fs::path& requested, bool optimize,
                      const char* argv0, std::string_view profile) {
    link_executable(analysis, requested, optimize, argv0, profile, false);
}

#if defined(__linux__) && defined(__GLIBC__) && defined(__x86_64__)
namespace {
constexpr std::string_view package_header = "VOID application package 1\ntarget x86_64-unknown-linux-gnu\n";
constexpr auto inventory_name = "void-package.txt";
constexpr auto executable_name = "app";
constexpr auto metadata_name = "application.meta";
constexpr std::size_t package_byte_limit = 256 * 1024 * 1024;
constexpr std::size_t inventory_byte_limit = 64 * 1024;
constexpr auto directory_mode = fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec |
                                fs::perms::others_read | fs::perms::others_exec;
constexpr auto data_mode = fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read | fs::perms::others_read;

bool hex_digest(std::string_view hash) {
    return hash.size() == 64 && hash.find_first_not_of("0123456789abcdef") == hash.npos;
}

std::string package_digest(std::string_view bytes) {
    const auto hash = llvm::SHA256::hash({reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()});
    std::string result;
    constexpr char digits[] = "0123456789abcdef";
    for (auto byte : hash) { result += digits[byte >> 4]; result += digits[byte & 15]; }
    return result;
}

bool package_leaf(std::string_view name) {
    if (name.size() > 128 || !valid_utf8(name) || name.find_first_of("\t\r\n/") != name.npos) return false;
    std::string portable(name);
    for (char& byte : portable) if (static_cast<unsigned char>(byte) >= 128) byte = 'x';
    return portable_path_component(portable);
}

void write_package_file(const fs::path& path, std::string_view bytes) {
    fs::create_directory(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    file.close();
    if (!file) throw std::runtime_error("Cannot write application package file: " + path_utf8(path));
    fs::permissions(path, data_mode);
}

void copy_notices(const fs::path& stage, const char* argv0) {
    const auto compiler = utf8_path(llvm::sys::fs::getMainExecutable(argv0, reinterpret_cast<void*>(&resources)));
    const auto path = compiler.parent_path() / "void-package-notices.txt";
    std::istringstream list(project::read_project_input(path, inventory_byte_limit));
    std::string line;
    if (!std::getline(list, line) || line != "VOID package notices 1")
        throw std::runtime_error("Invalid application package notice inventory: " + path_utf8(path));
    std::set<std::string> notices;
    while (std::getline(list, line)) {
        const auto tab = line.find('\t', 65);
        if (line.size() < 68 || line[64] != '\t' || tab == line.npos || !hex_digest(std::string_view(line).substr(0, 64)))
            throw std::runtime_error("Invalid application package notice record.");
        const auto name = line.substr(65, tab - 65);
        if (!package_leaf(name) || notices.size() == 64 || !notices.insert(project::fold_project_name(name)).second)
            throw std::runtime_error("Invalid or duplicate application package notice: " + name);
        const auto source = (path.parent_path() / utf8_path(line.substr(tab + 1))).lexically_normal();
        const auto bytes = project::read_project_input(source, 1024 * 1024);
        if (bytes.empty() || package_digest(bytes) != line.substr(0, 64))
            throw std::runtime_error("Application package notice checksum mismatch: " + name);
        write_package_file(stage / "licenses" / utf8_path(name), bytes);
    }
    if (!notices.contains("void-mit-license.txt")) throw std::runtime_error("Missing VOID application package license.");
}

void check_package_elf(std::string_view bytes) {
    auto image = llvm::object::ObjectFile::createObjectFile({llvm::StringRef(bytes), executable_name});
    if (!image) throw std::runtime_error("Invalid application package executable: " + llvm::toString(image.takeError()));
    const auto* elf = llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(image->get());
    if (!elf || elf->getArch() != llvm::Triple::x86_64 || elf->isRelocatableObject())
        throw std::runtime_error("Application package requires a Linux x86-64 executable.");
    auto tags = elf->getELFFile().dynamicEntries();
    if (!tags) throw std::runtime_error("Cannot inspect application package dynamic entries: " + llvm::toString(tags.takeError()));
    for (const auto& tag : *tags)
        if (tag.getTag() == llvm::ELF::DT_RPATH || tag.getTag() == llvm::ELF::DT_RUNPATH)
            throw std::runtime_error("Application package executable contains an embedded library search path.");
}

std::string package_inventory(const fs::path& root, bool prepare) {
    std::map<std::string, std::string> rows;
    std::size_t remaining = package_byte_limit;
    auto mode = [&](const fs::path& path, fs::perms expected) {
        if (prepare) fs::permissions(path, expected);
        else if (fs::status(path).permissions() != expected)
            throw std::runtime_error("Application package permissions changed: " + path_utf8(path));
    };
    auto file = [&](const fs::directory_entry& entry, const std::string& name) {
        if (rows.size() == 128 || !fs::is_regular_file(entry.symlink_status()))
            throw std::runtime_error("Invalid application package file: " + name);
        const auto bytes = project::read_project_input(entry.path(), remaining);
        remaining -= bytes.size();
        mode(entry.path(), name == executable_name ? directory_mode : data_mode);
        if (name == executable_name) check_package_elf(bytes);
        rows.emplace(name, package_digest(bytes) + '\t' + std::to_string(bytes.size()) + '\t' + name + '\n');
    };
    mode(root, directory_mode);
    for (const auto& entry : fs::directory_iterator(root)) {
        const auto name = path_utf8(entry.path().filename());
        if (name == inventory_name && !prepare) continue;
        if (name == executable_name || name == metadata_name) file(entry, name);
        else if ((name == "assets" || name == "native" || name == "licenses") && fs::is_directory(entry.symlink_status())) {
            mode(entry.path(), directory_mode);
            std::set<std::string> names;
            for (const auto& child : fs::directory_iterator(entry)) {
                const auto leaf = path_utf8(child.path().filename());
                if (!package_leaf(leaf) || !names.insert(project::fold_project_name(leaf)).second)
                    throw std::runtime_error("Invalid or duplicate application package path: " + name + '/' + leaf);
                file(child, name + '/' + leaf);
            }
            if (names.empty()) throw std::runtime_error("Unexpected empty application package directory: " + name);
        } else throw std::runtime_error("Unexpected application package entry: " + name);
    }
    if (!rows.contains(executable_name) || !rows.contains(metadata_name) || !rows.contains("licenses/VOID-MIT-LICENSE.txt"))
        throw std::runtime_error("Application package is missing its executable, metadata or license.");
    std::string result;
    for (const auto& [name, row] : rows) result += row;
    return result;
}

ApplicationMetadata::Owner verify_package(const fs::path& directory) {
    const auto root = project::inspect_creation_root(directory).path;
    const auto path = root / inventory_name;
    const auto text = project::read_project_input(path, inventory_byte_limit);
    if (fs::status(path).permissions() != data_mode || !text.starts_with(package_header))
        throw std::runtime_error("Invalid application package inventory: " + path_utf8(path));
    std::istringstream receipt(text);
    std::string magic, target, profile, lock, inputs;
    if (!std::getline(receipt, magic) || !std::getline(receipt, target) || !std::getline(receipt, profile) ||
        !std::getline(receipt, lock) || !std::getline(receipt, inputs) ||
        !profile.starts_with("profile project-profile-v1:") || !lock.starts_with("lock ") ||
        !inputs.starts_with("inputs ") || !hex_digest(std::string_view(inputs).substr(7)))
        throw std::runtime_error("Invalid application package inventory header.");
    lock.erase(0, 5);
    const std::string recorded{std::istreambuf_iterator<char>(receipt), {}};
    if ((lock != "-" && !hex_digest(lock)) || recorded != package_inventory(root, false))
        throw std::runtime_error("Application package inventory mismatch. A packaged file is missing, changed or unexpected.");
    return ApplicationMetadata::decode(project::read_project_input(root / metadata_name, ApplicationMetadata::byte_limit));
}
}
#endif

void build_application_package(const ProjectAnalysis& project_analysis, const fs::path& requested, bool optimize,
                               const char* argv0, std::string_view profile, bool replace) {
#if defined(__linux__) && defined(__GLIBC__) && defined(__x86_64__)
    if (!project_analysis.program()) throw std::runtime_error("Application packaging requires a checked application entry.");
    const auto& analysis = *project_analysis.program();
    const auto& metadata = analysis.application_metadata();
    if (!metadata || !analysis.application()) throw std::runtime_error("Application packaging requires a manifest-selected application entry.");
    if (!project_analysis.manifest()->native().empty()) throw std::runtime_error("Application packaging requires a managed catalog for native dependencies.");
    auto output = fs::absolute(requested).lexically_normal();
    if (output.has_relative_path() && output.filename().empty()) output = output.parent_path();
    const auto leaf = path_utf8(output.filename());
    if (!package_leaf(leaf) || leaf.starts_with('.') || project::fold_project_name(leaf).ends_with(".void"))
        throw std::runtime_error("Application package destination must name a portable directory.");
    auto existing_parent = output.parent_path();
    while (!fs::exists(fs::symlink_status(existing_parent))) existing_parent = existing_parent.parent_path();
    (void)project::inspect_creation_root(existing_parent);
    fs::create_directories(output.parent_path());
    const auto parent = project::inspect_creation_root(output.parent_path());
    std::function<void(const fs::path&)> verify_replacement;
    if (replace) verify_replacement = [&](const fs::path& previous) {
        if (verify_package(previous)->application_id() != metadata->application_id())
            throw std::runtime_error("Existing package belongs to a different application.");
    };
    project::publish_application_directory(parent, leaf, [&](const fs::path& stage) {
        copy_notices(stage, argv0);
        const auto license = project_analysis.root() / "LICENSE";
        if (fs::exists(fs::symlink_status(license)))
            write_package_file(stage / "licenses/APPLICATION-LICENSE.txt", project::read_project_input(license, 1024 * 1024));
        std::size_t remaining = project::application_resource_byte_limit;
        for (const auto& resource : project_analysis.manifest()->resources()) {
            const auto path = utf8_path(resource);
            const auto bytes = project::read_project_input(project_analysis.root() / path, remaining);
            remaining -= bytes.size();
            write_package_file(stage / path, bytes);
        }
        const auto link_digests = link_executable(analysis, stage / executable_name, optimize, argv0, profile, true);
        write_package_file(stage / metadata_name, metadata->bytes());
        const auto& lock = analysis.locked_dependencies();
        const auto files = package_inventory(stage, true);
        std::string inputs = std::string(compiler_build_identity()) + '\n' + std::string(profile) + '\n' +
            link_digests + '\n';
        for (const auto& module : analysis.modules()) {
            inputs += std::to_string(module.identity.size()) + ':' + module.identity;
            inputs += std::to_string(module.source.text().size()) + ':' + module.source.text();
        }
        std::istringstream records(files);
        std::string record;
        while (std::getline(records, record))
            if (!record.ends_with("\tapp")) inputs += record + '\n';
        const auto inventory = std::string(package_header) + "profile " + std::string(profile) +
            "\nlock " + (lock ? lock->identity() : "-") + "\ninputs " + package_digest(inputs) + '\n' + files;
        write_package_file(stage / inventory_name, inventory);
        (void)verify_package(stage);
    }, verify_replacement);
#else
    (void)project_analysis; (void)requested; (void)optimize; (void)argv0; (void)profile; (void)replace;
    throw std::runtime_error("Application packaging requires Linux x86-64 GNU.");
#endif
}

void verify_application_package(const fs::path& directory) {
#if defined(__linux__) && defined(__GLIBC__) && defined(__x86_64__)
    (void)verify_package(directory);
#else
    (void)directory;
    throw std::runtime_error("Application packaging requires Linux x86-64 GNU.");
#endif
}
}
