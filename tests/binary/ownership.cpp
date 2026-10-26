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

void collected(vd::Runtime& runtime) {
    vd::Runtime::Attachment attachment(runtime);
    vd::Runtime::Managed managed(runtime);
    check(runtime.collect() == vd::Runtime::Collection::completed,
          "collection was unavailable after binary call");
    check(runtime.snapshot().objects == 0,
          "binary call retained unreachable arrays after unwinding");
}

void documentation(const vd::ValidatedAnalysis& program) {
    check(program.warnings().empty(), "binary module emitted documentation warnings");
    std::set<std::string> public_symbols;

    for (const auto& declaration : program.declarations()) {
        if (declaration.module != "/@stdlib/encoding/binary.void" ||
            declaration.visibility != "public") {
            continue;
        }

        check(!declaration.documentation.description.empty(),
              "public binary declaration has no compiler-owned documentation");
        public_symbols.insert(declaration.identity);

        for (const auto& parameter : declaration.written_parameters) {
            check(!declaration.documentation.parameter(parameter.name).empty(),
                  "public binary parameter has no meaningful description");
        }

        if (declaration.kind == vd::AnalyzedDeclaration::Kind::function) {
            check(declaration.documentation.returns.has_value() &&
                  !declaration.documentation.returns->description.empty(),
                  "public binary function has no return documentation");
        }
    }

    for (const auto* name : {"byte_order", "error", "encode", "decode"}) {
        check(public_symbols.contains(std::string("/@stdlib/encoding/binary.void::") + name),
              "documented binary public API is missing from compiler metadata");
    }
}

void execute(const vd::ValidatedAnalysis& program, bool optimize) {
    vd::Jit jit;
    auto code = jit.compile(program, optimize);
    auto runtime = code->runtime_owner();

    managed_test::Probe probe;
    probe.inspect_roots = true;
    probe.collect_before = true;
    probe.frames.reserve(1024);

    {
        managed_test::Observe observe(probe);
        check(code->invoke(12345) == 12345,
              "binary output lost its value or independent storage under collection");
    }

    check(probe.roots_valid && probe.commits_valid && probe.frames.empty(),
          "binary output corrupted precise roots, publication or frames");
    check(probe.constructions > 0 && probe.roots_checked > 0,
          "binary ownership probe did not observe actual managed arrays");
    collected(*runtime);

    unsigned failures = 0;
    bool succeeded = false;

    for (int budget = 0; budget < 1024 && !succeeded; ++budget) {
        managed_test::Probe attempt;
        attempt.inspect_roots = true;
        attempt.frames.reserve(1024);
        std::optional<std::int32_t> result;

        {
            managed_test::Observe observe(attempt);
            managed_test::AllocationFailure failure(budget, true);

            try {
                result = code->invoke(12345);
            } catch (const std::exception&) {
                ++failures;
            }
        }

        if (result) {
            check(*result == 12345, "allocation sweep returned a partially encoded result");
            succeeded = true;
        }

        check(attempt.roots_valid && attempt.commits_valid && attempt.frames.empty(),
              "allocation failure corrupted binary roots or failure cleanup");
        collected(*runtime);
        check(code->invoke(42) == 42, "binary allocation failure poisoned the next invocation");
        collected(*runtime);
    }

    check(succeeded && failures > 0,
          "binary allocation sweep failed to reach both failure and success");
    check(code->invoke(42) == 42, "binary codec could not recover after allocation failures");
    collected(*runtime);

    runtime->close();
    check(runtime->memory_snapshot().handles == 0,
          "binary output left persistent handles at shutdown");

    std::cout << (optimize ? "O2" : "normal") << ": " << failures
              << " allocation failure budgets, collection and retry passed\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        check(argc == 2, "expected binary ownership source path");
        auto analysis = vd::analyze_project(std::filesystem::path(argv[1]));
        check(analysis.succeeded(), "binary ownership source analysis failed");
        check(analysis.diagnostics.empty(), "binary ownership source produced documentation warnings");
        documentation(*analysis.program);

        for (bool optimize : {false, true}) {
            execute(*analysis.program, optimize);
        }

        std::cout << "Binary ownership: " << checks << " checks passed\n";
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
