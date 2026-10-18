#include "void/backend.hpp"
#include "void/frontend.hpp"
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <limits>
namespace {
std::int32_t input(const char* text) {
    char* end = nullptr; errno = 0;
    auto value = std::strtoll(text, &end, 10);
    if (end == text || *end || errno == ERANGE || value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::int32_t>::max())
        throw std::invalid_argument("Expected a decimal i32 input (-2147483648 through 2147483647).");
    return static_cast<std::int32_t>(value);
}
}
int main(int argc, char** argv) {
    try {
        bool optimize = true;
        if (argc > 1 && std::string(argv[argc - 1]) == "--no-opt") { optimize = false; --argc; }
        if (argc == 3 && std::string(argv[1]) == "--emit-proof") {
            vd::emit_object(vd::ir::architecture_proof(), argv[2], true);
            std::cout << "Wrote native architecture proof object\n"; return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--jit-proof") {
            vd::Jit jit; auto code = jit.compile(vd::ir::architecture_proof(), "entry");
            auto result = code->invoke(0);
            std::cout << result << '\n'; return result == 42 ? 0 : 1;
        }
        if (argc >= 3) {
            std::string command = argv[1];
            bool check = command == "check" && argc == 3;
            bool run = command == "run" && (argc == 3 || argc == 4);
            bool emit = command == "emit" && argc == 5 && std::string(argv[3]) == "-o";
            if (check || run || emit) {
                auto program = vd::compile_project(argv[2]);
                const auto& source = program.entry; const auto& module = program.module;
                if(module.analysis)for(const auto& warning:module.analysis->warnings())std::cerr<<vd::render_diagnostic(warning)<<'\n';
                if (check) { std::cout << "Source checked successfully\n"; return 0; }
                vd::require_source_entry(source, module);
                if (run) {
                    auto value = argc == 4 ? input(argv[3]) : 0;
                    vd::Jit jit; auto code = jit.compile(module, module.entry_symbol, optimize);
                    std::cout << code->invoke(value) << '\n';
                } else {
                    vd::emit_object(module, argv[4], optimize, true);
                    std::cout << "Wrote native source object: " << argv[4] << '\n';
                }
                return 0;
            }
        }
        std::cerr << "Usage: voidc check <file.void>\n"
                     "       voidc run <file.void> [i32 input] [--no-opt]\n"
                     "       voidc emit <file.void> -o <object> [--no-opt]\n"
                     "Proof harness: --emit-proof <object> | --jit-proof\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
