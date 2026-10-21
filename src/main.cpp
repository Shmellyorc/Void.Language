#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/path.hpp"
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>
namespace {
std::int32_t input(const char* text) {
    char* end = nullptr; errno = 0;
    auto value = std::strtoll(text, &end, 10);
    if (end == text || *end || errno == ERANGE || value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::int32_t>::max())
        throw std::invalid_argument("Expected a decimal i32 input (-2147483648 through 2147483647).");
    return static_cast<std::int32_t>(value);
}
}
static int run_cli(int argc, char** argv) {
    try {
        bool optimize = true, cache=false, application=false;
        while(argc>1) {
            auto flag=std::string(argv[argc-1]);
            if(flag=="--app"){application=true;--argc;}
            else if(flag=="--cache"){cache=true;optimize=false;--argc;}
            else if(flag=="--no-opt"){optimize=false;--argc;}
            else break;
        }

        if (!application && argc == 3 && std::string(argv[1]) == "--emit-proof") {
            vd::emit_object(vd::ir::architecture_proof(), vd::utf8_path(argv[2]), true);
            std::cout << "Wrote native architecture proof object\n"; return 0;
        }
        if (!application && argc == 2 && std::string(argv[1]) == "--jit-proof") {
            vd::Jit jit; auto code = jit.compile(vd::ir::architecture_proof(), "entry");
            auto result = code->invoke(0);
            std::cout << result << '\n'; return result == 42 ? 0 : 1;
        }
        if (argc >= 3) {
            std::string command = argv[1];
            if(application && cache && command!="run")return 2;
            if(cache && command!="run")throw std::invalid_argument("--cache is available only for Debug JIT runs");
            bool check = !application && command == "check" && argc == 3;
            bool run = command == "run" && (argc == 3 || (!application && argc == 4));
            bool emit = command == "emit" && argc == 5 && std::string(argv[3]) == "-o";
            if (check || run || emit) {
                auto path=vd::utf8_path(argv[2]);
                auto selected=std::filesystem::is_directory(path) ? vd::discover_project(path).entry : path;
                if(cache && run) {
                    auto analysis=vd::analyze_project(selected);
                    if(!analysis.succeeded())throw vd::CompilationError(std::move(analysis.diagnostics));
                    for(const auto& warning:analysis.program->warnings())std::cerr<<vd::render_diagnostic(warning)<<'\n';
                    if(analysis.program->application()!=application)throw std::invalid_argument(application ? "Application entry requires executable top-level statements or synchronous main() with no parameters and no result or i32; legacy entry retains precedence." : "Application entries require explicit --app.");
                    vd::require_source_entry(*analysis.program);
                    vd::Jit jit({std::filesystem::canonical(selected.parent_path().empty() ? std::filesystem::path(".") : selected.parent_path()),std::string(vd::compiler_build_identity())});
                    auto code=jit.compile(*analysis.program,false);
                    std::cerr<<"Debug cache: "<<jit.cache_status()<<'\n';
                    auto status=code->invoke(argc==4 ? input(argv[3]) : 0);if(application)return status;std::cout<<status<<'\n';return 0;
                }
                auto program = vd::compile_project(selected);
                const auto& source = program.entry; const auto& module = program.module;
                if(module.analysis)for(const auto& warning:module.analysis->warnings())std::cerr<<vd::render_diagnostic(warning)<<'\n';
                if (check) { std::cout << "Source checked successfully\n"; return 0; }
                if(module.application!=application)throw std::invalid_argument(application ? "Application entry requires executable top-level statements or synchronous main() with no parameters and no result or i32; legacy entry retains precedence." : "Application entries require explicit --app.");
                vd::require_source_entry(source, module);
                if (run) {
                    auto value = argc == 4 ? input(argv[3]) : 0;
                    vd::Jit jit; auto code = jit.compile(module, module.entry_symbol, optimize);
                    auto status=code->invoke(value);if(application)return status;std::cout << status << '\n';
                } else {
                    vd::emit_object(module, vd::utf8_path(argv[4]), optimize, true);
                    std::cout << "Wrote native source object: " << argv[4] << '\n';
                }
                return 0;
            }
        }
        std::cerr << "Usage: voidc check <file.void>\n"
                     "       voidc run <file.void> [i32 input] [--no-opt] [--cache] [--app]\n"
                     "       voidc emit <file.void> -o <object> [--no-opt] [--app]\n"
                     "Proof harness: --emit-proof <object> | --jit-proof\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
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
