#include "void/frontend.hpp"
#include "void/filesystem_ops.hpp"
#include "void/ir.hpp"

#include <functional>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("I/O source fixture required");
        auto analysis = vd::analyze_project(argv[1]);
        if (!analysis.succeeded()) throw vd::CompilationError(analysis.diagnostics);
        auto valid = analysis.program->lower();
        vd::ir::validate(valid);
        unsigned checks = 0;
        auto reject = [&](vd::ir::Op selected,
                          const std::function<void(vd::ir::Function&, vd::ir::Instruction&)>& mutate) {
            auto invalid = valid;
            bool found = false;
            for (auto& function : invalid.functions) {
                for (auto& block : function.blocks) {
                    for (auto& instruction : block.instructions) {
                        if (!found && instruction.op == selected) {
                            mutate(function, instruction);
                            found = true;
                        }
                    }
                }
            }
            if (!found) throw std::runtime_error("Missing native filesystem IR fixture");
            try {
                vd::ir::validate(invalid);
            } catch (const std::invalid_argument&) {
                ++checks;
                return;
            }
            throw std::runtime_error("Forged native filesystem IR was accepted");
        };
        using vd::ir::Op;
        reject(Op::filesystem_native, [](auto& function, auto&) {
            function.module_identity = "/ordinary-user.void";
        });
        reject(Op::filesystem_native, [](auto&, auto& instruction) { instruction.immediate = -1; });
        reject(Op::filesystem_native, [](auto&, auto& instruction) { instruction.immediate = 99999; });
        reject(Op::filesystem_native, [](auto&, auto& instruction) { instruction.arguments.pop_back(); });
        reject(Op::filesystem_native, [](auto&, auto& instruction) { instruction.receiver.emplace(); });
        reject(Op::filesystem_native, [](auto&, auto& instruction) { instruction.callee = "ordinary"; });
        reject(Op::filesystem_native, [](auto&, auto& instruction) { instruction.type = vd::ir::Type::i64; });
        reject(Op::filesystem_native, [](auto&, auto& instruction) { instruction.type.readonly = true; });
        reject(Op::filesystem_native, [](auto&, auto& instruction) { instruction.op = Op::filesystem_async; });
        reject(Op::filesystem_control, [](auto&, auto& instruction) { instruction.immediate = 0; });
        reject(Op::filesystem_control, [](auto&, auto& instruction) { instruction.op = Op::filesystem_native; });
        std::cout << checks << " independent filesystem IR authority, arity, reply and opcode rejections passed\n";
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
