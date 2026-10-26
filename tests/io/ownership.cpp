#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/runtime.hpp"
#include "../managed/probe.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

namespace {
unsigned checks = 0;
void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
    ++checks;
}

void collect(vd::Runtime& runtime, const std::string& context = "successful execution") {
    vd::Runtime::Attachment attachment(runtime);
    vd::Runtime::Managed managed(runtime);
    require(runtime.collect() == vd::Runtime::Collection::completed, "I/O collection unavailable");
    require(runtime.snapshot().objects == 0, "I/O retained unreachable roots after " + context);
}

void documentation(const vd::ValidatedAnalysis& analysis) {
    require(analysis.warnings().empty(), "I/O public documentation warnings");
    std::set<std::string> hidden;
    for (const auto& declaration : analysis.declarations()) {
        if (declaration.visibility != "public") hidden.insert(declaration.identity);
    }
    unsigned declarations = 0;
    for (const auto& declaration : analysis.declarations()) {
        if ((declaration.module != "/@stdlib/io.void" && declaration.module != "/@stdlib/path.void") ||
            declaration.visibility != "public" || hidden.contains(declaration.owner)) continue;
        ++declarations;
        require(!declaration.documentation.description.empty(), "Missing documentation: " + declaration.identity);
        for (const auto& parameter : declaration.written_parameters) {
            require(!declaration.documentation.parameter(parameter.name).empty(),
                    "Missing parameter documentation: " + declaration.identity + " / " + parameter.name);
        }
        if ((declaration.kind == vd::AnalyzedDeclaration::Kind::function ||
             declaration.kind == vd::AnalyzedDeclaration::Kind::receiver_method) &&
            declaration.type != vd::ir::Type::none) {
            require(declaration.documentation.returns &&
                    !declaration.documentation.returns->description.empty(),
                    "Missing return documentation: " + declaration.identity);
        }
        if (declaration.identity == "/@stdlib/io.void::error") {
            require(declaration.base_identity == "/@stdlib/error.void::error", "I/O managed error ancestry");
        }
    }
    require(declarations > 120, "Incomplete I/O public documentation inventory");
    std::cout << declarations << " I/O/path public declarations documented\n";
}

void execute(const vd::ValidatedAnalysis& analysis, bool asynchronous, bool optimize) {
    vd::Jit jit;
    auto code = asynchronous ? jit.compile_async(analysis, optimize) : jit.compile(analysis, optimize);
    auto runtime = code->runtime_owner();
    {
        managed_test::Probe probe;
        probe.collect_before = true;
        probe.inspect_roots = true;
        probe.frames.reserve(8192);
        managed_test::Observe observe(probe);
        require(code->invoke(7) == 7, "I/O forced-GC result");
        require(probe.roots_valid && probe.commits_valid && probe.frames.empty(), "I/O precise root/commit balance");
    }
    collect(*runtime);
    unsigned failures = 0;
    bool succeeded = false;
    for (int budget = 0; budget < 4096 && !succeeded; ++budget) {
        managed_test::Probe probe;
        probe.inspect_roots = true;
        probe.frames.reserve(8192);
        {
            managed_test::Observe observe(probe);
            try {
                managed_test::AllocationFailure injection(budget);
                require(code->invoke(7) == 7, "I/O allocation recovery result");
                succeeded = true;
            } catch (const std::bad_alloc&) {
                ++failures;
            } catch (const std::runtime_error&) {
                ++failures;
            }
        }
        require(probe.roots_valid && probe.commits_valid && probe.frames.empty(), "I/O allocation failure root imbalance");
        collect(*runtime, std::string(asynchronous ? "async" : "sync") +
                " allocation budget " + std::to_string(budget));
    }
    require(succeeded && failures != 0, "I/O allocation sweep did not reach a successful boundary");
    require(code->invoke(7) == 7, "I/O recovery after allocation sweep");
    collect(*runtime);
    std::cout << (asynchronous ? "async " : "sync ") << (optimize ? "O2" : "Debug")
              << ": " << failures << " checked allocation failures recovered\n";
}
}

int main(int argc, char** argv) {
    try {
        require(argc == 3, "sync and async source required");
        for (int argument = 1; argument != 3; ++argument) {
            auto analysis = vd::analyze_project(argv[argument]);
            if (!analysis.succeeded()) throw vd::CompilationError(analysis.diagnostics);
            documentation(*analysis.program);
            for (bool optimize : {false, true}) execute(*analysis.program, argument == 2, optimize);
        }
        std::filesystem::remove("io ownership.bin");
        std::cout << checks << " I/O ownership/documentation checks passed\n";
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
