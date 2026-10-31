#include "void/frontend.hpp"
#include "void/ir.hpp"
#include "void/url_ops.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("URL source fixture required");
        auto analysis = vd::analyze_project(argv[1]);
        if (!analysis.succeeded()) throw vd::CompilationError(analysis.diagnostics);
        auto valid = analysis.program->lower();
        vd::ir::validate(valid);
        unsigned checks = 0;
        auto reject = [&](const std::function<void(vd::ir::Function&, vd::ir::Instruction&)>& mutate) {
            auto invalid = valid;
            bool found = false;
            for (auto& function : invalid.functions) for (auto& block : function.blocks) for (auto& instruction : block.instructions)
                if (!found && instruction.op == vd::ir::Op::url_native && instruction.arguments.size() > 1 &&
                    vd::url::source_operations.at(instruction.immediate).arguments[0] !=
                    vd::url::source_operations.at(instruction.immediate).arguments[1]) {
                    mutate(function, instruction); found = true;
                }
            if (!found) throw std::runtime_error("Missing URL IR fixture");
            try { vd::ir::validate(invalid); }
            catch (const std::invalid_argument&) { ++checks; return; }
            throw std::runtime_error("Forged URL IR accepted");
        };
        reject([](auto& f, auto&) { f.module_identity = "/ordinary/url.void"; });
        reject([](auto&, auto& i) { i.immediate = -1; });
        reject([](auto&, auto& i) { i.immediate = 999; });
        reject([](auto&, auto& i) { i.arguments.pop_back(); });
        reject([](auto&, auto& i) { i.arguments[1] = i.arguments[0]; });
        reject([](auto&, auto& i) { i.receiver.emplace(); });
        reject([](auto&, auto& i) { i.callee = "ordinary"; });
        reject([](auto&, auto& i) { i.type = vd::ir::Type::i64; });
        reject([](auto&, auto& i) { i.type.readonly = true; });
        reject([](auto&, auto& i) { i.op = vd::ir::Op::filesystem_native; });
        std::cout << checks << " URL IR authority/type/arity/opcode rejections passed\n";
        return 0;
    } catch (const std::exception& failure) { std::cerr << failure.what() << '\n'; return 1; }
}
