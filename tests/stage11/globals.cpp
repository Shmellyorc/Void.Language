#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/runtime.hpp"
#include "void/tooling.hpp"
#include "void/cache.hpp"
#include "void/native_runtime.hpp"
#include "../managed/probe.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
unsigned checks = 0;
void check(bool valid, const char* message) {
    if (!valid) throw std::runtime_error(message);
    ++checks;
}
auto analyze(const std::string& source) {
    auto result = vd::analyze_source({"/globals.void", source});
    if (!result.succeeded()) throw vd::CompilationError(result.diagnostics);
    return result.program;
}
void rejects(const std::string& source) {
    check(!vd::analyze_source({"/globals.void", source}).succeeded(), "invalid globals accepted");
}
void invalid(vd::ir::Module module) {
    bool rejected = false;
    try { vd::ir::validate(module); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "forged globals accepted");
}
void execution(bool optimize) {
    const std::string source = "var score = 10\nfunc bump() -> i32:\n    score += 1\n    return score\nfunc entry(n: i32) -> i32:\n    return bump() + n\n";
    auto program = analyze(source);
    vd::Jit jit;
    auto code = jit.compile(*program, optimize);
    check(code->invoke(2) == 13 && code->invoke(0) == 12, "global state did not persist");
    auto fresh = jit.compile(*program, optimize);
    check(fresh->invoke(0) == 11, "independent programs shared globals");
    auto changed = source;
    changed.replace(changed.find("return bump() + n"), 17, "return bump() + n + 20");
    vd::ReloadSlot slot(code);
    check(jit.replace(slot, *analyze(changed), optimize), "body-only reload rejected");
    check(slot.invoke(0) == 33, "compatible reload replayed initialization");
    changed = source; changed.replace(changed.find("= 10"), 4, "= 30");
    check(!jit.replace(slot, *analyze(changed), optimize), "initializer change accepted as reload");
    auto module = program->lower();
    module.globals[0].identity = "forged"; invalid(module);
    module = program->lower(); module.globals[0].cell = vd::ir::Type::i32; invalid(module);
    module = program->lower(); module.globals[0].initializer = "missing"; invalid(module);
    module = program->lower(); module.globals[0].module_identity = "/forged.void"; invalid(module);
    module = program->lower(); module.globals[0].cell.readonly = true; invalid(module);
    module = program->lower(); module.nominal_types.clear(); invalid(module);
    module = program->lower(); module.imports.push_back("vd_global_cell"); invalid(module);
    module = program->lower();
    for (auto& f : module.functions) for (auto& b : f.blocks) for (auto& i : b.instructions)
        if (i.op == vd::ir::Op::global_cell) i.callee = "missing";
    invalid(module);
    bool denied = false;
    std::thread other([&] { try { code->invoke(0); } catch (const std::exception&) { denied = true; } }); other.join();
    check(denied && code->invoke(0) == 14, "foreign owner access was admitted");
}
void collection(bool optimize) {
    auto program = analyze("struct box:\n    value: i32\nvalue struct packet:\n    item: box\nvar shared = box(value = 5)\nvar text = \"é🎮\"\nvar list = [shared]\nvar fixed: [packet, 2] = [packet(item = shared), packet(item = shared)]\nvar map = {\"key\": shared}\nvar fn = make_counter()\nfunc make_counter() -> func() -> i32:\n    var count = 0\n    return func() -> i32:\n        count += 1\n        return count\nfunc entry(n: i32) -> i32:\n    list[0].value += n\n    return fixed[1].item.value + map[\"key\"].value + (text.length as i32) + fn()\n");
    vd::Jit jit; auto code = jit.compile(*program, optimize); auto runtime = code->runtime_owner();
    managed_test::Probe probe; probe.inspect_roots = true; probe.collect_before = true; probe.frames.reserve(1024);
    { managed_test::Observe observe(probe); check(code->invoke(2) == 17, "global reference families lost initial identity"); }
    check(probe.roots_valid && probe.commits_valid && probe.frames.empty() && probe.constructions > 8, "global initialization corrupted precise roots");
    { vd::Runtime::Attachment attachment(*runtime); vd::Runtime::Managed managed(*runtime);
      check(runtime->collect() == vd::Runtime::Collection::completed && runtime->snapshot().objects > 8, "global roots were not persistent"); }
    check(code->invoke(1) == 20, "global references or closure captures did not survive collection");
    auto module = program->lower(); auto supplied = std::make_shared<vd::Runtime>(1);
    auto explicit_code = jit.compile(module, module.entry_symbol, optimize, {}, supplied);
    bool denied = false;
    try { (void)jit.compile(module, module.entry_symbol, optimize, {}, supplied); } catch (const std::invalid_argument&) { denied = true; }
    check(denied, "explicit Runtime admitted an independent global program");
    runtime->close();
    check(runtime->memory_snapshot().handles == 0, "global handles survived Runtime shutdown");
}
void close_during_root_setup() {
    auto runtime = std::make_shared<vd::Runtime>(1);
    std::atomic<bool> completed = false;
    vd::RuntimeFailure failure;
    int state = 0;
    std::thread owner([&] {
        vd::Runtime::Attachment attachment(*runtime);
        vd::Runtime::Managed managed(*runtime);
        state = runtime->globals_begin("root-close-regression", 32768, failure);
        completed = true;
    });
    while (!runtime->memory_snapshot().handles && !completed.load()) std::this_thread::yield();
    runtime->close();
    owner.join();
    check(state == 1 || (state == -1 && failure.message), "shutdown during global root setup returned an invalid state");
    check(runtime->memory_snapshot().handles == 0, "shutdown during global root setup retained persistent handles");
}
void cache(const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    auto program = analyze("var counter = 10\nfunc entry(n: i32) -> i32:\n    counter += 1\n    return counter\n");
    for(bool optimize:{false,true}) {
    vd::Jit jit({directory, std::string(vd::compiler_build_identity())});
    auto first = jit.compile(*program, optimize); auto status = jit.cache_status();
    check(status == "miss" || status == "hit" || status == "unavailable", "invalid global cache cold status");
    check(first->invoke(0) == 11 && first->invoke(0) == 12, "cached cold global state changed");
    auto hit = jit.compile(*program, optimize);
    check(jit.cache_status() == (status == "unavailable" ? "unavailable" : "hit"), "global object cache did not hit");
    check(hit->invoke(0) == 11 && first->invoke(0) == 13, "cache-hit applications shared live cells");
    check(hit->runtime_owner()!=first->runtime_owner(), "global cache reused a live Runtime");
    }
}
void reload_contracts(bool optimize) {
    const std::string source = "struct box:\n    value: i32\nvar state = box(value = seed())\nvar count: i32 = 2\nfunc seed() -> i32:\n    return 7\nfunc entry(n: i32) -> i32:\n    state.value += n\n    return state.value + (count as i32)\n";
    vd::Jit jit;
    auto code = jit.compile(*analyze(source), optimize);
    check(code->invoke(1) == 10, "reload contract fixture did not initialize");
    vd::ReloadSlot slot(code);
    auto reject_change = [&](std::string_view before, std::string_view after) {
        auto changed = source;
        changed.replace(changed.find(before), before.size(), after);
        check(!jit.replace(slot, *analyze(changed), optimize), "incompatible global contract admitted replacement");
    };
    reject_change("var state", "private var state");
    reject_change("value: i32", "value: i32\n    extra: i64");
    reject_change("count: i32", "count: i64");
    reject_change("var state = box(value = seed())\nvar count: i32 = 2", "var count: i32 = 2\nvar state = box(value = seed())");
    reject_change("return 7", "return 8");
    check(code->invoke(1) == 11, "rejected replacement disturbed existing global state");
    const std::string callable_source = "var counter = 0\nvar callback = increment\nfunc increment() -> i32:\n    counter += 1\n    return counter\nfunc entry(n: i32) -> i32:\n    return callback() + n\n";
    vd::Jit callable_jit;
    auto original = callable_jit.compile(*analyze(callable_source), optimize);
    check(original->invoke(0) == 1, "global callable reload fixture did not initialize");
    vd::ReloadSlot callable_slot(original);
    auto changed = callable_source;
    changed.replace(changed.find("return counter"), std::string("return counter").size(), "return counter + 20");
    check(callable_jit.replace(callable_slot, *analyze(changed), optimize), "stored global callable unnecessarily froze its unexecuted body");
    original.reset();
    check(callable_jit.live_generations() == 1, "global callable retained an obsolete code generation");
    check(callable_slot.invoke(0) == 22, "retained global callable lost shared state or stable dispatch after reload");
}
void resources(const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory); auto path = directory / "native.void";
    for (bool optimize : {false, true}) {
        // Runtime failure follows a successfully acquired globally rooted resource.
        { std::ofstream file(path); file << "use \"/native_bridge.void\" as bridge\nvar owner = bridge.acquire(true) as bridge.resource\nvar bad = fail(0)\nfunc fail(n: i32) -> i32:\n    return 1 / n\nfunc entry(n: i32) -> i32:\n    return n\n"; }
        auto result = vd::analyze_project(path, vd::native_fixture_project());
        if (!result.succeeded()) throw vd::CompilationError(result.diagnostics);
        vd::Jit jit; auto code = jit.compile(*result.program, optimize);
        for (unsigned attempt = 0; attempt < 2; ++attempt) {
            bool failed = false; try { code->invoke(3); } catch (const std::runtime_error&) { failed = true; }
            check(failed && vd::native_fixture_live() == 0, "failed global startup leaked a native resource");
        }
        { std::ofstream file(path); file << "use \"/native_bridge.void\" as bridge\nvar owner = bridge.acquire(true) as bridge.resource\nvar boot = connect\nfunc callback(n: i32) -> i32:\n    return n + 1\nfunc connect() -> i32:\n    bridge.register(owner, callback)\n    var value = bridge.invoke(owner, 1)\n    bridge.unregister(owner)\n    return value\nfunc entry(n: i32) -> i32:\n    if n == 4:\n        if boot() != 2:\n            return -7\n    return n\n"; }
        result = vd::analyze_project(path, vd::native_fixture_project());
        if (!result.succeeded()) throw vd::CompilationError(result.diagnostics);
        auto admitted = result.program;
        for (const auto& initializer : {"var boot = connect()", "var boot = connect\nvar eager = boot()"}) {
            std::ifstream file(path);
            std::string unsafe_source((std::istreambuf_iterator<char>(file)), {});
            auto begin = unsafe_source.find("var boot = connect");
            unsafe_source.replace(begin, std::string("var boot = connect").size(), initializer);
            { std::ofstream output(path); output << unsafe_source; }
            check(!vd::analyze_project(path, vd::native_fixture_project()).succeeded(), "global startup admitted native callback registration");
            unsafe_source.replace(begin, std::string(initializer).size(), "var boot = connect");
            { std::ofstream output(path); output << unsafe_source; }
        }
        result.program = admitted;
        auto failed_publication = jit.compile(*result.program, optimize);
        managed_test::Probe probe; probe.fail_construction = 1; probe.fail_allocation = 0; probe.frames.reserve(1024);
        bool failed = false;
        { managed_test::Observe observe(probe); try { failed_publication->invoke(0); } catch (const std::runtime_error&) { failed = true; } }
        check(failed && vd::native_fixture_live() == 0 && probe.frames.empty(), "failed global cell publication leaked its acquired native resource");
        code = jit.compile(*result.program, optimize);
        check(code->invoke(4) == 4 && vd::native_fixture_live() == 1, "global native resource did not retain explicit ownership");
        check(code->invoke(5) == 5 && vd::native_fixture_live() == 1, "global native acquisition replayed");
        code.reset(); check(vd::native_fixture_live() == 0, "Runtime destruction leaked global native resource");
    }
}
void workers(bool optimize) {
    auto program = analyze("var snapshot = 4\nfunc work(n: i32) -> i32:\n    return n + 1\nasync func entry(n: i32) -> i32:\n    var job = spawn work(snapshot)\n    snapshot += 1\n    return await job\n");
    vd::Jit jit; auto code = jit.compile_async(*program, optimize);
    check(code->invoke(0) == 5 && code->invoke(0) == 6, "owner global snapshots did not cross the worker boundary safely");
    auto module = program->lower();
    bool edited = false;
    for (auto& function : module.functions) if (function.source_name == "work") {
        vd::ir::Instruction read{vd::ir::Op::global_cell}; read.callee = module.globals[0].identity;
        function.blocks[0].instructions.push_back(read); edited = true;
    }
    check(edited, "worker IR fixture missing"); invalid(module);
}
void failure(bool optimize) {
    auto program = analyze("var ready = 1\nvar first = fail()\nvar later = 2\nfunc fail() -> i32:\n    return later\nfunc entry(n: i32) -> i32:\n    return 999\n");
    vd::Jit jit; auto code = jit.compile(*program, optimize);
    managed_test::Probe probe; probe.inspect_roots = true; probe.frames.reserve(1024);
    managed_test::Observe observe(probe);
    for (unsigned n = 0; n < 2; ++n) {
        bool failed = false;
        try { code->invoke(0); } catch (const std::runtime_error& error) { failed = std::string(error.what()).find("not initialized") != std::string::npos; }
        check(failed, "unready read exposed partial storage or failure was replayed");
    }
    check(probe.constructions == 1 && probe.frames.empty() && probe.roots_valid, "failed startup replayed earlier initializer effects");
}
}
int main(int argc, char** argv) {
    try {
        for (bool optimize : {false, true}) { execution(optimize); failure(optimize); collection(optimize); workers(optimize); reload_contracts(optimize); }
        close_during_root_setup();
        if (argc > 1) { resources(argv[1]); cache(std::filesystem::path(argv[1]) / "cache"); }
        rejects("var same = 1\nvar same = 2\n");
        rejects("var item = 1\nfunc read(item: i32) -> i32:\n    return item\n");
        rejects("var item = 1\nconst bad = item\n");
        rejects("var item = 1\nfunc read() -> i32:\n    return item\nasync func entry(n: i32) -> i32:\n    var job = spawn read()\n    return await job\n");
        rejects("var first = second\nvar second = first\n");
        std::cout << checks << " global checks passed\n";
    } catch (const vd::CompilationError& error) { std::cerr << error.what() << '\n'; return 1; }
      catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
