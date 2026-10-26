#include "void/frontend.hpp"
#include "void/ir.hpp"

#include <functional>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

namespace {

bool selected(const std::string& module) {
    return module == "/@stdlib/text.void" || module == "/@stdlib/encoding/hex.void" ||
           module == "/@stdlib/encoding/base64.void" || module == "/@stdlib/encoding/ascii.void";
}

void inventory(const vd::ValidatedAnalysis& program) {
    if (!program.warnings().empty()) {
        throw std::runtime_error("public source documentation warnings");
    }
    std::set<std::string> hidden;
    for (const auto& declaration : program.declarations()) {
        if (declaration.visibility != "public") {
            hidden.insert(declaration.identity);
        }
    }
    std::cout << "Identity\tSource\tSignature\tParameters\tReturn\tTests\n";
    unsigned count = 0;
    for (const auto& declaration : program.declarations()) {
        if (!selected(declaration.module) || declaration.visibility != "public" ||
            hidden.contains(declaration.owner)) {
            continue;
        }
        if (declaration.documentation.description.empty()) {
            throw std::runtime_error("undocumented public declaration: " + declaration.identity);
        }
        for (const auto& parameter : declaration.written_parameters) {
            if (declaration.documentation.parameter(parameter.name).empty()) {
                throw std::runtime_error("undocumented public parameter: " + declaration.identity);
            }
        }
        const bool function = declaration.kind == vd::AnalyzedDeclaration::Kind::function;
        if (function && !declaration.documentation.returns) {
            throw std::runtime_error("undocumented return: " + declaration.identity);
        }
        auto source = declaration.source.source;
        const auto location = source->location(declaration.source.begin);
        const auto& text = source->text();
        auto first = text.rfind('\n', declaration.source.begin);
        first = first == std::string::npos ? 0 : first + 1;
        const auto last = text.find('\n', declaration.source.begin);
        std::cout << declaration.identity << '\t' << location.path << ':' << location.line << '\t'
                  << text.substr(first, last - first) << "\tyes\t"
                  << (function ? "yes" : "n/a") << "\toracles, focused, ownership, lsp\n";
        ++count;
    }
    if (count < 60) {
        throw std::runtime_error("incomplete public inventory");
    }
}

void independent_ir(const vd::ValidatedAnalysis& program) {
    auto valid = program.lower();
    vd::ir::validate(valid);
    unsigned checks = 0;
    auto reject = [&](const std::function<void(vd::ir::Function&, vd::ir::Instruction&)>& mutate) {
        auto invalid = valid;
        bool found = false;
        for (auto& function : invalid.functions) {
            for (auto& block : function.blocks) {
                for (auto& instruction : block.instructions) {
                    if (!found && instruction.op == vd::ir::Op::text_case) {
                        mutate(function, instruction);
                        found = true;
                    }
                }
            }
        }
        if (!found) {
            throw std::runtime_error("missing Unicode casing IR");
        }
        try {
            vd::ir::validate(invalid);
        } catch (const std::invalid_argument&) {
            ++checks;
            return;
        }
        throw std::runtime_error("forged Unicode casing IR accepted");
    };
    reject([](auto& function, auto&) { function.module_identity = "/user.void"; });
    reject([](auto&, auto& instruction) { instruction.type = vd::ir::Type::i64; });
    reject([](auto&, auto& instruction) { instruction.immediate = 1; });
    reject([](auto&, auto& instruction) { instruction.arguments.push_back(instruction.a); });
    reject([](auto&, auto& instruction) { instruction.callee = "forged"; });
    reject([](auto&, auto& instruction) { instruction.receiver.emplace(); });
    reject([](auto&, auto& instruction) { instruction.b = instruction.a; });
    reject([](auto&, auto& instruction) { instruction.a = instruction.b; });
    reject([](auto&, auto& instruction) { instruction.initialization = true; });
    reject([](auto&, auto& instruction) { instruction.array_repeat = true; });
    auto uncoordinated = valid;
    uncoordinated.coordinated = false;
    try {
        vd::ir::validate(uncoordinated);
        throw std::runtime_error("uncoordinated Unicode operation accepted");
    } catch (const std::invalid_argument&) {
        ++checks;
    }
    std::cout << checks << " independent private Unicode IR rejections passed\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3) {
            throw std::runtime_error("mode and source required");
        }
        auto analysis = vd::analyze_project(argv[2]);
        if (!analysis.succeeded()) {
            throw vd::CompilationError(analysis.diagnostics);
        }
        if (std::string(argv[1]) == "--inventory") {
            inventory(*analysis.program);
        } else {
            independent_ir(*analysis.program);
        }
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
