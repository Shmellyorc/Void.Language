#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/source.hpp"

#include <filesystem>
#include <iostream>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    try {
        if (argc < 3) throw std::runtime_error("mode and source required");
        auto analysis = vd::analyze_project(std::filesystem::path(argv[2]));
        if (!analysis.succeeded()) throw vd::CompilationError(analysis.diagnostics);
        if (!analysis.program->warnings().empty()) throw std::runtime_error("Unexpected source documentation warning");
        std::string mode = argv[1];
        if (mode == "--inventory" || mode == "--inventory-log") {
            std::set<std::string> hidden;
            for (const auto& declaration : analysis.program->declarations()) {
                if (declaration.visibility != "public") hidden.insert(declaration.identity);
            }
            std::cout << "Identity\tSource\tSignature\tParameters documented\tReturn documented\n";
            for (const auto& declaration : analysis.program->declarations()) {
                const bool selected = mode == "--inventory-log" ? declaration.module == "/@stdlib/log.void" :
                    (declaration.module == "/@stdlib/io.void" || declaration.module == "/@stdlib/path.void");
                if (!selected || declaration.visibility != "public" || hidden.contains(declaration.owner)) continue;
                bool parameters = true;
                for (const auto& parameter : declaration.written_parameters) {
                    parameters &= !declaration.documentation.parameter(parameter.name).empty();
                }
                auto snapshot = declaration.source.source;
                auto location = snapshot->location(declaration.source.begin);
                const auto& text = snapshot->text();
                auto first = text.rfind('\n', declaration.source.begin);
                first = first == std::string::npos ? 0 : first + 1;
                auto last = text.find('\n', declaration.source.begin);
                auto header = text.substr(first, last - first);
                auto indentation = header.find_first_not_of(' ');
                if (indentation != std::string::npos) header.erase(0, indentation);
                std::cout << declaration.identity << '\t' << location.path << ':' << location.line
                          << '\t' << header << '\t'
                          << (parameters ? "yes" : "NO") << '\t'
                          << (declaration.documentation.returns ? "yes" : "n/a") << '\n';
            }
            return 0;
        }
        if (mode == "--emit-probe") {
            if (argc != 5) throw std::runtime_error("source, output and optimization required");
            auto module = analysis.program->lower();
            module.async_root = true;
            vd::emit_runtime_object(module, argv[3], std::string(argv[4]) == "optimized");
            std::string symbol = "__vd_";
            constexpr char digits[] = "0123456789abcdef";
            for (unsigned char byte : module.entry_symbol) {
                symbol += digits[byte >> 4];
                symbol += digits[byte & 15];
            }
            std::ofstream adapter(std::string(argv[3]) + ".cpp");
            adapter << "#include \"void/execution.hpp\"\n"
                    << "extern \"C\" std::int32_t " << symbol
                    << "(std::int32_t, vd::RuntimeFailure*);\n"
                    << "extern \"C\" std::int32_t io_probe_entry(std::int32_t input, vd::RuntimeFailure* failure) {\n"
                    << "    return " << symbol << "(input, failure);\n}\n";
            if (!adapter) throw std::runtime_error("Native probe adapter write failed");
            return 0;
        }
        if (mode == "--emit" || mode == "--emit-async") {
            if (argc != 5) throw std::runtime_error("source, output and optimization required");
            bool optimized = std::string(argv[4]) == "optimized";
            if (mode == "--emit-async") vd::emit_async_object(analysis.program->lower(), argv[3], optimized);
            else vd::emit_object(analysis.program->lower(), argv[3], optimized);
            return 0;
        }
        if (mode != "--async" && mode != "--sync") throw std::runtime_error("Unknown runner mode");
        for (bool optimized : {false, true}) {
            vd::Jit jit;
            auto code = mode == "--async" ? jit.compile_async(*analysis.program, optimized) :
                                           jit.compile(*analysis.program, optimized);
            auto answer = code->invoke(7);
            if (answer != 7) throw std::runtime_error("Expected 7, received " + std::to_string(answer));
            std::cout << (optimized ? "O2" : "Debug") << " source execution passed\n";
        }
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
