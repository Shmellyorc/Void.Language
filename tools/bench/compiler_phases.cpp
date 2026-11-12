// In-process phase control for the frozen compiler_project workload.
#include "void/backend.hpp"
#include "void/frontend.hpp"

#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: compiler_phases PROJECT SCRATCH REPETITIONS\n";
        return 2;
    }
    using Clock = std::chrono::steady_clock;
    auto elapsed = [](Clock::time_point start) {
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    };
    const std::filesystem::path project = argv[1], scratch = argv[2];
    const int repetitions = std::stoi(argv[3]);
    if (repetitions < 1 || !std::filesystem::is_directory(scratch) ||
        !std::filesystem::is_empty(scratch)) {
        std::cerr << "Use an empty scratch directory and a positive repetition count.\n";
        return 2;
    }
    std::cout << "validated_analysis_ms,typed_ir_ms,emit_normal_ms,emit_o2_ms\n";
    for (int sample = 0; sample < repetitions; ++sample) {
        auto start = Clock::now();
        auto analysis = vd::analyze_manifest_project(project);
        const auto analysis_ms = elapsed(start);
        if (!analysis.succeeded()) {
            for (const auto& diagnostic : analysis.diagnostics)
                std::cerr << vd::render_diagnostic(diagnostic) << '\n';
            return 1;
        }
        start = Clock::now();
        const auto ir = analysis.project->program()->lower();
        const auto ir_ms = elapsed(start);
        std::array<double, 2> emission{};
        for (int offset = 0; offset < 2; ++offset) {
            const int optimized = (sample + offset) % 2;
            const auto output = scratch / (optimized ? "phase-o2.o" : "phase-normal.o");
            std::filesystem::remove(output);
            start = Clock::now();
            vd::emit_object(ir, output, optimized != 0, true);
            emission[optimized] = elapsed(start);
            if (std::filesystem::file_size(output) == 0) return 1;
        }
        std::cout << analysis_ms << ',' << ir_ms << ',' << emission[0] << ',' << emission[1] << '\n';
    }
}
