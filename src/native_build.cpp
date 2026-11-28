#include "void/crash_diagnostics.hpp"
#include "void/compiler_observation.hpp"
#include "void/native_build.hpp"
#include "void/distribution_archive.hpp"
#include "void/publish.hpp"
#include "target_pack.hpp"
#include "macos_bundle.hpp"
#include "void/binary_audit.hpp"
#include "frontend/project_digest.hpp"
#include "void/native_import.hpp"
#include "void/backend.hpp"
#include "void/private_diagnostics.hpp"
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
#include <llvm/Support/VersionTuple.h>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <sstream>
#include <set>
#ifdef _WIN32
#include "native_windows.hpp"
#else
#include "runtime/host_posix.hpp"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#if (defined(__linux__) && defined(__GLIBC__) && defined(__x86_64__)) || \
    (defined(_WIN32) && defined(_MSC_VER) && defined(_M_X64))
#define VOID_APPLICATION_PACKAGING
#endif
#if defined(VOID_APPLICATION_PACKAGING) || (defined(__APPLE__) && defined(__aarch64__)) || \
    (defined(__linux__) && defined(__GLIBC__) && defined(__aarch64__)) || \
    (defined(_WIN32) && defined(_MSC_VER) && defined(_M_ARM64))
#define VOID_NATIVE_PUBLISHING
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
            if(!::mkdir(directory.c_str(),0700)) {
                const auto fd=::open(directory.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
                struct stat held{},named{};
                const bool secured=fd>=0 && !::fstat(fd,&held) && !::lstat(directory.c_str(),&named) &&
                    S_ISDIR(held.st_mode) && held.st_uid==::geteuid() && !(held.st_mode&0077) &&
                    held.st_dev==named.st_dev && held.st_ino==named.st_ino &&
                    host::clear_inherited_acl(fd) && host::safe_acl(fd);
                if(fd>=0)::close(fd);
                if(!secured){fs::remove(directory);throw std::runtime_error("Cannot secure native staging directory ACL.");}
                return;
            }
            if(errno!=EEXIST)throw std::runtime_error("Cannot create native staging directory: "+std::generic_category().message(errno));
        }
        throw std::runtime_error("Cannot reserve a unique native staging directory.");
    }
    ~Staging(){std::error_code ignored;fs::remove_all(directory,ignored);}
};
#endif
void publish_link_output(const fs::path& staged,const fs::path& output) {
    observation::Scope timing(observation::Phase::publication);
    // POSIX rename and Windows MoveFileEx replace in one operation. Never remove
    // a good executable first; an executing/locked Windows file leaves it intact.
#ifdef _WIN32
    native_windows::publish_output(staged,output);
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
fs::path native_config_path(const char* argv0) {
    const auto executable = utf8_path(llvm::sys::fs::getMainExecutable(argv0, reinterpret_cast<void*>(&native_config_path)));
    return executable.parent_path() / "void-native.txt";
}
bool native_config_header(std::string_view header) {
    return header == "VOID native 23.1.4 v1" || header == "VOID native 23.1.4 v2";
}
fs::path config_path_value(const fs::path& config, std::string_view value) {
    const auto path = utf8_path(value);
    return path.is_absolute() ? path : (config.parent_path() / path).lexically_normal();
}
#ifdef VOID_NATIVE_PUBLISHING
fs::path pack_tools_directory(const char* argv0) {
    const auto path = native_config_path(argv0);
    std::ifstream config(path);
    std::string header, driver;
    if (!std::getline(config, header) || !std::getline(config, driver) || !native_config_header(header) || driver.empty())
        throw std::runtime_error("Cannot identify the host LLVM SDK from " + path_utf8(path));
    return config_path_value(path, driver).parent_path();
}
#endif
Resources resources(const char* argv0) {
    observation::Scope timing(observation::Phase::toolchain_validation);
    const auto config=native_config_path(argv0);
    std::ifstream file(config);std::vector<std::string> lines;std::string line;
    while(std::getline(file,line)){if(!line.empty() && line.back()=='\r')line.pop_back();lines.push_back(line);}
    if(!file.eof() || lines.size()<5 || !native_config_header(lines[0]))
        throw std::runtime_error("Native toolchain resources missing or invalid: "+path_utf8(config));
    auto path=[&](const std::string& text){return config_path_value(config, text);};
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
    vd::crash::child_termination(utf8_path(arguments[0]),status,false);
    if(failed || status<0)throw std::runtime_error("Native toolchain execution failed: "+error);
    return status;
}
void version(const fs::path& tool,const char* expected,const fs::path& capture) {
    observation::Scope timing(observation::Phase::toolchain_validation);
    if(execute({path_utf8(tool),"--version"},capture)!=0)throw std::runtime_error("Native toolchain version check failed: "+path_utf8(tool));
    std::ifstream file(capture);std::string text((std::istreambuf_iterator<char>(file)),{});
    const auto position=text.find(expected);
    if(position==std::string::npos || (position+std::char_traits<char>::length(expected)<text.size() && std::isdigit(static_cast<unsigned char>(text[position+std::char_traits<char>::length(expected)]))))
        throw std::runtime_error("Native toolchain requires matching LLVM/Clang/LLD 23.1.4: "+path_utf8(tool));
}
}
std::filesystem::path native_clang_driver(const char* argv0) { return resources(argv0).driver; }
bool native_source_debug_supported() {
#if (defined(__linux__) && defined(__GLIBC__) && defined(__x86_64__)) || \
    (defined(_WIN32) && defined(_MSC_VER) && defined(_M_X64))
    return true;
#else
    return false;
#endif
}
void publish_object(const ValidatedAnalysis& analysis,const fs::path& requested,bool optimize,std::string_view profile,
                    std::optional<project::DebugInfo> source_debug) {
    auto output=output_path(analysis,requested);Staging stage(output);
    auto object=stage.directory/"program.o";
    emit_object(analysis.lower(),object,optimize,true,std::string(compiler_build_identity())+":"+std::string(profile),source_debug);
    output_path(analysis,output);publish_link_output(object,output);
}
namespace {
struct ProductionLink {
    const publish::PublishPlan& plan;
    const PublicationResources& assets;
    std::optional<diagnostics::PrivateMap> map;
    std::string symbols;
    std::map<std::string, std::string> linux_http_libraries;
};
constexpr auto linux_http_runpath = "$ORIGIN/native";
bool uses_linux_http_library(const audit::Report& report) {
    return std::ranges::any_of(publish::linux_http_libraries, [&](auto name) {
        return std::ranges::find(report.dependencies, name) != report.dependencies.end();
    });
}
bool linux_http_image(const audit::Report& report) {
    return uses_linux_http_library(report) && report.loader_paths == std::vector<std::string>{linux_http_runpath};
}
void require_glibc_floor(const audit::Report& report, std::string_view minimum) {
    llvm::VersionTuple floor;
    if (floor.tryParse(llvm::StringRef(minimum).drop_front(6)))
        throw std::runtime_error("Invalid Linux pack deployment floor.");
    for (const auto& requirement : report.versions) {
        const auto marker = requirement.find(":GLIBC_");
        if (marker == requirement.npos) continue;
        const auto version = llvm::StringRef(requirement).drop_front(marker + 7);
        llvm::VersionTuple required;
        const auto numeric = version == "ABI_DT_RELR" ? llvm::StringRef("2.36") : version;
        if (required.tryParse(numeric) || required > floor)
            throw std::runtime_error("Target pack output exceeds " + std::string(minimum) + ": " + requirement);
    }
}
std::string publishing_inputs(const ProductionLink& production, const ValidatedAnalysis& analysis,
                              std::string_view link_digest, const std::map<std::string, std::string>& payload) {
    project::Digest inputs;
    inputs.field(publish::policy_revision);
    inputs.field(compiler_build_identity());
    inputs.field(production.plan.target());
    inputs.field(production.plan.profile());
    inputs.field(production.plan.optimization());
    inputs.field(production.plan.resources());
    inputs.field(production.plan.symbols());
    inputs.field(production.plan.thin_lto() ? "thin" : "none");
    inputs.field(link_digest);
    const auto& locked = analysis.locked_dependencies();
    inputs.field(locked ? locked->identity() : "");
    inputs.field(analysis.entry_identity());
    inputs.field(analysis.application_metadata()->bytes());
    for (const auto& module : analysis.modules()) {
        inputs.field(module.identity);
        inputs.field(module.source.text());
    }
    for (const auto& [name, bytes] : production.assets) {
        inputs.field(name);
        inputs.field(bytes);
    }
    for (const auto& [name, bytes] : payload) {
        inputs.field(name);
        inputs.field(bytes);
    }
    return inputs.finish();
}

#ifdef VOID_NATIVE_PUBLISHING
void link_target_pack(const ValidatedAnalysis& analysis, const fs::path& requested, const char* argv0,
                      const project::ProjectDirectoryReader& reader, ProductionLink& production,
                      const publish::Pack& pack) {
    if (analysis.c_primitive_admission())
        throw std::runtime_error("Target pack publishing requires a destination-compatible native provider.");
    const auto output = output_path(analysis, requested);
#ifdef _WIN32
    const auto temporary_root = native_windows::temporary_root();
    exact_path(temporary_root);
    native_windows::Directory stage(temporary_root, true);
#else
    (void)reader;
    Staging stage(output);
#endif
    const auto& work = stage.directory;
    const auto read_link_output = [](const fs::path& path) {
#ifdef _WIN32
        // The native linker workspace may sit below a verified DOS short
        // name. Read the private artifact by handle, with writers excluded.
        const auto file = native_windows::read_file(path);
        const auto size = native_windows::size(file.value);
        if (!size || size > audit::byte_limit)
            throw std::runtime_error("Invalid target-pack linker output size.");
        std::string bytes(static_cast<std::size_t>(size), '\0');
        std::size_t offset = 0;
        while (offset != bytes.size()) {
            const auto got = native_windows::read(file.value, bytes.data() + offset,
                static_cast<DWORD>(bytes.size() - offset));
            if (!got) throw std::runtime_error("Short read from private target-pack linker output.");
            offset += got;
        }
        return bytes;
#else
        return project::read_project_input(path, audit::byte_limit);
#endif
    };
    const auto object = work / "program.o";
    const bool windows = pack.target->system == publish::System::windows_msvc;
    const auto executable = work / (windows ? "program.exe" : "program");
    const auto directory = pack_tools_directory(argv0);
    const auto tool = [&](std::string_view name) {
        auto path = directory / utf8_path(name);
#ifdef _WIN32
        path += ".exe";
#endif
        exact_path(path);
        if (!llvm::sys::fs::can_execute(path_utf8(path)))
            throw std::runtime_error("Missing LLVM target-pack linker tool: " + path_utf8(path));
        return path;
    };
    // Each installed SDK keeps one LLD binary under its native driver name.
    // The explicit flavor selects the destination without another LLD copy.
#if defined(_WIN32)
    const auto linker = tool("lld-link");
#elif defined(__APPLE__)
    const auto linker = tool("ld64.lld");
#else
    const auto linker = tool("ld.lld");
#endif
    version(linker, "LLD 23.1.4", work / "version.txt");
    for (const auto& [name, bytes] : pack.files) {
        if (!name.starts_with("link/")) continue;
        std::ofstream file(work / utf8_path(std::string_view(name).substr(5)), std::ios::binary);
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        file.close();
        if (!file) throw std::runtime_error("Cannot stage pack link input: " + name);
    }
    const bool arm = pack.target->architecture == publish::Architecture::arm64;
    std::vector<std::string> arguments{path_utf8(linker), "-flavor", windows ? "link" : "gnu"};
    const auto input = [&](const char* name) { arguments.push_back(path_utf8(work / name)); };
    if (windows) {
        arguments.insert(arguments.end(), {arm ? "/machine:arm64" : "/machine:x64", "/subsystem:console",
            "/entry:wmainCRTStartup", "/nodefaultlib", "/opt:ref", "/opt:noicf", "/incremental:no",
            "/brepro", "/nxcompat", "/dynamicbase", "/highentropyva", "/manifest:embed",
            "/out:" + path_utf8(executable), path_utf8(object)});
        if (production.plan.symbols() == "private") arguments.push_back("/debug:dwarf");
        for (auto name : publish::windows_archives) input(name);
        for (auto name : publish::windows_libraries) input(name);
        if (arm) input(publish::windows_arm64_synchronization);
    } else {
        const auto loader = publish::linux_loader(*pack.target);
        const auto interpreter = std::string(arm ? "/lib/" : "/lib64/") + loader;
        arguments.insert(arguments.end(), {"-pie", "--as-needed", "--gc-sections", "--no-undefined",
            "--eh-frame-hdr", "--build-id=sha1", "-z", "relro", "-z", "now", "-z", "noexecstack",
            "--dynamic-linker", interpreter, "-o", path_utf8(executable)});
        for (auto name : publish::linux_start) input(name);
        arguments.push_back(path_utf8(object));
        arguments.push_back("--start-group");
        for (auto name : publish::linux_archives) input(name);
        for (auto name : publish::linux_libraries) input(name);
        input(loader);
        arguments.push_back("--end-group");
        for (auto name : publish::linux_end) input(name);
    }

    const auto& plan = production.plan;
    const bool keep_symbols = plan.symbols() == "private";
    production.map = emit_publication_object(analysis.lower(ir::Emission::entry), object, plan.optimization() != "none",
        publishing_inputs(production, analysis, pack.digest, {}), plan.optimization() == "size", keep_symbols,
        plan.resources() == "embed" ? &production.assets : nullptr, false, pack.target->triple);
    const auto link = [&] {
        observation::Scope timing(observation::Phase::native_link_driver);
        if (execute(arguments) != 0) {
            timing.fail();
            throw std::runtime_error("Target pack link failed for " + pack.id + ". Review the linker diagnostics above.");
        }
    };
    link();
    if (windows) {
        auto image = read_link_output(executable);
        if (keep_symbols) production.symbols = image;
        if (execute({path_utf8(tool("llvm-objcopy")), "--strip-all", path_utf8(executable)}))
            throw std::runtime_error("Cannot strip target-pack PE image.");
        image = read_link_output(executable);
        audit::remove_pe_debug_records(image);
        std::ofstream public_image(executable, std::ios::binary | std::ios::trunc);
        public_image.write(image.data(), static_cast<std::streamsize>(image.size()));
        public_image.close();
        if (!public_image) throw std::runtime_error("Cannot finish the target-pack PE image.");
    } else {
        const auto objcopy = path_utf8(tool("llvm-objcopy"));
        const auto linked = read_link_output(executable);
        const auto imports = audit::inspect(linked, {2, std::string(pack.target->id), false, false, true, {}});
        if (uses_linux_http_library(imports)) {
            if (!pack.files.contains(std::string("source/") + publish::linux_http_sources.front()))
                throw std::runtime_error("This Linux pack lacks the pinned GNU HTTP libraries and their corresponding source. Rebuild the pack with the current generator.");
            arguments.insert(arguments.end(), {"--enable-new-dtags", "-rpath", linux_http_runpath});
            link();
            for (const auto name : publish::linux_http_libraries) {
                const auto library = work / name;
                if (execute({objcopy, "--strip-all", "--remove-section=.comment", path_utf8(library)}))
                    throw std::runtime_error("Cannot prepare the pinned GNU HTTP library for distribution.");
                production.linux_http_libraries.emplace(name, read_link_output(library));
            }
        }
        if (keep_symbols) {
            const auto debug = work / "program.debug";
            if (execute({objcopy, "--only-keep-debug", path_utf8(executable), path_utf8(debug)}))
                throw std::runtime_error("Cannot extract target-pack private debug symbols.");
            production.symbols = read_link_output(debug);
        }
        if (execute({objcopy, "--strip-all", "--remove-section=.comment", path_utf8(executable)}))
            throw std::runtime_error("Cannot strip target-pack executable.");
    }
#ifdef _WIN32
    const auto pins = native_windows::pin_directories(output.parent_path(), reader.reservation);
    native_windows::copy_checked(executable, output);
#else
    if (windows) fs::permissions(executable, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
        fs::perm_options::add);
    if (!llvm::sys::fs::can_execute(path_utf8(executable)))
        throw std::runtime_error("Target linker did not set executable file permissions.");
    publish_link_output(executable, output);
#endif
}
#endif

}
static std::string link_executable(const ValidatedAnalysis& analysis, const fs::path& requested, bool optimize,
                                   const char* argv0, std::string_view profile, bool package,
                                   const project::ProjectDirectoryReader& reader={},
                                   std::optional<project::DebugInfo> source_debug={}, ProductionLink* production=nullptr) {
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
    if (package && config.verified_digests.empty())
        throw std::runtime_error("Application packaging requires a verified v2 native resource inventory.");
#ifdef _WIN32
    const bool embedded_dwarf=source_debug && *source_debug!=project::DebugInfo::none;
    if (package || embedded_dwarf) {
        config.linker = config.driver.parent_path() / "lld-link.exe";
        if (!fs::is_regular_file(config.linker) || project::project_path_alias(config.linker))
            throw std::runtime_error(package?"Windows application packaging requires the configured Clang SDK's lld-link.exe.":
                "Windows source debugging requires the configured Clang SDK's lld-link.exe.");
        exact_path(config.linker);
    }
#else
    if (package) {
        // NativeBuild.cmake emits this group to locate the development IDN
        // libraries. Package executables use their OS-provided SONAMEs.
        const std::array<std::string,3> prefix{"-Xlinker", "-rpath", "-Xlinker"};
        auto found = std::search(config.flags.begin(), config.flags.end(), prefix.begin(), prefix.end());
        if (found != config.flags.end() && config.flags.end() - found >= 4) config.flags.erase(found, found + 4);
    }
#endif
    const auto& native = analysis.c_primitive_admission();
    const auto managed = native && native->dynamic() && !native->dynamic()->payload().empty();
    auto output=output_path(analysis,requested,managed);
#ifdef _WIN32
    std::optional<Staging> stage;
    if (!package) stage.emplace(output);
#else
    Staging stage(output);
#endif
    const auto payload = managed ? project::read_managed_payload(*native) : std::map<std::string, std::string>{};
    const auto destination = managed ? std::optional(reader.inspect(output.parent_path())) : std::nullopt;
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
#if defined(__linux__)
    if (production) args.insert(args.end(), {"-Xlinker", "--as-needed"});
#endif
#ifdef _WIN32
    if (package || embedded_dwarf) {
        args.push_back("-B"+path_utf8(config.linker.parent_path()));
        args.push_back("-fuse-ld=lld");
        if(embedded_dwarf)args.insert(args.end(),{"-Xlinker","/debug:dwarf","-Xlinker","/incremental:no",
            "-Xlinker","/opt:ref","-Xlinker","/opt:noicf","-Xlinker","/brepro"});
    } else
#endif
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
    if (production) {
#if defined(_WIN32)
        args.insert(args.end(), {"-Xlinker", "/opt:ref", "-Xlinker", "/opt:noicf",
            "-Xlinker", "/incremental:no", "-Xlinker", "/brepro"});
#elif defined(__APPLE__)
        args.insert(args.end(), {"-Xlinker", "-dead_strip", "-Xlinker", "-exported_symbol", "-Xlinker", "_main"});
#else
        args.insert(args.end(), {"-Xlinker", "--gc-sections"});
        if (production->plan.thin_lto()) {
            if (config.linker.empty())
                throw std::runtime_error("Publishing ThinLTO requires the configured LLVM 23.1.4 LLD linker.");
            args.insert(args.end(), {"-flto=thin", "-Xlinker", "--lto-O2", "-Xlinker", "--thinlto-jobs=1"});
        }
#endif
        production->map = emit_publication_object(analysis.lower(ir::Emission::entry), object, optimize,
            publishing_inputs(*production, analysis, config.verified_digests, payload), production->plan.optimization() == "size", production->plan.symbols() == "private",
            production->plan.resources() == "embed" ? &production->assets : nullptr, production->plan.thin_lto());
    } else {
        emit_object(analysis.lower(ir::Emission::entry),object,optimize,true,provenance,source_debug);
    }
    observation::Scope link_timing(observation::Phase::native_link_driver);
    auto status=execute(args);
    if(status!=0)link_timing.fail();
    link_timing.finish();
    if(status!=0)throw std::runtime_error("Native linker failed (status "+std::to_string(status)+
      "). Driver: "+path_utf8(config.driver)+"\nCheck the linker diagnostics above and host C/C++ development libraries. See docs/install/README.md.");
    if(!fs::is_regular_file(executable) || fs::file_size(executable)==0 || !llvm::sys::fs::can_execute(path_utf8(executable)))
        throw std::runtime_error("Native linker did not produce a usable executable.");
    if (production) {
        const bool keep_symbols = production->plan.symbols() == "private";
        const auto tool = [&](std::string_view leaf) {
            const auto path = config.driver.parent_path() / utf8_path(leaf);
            exact_path(path);
            if (!llvm::sys::fs::can_execute(path_utf8(path)))
                throw std::runtime_error("Missing LLVM publishing tool: " + path_utf8(path));
            return path_utf8(path);
        };
        const auto run = [&](const std::vector<std::string>& command) {
            if (execute(command) != 0)
                throw std::runtime_error("Publishing tool failed: " + command.front());
        };
#if defined(__APPLE__)
        if (keep_symbols) {
            const auto bundle = work / "program.dSYM";
            run({tool("dsymutil"), path_utf8(executable), "-o", path_utf8(bundle)});
            production->symbols = project::read_project_input(bundle / "Contents/Resources/DWARF/program", audit::byte_limit);
        }
        // Apple's -x can retain a synthetic local nlist entry for the
        // indirect symbol table. -N removes nlists from dyld executables
        // while preserving dyld exports and fixups.
        run({"/usr/bin/strip", "-S", "-x", "-N", path_utf8(executable)});
        // Apple Silicon enforces a local code signature after image mutation.
        run({"/usr/bin/codesign", "--force", "--sign", "-", path_utf8(executable)});
#elif defined(_WIN32)
        // LLVM supports stripping embedded DWARF from PE. Retain the complete
        // linked image privately so a debugger can use its original RVAs.
        if (keep_symbols) production->symbols = project::read_project_input(executable, audit::byte_limit);
        run({tool("llvm-objcopy.exe"), "--strip-all", path_utf8(executable)});
        auto public_image = project::read_project_input(executable, audit::byte_limit);
        audit::remove_pe_debug_records(public_image);
        std::ofstream stripped(executable, std::ios::binary | std::ios::trunc);
        stripped.write(public_image.data(), static_cast<std::streamsize>(public_image.size()));
        stripped.close();
        if (!stripped) throw std::runtime_error("Cannot finish the stripped PE executable.");
#else
        if (keep_symbols) {
            const auto debug = work / "program.debug";
            run({tool("llvm-objcopy"), "--only-keep-debug", path_utf8(executable), path_utf8(debug)});
            production->symbols = project::read_project_input(debug, audit::byte_limit);
        }
        run({tool("llvm-objcopy"), "--strip-all", "--remove-section=.comment", path_utf8(executable)});
#endif
    }
    if (managed) {
        std::map<std::string, std::string_view> files(payload.begin(), payload.end());
        project::publish_project_directory(*destination, managed_payload_directory, files,
                                           [&](const fs::path& directory, const project::ProjectDirectoryReader& reader) {
            if (reader.inspect(directory).path != directory)
                throw std::runtime_error("Managed native output directory spelling changed.");
            std::size_t count = 0;
            for (const auto& entry : fs::directory_iterator(directory)) {
                const auto file = payload.find(path_utf8(entry.path().filename()));
                if (file == payload.end() || reader.read(entry.path(), file->second.size()) != file->second)
                    throw std::runtime_error("Managed native output has a different payload; select a fresh output directory.");
                ++count;
            }
            if (count != payload.size()) throw std::runtime_error("Managed native output payload is incomplete.");
        }, {}, reader);
    }
#ifdef _WIN32
    // Check PE structure through LLVM, not merely child status or nonempty bytes.
    // Keep the original pinned, with writes/deletion denied, through transfer.
    auto pinned=native_windows::verified_executable(executable);
    if (package) {
        // The package owner reserves this parent. Copy exclusively into its
        // private stage and let the outer directory rename publish the image.
        const auto pins=native_windows::pin_directories(output.parent_path(),reader.reservation);
        output_path(analysis,output);
        native_windows::copy_checked(executable,output,true);
    } else {
        const auto staged=stage->directory/"program.exe";
        native_windows::copy_checked(executable,staged,true);
        output_path(analysis,output);publish_link_output(staged,output);
    }
#else
    output_path(analysis,output);publish_link_output(executable,output);
#endif
    return std::move(config.verified_digests);
}

void build_executable(const ValidatedAnalysis& analysis, const fs::path& requested, bool optimize,
                      const char* argv0, std::string_view profile,std::optional<project::DebugInfo> source_debug) {
    link_executable(analysis, requested, optimize, argv0, profile, false,{},source_debug);
}

#ifdef VOID_NATIVE_PUBLISHING
namespace {
#ifdef _WIN32
constexpr std::string_view package_header = "VOID application package 1\ntarget x86_64-pc-windows-msvc\n";
constexpr auto executable_name = "app.exe";
#elif defined(__APPLE__)
constexpr std::string_view package_header = "VOID application package 1\ntarget arm64-apple-macos\n";
constexpr auto executable_name = "app";
#else
constexpr std::string_view package_header = "VOID application package 1\ntarget x86_64-unknown-linux-gnu\n";
constexpr auto executable_name = "app";
#endif
constexpr auto inventory_name = "void-package.txt";
constexpr auto metadata_name = "application.meta";
constexpr std::size_t package_byte_limit = 256 * 1024 * 1024;
constexpr std::size_t inventory_byte_limit = 64 * 1024;
constexpr std::size_t audit_report_byte_limit = 16 * 1024 * 1024;

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
#ifdef _WIN32
    const auto device=project::fold_project_name(name.substr(0,name.find('.')));
    if (device=="conin$" || device=="conout$" ||
        ((device.starts_with("com") || device.starts_with("lpt")) &&
         (device.substr(3)=="¹" || device.substr(3)=="²" || device.substr(3)=="³"))) return false;
#endif
    std::string portable(name);
    for (char& byte : portable) if (static_cast<unsigned char>(byte) >= 128) byte = 'x';
    return portable_path_component(portable);
}

#ifdef _WIN32
void require_package_path_spelling(const fs::path& path) {
    // Windows absolute-path resolution can erase trailing spaces and dots
    // before physical admission gets a chance to reject the alias spelling.
    for (const auto& part : path.relative_path()) {
        const auto& text = part.native();
        if (part != L"." && part != L".." && !text.empty() &&
            (text.back() == L' ' || text.back() == L'.'))
            throw std::runtime_error("Application package path must use portable directory spelling: " + path_utf8(path));
    }
}
#endif

void write_package_file(const fs::path& path, std::string_view bytes, const project::ProjectDirectoryReader& reader) {
    fs::create_directory(path.parent_path());
#ifdef _WIN32
    reader.inspect(path.parent_path());
    const auto pins=native_windows::pin_directories(path.parent_path(),reader.reservation);
    native_windows::Handle file(CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,
        FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    if(file.value==INVALID_HANDLE_VALUE)native_windows::fail("Cannot create application package file");
    DWORD written=0;
    if(!WriteFile(file.value,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr) ||
       written!=bytes.size() || !FlushFileBuffers(file.value))
        native_windows::fail("Cannot finish application package file");
#else
    (void)reader;
    std::ofstream file(path, std::ios::binary);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    file.close();
    if (!file) throw std::runtime_error("Cannot write application package file: " + path_utf8(path));
#endif
}

void copy_notices(const fs::path& stage, const char* argv0, const project::ProjectDirectoryReader& reader) {
#ifdef _WIN32
    static constexpr std::pair<std::string_view,std::string_view> required[] = {
#include "void-package-notices.inc"
    };
#endif
    const auto compiler = utf8_path(llvm::sys::fs::getMainExecutable(argv0, reinterpret_cast<void*>(&resources)));
    const auto path = compiler.parent_path() / "void-package-notices.txt";
    std::istringstream list(project::read_project_input(path, inventory_byte_limit));
    std::string line;
    if (!std::getline(list, line) || (line != "VOID package notices 1" && line != "VOID package notices 1\r"))
        throw std::runtime_error("Invalid application package notice inventory: " + path_utf8(path));
    std::set<std::string> notices;
    while (std::getline(list, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto tab = line.find('\t', 65);
        if (line.size() < 68 || line[64] != '\t' || tab == line.npos || !hex_digest(std::string_view(line).substr(0, 64)))
            throw std::runtime_error("Invalid application package notice record.");
        const auto name = line.substr(65, tab - 65);
        if (!package_leaf(name) || notices.size() == 64 || !notices.insert(project::fold_project_name(name)).second)
            throw std::runtime_error("Invalid or duplicate application package notice: " + name);
        const auto source = (path.parent_path() / utf8_path(line.substr(tab + 1))).lexically_normal();
        const auto bytes = project::read_project_input(source, 1024 * 1024);
        const auto digest = std::string_view(line).substr(0,64);
        if (bytes.empty() || package_digest(bytes) != digest)
            throw std::runtime_error("Application package notice checksum mismatch: " + name);
#ifdef _WIN32
        if (std::find(std::begin(required),std::end(required),std::pair<std::string_view,std::string_view>{name,digest})
            == std::end(required))
            throw std::runtime_error("Unexpected application package notice identity: " + name);
#endif
        write_package_file(stage / "licenses" / utf8_path(name), bytes, reader);
    }
#ifdef _WIN32
    if (notices.size() != std::size(required)) throw std::runtime_error("Missing required Windows application package notices.");
#endif
    if (!notices.contains("void-mit-license.txt")) throw std::runtime_error("Missing VOID application package license.");
}

bool production_image(const audit::Report& report) {
    return report.inspected && (report.image_kind == "executable" ||
        (report.format == "ELF" && report.image_kind == "shared-or-pie" && !report.interpreter.empty()));
}

void verify_linux_http_payload(const fs::path& root, const publish::Target& target,
                               const project::ProjectDirectoryReader& reader, std::string_view minimum = {}) {
    const auto directory = reader.inspect(root / "native").path;
    std::set<std::string> expected(publish::linux_http_libraries.begin(), publish::linux_http_libraries.end());
    for (const auto& entry : fs::directory_iterator(directory)) {
        const auto name = path_utf8(entry.path().filename());
        if (!expected.erase(name)) throw std::runtime_error("Unexpected Linux HTTP library: " + name);
        const auto bytes = reader.read(entry.path(), audit::byte_limit);
        const auto report = audit::inspect(bytes, {2, std::string(target.id), true, true, false, {}});
        const bool idn = name == publish::linux_http_libraries.front();
        const bool valid_paths = idn ? report.loader_paths == std::vector<std::string>{"$ORIGIN"} : report.loader_paths.empty();
        if (!report.inspected || report.image_kind != "shared-or-pie" || !report.interpreter.empty() ||
            !report.findings.empty() || !valid_paths)
            throw std::runtime_error("Invalid relocatable Linux HTTP library: " + name);
        if (!minimum.empty()) require_glibc_floor(report, minimum);
        for (const auto& required : report.dependencies)
            if (required != "libc.so.6" && required != "libm.so.6" && required != "libgcc_s.so.1" &&
                required != publish::linux_loader(target) && !(idn && required == publish::linux_http_libraries.back()))
                throw std::runtime_error("Unexpected transitive Linux HTTP library: " + required);
    }
    if (!expected.empty()) throw std::runtime_error("Incomplete Linux HTTP library closure.");
    for (const auto name : publish::linux_http_sources)
        if (reader.read(root / "licenses" / name, 8 * 1024 * 1024).empty())
            throw std::runtime_error("Missing corresponding source for a Linux HTTP library.");
    for (const auto name : {"GNU-HTTP-LIBRARIES.txt", "libidn2-2.3.8-COPYING", "libidn2-2.3.8-COPYING.LESSERv3",
                           "libunistring-1.5-COPYING", "libunistring-1.5-COPYING.LIB"})
        if (reader.read(root / "licenses" / name, 1024 * 1024).empty())
            throw std::runtime_error("Missing Linux HTTP library notice.");
}

void check_package_image(std::string_view bytes) {
#ifdef _WIN32
    native_windows::check_executable({llvm::StringRef(bytes),executable_name},true);
#elif defined(__APPLE__)
    const auto report = audit::inspect(bytes, {2, "macos-arm64", false, false, true, {}});
    if (!report.inspected || report.image_kind != "executable" || !report.findings.empty())
        throw std::runtime_error("Application distribution requires a native Mach-O ARM64 executable.");

#else
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
#endif
}

std::string package_inventory(const fs::path& root, bool prepare, const project::ProjectDirectoryReader& reader, std::string_view production_target = {},
                              const std::string& bundle = {}) {
    const bool production = !production_target.empty();
    const auto executable = production_target.starts_with("windows-") ? "app.exe" : production ? "app" : executable_name;
    std::map<std::string, std::string> rows;
    std::size_t remaining = package_byte_limit;
    auto file = [&](const fs::directory_entry& entry, const std::string& name) {
        if (rows.size() == 128 || !fs::is_regular_file(entry.symlink_status()))
            throw std::runtime_error("Invalid application package file: " + name);
        const auto bytes = reader.read(entry.path(), remaining);
        remaining -= bytes.size();
#ifndef _WIN32
        if (name == executable && !llvm::sys::fs::can_execute(path_utf8(entry.path())))
            throw std::runtime_error("Application package executable is not executable: " + path_utf8(entry.path()));
#endif
#ifdef _WIN32
        // Preserve the native AMD64 PE entry-point and section checks. The
        // portable audit below separately applies the receipt's destination.
        if (name == executable && (!production || production_target == "windows-x64"))
            check_package_image(bytes);
#else
        if (name == executable && !production) check_package_image(bytes);
#endif
        rows.emplace(name, package_digest(bytes) + '\t' + std::to_string(bytes.size()) + '\t' + name + '\n');
    };
    auto directory = [&](const fs::path& path) {
        if (!fs::is_directory(fs::symlink_status(path)))
            throw std::runtime_error("Invalid macOS bundle directory: " + path_utf8(path));
        reader.inspect(path);
    };
    auto exact_contents = [&](const fs::path& path, std::set<std::string> allowed) {
        directory(path);
        for (const auto& entry : fs::directory_iterator(path))
            if (!allowed.erase(path_utf8(entry.path().filename())))
                throw std::runtime_error("Unexpected macOS bundle entry: " + path_utf8(entry.path()));
        if (!allowed.empty()) throw std::runtime_error("Incomplete macOS application bundle.");
    };
    auto flat_files = [&](const fs::path& path, const std::string& name) {
        directory(path);
        std::set<std::string> names;
        for (const auto& child : fs::directory_iterator(path)) {
            const auto leaf = path_utf8(child.path().filename());
            if (!package_leaf(leaf) || !names.insert(project::fold_project_name(leaf)).second)
                throw std::runtime_error("Invalid or duplicate bundle resource path: " + name + '/' + leaf);
            file(child, name + '/' + leaf);
        }
        if (names.empty()) throw std::runtime_error("Unexpected empty bundle asset directory.");
    };
    const auto executable_path = bundle.empty() ? std::string(executable) : bundle + "/Contents/MacOS/app";
    if (!bundle.empty()) {
        const auto contents = root / utf8_path(bundle) / "Contents";
        exact_contents(contents.parent_path(), {"Contents"});
        std::set<std::string> contents_names{"Info.plist", "MacOS", "Resources"};
        const auto signature = contents / "_CodeSignature";
        if (fs::exists(fs::symlink_status(signature))) {
            contents_names.insert("_CodeSignature");
            exact_contents(signature, {"CodeResources"});
            file(fs::directory_entry(signature / "CodeResources"), bundle + "/Contents/_CodeSignature/CodeResources");
        }
        exact_contents(contents, std::move(contents_names));
        exact_contents(contents / "MacOS", {"app"});
        const auto resources = contents / "Resources";
        directory(resources);
        file(fs::directory_entry(contents / "Info.plist"), bundle + "/Contents/Info.plist");
        file(fs::directory_entry(contents / "MacOS/app"), executable_path);
        for (const auto& entry : fs::directory_iterator(resources)) {
            const auto name = path_utf8(entry.path().filename());
            if (name == "AppIcon.icns") file(entry, bundle + "/Contents/Resources/" + name);
            else if (name == "assets") flat_files(entry.path(), bundle + "/Contents/Resources/assets");
            else throw std::runtime_error("Unexpected macOS bundle resource: " + name);
        }
#ifndef _WIN32
        if (!llvm::sys::fs::can_execute(path_utf8(contents / "MacOS/app")))
            throw std::runtime_error("macOS bundle executable is missing execute permissions.");
#endif
    }
    bool has_inventory = false;
    for (const auto& entry : fs::directory_iterator(root)) {
        const auto name = path_utf8(entry.path().filename());
        if (name == inventory_name && !prepare) { has_inventory = true; continue; }
        if (!bundle.empty() && name == bundle) continue;
        if ((bundle.empty() && name == executable) || name == metadata_name || (production && name == "binary-audit.json")) file(entry, name);
        else if (((bundle.empty() && name == "assets") || name == "native" || name == "licenses") && fs::is_directory(entry.symlink_status())) {
#ifdef _WIN32
            reader.inspect(entry.path());
            const auto pins=native_windows::pin_directories(entry.path(),reader.reservation);
            std::vector<fs::path> physical_names;
#endif
            std::set<std::string> names;
            for (const auto& child : fs::directory_iterator(entry)) {
                const auto leaf = path_utf8(child.path().filename());
                if (!package_leaf(leaf) || !names.insert(project::fold_project_name(leaf)).second)
                    throw std::runtime_error("Invalid or duplicate application package path: " + name + '/' + leaf);
#ifdef _WIN32
                auto physical=child.path().filename();
                if (std::any_of(physical_names.begin(),physical_names.end(),[&](const auto& prior) {
                        return native_windows::same_filename(prior,physical);
                    }))
                    throw std::runtime_error("Duplicate Windows application package path: " + name + '/' + leaf);
                physical_names.push_back(std::move(physical));
#endif
                file(child, name + '/' + leaf);
            }
            if (names.empty()) throw std::runtime_error("Unexpected empty application package directory: " + name);
        } else throw std::runtime_error("Unexpected application package entry: " + name);
    }
    if (!prepare && !has_inventory) throw std::runtime_error("Missing exact application package inventory filename.");
    if (!rows.contains(executable_path) || !rows.contains(metadata_name) || !rows.contains("licenses/VOID-MIT-LICENSE.txt"))
        throw std::runtime_error("Application package is missing its executable, metadata or license.");
    std::string result;
    for (const auto& [name, row] : rows) result += row;
    return result;
}

ApplicationMetadata::Owner verify_package(const fs::path& directory, const project::ProjectDirectoryReader& reader={}) {
#ifdef _WIN32
    require_package_path_spelling(directory);
#endif
    const auto root = reader.inspect(directory).path;
#ifdef _WIN32
    const auto pins=native_windows::pin_directories(root,reader.reservation);
#endif
    const auto path = root / inventory_name;
    const auto text = reader.read(path, inventory_byte_limit);
    if (!text.starts_with(package_header))
        throw std::runtime_error("Invalid application package inventory: " + path_utf8(path));
    std::istringstream receipt(text);
    std::string magic, target, profile, lock, inputs;
    if (!std::getline(receipt, magic) || !std::getline(receipt, target) || !std::getline(receipt, profile) ||
        !std::getline(receipt, lock) || !std::getline(receipt, inputs) ||
        (!profile.starts_with("profile project-profile-v1:") &&
         !profile.starts_with("profile project-profile-v2:")) || !lock.starts_with("lock ") ||
        !inputs.starts_with("inputs ") || !hex_digest(std::string_view(inputs).substr(7)))
        throw std::runtime_error("Invalid application package inventory header.");
    lock.erase(0, 5);
    const std::string recorded{std::istreambuf_iterator<char>(receipt), {}};
    if ((lock != "-" && !hex_digest(lock)) || recorded != package_inventory(root, false, reader))
        throw std::runtime_error("Application package inventory mismatch. A packaged file is missing, changed or unexpected.");
    return ApplicationMetadata::decode(reader.read(root / metadata_name, ApplicationMetadata::byte_limit));
}
}
#endif

void build_application_package(const ProjectAnalysis& project_analysis, const fs::path& requested, bool optimize,
                               const char* argv0, std::string_view profile, bool replace) {
    observation::Scope timing(observation::Phase::package_assembly);
#ifdef VOID_APPLICATION_PACKAGING
    if (!project_analysis.program()) throw std::runtime_error("Application packaging requires a checked application entry.");
    const auto& analysis = *project_analysis.program();
    const auto& metadata = analysis.application_metadata();
    if (!metadata || !analysis.application()) throw std::runtime_error("Application packaging requires a manifest-selected application entry.");
    if (!project_analysis.manifest()->native().empty()) throw std::runtime_error("Application packaging requires a managed catalog for native dependencies.");
#ifdef _WIN32
    require_package_path_spelling(requested);
#endif
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
    project::publish_application_directory(parent, leaf, [&](const fs::path& stage, const project::ProjectDirectoryReader& reader) {
        copy_notices(stage, argv0, reader);
        const auto license = project_analysis.root() / "LICENSE";
        if (fs::exists(fs::symlink_status(license)))
            write_package_file(stage / "licenses/APPLICATION-LICENSE.txt", project::read_project_input(license, 1024 * 1024), reader);
        std::size_t remaining = project::application_resource_byte_limit;
        for (const auto& resource : project_analysis.manifest()->resources()) {
            const auto path = utf8_path(resource);
            const auto bytes = project::read_project_input(project_analysis.root() / path, remaining);
            remaining -= bytes.size();
            write_package_file(stage / path, bytes, reader);
        }
        const auto link_digests = link_executable(analysis, stage / executable_name, optimize, argv0, profile, true, reader);
        write_package_file(stage / metadata_name, metadata->bytes(), reader);
        const auto& lock = analysis.locked_dependencies();
        const auto files = package_inventory(stage, true, reader);
        std::string inputs = std::string(compiler_build_identity()) + '\n' + std::string(profile) + '\n' +
            link_digests + '\n';
        for (const auto& module : analysis.modules()) {
            inputs += std::to_string(module.identity.size()) + ':' + module.identity;
            inputs += std::to_string(module.source.text().size()) + ':' + module.source.text();
        }
        std::istringstream records(files);
        std::string record;
        while (std::getline(records, record))
            if (std::string_view(record).substr(record.rfind('\t')+1) != executable_name) inputs += record + '\n';
        const auto inventory = std::string(package_header) + "profile " + std::string(profile) +
            "\nlock " + (lock ? lock->identity() : "-") + "\ninputs " + package_digest(inputs) + '\n' + files;
        write_package_file(stage / inventory_name, inventory, reader);
        (void)verify_package(stage, reader);
    }, verify_replacement);
#else
    (void)project_analysis; (void)requested; (void)optimize; (void)argv0; (void)profile; (void)replace;
    throw std::runtime_error("Application packaging requires Linux x86-64 GNU or Windows x64 MSVC.");
#endif
}

fs::path publish_native_application(const ProjectAnalysis& project_analysis, const publish::PublishPlan& plan, const char* argv0, const publish::Pack* pack) {
#ifdef VOID_NATIVE_PUBLISHING
    const auto target = pack ? pack->target->id : std::string_view(plan.target());
    const auto executable = target.starts_with("windows-") ? "app.exe" : "app";
    const auto& application = *project_analysis.manifest()->application();
    const auto bundle = plan.app_bundle() ? application.id + ".app" : std::string{};
    if (!bundle.empty() && !package_leaf(bundle))
        throw std::runtime_error("macOS bundle requires a portable application identifier.");
    std::optional<std::string> icon;
    const auto& analysis = *project_analysis.program();
    const auto& root = project_analysis.root();
    const auto& requested = plan.output();
    if (plan.icon()) {
        icon = project::read_project_input(root / utf8_path(*plan.icon()), 8 * 1024 * 1024);
        if (icon->size() < 8 || !icon->starts_with("icns"))
            throw std::runtime_error("Expected a macOS ICNS application icon.");
    }
    project::require_outside_user_package_store(requested);
    project::require_outside_native_approval_store(requested, true);
#ifdef _WIN32
    require_package_path_spelling(requested);
#endif
    const auto output = fs::absolute(requested).lexically_normal();
    const auto leaf = path_utf8(output.filename());
    if (!package_leaf(leaf) || leaf.starts_with('.') || project::fold_project_name(leaf).ends_with(".void"))
        throw std::runtime_error("Publishing requires a portable output directory name.");
    const auto relative = output.lexically_relative(root);
    if (!relative.empty()) {
        const auto first = project::fold_project_name(path_utf8(*relative.begin()));
        if (first == ".void" || first == ".git" || first == "libs" || first == "native" || first == "assets")
            throw std::runtime_error("Publish output conflicts with project storage: " + path_utf8(output));
    }
    if (fs::exists(fs::symlink_status(output)))
        throw std::runtime_error("Publish output must be a fresh directory: " + path_utf8(output));

    PublicationResources assets;
    std::size_t remaining = project::application_resource_byte_limit;
    for (const auto& name : project_analysis.manifest()->resources()) {
        auto bytes = project::read_project_input(root / utf8_path(name), remaining);
        remaining -= bytes.size();
        assets.emplace(name, std::move(bytes));
    }
    auto existing = output.parent_path();
    while (!fs::exists(fs::symlink_status(existing))) existing = existing.parent_path();
    const auto admitted = project::inspect_creation_root(existing);
    const auto physical_parent = admitted.path / output.parent_path().lexically_relative(existing);
    fs::create_directories(physical_parent);
    const auto parent = project::inspect_creation_root(physical_parent);
    const auto actual_output = parent.path / output.filename();
    fs::path archive;
    project::publish_application_directory(parent, leaf, [&](const fs::path& stage, const project::ProjectDirectoryReader& reader) {
        ProductionLink production{plan, assets, {}, {}, {}};
        auto image_path = stage / executable;
        auto asset_root = stage;
        if (!bundle.empty()) {
            const auto contents = stage / utf8_path(bundle) / "Contents";
            fs::create_directories(contents / "MacOS");
            asset_root = contents / "Resources";
            fs::create_directory(asset_root);
            image_path = contents / "MacOS/app";
            write_package_file(contents / "Info.plist", publish::bundle_plist(application, icon.has_value()), reader);
            if (icon) write_package_file(asset_root / "AppIcon.icns", *icon, reader);
        }
        if (pack) {
            for (const auto& [name, bytes] : pack->files)
                if (name.starts_with("licenses/")) write_package_file(stage / utf8_path(name), bytes, reader);
        } else copy_notices(stage, argv0, reader);
        const auto license = root / "LICENSE";
        if (fs::exists(fs::symlink_status(license)))
            write_package_file(stage / "licenses/APPLICATION-LICENSE.txt", project::read_project_input(license, 1024 * 1024), reader);
        if (plan.resources() == "files")
            for (const auto& [name, bytes] : assets)
                write_package_file(asset_root / utf8_path(name), bytes, reader);

        const auto debug = plan.symbols() == "private" ? project::DebugInfo::full : project::DebugInfo::none;
        if (pack) link_target_pack(analysis, image_path, argv0, reader, production, *pack);
        else link_executable(analysis, image_path, plan.optimization() != "none", argv0, plan.profile(), true, reader, debug, &production);
#ifdef __APPLE__
        if (!bundle.empty() && (execute({"/usr/bin/codesign", "--force", "--sign", "-", path_utf8(stage / utf8_path(bundle))}) ||
            execute({"/usr/bin/codesign", "--verify", "--strict", path_utf8(stage / utf8_path(bundle))})))
            throw std::runtime_error("Cannot ad hoc sign and verify the macOS application bundle.");
#endif
        const bool linux_http = !production.linux_http_libraries.empty();
        if (linux_http) {
            for (const auto& [name, bytes] : production.linux_http_libraries)
                write_package_file(stage / "native" / utf8_path(name), bytes, reader);
            for (const auto name : publish::linux_http_sources)
                write_package_file(stage / "licenses" / name, pack->files.at(std::string("source/") + name), reader);
            write_package_file(stage / "licenses/GNU-HTTP-LIBRARIES.txt",
                "This application uses the GNU libidn2 and libunistring shared libraries.\n"
                "The libraries are distributed under the included GNU license texts.\n"
                "This directory includes their corresponding source archives.\n"
                "Build the shared libunistring first with --enable-shared --disable-static.\n"
                "Build libidn2 with the same flags, --disable-nls --disable-doc --disable-rpath\n"
                "and --with-libunistring-prefix pointing to the installed libunistring.\n"
                "The VOID build uses Clang 23.1.4, -O2 -fPIC and a sibling $ORIGIN RUNPATH for libidn2.\n"
                "Replace the libraries in native/ with compatible builds under the same names.\n", reader);
            verify_linux_http_payload(stage, *pack->target, reader, pack->minimum);
        }
        const auto image = reader.read(image_path, audit::byte_limit);
        audit::Policy policy{2, std::string(target), true, true, !linux_http, {}};
        auto forbid = [&](const fs::path& path) {
            auto text = path_utf8(path);
            if (text.size() > 3 && text.size() <= 256 &&
                std::find(policy.forbidden_strings.begin(), policy.forbidden_strings.end(), text) == policy.forbidden_strings.end())
                policy.forbidden_strings.push_back(std::move(text));
        };
        forbid(root);
        forbid(parent.path);
        forbid(standard_library_root());
        const auto report = audit::inspect(image, policy);
        if (pack && pack->target->system == publish::System::linux_gnu && uses_linux_http_library(report) && !linux_http)
            throw std::runtime_error("Target pack output is missing its required GNU HTTP libraries.");
        if (!production_image(report) || !report.findings.empty() || (linux_http && !linux_http_image(report))) {
            std::string details;
            for (const auto& finding : report.findings) details += "\n" + finding.code + ": " + finding.detail;
            throw std::runtime_error("Final executable failed binary auditing." + details);
        }
        for (const auto& name : report.dependencies) {
            const bool system = target.starts_with("macos-") ?
                name.starts_with("/usr/lib/") || name.starts_with("/System/Library/Frameworks/") :
                !name.empty() && name.find_first_of("/\\:") == name.npos;
            if (!system) throw std::runtime_error("Non-relocatable executable dependency: " + name);
        }
        if (pack && pack->target->system == publish::System::linux_gnu) {
            const auto interpreter = std::string(pack->target->architecture == publish::Architecture::arm64 ? "/lib/" : "/lib64/") +
                publish::linux_loader(*pack->target);
            if (report.interpreter != interpreter)
                throw std::runtime_error("Final ELF interpreter differs from the declared target ABI.");
            require_glibc_floor(report, pack->minimum);
        }
        production.map->bind_image(image);
        const auto report_text = audit::json(report) + '\n';
        if (report_text.size() > audit_report_byte_limit)
            throw std::runtime_error("Binary audit report exceeds the publication size limit.");
        write_package_file(stage / "binary-audit.json", report_text, reader);
        write_package_file(stage / metadata_name, analysis.application_metadata()->bytes(), reader);
        const auto files = package_inventory(stage, true, reader, target, bundle);
        const auto magic = linux_http ? "VOID native publication 3" :
            bundle.empty() ? "VOID native publication 1" : "VOID native publication 2";
        const auto inventory = std::string(magic) + "\ntarget " + std::string(target) +
            "\nprofile " + plan.profile() + "\nbuild " + production.map->build_id() + "\n" +
            (bundle.empty() ? std::string{} : "bundle " + bundle + "\n") + files;
        write_package_file(stage / inventory_name, inventory, reader);
        if (files != package_inventory(stage, false, reader, target, bundle))
            throw std::runtime_error("Publication file inventory changed before delivery.");

        const auto private_root = root / ".void" / "publish-diagnostics";
        const auto archive_name = production.map->image_sha256();
        const auto map = production.map->encode();
        const auto symbol_file = target.starts_with("macos-") ? "app.dwarf" : target.starts_with("windows-") ?
            "app.unstripped.exe" : "app.debug";
        std::map<std::string, std::string_view> private_files{{"diagnostics.vdiag", map}};
        if (!production.symbols.empty()) private_files.emplace(symbol_file, production.symbols);
        project::with_user_package_store(private_root, true, [&](const project::CreationRoot& destination) {
            project::publish_project_directory(destination, archive_name, private_files,
                [&](const fs::path& candidate, const project::ProjectDirectoryReader& private_reader) {
                    for (const auto& [name, bytes] : private_files)
                        if (private_reader.read(candidate / utf8_path(name), bytes.size()) != bytes)
                            throw std::runtime_error("Private diagnostic archive differs for the same image.");
                }, {}, {}, true);
            archive = destination.path / utf8_path(archive_name);
        }, true);
    }, {});
    std::cout << "Published application: " << path_utf8(actual_output) << '\n';
    return archive;
#else
    (void)project_analysis; (void)plan; (void)argv0; (void)pack;
    throw std::runtime_error("Publishing requires Linux x64 GNU, Windows x64 MSVC or macOS ARM64.");
#endif
}

#ifdef VOID_NATIVE_PUBLISHING
namespace {
struct VerifiedPublication {
    fs::path root;
    std::string platform, bundle, files;
    ApplicationMetadata::Owner metadata;
};
VerifiedPublication verify_publication(const fs::path& directory) {
#ifdef _WIN32
    require_package_path_spelling(directory);
#endif
    const auto root = project::inspect_creation_root(directory).path;
    const auto receipt = project::read_project_input(root / inventory_name, inventory_byte_limit);
    std::istringstream rows(receipt);
    std::string magic, target, profile, build;
    if (!std::getline(rows, magic) || !std::getline(rows, target) || !std::getline(rows, profile) ||
        !std::getline(rows, build) || (magic != "VOID native publication 1" && magic != "VOID native publication 2" &&
        magic != "VOID native publication 3") ||
        !target.starts_with("target ") || !publish::find_target(std::string_view(target).substr(7)) ||
        (profile != "profile release" && profile != "profile performance") ||
        !build.starts_with("build ") || !hex_digest(std::string_view(build).substr(6)))
        throw std::runtime_error("Invalid native publication inventory.");
    const auto platform = std::string_view(target).substr(7);
    const bool linux_http = magic == "VOID native publication 3";
    if (linux_http && !platform.starts_with("linux-"))
        throw std::runtime_error("GNU HTTP library closure requires a Linux publication.");
    const auto metadata = ApplicationMetadata::decode(project::read_project_input(root / metadata_name, ApplicationMetadata::byte_limit));
    std::string bundle;
    if (magic == "VOID native publication 2") {
        bundle = std::string(metadata->application_id()) + ".app";
        std::string claim;
        if (!platform.starts_with("macos-") || !package_leaf(bundle) || !std::getline(rows, claim) || claim != "bundle " + bundle)
            throw std::runtime_error("Invalid macOS bundle identity in publication inventory.");
    }
    const std::string recorded{std::istreambuf_iterator<char>(rows), {}};
    if (recorded != package_inventory(root, false, {}, platform, bundle))
        throw std::runtime_error("Native publication inventory mismatch. A file is missing, changed or unexpected.");
    if (linux_http) verify_linux_http_payload(root, *publish::find_target(platform), {});
    const auto executable = bundle.empty() ? (platform.starts_with("windows-") ? "app.exe" : "app") : bundle + "/Contents/MacOS/app";
    const auto image = project::read_project_input(root / utf8_path(executable), audit::byte_limit);
    const auto report = audit::inspect(image, {2, std::string(platform), true, true, !linux_http, {}});
    if (!production_image(report) || !report.findings.empty() || (linux_http && !linux_http_image(report)) ||
        project::read_project_input(root / "binary-audit.json", audit_report_byte_limit) != audit::json(report) + '\n')
        throw std::runtime_error("Native publication binary audit mismatch.");
    return {root, std::string(platform), std::move(bundle), recorded, metadata};
}
}
#endif

void verify_native_publication(const fs::path& directory) {
#ifdef VOID_NATIVE_PUBLISHING
    (void)verify_publication(directory);
#else
    (void)directory;
    throw std::runtime_error("Native publication verification is unavailable on this host.");
#endif
}

void archive_macos_application(const fs::path& directory, const fs::path& output) {
#ifdef VOID_NATIVE_PUBLISHING
    const auto publication = verify_publication(directory);
    if (!publication.platform.starts_with("macos-") || output.extension() != ".zip")
        throw std::invalid_argument("Bundle ZIP requires an existing macOS publication and a new .zip output.");
    const auto fields = publication.metadata->fields();
    const project::Identity application{std::string(fields[3]), std::string(fields[4]), *project::parse_version(fields[5])};
    const auto bundle = application.id + ".app";
    if (!package_leaf(bundle)) throw std::invalid_argument("Bundle ZIP needs a portable application identifier.");
    const auto contents = bundle + "/Contents/";
    const auto executable = contents + "MacOS/app";
    std::vector<host::DistributionFile> members{
        {bundle + "/", {}}, {contents, {}}, {contents + "MacOS/", {}}, {contents + "Resources/", {}}
    };
    if (publication.bundle.empty())
        members.push_back({contents + "Info.plist", publish::bundle_plist(application, false)});
    std::istringstream rows(publication.files);
    std::string row;
    std::size_t remaining = package_byte_limit;
    while (std::getline(rows, row)) {
        const auto name = row.substr(row.rfind('\t') + 1);
        auto archive_path = name;
        if (publication.bundle.empty()) {
            if (name == "app") archive_path = executable;
            else if (name.starts_with("assets/")) archive_path = contents + "Resources/" + name;
        }
        if (!archive_path.starts_with(contents) && !archive_path.starts_with("licenses/")) continue;
        auto bytes = project::read_project_input(publication.root / utf8_path(name), remaining);
        remaining -= bytes.size();
        if (!row.starts_with(package_digest(bytes) + '\t'))
            throw std::runtime_error("Publication changed during macOS ZIP assembly: " + name);
        const auto slash = archive_path.rfind('/');
        auto parent = archive_path.substr(0, slash + 1);
        if (std::none_of(members.begin(), members.end(), [&](const auto& member) { return member.name == parent; }))
            members.push_back({std::move(parent), {}});
        members.push_back({archive_path, std::move(bytes), archive_path == executable});
    }
    project::require_outside_user_package_store(output);
    project::require_outside_native_approval_store(output, true);
    project::FreshProjectFile destination(output);
    destination.publish(host::distribution_zip(std::move(members)));
#else
    (void)directory; (void)output;
    throw std::runtime_error("macOS publication archive assembly is unavailable on this host.");
#endif
}
void verify_application_package(const fs::path& directory) {
#ifdef VOID_APPLICATION_PACKAGING
    (void)verify_package(directory);
#else
    (void)directory;
    throw std::runtime_error("Application packaging requires Linux x86-64 GNU or Windows x64 MSVC.");
#endif
}
}
