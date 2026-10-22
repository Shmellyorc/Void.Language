#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/path.hpp"
#include "void/native_build.hpp"
#include <llvm/Support/FileSystem.h>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>
namespace {
struct UsageError : std::invalid_argument {using std::invalid_argument::invalid_argument;};
std::int32_t input(const char* text) {
    char* end = nullptr; errno = 0;
    auto value = std::strtoll(text, &end, 10);
    if (end == text || *end || errno == ERANGE || value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::int32_t>::max())
        throw UsageError("Expected a decimal i32 input (-2147483648 through 2147483647).");
    return static_cast<std::int32_t>(value);
}
}
static int run_cli(int argc, char** argv) {
    try {
        vd::configure_standard_library_root(vd::utf8_path(llvm::sys::fs::getMainExecutable(argv[0],reinterpret_cast<void*>(&run_cli))));
        for(int i=1;i<argc;++i)if(std::string(argv[i])=="--app")throw UsageError("--app has been removed; entry selection is automatic.");
        if(argc==3 && std::string(argv[1])=="--emit-proof") {
            vd::emit_object(vd::ir::architecture_proof(),vd::utf8_path(argv[2]),true);
            std::cout<<"Wrote native architecture proof object\n";return 0;
        }
        if(argc==2 && std::string(argv[1])=="--jit-proof") {
            vd::Jit jit;auto code=jit.compile(vd::ir::architecture_proof(),"entry");
            auto result=code->invoke(0);std::cout<<result<<'\n';return result==42?0:1;
        }
        if(argc<2)throw UsageError("Expected a command.");
        const std::string command=argv[1];
        if(command!="check" && command!="run" && command!="emit" && command!="build")
            throw UsageError("Unknown command: "+command);
        bool optimize=true,cache=false,no_opt=false;
        std::vector<std::string> operands;std::optional<std::filesystem::path> output;
        for(int i=2;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--no-opt") {if(no_opt)throw UsageError("Duplicate --no-opt.");no_opt=true;optimize=false;}
            else if(arg=="--cache") {if(cache)throw UsageError("Duplicate --cache.");cache=true;optimize=false;}
            else if(arg=="-o") {
                if(output || ++i==argc || !*argv[i] || argv[i][0]=='-')throw UsageError("Expected one output path after -o.");
                output=vd::utf8_path(argv[i]);
            } else if(arg.starts_with("--") || (arg.starts_with("-") && (arg.size()<2 || arg[1]<'0' || arg[1]>'9')))
                throw UsageError("Unknown option: "+arg);
            else operands.push_back(arg);
        }
        if(cache && command!="run")throw UsageError("--cache is available only for Debug JIT runs.");
        if(output && command!="emit" && command!="build")throw UsageError("-o is available only for emit/build.");
        if(command=="emit" && !output)throw UsageError("emit requires -o <object>.");
        if((command=="check" || command=="emit") && operands.empty())throw UsageError("Expected a file or directory.");
        if(operands.size()>(command=="run"?2u:1u))throw UsageError("Too many operands.");
        // Parse legacy input syntax before analysis, but its admission follows classification.
        auto value=operands.size()==2?input(operands[1].c_str()):0;
        auto path=operands.empty()?std::filesystem::current_path():vd::utf8_path(operands[0]);
        auto selected=std::filesystem::is_directory(path)?vd::discover_project(path).entry:path;
        auto analysis=vd::analyze_project(selected);
        if(!analysis.succeeded())throw vd::CompilationError(std::move(analysis.diagnostics));
        for(const auto& warning:analysis.program->warnings())std::cerr<<vd::render_diagnostic(warning)<<'\n';
        if(command=="check") {std::cout<<"Source checked successfully\n";return 0;}
        const bool application=analysis.program->application();
        if(application && operands.size()==2)throw UsageError("Application entries do not accept arguments.");
        vd::require_source_entry(*analysis.program);
        if(command=="run") {
            std::unique_ptr<vd::Jit> jit;
            if(cache)jit=std::make_unique<vd::Jit>(vd::DebugCache{std::filesystem::canonical(selected.parent_path().empty()?std::filesystem::path("."):selected.parent_path()),std::string(vd::compiler_build_identity())});
            else jit=std::make_unique<vd::Jit>();
            auto code=jit->compile(*analysis.program,optimize);
            if(cache)std::cerr<<"Debug cache: "<<jit->cache_status()<<'\n';
            auto status=code->invoke(value);
            if(application)return status;
            std::cout<<status<<'\n';return 0;
        }
        if(command=="build") {
            if(!output) {
                auto entry=std::filesystem::absolute(selected);
                auto name=entry.stem();
#ifdef _WIN32
                name+=".exe";
#endif
                output=entry.parent_path()/"build"/name;
            }
            vd::build_executable(*analysis.program,*output,optimize,argv[0]);
            std::cout<<"Built native executable: "<<vd::path_utf8(*output)<<'\n';
        } else {
            vd::publish_object(*analysis.program,*output,optimize);
            std::cout<<"Wrote native source object: "<<vd::path_utf8(*output)<<'\n';
        }
        return 0;
    } catch(const UsageError& error) {
        std::cerr<<"Invalid invocation: "<<error.what()<<"\nUsage: voidc check <file-or-directory>\n"
          "       voidc run [file-or-directory] [legacy-i32-input] [--no-opt] [--cache]\n"
          "       voidc build [file-or-directory] [-o executable] [--no-opt]\n"
          "       voidc emit <file-or-directory> -o <object> [--no-opt]\n";return 2;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    try {
        // Keep the shared CLI in UTF-8; never pass native arguments through ACP.
        std::vector<std::string> arguments;
        arguments.reserve(argc);
        for(int i=0;i<argc;++i) {
            auto text=std::filesystem::path(argv[i]).u8string();
            arguments.emplace_back(text.begin(),text.end());
        }
        std::vector<char*> pointers;
        for(auto& argument:arguments)pointers.push_back(argument.data());
        pointers.push_back(nullptr);
        return run_cli(argc,pointers.data());
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
#else
int main(int argc, char** argv) { return run_cli(argc,argv); }
#endif
