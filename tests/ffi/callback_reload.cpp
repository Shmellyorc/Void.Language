#include "void/backend.hpp"
#include "void/c_primitive.hpp"
#include "void/frontend.hpp"
#include "void/runtime.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
using Path = std::filesystem::path;
using Modules = std::array<Path, 2>;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

#ifdef _WIN32
void expect_modules(const Modules& paths, bool loaded) {
    for (const auto& path : paths) {
        // Observe the mapping without taking a loader reference.
        const auto module = GetModuleHandleW(path.filename().c_str());
        require(bool(module) == loaded, "unexpected callback provider or adapter lifetime");
        if (loaded) {
            std::wstring name(32768, L'\0');
            const auto length = GetModuleFileNameW(module, name.data(), static_cast<DWORD>(name.size()));
            require(length && length < name.size(), "could not identify mapped callback DLL");
            name.resize(length);
            require(std::filesystem::equivalent(Path(name), path), "wrong callback DLL was mapped");
        }
    }
}
#endif

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

template<class Analyze, class Incompatible>
void qualify(const Path& entry, const std::string& original, bool optimize,
             const Analyze& analyze_current, const Incompatible& analyze_incompatible,
             [[maybe_unused]] const Modules& modules) {
    write(entry, original);
    auto before = analyze_current();
    check_ir(*before);
    vd::Jit jit;
    auto old = jit.compile(*before, optimize);
    auto runtime = old->runtime_owner();
    const auto* binding = runtime->dynamic_binding().get();
    vd::ReloadSlot slot(old);
    require(slot.invoke(0) == 1, "register original generation");

    auto edited = original;
    const std::string result = "return s.bias + value";
    const auto position = edited.find(result);
    require(position != std::string::npos, "fixture callback body");
    edited.replace(position, result.size(), result + " + 100");
    write(entry, edited);
    auto replacement = analyze_current();
    require(jit.replace(slot, *replacement, optimize), "compatible callback body reload rejected");
    require(slot.acquire()->runtime_owner() == runtime && runtime->dynamic_binding().get() == binding,
            "compatible callback reload changed the Runtime or dynamic binding");
    old.reset();
    require(jit.live_generations() == 2, "registration lost its original ORC generation");
#ifdef _WIN32
    expect_modules(modules, true);
#endif
    {
        vd::Runtime::Attachment attachment(*runtime);
        vd::Runtime::Managed managed(*runtime);
        require(runtime->collect() == vd::Runtime::Collection::completed, "collect while retained");
    }
    require(slot.invoke(2) == 42, "old callback dispatched to the replacement body");
    require(!jit.replace(slot, *analyze_incompatible(), optimize), "incompatible callback C contract reloaded");
    require(slot.invoke(3) == 43, "incompatible candidate touched original registration");
    expect_failure(slot, -1, "C resource operation rejected");
    require(jit.live_generations() == 2, "transport refusal retired the original ORC generation");
#ifdef _WIN32
    expect_modules(modules, true);
#endif
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
#ifdef _WIN32
    expect_modules(modules, true); // The live Runtime still owns the binding.
#endif
    std::cout << "G5 ORC " << (optimize ? "O2" : "normal")
              << " capture, compatible/incompatible reload, refusal/retry, retirement and self-drain passed\n";
}

void qualify_managed(const Path& project, const Path& catalog, const Path& alternate) {
    const std::string entry_name = "src/main.void";
    const auto entry = project / entry_name;
    const auto source = vd::read_source(entry).text();
    auto current = [&] {
        auto result = vd::analyze_manifest_project(project, entry_name, {}, {}, true);
        if (!result.succeeded()) throw vd::CompilationError(result.diagnostics);
        auto program = result.project->program();
        const auto& admission = program->c_primitive_admission();
        require(program->locked_dependencies() && admission &&
                admission->dynamic_contract().starts_with("void-native-managed 1\n"),
                "reload fixture did not use the locked managed provider");
        return program;
    };
    // Use the same project, source and loaded libraries with a second approved
    // payload identity. Change only the notice and the catalog/lock that bind it.
    const std::array files{catalog / "LICENSE.txt", catalog / "void.native.toml", project / "void.lock"};
    auto incompatible = [&] {
        std::array<std::string, files.size()> originals;
        for (std::size_t index = 0; index < files.size(); ++index) {
            originals[index] = vd::read_source(files[index]).text();
            write(files[index], vd::read_source(alternate / files[index].filename()).text());
        }
        auto replacement = current();
        for (std::size_t index = 0; index < files.size(); ++index) write(files[index], originals[index]);
        return replacement;
    };
    const Modules modules{catalog / "g5_fixture.dll", catalog / "g5_refusal_adapter.dll"};
    for (bool optimize : {false, true}) {
        qualify(entry, source, optimize, current, incompatible, modules);
#ifdef _WIN32
        expect_modules(modules, false);
#endif
    }
    write(entry, source);
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 5 && std::string_view(argv[1]) == "--managed") {
            qualify_managed(argv[2], argv[3], argv[4]);
            return 0;
        }
        require(argc == 6, "expected entry, native root, provider, adapter and descriptor");
        const Path entry = argv[1];
        const auto descriptor = vd::read_source(argv[5]).text();
        const auto source = vd::read_source(entry).text();
        const auto contract = vd::CPrimitiveAdmission::admit_conventional(descriptor, argv[2], argv[3], argv[4]);
        auto current = [&] { return analyze(entry, contract); };
        auto incompatible = [&] {
            auto text = descriptor;
            const std::string signature = "callback integer context-first i32 i32";
            const auto position = text.find(signature);
            require(position != std::string::npos, "fixture callback signature");
            text.replace(position, signature.size(), "callback integer context-first c_int c_int");
            return analyze(entry, vd::CPrimitiveAdmission::admit_conventional(text, argv[2], argv[3], argv[4]));
        };
        const Modules modules{Path(argv[2]) / argv[3], Path(argv[2]) / argv[4]};
        for (bool optimize : {false, true}) {
            qualify(entry, source, optimize, current, incompatible, modules);
#ifdef _WIN32
            expect_modules(modules, false);
#endif
        }
        write(entry, source);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
