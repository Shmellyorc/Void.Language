#include "void/backend.hpp"
#include "void/frontend.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <vector>

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            throw std::runtime_error("performance source required");
        }
        auto analysis = vd::analyze_project(argv[1]);
        if (!analysis.succeeded()) {
            throw vd::CompilationError(analysis.diagnostics);
        }
        std::cout << "mode,input_bytes,median_ms,samples\n";
        for (bool optimized : {false, true}) {
            vd::Jit jit;
            auto code = jit.compile(*analysis.program, optimized);
            for (int count : {256, 1024, 4096, 16384}) {
                std::vector<double> samples;
                for (unsigned repetition = 0; repetition < 3; ++repetition) {
                    const auto start = std::chrono::steady_clock::now();
                    if (code->invoke(count) != 1) {
                        throw std::runtime_error("benchmark correctness invariant failed");
                    }
                    samples.push_back(std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - start).count());
                }
                std::sort(samples.begin(), samples.end());
                std::cout << (optimized ? "O2" : "normal") << ',' << count * 4 << ','
                          << samples[1] << ',' << samples.size() << '\n';
            }
        }
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
