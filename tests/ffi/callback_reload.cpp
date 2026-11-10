#include "void/backend.hpp"
#include "void/c_primitive.hpp"
#include "void/frontend.hpp"
#include "void/runtime.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using Path = std::filesystem::path;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void expect_failure(vd::ReloadSlot& slot, int input, std::string_view message) {
    try { (void)slot.invoke(input); }
    catch (const std::runtime_error& error) {
        if (std::string_view(error.what()).find(message) != std::string_view::npos) return;
        throw;
    }
    throw std::runtime_error("Expected checked failure: " + std::string(message));
}

void write(const Path& path, const std::string& data) {
    std::ofstream output(path, std::ios::binary);
    output << data;
    require(bool(output), "write callback reload fixture");
}

auto analyze(const Path& entry, const vd::CPrimitiveAdmission::Owner& contract) {
    auto analysis = vd::analyze_project(entry, contract);
    if (!analysis.succeeded()) throw vd::CompilationError(analysis.diagnostics);
    return analysis.program;
}

void check_ir(const vd::ValidatedAnalysis& analysis) {
    auto reject = [&](auto mutate, const std::string& diagnostic) {
        auto module = analysis.lower();
        mutate(module);
        try { vd::ir::validate(module); }
        catch (const std::invalid_argument& error) {
            require(std::string(error.what()).find(diagnostic) != std::string::npos, error.what());
            return;
        }
        throw std::runtime_error("forged C callback IR accepted");
    };
    reject([](auto& module) { module.imports.emplace_back(vd::c_callback_bridge); }, "requires host admission");
    reject([](auto& module) {
        for (auto& function : module.functions) for (auto& block : function.blocks)
            for (auto& instruction : block.instructions)
                if (instruction.op == vd::ir::Op::call &&
                    instruction.callee.starts_with("/callback_checked.void::register")) {
                    for (unsigned n = instruction.arguments.front() + 1; n < block.instructions.size(); ++n)
                        if (&block.instructions[n] == &instruction) break;
                        else if (block.instructions[n].op == vd::ir::Op::load) {
                            instruction.arguments.front() = n;
                            return;
                        }
                }
        throw std::runtime_error("callback invocation missing a rooted reload control");
    }, "generation-local");
}

void qualify(const Path& entry, const Path& native, const std::string& provider, const std::string& adapter,
             const std::string& descriptor, const std::string& original, bool optimize) {
    write(entry, original);
    auto contract = vd::CPrimitiveAdmission::admit_conventional(descriptor, native, provider, adapter);
    auto before = analyze(entry, contract);
    check_ir(*before);
    vd::Jit jit;
    auto old = jit.compile(*before, optimize);
    auto runtime = old->runtime_owner();
    vd::ReloadSlot slot(old);
    require(slot.invoke(0) == 1, "register original generation");

    auto edited = original;
    const std::string result = "return s.bias + value";
    const auto position = edited.find(result);
    require(position != std::string::npos, "fixture callback body");
    edited.replace(position, result.size(), result + " + 100");
    write(entry, edited);
    auto replacement = analyze(entry, contract);
    require(jit.replace(slot, *replacement, optimize), "compatible callback body reload rejected");
    old.reset();
    require(jit.live_generations() == 2, "registration lost its original ORC generation");
    {
        vd::Runtime::Attachment attachment(*runtime);
        vd::Runtime::Managed managed(*runtime);
        require(runtime->collect() == vd::Runtime::Collection::completed, "collect while retained");
    }
    require(slot.invoke(2) == 42, "old callback dispatched to the replacement body");
    auto incompatible = descriptor;
    const auto found = incompatible.find("callback integer context-first i32 i32");
    require(found != std::string::npos, "fixture callback signature");
    incompatible.replace(found, std::string("callback integer context-first i32 i32").size(),
                         "callback integer context-first c_int c_int");
    auto mismatched = vd::CPrimitiveAdmission::admit_conventional(incompatible, native, provider, adapter);
    require(!jit.replace(slot, *analyze(entry, mismatched), optimize), "incompatible callback C contract reloaded");
    require(slot.invoke(3) == 43, "incompatible candidate touched original registration");
    expect_failure(slot, -1, "C resource operation rejected");
    require(jit.live_generations() == 2, "transport refusal retired the original ORC generation");
    const auto after_refusal = slot.invoke(4);
    require(slot.invoke(-1) == 0 && jit.live_generations() == 1, "unregister did not retire original ORC generation");
    std::cout << "G5 callback after refused release returned " << after_refusal << " (expected 44)\n";
    require(after_refusal == 44, "refused release lost retained callback admission");
    require(slot.invoke(0) == 1 && slot.invoke(2) == 142, "replacement registration did not use replacement body");
    expect_failure(slot, -20, "Retained C callback failed");
    expect_failure(slot, -1, "C resource operation rejected");
    expect_failure(slot, -21, "Retained C callback failed");
    require(slot.invoke(-22) == 0, "refused release reopened a faulted callback");
    const auto failure = runtime->close();
    require(failure.kind == vd::console::ErrorKind::io_failure &&
            failure.os_code == static_cast<int>(std::errc::operation_canceled),
            "retained failure was not reported after shutdown drain");
    std::cout << "G5 ORC " << (optimize ? "O2" : "normal")
              << " capture, compatible/incompatible reload, refusal/retry, retirement and self-drain passed\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 6, "expected entry, native root, provider, adapter and descriptor");
        const Path entry = argv[1];
        const auto descriptor = vd::read_source(argv[5]).text();
        const auto source = vd::read_source(entry).text();
        for (bool optimize : {false, true}) qualify(entry, argv[2], argv[3], argv[4], descriptor, source, optimize);
        write(entry, source);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
