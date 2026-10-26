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

void check(bool valid, const char* message) {
    if (!valid) {
        throw std::runtime_error(message);
    }

    ++checks;
}

std::size_t collected(vd::Runtime& runtime) {
    vd::Runtime::Attachment attachment(runtime);
    vd::Runtime::Managed managed(runtime);
    check(runtime.collect() == vd::Runtime::Collection::completed,
          "memory stream collection was unavailable");
    return runtime.snapshot().objects;
}

void documentation(const vd::ValidatedAnalysis& program) {
    check(program.warnings().empty(), "memory module emitted documentation warnings");
    unsigned public_declarations = 0;
    std::set<std::string> private_owners;

    for (const auto& declaration : program.declarations()) {
        if (declaration.module == "/@stdlib/io/memory.void" && declaration.visibility == "private") {
            private_owners.insert(declaration.identity);
        }
    }

    for (const auto& declaration : program.declarations()) {
        if (declaration.module != "/@stdlib/io/memory.void" ||
            declaration.visibility != "public" || private_owners.contains(declaration.owner)) {
            continue;
        }

        ++public_declarations;
        check(!declaration.documentation.description.empty(),
              "public memory declaration has no documentation");

        for (const auto& parameter : declaration.written_parameters) {
            check(!declaration.documentation.parameter(parameter.name).empty(),
                  "public memory parameter has no description");
        }

        if (declaration.kind == vd::AnalyzedDeclaration::Kind::receiver_method) {
            check(declaration.documentation.returns.has_value() &&
                  !declaration.documentation.returns->description.empty(),
                  "public memory method/property has no return documentation");
        }
    }

    check(public_declarations >= 18, "memory public metadata audit missed declarations");
}

void execute(const vd::ValidatedAnalysis& program, bool optimize) {
    vd::Jit jit;
    auto code = jit.compile(program, optimize);
    auto runtime = code->runtime_owner();
    unsigned total_failures = 0;

    for (int scenario = 0; scenario < 12; ++scenario) {
        check(code->invoke(100 + scenario) == 1, "memory setup failed");

        managed_test::Probe collection;
        collection.inspect_roots = true;
        collection.collect_before = true;
        collection.frames.reserve(2048);

        {
            managed_test::Observe observe(collection);
            check(code->invoke(0) == 1, "memory operation failed under forced collection");
            code->invoke(-3);
        }

        check(code->invoke(-2) == 1, "memory operation committed incorrect state");
        check(collection.roots_valid && collection.commits_valid && collection.frames.empty(),
              "memory forced collection corrupted roots, publication or frames");
        check(collection.constructions > 0 && collection.roots_checked > 0,
              "memory GC probe did not observe actual managed state");

        check(code->invoke(100 + scenario) == 1, "memory baseline reset failed");
        check(code->invoke(-1) == 1, "memory baseline state check failed");
        const auto baseline = collected(*runtime);
        check(baseline > 0, "memory global roots did not retain the active stream");
        unsigned failures = 0;
        bool succeeded = false;

        for (int budget = 0; budget < 2048 && !succeeded; ++budget) {
            managed_test::Probe attempt;
            attempt.inspect_roots = true;
            attempt.frames.reserve(2048);
            std::optional<std::int32_t> result;

            {
                managed_test::Observe observe(attempt);
                managed_test::AllocationFailure fault(budget, true);

                try {
                    result = code->invoke(0);
                } catch (const std::exception&) {
                    ++failures;
                }
            }

            check(attempt.roots_valid && attempt.commits_valid && attempt.frames.empty(),
                  "memory allocation failure corrupted precise roots or frames");

            if (result) {
                check(*result == 1, "memory operation returned a partial or failed result");
                check(code->invoke(-2) == 1, "successful memory publication was incorrect");
                succeeded = true;
            } else {
                if (code->invoke(-1) != 1) {
                    throw std::runtime_error("failed memory operation changed state: scenario " +
                                             std::to_string(scenario) + ", budget " + std::to_string(budget));
                }

                ++checks;
                check(collected(*runtime) == baseline,
                      "failed memory operation retained abandoned buffers or state");
                check(code->invoke(0) == 1, "memory allocation failure poisoned retry");
                check(code->invoke(-2) == 1, "memory retry committed incorrect bytes or position");
            }

            check(code->invoke(100 + scenario) == 1, "memory fault sweep reset failed");
            check(collected(*runtime) == baseline,
                  "memory operation retained unreachable temporary objects after reset");
        }

        check(succeeded && failures > 0,
              "memory fault sweep failed to reach both resource failure and success");
        total_failures += failures;
        std::cout << (optimize ? "O2" : "normal") << " scenario " << scenario
                  << ": " << failures << " allocation failure budgets passed\n";
    }

    runtime->close();
    check(runtime->memory_snapshot().handles == 0,
          "memory stream roots survived Runtime shutdown");
    std::cout << (optimize ? "O2" : "normal") << ": " << total_failures
              << " failure budgets across 12 operations; atomic state and retry passed\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        check(argc == 2, "expected memory ownership source path");
        auto analysis = vd::analyze_project(std::filesystem::path(argv[1]));
        if (!analysis.succeeded()) {
            throw vd::CompilationError(analysis.diagnostics);
        }

        check(analysis.diagnostics.empty(), "memory source produced diagnostics");
        documentation(*analysis.program);

        for (bool optimize : {false, true}) {
            execute(*analysis.program, optimize);
        }

        std::cout << "Memory ownership: " << checks << " checks passed\n";
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
