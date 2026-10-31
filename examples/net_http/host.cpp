#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/runtime.hpp"
#include <iostream>

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("Supply streaming_async.void or cancellation.void");
        auto analysis = vd::analyze_project(argv[1]);
        if (!analysis.succeeded()) throw vd::CompilationError(analysis.diagnostics);
        auto module = analysis.program->lower();
        module.async_root = true;
        auto runtime = std::make_shared<vd::Runtime>();
        runtime->async_worker_limit(1);
        vd::Jit jit;
        auto code = jit.compile(module, module.entry_symbol, true, {}, runtime);
        return code->invoke(0);
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
