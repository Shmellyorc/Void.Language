#include "void/native_build.hpp"
#include "void/backend.hpp"
#include "void/path.hpp"
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>
#include <fstream>
#include <cctype>
#include <stdexcept>
#ifdef _WIN32
#include "native_windows.hpp"
#endif
namespace vd {
namespace {
namespace fs = std::filesystem;
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
fs::path output_path(const ValidatedAnalysis& analysis,const fs::path& requested) {
    auto path=fs::absolute(requested).lexically_normal();
    if(path.extension()==".void")throw std::runtime_error("Unsafe source/output collision: output has a source extension: "+path_utf8(path));
    if(path.filename().empty())throw std::runtime_error("Output must name a file.");
    // Check before directory creation, so a failed collision never touches input.
    for(const auto& module:analysis.modules()) {
        auto source=utf8_path(module.source.path());
        if(path==source || (fs::exists(path) && fs::equivalent(path,source)))
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
};
Resources resources(const char* argv0) {
    auto executable=utf8_path(llvm::sys::fs::getMainExecutable(argv0,reinterpret_cast<void*>(&resources)));
    const auto config=executable.parent_path()/"void-native.txt";
    std::ifstream file(config);std::vector<std::string> lines;std::string line;
    while(std::getline(file,line)){if(!line.empty() && line.back()=='\r')line.pop_back();lines.push_back(line);}
    if(!file.eof() || lines.size()<5 || lines[0]!="VOID native 19.1.7 v1")
        throw std::runtime_error("Native toolchain resources missing or invalid: "+path_utf8(config));
    auto path=[&](const std::string& text){auto p=utf8_path(text);return p.is_absolute()?p:(config.parent_path()/p).lexically_normal();};
    Resources result{path(lines[1]),lines[2].empty()?fs::path{}:path(lines[2]),path(lines[3]),path(lines[4]),{}};
    for(const auto& p:{result.driver,result.runtime,result.fixture}) {
        if(!fs::is_regular_file(p))throw std::runtime_error("Native toolchain resource missing: "+path_utf8(p));
        exact_path(p);
    }
    if(!result.linker.empty()) {
        if(!fs::is_regular_file(result.linker))throw std::runtime_error("Native linker resource missing: "+path_utf8(result.linker));
        exact_path(result.linker);
    }
    if(!llvm::sys::fs::can_execute(path_utf8(result.driver)))throw std::runtime_error("Native Clang driver is not executable: "+path_utf8(result.driver));
    result.flags.assign(lines.begin()+5,lines.end());return result;
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
        throw std::runtime_error("Native toolchain requires matching LLVM/Clang/LLD 19.1.7: "+path_utf8(tool));
}
}
void publish_object(const ValidatedAnalysis& analysis,const fs::path& requested,bool optimize) {
    auto output=output_path(analysis,requested);Staging stage(output);
    auto object=stage.directory/"program.o";
    emit_object(analysis.lower(),object,optimize,true);
    output_path(analysis,output);publish(object,output);
}
void build_executable(const ValidatedAnalysis& analysis,const fs::path& requested,bool optimize,const char* argv0) {
    auto config=resources(argv0);
    auto output=output_path(analysis,requested);Staging stage(output);
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
    version(config.driver,"clang version 19.1.7",work/"version.txt");
    if(!config.linker.empty())version(config.linker,"LLD 19.1.7",work/"version.txt");
    const auto object=work/"program.o";
    emit_object(analysis.lower(),object,optimize,true);
    std::vector<std::string> args{path_utf8(config.driver),"--driver-mode=g++"};
    if(!config.linker.empty())args.push_back("--ld-path="+path_utf8(config.linker));
    args.insert(args.end(),config.flags.begin(),config.flags.end());
    // LLD 19 interprets '%' in absolute output paths. The driver's child working
    // directory gives the linker a relative name. Windows uses a short private
    // workspace because Clang cannot chdir to a genuine long project path.
#ifdef _WIN32
    const auto object_argument=object.filename();
#else
    const auto& object_argument=object;
#endif
    args.insert(args.end(),{"-working-directory",path_utf8(work),path_utf8(object_argument),path_utf8(config.runtime),path_utf8(config.fixture),"-o",path_utf8(executable.filename())});
#if defined(__linux__)
    args.push_back("-ldl"); // Optional system terminfo binding in the Runtime archive.
#endif
    auto status=execute(args);
    if(status!=0)throw std::runtime_error("Native linker failed (status "+std::to_string(status)+").");
    if(!fs::is_regular_file(executable) || fs::file_size(executable)==0 || !llvm::sys::fs::can_execute(path_utf8(executable)))
        throw std::runtime_error("Native linker did not produce a usable executable.");
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
}
}
