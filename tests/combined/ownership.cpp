#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/runtime.hpp"
#include "../managed/probe.hpp"

#include <filesystem>
#include <iostream>
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

void collect(vd::Runtime& runtime) {
    vd::Runtime::Attachment attachment(runtime);
    vd::Runtime::Managed managed(runtime);
    check(runtime.collect() == vd::Runtime::Collection::completed, "collection unavailable");
    check(runtime.snapshot().objects == 0, "combined operation retained unreachable objects");
}

void documentation(const vd::ValidatedAnalysis& program) {
    check(program.warnings().empty(), "standard library emitted documentation warnings");
    std::set<std::string> hidden;
    for (const auto& declaration : program.declarations()) {
        if (declaration.visibility != "public") {
            hidden.insert(declaration.identity);
        }
    }
    unsigned declarations = 0;
    unsigned derived_errors = 0;
    bool canonical_error = false;
    for (const auto& declaration : program.declarations()) {
        if (!declaration.module.starts_with("/@stdlib/") || declaration.visibility != "public" ||
            hidden.contains(declaration.owner)) {
            continue;
        }
        if (declaration.identity == "/@stdlib/error.void::error") {
            const auto& fields = declaration.type.definition->fields;
            check(declaration.type.kind == vd::ir::Type::managed_ref && fields.size() == 3,
                  "canonical error must have exactly three managed fields");
            check(fields[0].name == "message" && vd::ir::Type(fields[0].type) == vd::ir::Type::string &&
                  fields[1].name == "path" && vd::ir::Type(fields[1].type) == vd::ir::Type::string &&
                  fields[2].name == "code" && vd::ir::Type(fields[2].type) == vd::ir::Type::i32,
                  "canonical error field contract changed");
            canonical_error = true;
        }
        if (declaration.kind == vd::AnalyzedDeclaration::Kind::managed_structure &&
            declaration.identity != "/@stdlib/error.void::error" &&
            (declaration.identity.ends_with("::error") || declaration.identity.ends_with("::io_error"))) {
            check(declaration.base_identity == "/@stdlib/error.void::error",
                  "installed public error does not derive from the canonical base");
            ++derived_errors;
        }
        ++declarations;
        if (declaration.documentation.description.empty()) {
            throw std::runtime_error("Missing public documentation: " + declaration.identity);
        }
        for (const auto& parameter : declaration.written_parameters) {
            if (declaration.documentation.parameter(parameter.name).empty()) {
                throw std::runtime_error("Missing parameter documentation: " + declaration.identity +
                                         " / " + parameter.name);
            }
            ++checks;
        }
        if ((declaration.kind == vd::AnalyzedDeclaration::Kind::receiver_method ||
             declaration.kind == vd::AnalyzedDeclaration::Kind::function) &&
            declaration.type != vd::ir::Type::none) {
            check(declaration.documentation.returns.has_value() &&
                  !declaration.documentation.returns->description.empty(),
                  "missing public return documentation");
        }
        ++checks;
    }
    check(canonical_error && derived_errors == 5, "installed error hierarchy audit was incomplete");
    check(declarations > 190, "installed public declaration audit was incomplete");
    std::cout << declarations << " installed public declarations documented\n";
}

void execute(const vd::ValidatedAnalysis& program, bool optimize) {
    vd::Jit jit;
    auto code = jit.compile(program, optimize);
    auto runtime = code->runtime_owner();
    unsigned failures = 0;
    for (int scenario = 0; scenario < 7; ++scenario) {
        managed_test::Probe probe;
        probe.inspect_roots = true;
        probe.collect_before = true;
        probe.frames.reserve(4096);
        {
            managed_test::Observe observe(probe);
            check(code->invoke(scenario) == 1, "derived error/unwrap ownership failure");
        }
        check(probe.roots_valid && probe.commits_valid && probe.frames.empty(),
              "forced collection corrupted roots or managed publication");
        check(probe.roots_checked > 0, "forced collection did not inspect roots");
        collect(*runtime);
        bool succeeded = false;
        for (int budget = 0; budget < 2048 && !succeeded; ++budget) {
            managed_test::Probe attempt;
            attempt.inspect_roots = true;
            attempt.frames.reserve(4096);
            {
                managed_test::Observe observe(attempt);
                managed_test::AllocationFailure fault(budget, true);
                try {
                    succeeded = code->invoke(scenario) == 1;
                } catch (const std::exception&) {
                    ++failures;
                }
            }
            check(attempt.roots_valid && attempt.commits_valid && attempt.frames.empty(),
                  "allocation failure corrupted roots or publication");
            collect(*runtime);
            check(code->invoke(scenario) == 1, "allocation failure poisoned retry");
            collect(*runtime);
        }
        check(succeeded, "allocation sweep never reached success");
    }
    std::cout << (optimize ? "O2" : "normal") << ": " << failures
              << " allocation failure budgets passed across all derived errors and unwrap\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 5 && std::string(argv[1]) == "--emit-async") {
            auto analysis = vd::analyze_project(argv[2]);
            if (!analysis.succeeded()) {
                throw vd::CompilationError(analysis.diagnostics);
            }
            vd::emit_async_object(analysis.program->lower(), argv[3],
                                  std::string(argv[4]) == "optimized");
            return 0;
        }
        if (argc != 3) {
            throw std::invalid_argument("expected ownership and async fixtures");
        }
        auto analysis = vd::analyze_project(argv[1]);
        if (!analysis.succeeded()) {
            throw vd::CompilationError(analysis.diagnostics);
        }
        documentation(*analysis.program);
        for (bool optimize : {false, true}) {
            execute(*analysis.program, optimize);
        }
        auto asynchronous = vd::analyze_project(argv[2]);
        if (!asynchronous.succeeded()) {
            throw vd::CompilationError(asynchronous.diagnostics);
        }
        for (bool optimize : {false, true}) {
            vd::Jit jit;
            auto code = jit.compile_async(*asynchronous.program, optimize);
            managed_test::Probe probe;
            probe.inspect_roots = true;
            probe.collect_before = true;
            probe.frames.reserve(4096);
            {
                managed_test::Observe observe(probe);
                check(code->invoke(7) == 7,
                      "managed async errors lost repeated-await or propagation state");
            }
            check(probe.roots_valid && probe.commits_valid && probe.frames.empty() &&
                  probe.roots_checked > 0, "async error collection corrupted precise roots");
            collect(*code->runtime_owner());
        }
        std::cout << "Combined ownership/documentation: " << checks << " checks passed\n";
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
