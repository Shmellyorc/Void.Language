#include "void/backend.hpp"
#include "void/frontend.hpp"
#include <chrono>
#include <ctime>
#include <iostream>
#include <stdexcept>
#if defined(__unix__)
#include <sys/resource.h>
#endif
int main(int argc, char** argv) {
    try {
        if (argc != 5) throw std::runtime_error("source, iterations, checksum per iteration and samples required");
        const int iterations = std::stoi(argv[2]), expected = std::stoi(argv[3]), samples = std::stoi(argv[4]);
        const auto start = std::chrono::steady_clock::now(); const auto cpu = std::clock();
        auto analysis = vd::analyze_project(argv[1]);
        if (!analysis.succeeded()) throw vd::CompilationError(analysis.diagnostics);
        vd::Jit jit; auto code = jit.compile(*analysis.program, true);
        const double compile_wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        const double compile_cpu = double(std::clock() - cpu) / CLOCKS_PER_SEC;
        code->invoke(0);
        if (code->invoke(2) != expected * 2) throw std::runtime_error("benchmark warmup checksum failed");
        std::cout << "{\"compile_wall\":" << compile_wall << ",\"compile_cpu\":" << compile_cpu
                  << ",\"cache\":\"disabled\",\"iterations\":" << iterations << ",\"samples\":[";
        for (int sample = 0; sample < samples; ++sample) {
            const auto before = std::chrono::steady_clock::now(); const auto cpu_before = std::clock();
            const int checksum = code->invoke(iterations);
            const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - before).count();
            const double cpu_elapsed = double(std::clock() - cpu_before) / CLOCKS_PER_SEC;
            if (checksum != expected * iterations) throw std::runtime_error("benchmark measured checksum failed");
            long rss = 0;
#if defined(__unix__)
            rusage usage{}; getrusage(RUSAGE_SELF, &usage); rss = usage.ru_maxrss;
#if defined(__APPLE__)
            rss /= 1024;
#endif
#endif
            if (sample) std::cout << ',';
            std::cout << "{\"wall\":" << elapsed << ",\"cpu\":" << cpu_elapsed << ",\"process_peak_rss_kib\":" << rss
                      << ",\"checksum\":" << checksum << '}';
        }
        std::cout << "]}\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
