#include "void/frontend.hpp"
#include "void/http_ops.hpp"
#include "void/ir.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("HTTP source fixture required");
        auto analysis = vd::analyze_project(argv[1]);
        if (!analysis.succeeded()) throw vd::CompilationError(analysis.diagnostics);
        auto valid = analysis.program->lower(); vd::ir::validate(valid); unsigned checks = 0;
        auto reject = [&](vd::http::SourceOperation operation, const std::function<void(vd::ir::Module&, vd::ir::Function&, vd::ir::Instruction&)>& mutate) {
            auto invalid = valid; bool found = false;
            for (auto& function : invalid.functions) for (auto& block : function.blocks) for (auto& instruction : block.instructions)
                if (!found && (instruction.op == vd::ir::Op::http_native || instruction.op == vd::ir::Op::http_control) &&
                    instruction.immediate == static_cast<unsigned>(operation)) {
                    mutate(invalid, function, instruction); found = true;
                }
            if (!found) throw std::runtime_error("Missing HTTP IR fixture");
            try { vd::ir::validate(invalid); } catch (const std::invalid_argument&) { ++checks; return; }
            throw std::runtime_error("Forged HTTP IR accepted");
        };
        auto start = [&](auto mutate) { reject(vd::http::SourceOperation::start, mutate); };
        start([](auto& m, auto&, auto&) { m.coordinated = false; });
        start([](auto&, auto& f, auto&) { f.module_identity = "/untrusted/net/http.void"; });
        start([](auto&, auto&, auto& i) { i.immediate = -1; });
        start([](auto&, auto&, auto& i) { i.immediate = 999; });
        start([](auto&, auto&, auto& i) { i.arguments.pop_back(); });
        start([](auto&, auto&, auto& i) { i.arguments[0] = i.arguments[1]; });
        start([](auto&, auto&, auto& i) { i.type = vd::ir::Type::i64; });
        start([](auto&, auto&, auto& i) { i.type.readonly = true; });
        start([](auto&, auto&, auto& i) { i.receiver.emplace(); });
        start([](auto&, auto&, auto& i) { i.callee = "untrusted"; });
        start([](auto&, auto&, auto& i) { i.operator_glyph = "+"; });
        start([](auto&, auto&, auto& i) { i.op = vd::ir::Op::http_async; });
        start([](auto&, auto& f, auto&) { f.worker = true; });
        start([](auto&, auto&, auto& i) { i.initialization = true; });
        start([](auto&, auto&, auto& i) { i.array_repeat = true; });
        for (auto operation : {vd::http::SourceOperation::cleanup_client, vd::http::SourceOperation::cleanup_transfer}) {
            reject(operation, [](auto& m, auto&, auto&) { m.coordinated = false; });
            reject(operation, [](auto&, auto& f, auto&) { f.module_identity = "/untrusted/net/http.void"; });
            reject(operation, [](auto&, auto& f, auto&) { f.worker = true; });
            reject(operation, [](auto&, auto&, auto& i) { i.op = vd::ir::Op::http_native; });
            reject(operation, [](auto&, auto&, auto& i) { i.op = vd::ir::Op::http_async; });
            reject(operation, [](auto&, auto&, auto& i) { i.type = vd::ir::Type::i64; });
            reject(operation, [](auto&, auto&, auto& i) { i.arguments.clear(); });
            reject(operation, [](auto&, auto&, auto& i) { i.receiver.emplace(); });
            reject(operation, [](auto&, auto&, auto& i) { i.initialization = true; });
            reject(operation, [](auto&, auto&, auto& i) { i.immediate = static_cast<unsigned>(vd::http::SourceOperation::capabilities); i.arguments.clear(); });
        }
        reject(vd::http::SourceOperation::form, [](auto& m, auto&, auto&) { m.coordinated = false; });
        reject(vd::http::SourceOperation::form, [](auto&, auto& f, auto&) { f.module_identity = "/untrusted/net/http.void"; });
        reject(vd::http::SourceOperation::form, [](auto&, auto& f, auto&) { f.worker = true; });
        reject(vd::http::SourceOperation::form, [](auto&, auto&, auto& i) { i.arguments.clear(); });
        reject(vd::http::SourceOperation::form, [](auto&, auto&, auto& i) { i.a = i.arguments.front(); });
        reject(vd::http::SourceOperation::form, [](auto&, auto&, auto& i) { i.type = vd::ir::Type::i64; });
        reject(vd::http::SourceOperation::form, [](auto&, auto&, auto& i) { i.op = vd::ir::Op::http_control; });
        reject(vd::http::SourceOperation::form, [](auto&, auto&, auto& i) { i.op = vd::ir::Op::http_async; });
        std::cout << checks << " HTTP IR module/type/arity/authority/worker/effect rejections passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
