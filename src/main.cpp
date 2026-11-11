#include "void/backend.hpp"
#include "void/c_primitive.hpp"
#include "void/frontend.hpp"
#include "void/path.hpp"
#include "void/native_build.hpp"
#include "void/project_creation.hpp"
#include "void/project_resolution.hpp"
#include "void/runtime.hpp"
#include <llvm/Support/FileSystem.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/Support/raw_ostream.h>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <vector>
namespace {
struct UsageError : std::invalid_argument {using std::invalid_argument::invalid_argument;};
bool known_command(const std::string& command) {
    return command=="check" || command=="run" || command=="build" || command=="package" || command=="emit" || command=="init" || command=="new" || command=="resolve" || command=="deps" || command=="native-bind";
}
void usage(std::ostream& out, const std::string& command={}) {
    if(command.empty() || command=="check")out<<"Usage: voidc check <file-or-directory> [--no-opt]\n";
    if(command.empty() || command=="run")out<<"Usage: voidc run [file-or-directory] [legacy-i32-input] [--no-opt] [--no-cache | --cache] [-- application-arguments...]\n";
    if(command.empty() || command=="build")out<<"Usage: voidc build [file-or-directory] [-o executable] [--no-opt]\n";
    if(command.empty() || command=="package")out<<"Usage: voidc package [--project DIR] [--profile debug|release] [-o directory] [--allow-native] [--replace]\n"
      "       voidc package --verify DIR\n";
    if(command.empty() || command=="emit")out<<"Usage: voidc emit <file-or-directory> -o <object> [--no-opt]\n";
    if(command.empty() || command=="init")out<<"Usage: voidc init [DIR] [project options]\n";
    if(command.empty() || command=="new")out<<"Usage: voidc new PATH [project options] [--source PATH]\n";
    if(command.empty() || command=="resolve")out<<"Usage: voidc resolve --project DIR [--locked | --refresh]\n";
    if(command.empty() || command=="deps")out<<"Usage: voidc deps fetch --project DIR [--allow-git ORIGIN REV]...\n";
    if(command.empty() || command=="native-bind")out<<"Usage: voidc native-bind <descriptor> <C-header> -o <adapter.c>\n";
}
void help(const std::string& command={}) {
    std::cout<<"VOID Language Compiler v"<<VOID_COMPILER_VERSION<<" (development)\nLLVM "<<LLVM_VERSION_STRING<<"\n\n";
    usage(std::cout,command);
    if(command.empty()) {
        std::cout<<"\nCommands:\n"
          "  check   Analyze source and imports without executing startup code.\n"
          "  run     Execute through the JIT.\n"
          "  build   Compile and link a native executable with the matching Runtime.\n"
          "  package Assemble a relocatable application directory.\n"
          "  emit    Write a native object for manual linking.\n"
          "  init    Add a project manifest to an existing directory.\n"
          "  new     Create a project and starter source in a new directory.\n"
          "  resolve Resolve available library dependencies and verify or write void.lock.\n"
          "  deps fetch   Explicitly acquire pinned Git source snapshots.\n"
          "  native-bind  Generate a checked C11 adapter for a native library.\n"
          "\nOptions:\n"
          "  -h, --help  Show help; use <command> --help for examples.\n"
          "  --version   Show compiler, LLVM, target and build identity.\n"
          "  --no-opt    Disable O2 (accepted as a no-op by check).\n"
          "  --cache     Select Debug code and report cache status (run only).\n"
          "  --no-cache  Disable disk caching (run only).\n"
          "  --project DIR  Select exactly DIR/void.project.toml.\n"
          "  --allow-native  Authorize this project's declared native provider for run/build/emit/package.\n"
          "  --entry PATH   Override its entry with a manifest-relative source.\n"
          "  --profile debug|release  Select project optimization/cache settings.\n"
          "                           line/full debug_info warns; profile-controlled emission is not implemented.\n"
          "  -o PATH     Set output path (build, emit or package; required by emit).\n"
          "  --          End options before a source path.\n";
    }
    if(command=="check")std::cout<<"\nCheck source and imported modules; no application entry is required.\n"
      "--no-opt is accepted for compatibility and has no effect on checking.\n"
      "Example: voidc check main.void\n";
    if(command=="package")std::cout<<"\nPackage a manifest-selected Linux x86-64 GNU or Windows x64 MSVC application.\n"
      "The default directory is <project>/dist. On Linux, --replace accepts an existing verified package of the same application.\n"
      "Windows requires a fresh destination, including when --replace is supplied.\n"
      "Use --verify DIR to check the package inventory without building.\n"
      "Example: voidc package --project . --profile release -o dist\n";
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
    if(command=="init" || command=="new")std::cout<<"\nProject options: --kind application|library, --name TEXT, --version SEMVER,\n"
      "  --project-id ID, --app-id ID or --package-id ID --package-name IDENTIFIER, --nested.\n"
      "Default: application, destination basename, version 0.1.0 and independent local IDs.\n"
      "--source is available with new. The destination and manifest are created exclusively.\n";
    if(command=="resolve")std::cout<<"\nVerify an existing local dependency lock or create one.\n"
      "  --locked   Require an existing lock when dependencies are declared.\n"
      "  --refresh  Replace a stale lock after recomputing the local graph.\n";
    std::cout<<"\nA directory with void.project.toml selects its project; otherwise it selects main.void.\n"
      "run/build default to the current directory. Bare explicit files retain their parent root.\n"
      "Project discovery selects a unique eligible main in any filename; --entry bypasses scanning.\n"
      "Library projects support check only. Project build output defaults to <project-root>/build/<entry-stem>.\n"
      "Application entry selection is automatic (main or executable top-level code);\n"
      "Use -- before a source path beginning with '-'.\n"
      "Quote paths containing spaces. Options may precede or follow operands.\n"
      "Exit status: 0 success; 1 compilation/toolchain failure; 2 invocation error.\n"
      "run propagates an application's process status.\n"
      "Documentation: https://github.com/Shmellyorc/Void.Language/wiki\n";
}
std::int32_t input(const char* text) {
    char* end = nullptr; errno = 0;
    auto value = std::strtoll(text, &end, 10);
    if (end == text || *end || errno == ERANGE || value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::int32_t>::max())
        throw UsageError("Expected a decimal i32 input (-2147483648 through 2147483647).");
    return static_cast<std::int32_t>(value);
}
int resolve_project(int argc,char** argv) {
    using namespace vd::project;
    std::optional<std::filesystem::path> root;
    auto mode=ResolveMode::create;
    for(int i=2;i<argc;++i) {
        const std::string_view option=argv[i];
        if(option=="--project") {
            if(root || ++i==argc || !*argv[i] || argv[i][0]=='-')
                throw UsageError("Expected one directory after --project.");
            root=vd::utf8_path(argv[i]);
        } else if(option=="--locked" || option=="--refresh") {
            if(mode!=ResolveMode::create)throw UsageError("Use at most one of --locked and --refresh.");
            mode=option=="--locked"?ResolveMode::locked:ResolveMode::refresh;
        } else throw UsageError("Unknown resolve option: "+std::string(option));
    }
    if(!root)throw UsageError("resolve requires --project DIR.");
    switch(resolve_dependencies(*root,mode)) {
    case ResolveResult::no_dependencies: std::cout<<"No project dependencies.\n"; break;
    case ResolveResult::verified: std::cout<<"Verified void.lock.\n"; break;
    case ResolveResult::created: std::cout<<"Created void.lock.\n"; break;
    case ResolveResult::refreshed: std::cout<<"Refreshed void.lock.\n"; break;
    }
    return 0;
}
int fetch_project(int argc, char** argv) {
    using namespace vd::project;
    if (argc < 3 || std::string_view(argv[2]) != "fetch") throw UsageError("Expected deps fetch.");
    std::optional<std::filesystem::path> root;
    std::vector<GitPin> allowed;
    for (int i = 3; i < argc; ++i) {
        const std::string_view option = argv[i];
        if (option == "--project") {
            if (root || ++i == argc || !*argv[i] || argv[i][0] == '-')
                throw UsageError("Expected one directory after --project.");
            root = vd::utf8_path(argv[i]);
        } else if (option == "--allow-git") {
            if (i + 2 >= argc || allowed.size() == dependency_edge_limit)
                throw UsageError("Expected --allow-git ORIGIN REV.");
            GitPin pin{argv[i + 1], argv[i + 2]};
            i += 2;
            if (!valid_git_pin(pin)) throw UsageError("Invalid --allow-git origin or revision.");
            allowed.push_back(std::move(pin));
        } else throw UsageError("Unknown deps fetch option: " + std::string(option));
    }
    if (!root) throw UsageError("deps fetch requires --project DIR.");
    const auto count = fetch_dependencies(*root, allowed);
    std::cout << "Verified Git source snapshots: " << count << '\n';
    return 0;
}
int create_project(const std::string& command,int argc,char** argv) {
    vd::project::CreationOptions options;options.new_directory=command=="new";
    std::optional<std::filesystem::path> destination;
    std::set<std::string> seen;
    bool operands=false;
    for(int i=2;i<argc;++i) {
        std::string arg=argv[i];
        if(!operands && arg=="--"){operands=true;continue;}
        if(operands || !arg.starts_with('-')) {
            if(destination || arg.empty())throw UsageError("Expected one project destination.");
            destination=vd::utf8_path(arg);continue;
        }
        if(!seen.insert(arg).second)throw UsageError("Duplicate creation option "+arg+".");
        if(arg=="--nested"){options.nested=true;continue;}
        if(arg!="--kind" && arg!="--name" && arg!="--version" && arg!="--project-id" &&
           arg!="--app-id" && arg!="--package-id" && arg!="--package-name" && arg!="--source")
            throw UsageError("Unknown creation option "+arg+".");
        if(++i==argc || std::string_view(argv[i]).starts_with("--"))throw UsageError("Missing value for "+arg+".");
        std::string value=argv[i];
        if(arg=="--kind") {
            if(value!="application" && value!="library")throw UsageError("Expected --kind application or library.");
            options.kind=value=="application"?vd::project::Kind::application:vd::project::Kind::library;
        } else if(arg=="--name")options.name=value;
        else if(arg=="--version")options.version=value;
        else if(arg=="--project-id")options.project_id=value;
        else if(arg=="--app-id")options.application_id=value;
        else if(arg=="--package-id")options.package_id=value;
        else if(arg=="--package-name")options.package_name=value;
        else options.source=value;
    }
    const bool app=options.kind==vd::project::Kind::application;
    if(!destination && options.new_directory)throw UsageError("new requires a destination PATH.");
    if(options.source && !options.new_directory)throw UsageError("--source is only available with new.");
    if((app && (options.package_name || options.package_id)) || (!app && options.application_id))
        throw UsageError("Identity options must match the selected project kind.");
    const auto plan=vd::project::plan_creation(destination?*destination:std::filesystem::current_path(),options);
    const auto manifest=plan.root/"void.project.toml";
    std::cout<<"Manifest: "<<vd::path_utf8(manifest)<<"\nProject ID: "<<plan.manifest->project().id<<'\n'
             <<(app?"Application ID: ":"Package ID: ")<<(app?plan.manifest->application()->id:plan.manifest->package()->id)
             <<"\nNext: from the displayed manifest directory, run voidc check --project .\n"<<std::flush;
    vd::project::publish_creation(plan);
    std::cout<<"Created project manifest.\n";
    if(app)std::cout<<"Automatic application selection requires one eligible main(). Use --entry for a deliberate override.\n";
    return 0;
}
void generate_native_binding(int argc, char** argv) {
    if (argc != 6 || std::string_view(argv[4]) != "-o")
        throw UsageError("Expected native-bind <descriptor> <C-header> -o <adapter.c>.");
    const auto descriptor = vd::utf8_path(argv[2]);
    const auto output = vd::utf8_path(argv[5]);
    if (output.extension() != ".c")
        throw UsageError("Expected a new .c output path.");
    if (!std::filesystem::is_regular_file(descriptor))
        throw std::runtime_error("Missing native descriptor: " + vd::path_utf8(descriptor));
    std::ifstream source(descriptor, std::ios::binary);
    std::string bytes(16385, '\0');
    source.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (source.bad() || !source.eof() || source.gcount() == 0)
        throw std::runtime_error("Cannot read native descriptor (expected 1..16384 bytes).");
    bytes.resize(static_cast<std::size_t>(source.gcount()));
    const auto generated = vd::CPrimitiveAdmission::generate_adapter(bytes, argv[3]);
    int fd;
    const auto name = vd::path_utf8(output);
    if (auto error = llvm::sys::fs::openFileForWrite(name, fd, llvm::sys::fs::CD_CreateNew))
        throw std::runtime_error("Cannot create C adapter " + name + ": " + error.message());
    llvm::raw_fd_ostream file(fd, true);
    file << generated;
    file.close();
    if (file.has_error()) {
        file.clear_error();
        std::error_code ignored;
        std::filesystem::remove(output, ignored);
        throw std::runtime_error("Cannot write C adapter: " + name);
    }
}
}
static int run_cli(int argc, char** argv) {
    std::string command;
    try {
        if(argc>=2 && (std::string_view(argv[1])=="init" || std::string_view(argv[1])=="new")) {
            command=argv[1];
            if(argc==3 && (std::string_view(argv[2])=="--help" || std::string_view(argv[2])=="-h")){help(command);return 0;}
            return create_project(command,argc,argv);
        }
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
        if(argc==1 || (argc==2 && (std::string(argv[1])=="--help" || std::string(argv[1])=="-h"))) {help();return 0;}
        if(argc==2 && std::string(argv[1])=="--version") {
            std::cout<<"VOID Language Compiler v"<<VOID_COMPILER_VERSION<<" (development)\nLLVM "<<LLVM_VERSION_STRING
              <<"\nTarget: "<<LLVM_DEFAULT_TARGET_TRIPLE<<"\nBuild identity: "<<vd::compiler_build_identity()<<'\n';
            return 0;
        }
        if(argc>=2 && std::string(argv[1])=="help") {
            if(argc==2) {help();return 0;}
            if(argc!=3 || !known_command(argv[2]))throw UsageError("Expected help [check|run|build|package|emit|init|new|resolve|deps|native-bind].");
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
        command=argv[1];
        if(command=="--app")throw UsageError("--app has been removed; entry selection is automatic.");
        if(!known_command(command))
            throw UsageError("Unknown command: "+command);
        if(argc==3 && (std::string(argv[2])=="--help" || std::string(argv[2])=="-h")) {help(command);return 0;}
        if(command=="resolve")return resolve_project(argc,argv);
        if(command=="deps")return fetch_project(argc,argv);
        if(command=="native-bind") {generate_native_binding(argc,argv);return 0;}
        if(command=="package" && argc>=3 && std::string_view(argv[2])=="--verify") {
            if(argc!=4 || !*argv[3] || argv[3][0]=='-')throw UsageError("Expected one directory after package --verify.");
            vd::verify_application_package(vd::utf8_path(argv[3]));
            std::cout<<"Application package verified\n";
            return 0;
        }
        bool optimize=true,cache=false,no_cache=false,no_opt=false;
        bool allow_native=false;
        bool replace_package=false;
        bool positional_only=false;
        bool forwarded=false;
        std::vector<std::string> application_arguments;
        std::vector<std::string> operands;std::optional<std::filesystem::path> output;
        std::optional<std::filesystem::path> project_root;
        std::optional<std::string> project_entry;
        std::optional<vd::project::ProfileName> profile;
        std::string profile_key;
        for(int i=2;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--" && command=="run" && (!operands.empty() || project_root)) {
                if(operands.size()>1)throw UsageError("Application arguments require one explicit source or project selector.");
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
            else if(arg=="--allow-native") {if(allow_native)throw UsageError("Duplicate --allow-native.");allow_native=true;}
            else if(arg=="--replace" && command=="package") {
                if(replace_package)throw UsageError("Duplicate --replace.");
                replace_package=true;
            }
            else if(arg=="--project") {
                if(project_root || ++i==argc || !*argv[i] || argv[i][0]=='-')throw UsageError("Expected one directory after --project.");
                project_root=vd::utf8_path(argv[i]);
            }
            else if(arg=="--profile") {
                if(profile || ++i==argc)throw UsageError("Expected one debug|release value after --profile.");
                const std::string value=argv[i];
                if(value=="debug")profile=vd::project::ProfileName::debug;
                else if(value=="release")profile=vd::project::ProfileName::release;
                else throw UsageError("--profile requires exactly debug or release.");
            }
            else if(arg=="--entry") {
                if(project_entry || ++i==argc || !*argv[i] || argv[i][0]=='-')throw UsageError("Expected one manifest-relative source after --entry.");
                project_entry=argv[i];
            }
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
        if(output && command!="emit" && command!="build" && command!="package")throw UsageError("-o is available only for emit/build/package.");
        if(command=="emit" && !output)throw UsageError("emit requires -o <object>.");
        if(project_root && !operands.empty())throw UsageError("--project cannot accompany a positional file or directory selector.");
        if((command=="check" || command=="emit") && operands.empty() && !project_root)throw UsageError("Expected a file or directory.");
        if(operands.size()>(command=="run"?2u:1u))throw UsageError("Too many operands.");
        // Parse legacy input syntax before analysis, but its admission follows classification.
        auto value=operands.size()==2?input(operands[1].c_str()):0;
        auto path=project_root?*project_root:operands.empty()?std::filesystem::current_path():vd::utf8_path(operands[0]);
        const bool project_mode=project_root || (std::filesystem::is_directory(path) && vd::project_manifest_present(path));
        if(command=="package" && !project_mode)throw UsageError("Package requires a manifest-selected application project.");
        if(allow_native && (!project_mode || command=="check"))
            throw UsageError("--allow-native requires a manifest-selected run, build or emit.");
        if(profile && !project_mode)throw UsageError("--profile requires a manifest-selected project context.");
        if(project_entry && !project_mode)throw UsageError("--entry requires a manifest-selected project context.");
        std::shared_ptr<const vd::ProjectAnalysis> project;
        std::filesystem::path selected;
        vd::AnalysisResult analysis;
        if(project_mode) {
            auto result=vd::analyze_manifest_project(path,project_entry,{},{},allow_native);
            if(result.manifest && (!result.manifest->native().empty() || result.manifest->native_requirement()) &&
                command!="check" && !allow_native)
                throw UsageError("This project declares native code. Pass --allow-native to authorize its execution.");
            if(result.manifest && result.manifest->kind()==vd::project::Kind::library && project_entry)throw UsageError("--entry cannot select an application entry in a library project.");
            if(result.manifest) {
                vd::project::EffectiveProfile selected_profile;
                try{selected_profile=vd::project::resolve_profile(*result.manifest,profile,no_opt,cache);}
                catch(const std::invalid_argument& error){throw UsageError(error.what());}
                optimize=selected_profile.optimize;profile_key=selected_profile.cache_key;
                for(const auto& warning:selected_profile.diagnostics)std::cerr<<vd::render_diagnostic(warning)<<'\n';
            }
            if(!result.succeeded())throw vd::CompilationError(std::move(result.diagnostics));
            project=std::move(result.project);
            if(project->manifest()->kind()==vd::project::Kind::library) {
                if(command=="package")throw UsageError("Package requires an application project.");
                if(command!="check")throw std::invalid_argument("Library project run/build/emit is unsupported; use check for source analysis.");
                for(const auto& warning:result.diagnostics)std::cerr<<vd::render_diagnostic(warning)<<'\n';
                std::cout<<"Library project checked successfully\n";return 0;
            }
            selected=*project->entry();analysis={project->program(),std::move(result.diagnostics)};
        } else {selected=std::filesystem::is_directory(path)?vd::discover_project(path).entry:path;analysis=vd::analyze_project(selected);}
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
        if(command=="package") {
            if(!output)output=project->root()/"dist";
            vd::build_application_package(*project,*output,optimize,argv[0],profile_key,replace_package);
            std::cout<<"Packaged application: "<<vd::path_utf8(*output)<<'\n';
            return 0;
        }
        if(command=="run") {
            std::unique_ptr<vd::Jit> jit;
            if(!no_cache)jit=std::make_unique<vd::Jit>(vd::DebugCache{project?project->root():std::filesystem::canonical(selected.parent_path().empty()?std::filesystem::path("."):selected.parent_path()),std::string(vd::compiler_build_identity())+(project?":project-selection-v1:"+project->selection_key()+":"+profile_key:"")},vd::ir::Emission::entry);
            else jit=std::make_unique<vd::Jit>(vd::DebugCache{{},std::string(vd::compiler_build_identity())+(project?":"+profile_key:"")},vd::ir::Emission::entry);
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
                output=(project?project->root():entry.parent_path())/"build"/name;
            }
            vd::build_executable(*analysis.program,*output,optimize,argv[0],profile_key);
            std::cout<<"Built native executable: "<<vd::path_utf8(*output)<<'\n';
        } else {
            vd::publish_object(*analysis.program,*output,optimize,profile_key);
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
