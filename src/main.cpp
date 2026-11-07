#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/path.hpp"
#include "void/native_build.hpp"
#include "void/runtime.hpp"
#include <llvm/Support/FileSystem.h>
#include <llvm/Config/llvm-config.h>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>
namespace {
struct UsageError : std::invalid_argument {using std::invalid_argument::invalid_argument;};
bool known_command(const std::string& command) {
    return command=="check" || command=="run" || command=="build" || command=="emit";
}
void usage(std::ostream& out, const std::string& command={}) {
    if(command.empty() || command=="check")out<<"Usage: voidc check <file-or-directory> [--no-opt]\n";
    if(command.empty() || command=="run")out<<"Usage: voidc run [file-or-directory] [legacy-i32-input] [--no-opt] [--no-cache | --cache] [-- application-arguments...]\n";
    if(command.empty() || command=="build")out<<"Usage: voidc build [file-or-directory] [-o executable] [--no-opt]\n";
    if(command.empty() || command=="emit")out<<"Usage: voidc emit <file-or-directory> -o <object> [--no-opt]\n";
}
void help(const std::string& command={}) {
    std::cout<<"VOID Language Compiler v"<<VOID_COMPILER_VERSION<<" (development)\nLLVM "<<LLVM_VERSION_STRING<<"\n\n";
    usage(std::cout,command);
    if(command.empty()) {
        std::cout<<"\nCommands:\n"
          "  check   Analyze source and imports without executing startup code.\n"
          "  run     Execute through the JIT.\n"
          "  build   Compile and link a native executable with the matching Runtime.\n"
          "  emit    Write a native object for manual linking.\n"
          "\nOptions:\n"
          "  -h, --help  Show help; use <command> --help for examples.\n"
          "  --version   Show compiler, LLVM, target and build identity.\n"
          "  --no-opt    Disable O2 (accepted as a no-op by check).\n"
          "  --cache     Select Debug code and report cache status (run only).\n"
          "  --no-cache  Disable disk caching (run only).\n"
          "  -o PATH     Set output path (build/emit only; required by emit).\n"
          "  --          End options before a source path.\n";
    }
    if(command=="check")std::cout<<"\nCheck source and imported modules; no application entry is required.\n"
      "--no-opt is accepted for compatibility and has no effect on checking.\n"
      "Example: voidc check main.void\n";
    if(command=="run")std::cout<<"\nRun a synchronous application; its return value is the process status.\n"
      "Legacy entry(i32) -> i32 accepts one decimal i32 (default 0), prints its\n"
      "result, and exits 0. Applications read os.args() after an explicit source\n"
      "and -- separator; subsequent tokens are literal application arguments.\n"
      "  --no-opt    Disable O2; eligible Debug code uses the automatic disk cache.\n"
      "  --cache     Select Debug code and print cache status.\n"
      "  --no-cache  Disable disk caching; does not disable O2.\n"
      "Examples: voidc run .\n"
      "          voidc run calculation.void 10 --no-cache\n"
      "          voidc run main.void -- --help \"two words\"\n";
    if(command=="build")std::cout<<"\nLink an executable using the installed Runtime, dependency archives and\n"
      "void-native.txt beside voidc. The original Clang/LLD 23.1.4 SDK and host\n"
      "development libraries must remain available; this is a source installation.\n"
      "  -o PATH     Output file; default: <entry-directory>/build/<entry-stem>\n"
      "              (with .exe on Windows). Missing parent directories are created.\n"
      "  --no-opt    Disable O2.\n"
      "Examples: voidc build .\n"
      "          voidc build main.void -o \"output folder/hello\"\n";
    if(command=="emit")std::cout<<"\nWrite a host-native object, not an executable. Manual linking must supply\n"
      "the matching Runtime and dependencies; prefer build for an application.\n"
      "  -o PATH     Required output object path (.o on Unix, .obj on Windows).\n"
      "  --no-opt    Disable O2.\n"
      "Example: voidc emit main.void -o main.o\n";
    std::cout<<"\nA directory selects main.void. run/build default to the current directory.\n"
      "Application entry selection is automatic (main or executable top-level code);\n"
      "--app has been removed. Use -- before a source path beginning with '-'.\n"
      "Quote paths containing spaces. Options may precede or follow operands.\n"
      "Exit status: 0 success; 1 compilation/toolchain failure; 2 invocation error.\n"
      "run propagates an application's process status.\n"
      "Documentation: https://voidlang.com/docs/compiler-workflow/\n";
}
std::int32_t input(const char* text) {
    char* end = nullptr; errno = 0;
    auto value = std::strtoll(text, &end, 10);
    if (end == text || *end || errno == ERANGE || value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::int32_t>::max())
        throw UsageError("Expected a decimal i32 input (-2147483648 through 2147483647).");
    return static_cast<std::int32_t>(value);
}
}
static int run_cli(int argc, char** argv) {
    std::string command;
    try {
        // Preserve the established early removed-option diagnostic, but never
        // scan the explicit application's forwarded argument region.
        bool source_seen=false, source_only=false, output_operand=false;
        for(int i=1;i<argc;++i) {
            const std::string argument=argv[i];
            if(i>=2 && std::string(argv[1])=="run") {
                if(argument=="--" && source_seen)break;
                if(argument=="--"){source_only=true;continue;}
                if(argument=="--app")throw UsageError("--app has been removed; entry selection is automatic.");
                if(output_operand){output_operand=false;continue;}
                if(argument=="-o" && !source_only){output_operand=true;continue;}
                if(source_only || !argument.starts_with("-") ||
                   (argument.size()>1 && argument[1]>='0' && argument[1]<='9'))source_seen=true;
            }
            if(argument=="--app")throw UsageError("--app has been removed; entry selection is automatic.");
        }
        if(argc==2 && (std::string(argv[1])=="--help" || std::string(argv[1])=="-h")) {help();return 0;}
        if(argc==2 && std::string(argv[1])=="--version") {
            std::cout<<"VOID Language Compiler v"<<VOID_COMPILER_VERSION<<" (development)\nLLVM "<<LLVM_VERSION_STRING
              <<"\nTarget: "<<LLVM_DEFAULT_TARGET_TRIPLE<<"\nBuild identity: "<<vd::compiler_build_identity()<<'\n';
            return 0;
        }
        if(argc>=2 && std::string(argv[1])=="help") {
            if(argc==2) {help();return 0;}
            if(argc!=3 || !known_command(argv[2]))throw UsageError("Expected help [check|run|build|emit].");
            help(argv[2]);return 0;
        }
        vd::configure_standard_library_root(vd::utf8_path(llvm::sys::fs::getMainExecutable(argv[0],reinterpret_cast<void*>(&run_cli))));
        if(argc==3 && std::string(argv[1])=="--emit-proof") {
            vd::emit_object(vd::ir::architecture_proof(),vd::utf8_path(argv[2]),true);
            std::cout<<"Wrote native architecture proof object\n";return 0;
        }
        if(argc==2 && std::string(argv[1])=="--jit-proof") {
            vd::Jit jit;auto code=jit.compile(vd::ir::architecture_proof(),"entry");
            auto result=code->invoke(0);std::cout<<result<<'\n';return result==42?0:1;
        }
        if(argc<2)throw UsageError("Expected a command.");
        command=argv[1];
        if(command=="--app")throw UsageError("--app has been removed; entry selection is automatic.");
        if(!known_command(command))
            throw UsageError("Unknown command: "+command);
        if(argc==3 && (std::string(argv[2])=="--help" || std::string(argv[2])=="-h")) {help(command);return 0;}
        bool optimize=true,cache=false,no_cache=false,no_opt=false;
        bool positional_only=false;
        bool forwarded=false;
        std::vector<std::string> application_arguments;
        std::vector<std::string> operands;std::optional<std::filesystem::path> output;
        for(int i=2;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--" && command=="run" && !operands.empty()) {
                if(operands.size()!=1)throw UsageError("Application arguments require one explicit source operand.");
                forwarded=true;
                for(++i;i<argc;++i)application_arguments.emplace_back(argv[i]);
                break;
            }
            if(arg=="--app" && !positional_only)throw UsageError("--app has been removed; entry selection is automatic.");
            if(positional_only)operands.push_back(arg);
            else if(arg=="--")positional_only=true;
            else if(arg=="--no-opt") {if(no_opt)throw UsageError("Duplicate --no-opt.");no_opt=true;optimize=false;}
            else if(arg=="--cache") {if(cache)throw UsageError("Duplicate --cache.");cache=true;optimize=false;}
            else if(arg=="--no-cache") {if(no_cache)throw UsageError("Duplicate --no-cache.");no_cache=true;}
            else if(arg=="-o") {
                if(output || ++i==argc || !*argv[i] || argv[i][0]=='-')throw UsageError("Expected one output path after -o.");
                output=vd::utf8_path(argv[i]);
            } else if(arg.starts_with("--") || (arg.starts_with("-") && (arg.size()<2 || arg[1]<'0' || arg[1]>'9')))
                throw UsageError("Unknown option: "+arg);
            else operands.push_back(arg);
        }
        if(cache && command!="run")throw UsageError("--cache is available only for Debug JIT runs.");
        if(no_cache && command!="run")throw UsageError("--no-cache is available only for JIT runs.");
        if(cache && no_cache)throw UsageError("--cache and --no-cache cannot be combined.");
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
        if(forwarded && !application) {
            if(application_arguments.size()>1)throw UsageError("Legacy entry accepts one optional decimal i32 input.");
            value=application_arguments.empty()?0:input(application_arguments[0].c_str());
            application_arguments.clear();
        }
        if(application && operands.size()==2)throw UsageError("Application entries do not accept arguments.");
        vd::require_source_entry(*analysis.program);
        if(command=="run") {
            std::unique_ptr<vd::Jit> jit;
            if(!no_cache)jit=std::make_unique<vd::Jit>(vd::DebugCache{std::filesystem::canonical(selected.parent_path().empty()?std::filesystem::path("."):selected.parent_path()),std::string(vd::compiler_build_identity())},vd::ir::Emission::entry);
            else jit=std::make_unique<vd::Jit>(vd::DebugCache{},vd::ir::Emission::entry);
            auto code=jit->compile(*analysis.program,optimize);
            if(auto runtime=code->runtime_owner())
                runtime->application_arguments(vd::checked_application_arguments(std::move(application_arguments)));
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
        std::cerr<<"Invalid invocation: "<<error.what()<<'\n';
        usage(std::cerr,known_command(command)?command:std::string{});
        std::cerr<<"Try 'voidc "<<(known_command(command)?command+" ":std::string{})<<"--help' for examples.\n";
        return 2;
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
