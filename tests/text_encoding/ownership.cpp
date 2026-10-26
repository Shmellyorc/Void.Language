#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/runtime.hpp"
#include "../managed/probe.hpp"

#include <filesystem>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>

namespace {

unsigned checks = 0;

void require(bool valid, const char* message) {
    if (!valid) {
        throw std::runtime_error(message);
    }
    ++checks;
}

std::size_t collect(vd::Runtime& runtime) {
    vd::Runtime::Attachment attachment(runtime);
    vd::Runtime::Managed managed(runtime);
    require(runtime.collect() == vd::Runtime::Collection::completed, "collection unavailable");
    return runtime.snapshot().objects;
}

unsigned forced_collections = 0;

void collect_at_poll() {
    require(vd::Runtime::current().collect() == vd::Runtime::Collection::completed,
            "forced poll collection unavailable");
    ++forced_collections;
}

struct ForceCollection {
    ForceCollection() {
        managed_test::concurrent_poll.store(collect_at_poll, std::memory_order_release);
    }

    ~ForceCollection() {
        managed_test::concurrent_poll.store(nullptr, std::memory_order_release);
    }
};

void execute(const vd::ValidatedAnalysis& program, bool optimized) {
    vd::Jit jit;
    auto code = jit.compile(program, optimized);
    const auto runtime = code->runtime_owner();
    unsigned failures = 0;
    for (int scenario = 0; scenario < 19; ++scenario) {
        require(code->invoke(100 + scenario) == 1, "setup failed");
        managed_test::Probe forced;
        forced.inspect_roots = true;
        forced.collect_before = true;
        forced.frames.reserve(2048);
        {
            managed_test::Observe observe(forced);
            ForceCollection every_poll;
            require(code->invoke(0) == 1, "forced collection operation failed");
        }
        require(forced.roots_valid && forced.commits_valid && forced.frames.empty(),
                "forced collection damaged roots or frames");
        require(forced.polls > 0 && forced.roots_checked > 0 && forced_collections > 0,
                "no real GC observations");
        require(code->invoke(-1) == 1, "input changed");
        const auto baseline = collect(*runtime);
        bool succeeded = false;
        unsigned rejected = 0;
        for (int budget = 0; budget < 4096 && !succeeded; ++budget) {
            managed_test::Probe attempt;
            attempt.inspect_roots = true;
            attempt.frames.reserve(2048);
            std::optional<std::int32_t> answer;
            {
                managed_test::Observe observe(attempt);
                managed_test::AllocationFailure allocation(budget, true);
                try {
                    answer = code->invoke(0);
                } catch (const std::exception&) {
                    ++rejected;
                }
            }
            require(attempt.roots_valid && attempt.commits_valid && attempt.frames.empty(),
                    "allocation failure damaged roots/publication/frames");
            require(code->invoke(-1) == 1, "allocation failure changed immutable input");
            require(collect(*runtime) == baseline, "temporary retained after collection");
            if (answer) {
                require(*answer == 1, "operation returned incorrect result");
                succeeded = true;
            } else {
                require(code->invoke(0) == 1, "allocation failure poisoned retry");
                require(collect(*runtime) == baseline, "retry retained temporary objects");
            }
        }
        require(succeeded && rejected > 0, "sweep missed success or failures");
        failures += rejected;
        std::cout << (optimized ? "O2" : "normal") << " scenario " << scenario
                  << ": " << rejected << " failed budgets recovered\n";
    }
    runtime->close();
    require(runtime->memory_snapshot().handles == 0, "roots survived shutdown");
    std::cout << (optimized ? "O2" : "normal") << ": " << failures << " allocation failures recovered\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 2, "ownership source required");
        auto analysis = vd::analyze_project(std::filesystem::path(argv[1]));
        if (!analysis.succeeded()) {
            throw vd::CompilationError(analysis.diagnostics);
        }
        require(analysis.program->warnings().empty(), "source documentation warnings");
        for (bool optimized : {false, true}) {
            execute(*analysis.program, optimized);
        }
        std::cout << checks << " ownership/root checks passed\n";
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
