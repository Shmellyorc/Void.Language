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
#include <windows.h>
#endif
namespace vd {
namespace {
namespace fs = std::filesystem;
// Output paths are physical paths, never VFS identities. Permit ancestor aliases
// such as macOS /var, but require exact spelling of each existing component.
void exact_path(const fs::path& path) {
    auto current=path.root_path();
    for(const auto& component:path.relative_path()) {
        bool found=false;
        for(const auto& child:fs::directory_iterator(current))
            if(child.path().filename()==component){found=true;break;}
        if(!found)throw std::runtime_error("Output path has incorrect spelling: "+path_utf8(current/component));
        current/=component;
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
    version(config.driver,"clang version 19.1.7",stage.directory/"version.txt");
    if(!config.linker.empty())version(config.linker,"LLD 19.1.7",stage.directory/"version.txt");
    const auto object=stage.directory/"program.o";
#ifdef _WIN32
    const auto executable=stage.directory/"program.exe";
#else
    const auto executable=stage.directory/"program";
#endif
    emit_object(analysis.lower(),object,optimize,true);
    std::vector<std::string> args{path_utf8(config.driver),"--driver-mode=g++"};
    if(!config.linker.empty())args.push_back("--ld-path="+path_utf8(config.linker));
    args.insert(args.end(),config.flags.begin(),config.flags.end());
    // LLD 19 interprets '%' in absolute output paths. The driver's child working
    // directory keeps the real project path and gives the linker a relative name.
    args.insert(args.end(),{"-working-directory",path_utf8(stage.directory),path_utf8(object),path_utf8(config.runtime),path_utf8(config.fixture),"-o",path_utf8(executable.filename())});
    auto status=execute(args);
    if(status!=0)throw std::runtime_error("Native linker failed (status "+std::to_string(status)+").");
    if(!fs::is_regular_file(executable) || fs::file_size(executable)==0 || !llvm::sys::fs::can_execute(path_utf8(executable)))
        throw std::runtime_error("Native linker did not produce a usable executable.");
    output_path(analysis,output);publish(executable,output);
}
}
