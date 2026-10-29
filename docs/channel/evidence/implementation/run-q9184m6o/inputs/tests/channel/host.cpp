#include "void/backend.hpp"
#include "void/cache.hpp"
#include "void/frontend.hpp"
#include "void/native_runtime.hpp"
#include "../managed/probe.hpp"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
unsigned checks = 0;
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); ++checks; }
auto analyze(const std::filesystem::path& path, const vd::SourceOverrides& overrides = {}) {
    auto result = vd::analyze_project(path, overrides);
    if (!result.succeeded()) throw vd::CompilationError(result.diagnostics);
    return result.program;
}
void collect(vd::Runtime& runtime) {
    vd::Runtime::Attachment attachment(runtime); vd::Runtime::Managed managed(runtime);
    check(runtime.collect() == vd::Runtime::Collection::completed, "collection failed");
}
auto compile(vd::Jit& jit, const vd::ValidatedAnalysis& program, bool optimize) {
    auto module = program.lower();
    return jit.compile(module, module.entry_symbol, optimize, {}, std::make_shared<vd::Runtime>(1));
}
void ir(const std::filesystem::path& root, const std::filesystem::path& evidence) {
    auto module = analyze(root / "tests/channel/semantics.void")->lower();
    vd::ir::validate(module);
    std::ofstream out(evidence / "typed-ir.txt");
    unsigned subscribe = 0, clear = 0;
    using vd::ir::Op;
    for (const auto& fn : module.functions) {
        if (fn.module_identity != "/@stdlib/channel.void") continue;
        out << fn.name << "\n";
        for (unsigned b = 0; b < fn.blocks.size(); ++b) {
            out << " block " << b << "\n";
            for (const auto& i : fn.blocks[b].instructions)
                out << "  op=" << static_cast<int>(i.op) << " a=" << i.a << " b=" << i.b
                    << " field=" << i.immediate << " callee=" << i.callee << " line=" << i.source.line << "\n";
            out << "  terminator=" << static_cast<int>(fn.blocks[b].end.kind) << "\n";
        }
        if (fn.method_name == "subscribe") {
            ++subscribe; unsigned appends = 0;
            for (const auto& block : fn.blocks) for (std::size_t n = 0; n < block.instructions.size(); ++n) {
                if (block.instructions[n].op != Op::list_push) continue;
                ++appends; unsigned commits = 0;
                for (++n; n < block.instructions.size(); ++n) {
                    const auto& i = block.instructions[n];
                    check(i.op == Op::load || i.op == Op::store || i.op == Op::gc_write,
                          "fallible work after event publication");
                    commits += i.op == Op::gc_write;
                }
                check(commits == 1 && block.end.kind == vd::ir::Terminator::Kind::return_value,
                      "publication did not commit directly and return");
            }
            check(appends == 1, "subscribe must publish through exactly one event append");
        }
        if (fn.method_name == "clear") {
            ++clear; unsigned writes = 0;
            for (const auto& block : fn.blocks) for (const auto& i : block.instructions) {
                check(i.op == Op::load || i.op == Op::store || i.op == Op::argument ||
                      i.op == Op::constant || i.op == Op::gc_write, "clear contains fallible work");
                writes += i.op == Op::gc_write;
            }
            check(writes == 1, "clear must detach one reference");
        }
    }
    check(subscribe && clear, "concrete channel IR was not inspected");
}
void semantics(const std::filesystem::path& root, bool optimize) {
    vd::Jit jit;
    auto code = compile(jit, *analyze(root / "tests/channel/semantics.void"), optimize);
    managed_test::Probe probe; probe.inspect_roots = true; probe.collect_before = true; probe.frames.reserve(2048);
    { managed_test::Observe observe(probe); check(code->invoke(0) == 7, "channel semantic checks failed"); }
    check(probe.roots_valid && probe.commits_valid && probe.frames.empty() && probe.roots_checked,
          "channel precise roots, publication or frame balance failed");
    collect(*code->runtime_owner());
    check(code->runtime_owner()->snapshot().objects == 0, "local router/callback cycles leaked");
    auto lifetime = compile(jit, *analyze(root / "tests/channel/lifetime.void"), optimize);
    auto runtime = lifetime->runtime_owner();
    check(lifetime->invoke(1) == 0, "lifetime global setup failed");
    managed_test::Probe capture; capture.capture = true; capture.capture_at = 1;
    { managed_test::Observe observe(capture); check(lifetime->invoke(0) == 0, "lifetime setup failed"); }
    void* router = nullptr;
    { vd::Runtime::Attachment attachment(*runtime); vd::Runtime::Managed managed(*runtime);
      check(bool(capture.retained), "router observation missing"); router = capture.retained->get(); }
    capture.retained.reset(); collect(*runtime);
    check(!runtime->contains(router) && lifetime->invoke(1) == 0, "retained token kept a router/callback cycle or GC ran deinit");
}
void faults(const std::filesystem::path& root, bool optimize) {
    vd::Jit jit;
    auto program = analyze(root / "tests/channel/persistent.void");
    auto code = compile(jit, *program, optimize);
    auto runtime = code->runtime_owner();
    check(code->invoke(0) == 0 && code->invoke(3) == 1, "persistent router setup failed");
    for (bool once : {false, true}) {
        for (int path = 0; path < 4; ++path) {
            bool complete = false; unsigned failures = 0, markers = 0;
            for (int budget = 0; budget < 4096 && !complete; ++budget) {
                code->invoke(0);
                if (path == 0) code->invoke(4);
                // Fill the event's initial capacity to reach its growth publication path.
                if (path == 2) for (unsigned i = 0; i < 3; ++i) code->invoke(10);
                collect(*runtime);
                managed_test::Probe probe; probe.inspect_roots = true; probe.frames.reserve(2048);
                probe.capture = true; probe.capture_at = 1; probe.capture_into_existing = true;
                { vd::Runtime::Attachment attachment(*runtime); vd::Runtime::Managed managed(*runtime);
                  probe.retained = std::make_unique<vd::Runtime::Handle>(*runtime, nullptr); }
                bool failed = false; int result = -1;
                { managed_test::Observe observe(probe);
                  try { managed_test::AllocationFailure inject(budget, once); result = code->invoke(path == 3 ? 12 : 1); }
                  catch (const std::bad_alloc&) { failed = true; }
                  catch (const std::runtime_error&) { failed = true; } }
                check(probe.roots_valid && probe.commits_valid && probe.frames.empty(), "failed subscribe corrupted roots");
                const int existing = path == 0 ? 0 : path == 2 ? 4 : 1;
                if (failed) {
                    ++failures;
                    check(code->invoke(3) == existing, "failed subscribe installed an unreturned callback");
                    check(code->invoke(13) == 0, "failed new-topic subscribe installed an unreturned callback");
                    check(code->invoke(8) == 0, "failed subscribe returned a handle");
                } else {
                    check(result == 1 && code->invoke(3) == existing + (path == 3 ? 0 : 1234568), "successful subscribe lost callback");
                    if (path == 3) check(code->invoke(13) == 1234568, "new-topic callback did not deliver");
                    check(code->invoke(8) == 1, "successful subscribe could not remove exact callback");
                    complete = true;
                }
                void* marker = nullptr;
                { vd::Runtime::Attachment attachment(*runtime); vd::Runtime::Managed managed(*runtime);
                  marker = probe.retained->get(); }
                probe.retained.reset();
                collect(*runtime);
                if (marker) { ++markers; check(!runtime->contains(marker), "pending/removed metadata retained callback marker"); }
                check(code->invoke(2) == (existing ? 1 : 0) && code->invoke(3) == (existing ? existing - 1 : 0), "failed subscribe damaged a prior registration");
                // State detachment must work even with sustained allocation failure.
                { managed_test::AllocationFailure inject(0); check(code->invoke(4) == 0, "clear failed under OOM"); }
            }
            check(complete && failures && markers, "subscribe allocation sweep did not reach completion and marker paths");
            std::cout << "subscribe O" << (optimize ? 2 : 0) << " once=" << once << " path=" << path
                      << " failures=" << failures << " released_markers=" << markers << '\n';
        }
      for (bool survivor : {false, true}) {
        bool complete = false; unsigned before = 0, after = 0;
        for (int budget = 0; budget < 4096 && !complete; ++budget) {
            code->invoke(0); if (survivor) code->invoke(5); collect(*runtime);
            managed_test::Probe probe; probe.inspect_roots = true; probe.frames.reserve(2048);
            bool failed = false; int result = -1;
            { managed_test::Observe observe(probe);
              try { managed_test::AllocationFailure inject(budget, once); result = code->invoke(2); }
              catch (const std::bad_alloc&) { failed = true; }
              catch (const std::runtime_error&) { failed = true; } }
            check(probe.roots_valid && probe.commits_valid && probe.frames.empty(), "failed unsubscribe corrupted roots");
            int live = code->invoke(3);
            const int base = survivor ? 2 : 1;
            check(live == base - 1 || live == base, "unsubscribe lost unrelated registration");
            if (failed) {
                if (live == base) { ++before; check(code->invoke(2) == 1, "active record lost before removal"); }
                else {
                    ++after;
                    // Remove the last live callback, recreate this topic, then retry
                    // an old inert handle. It must not prune the replacement bucket.
                    if (survivor) check(code->invoke(8) == 1, "survivor removal failed");
                    code->invoke(5);
                    check(code->invoke(2) == 0 && code->invoke(3) == 1, "inactive cleanup pruned a replacement topic");
                    code->invoke(8);
                    if (survivor) code->invoke(5);
                }
            } else { complete = true; check(result == 1 && live == base - 1, "unsubscribe success did not remove callback"); }
            check(code->invoke(3) == base - 1, "unsubscribe retry damaged valid registration");
        }
        check(complete && before && after, "unsubscribe sweep did not cover both sides of removal");
        std::cout << "unsubscribe O" << (optimize ? 2 : 0) << " once=" << once << " survivor=" << survivor << " before=" << before << " after=" << after << '\n';
      }
    }
    code->invoke(0); code->invoke(2); collect(*runtime);
    const auto empty_count = runtime->snapshot().objects;
    code->invoke(14); collect(*runtime);
    check(runtime->snapshot().objects == empty_count, "successful unique-topic churn retained historical topic/record graphs");
    bool emitted = false; unsigned snapshot_failures = 0, partial_failures = 0;
    for (int budget = 0; budget < 4096 && !emitted; ++budget) {
        code->invoke(0); for (unsigned i = 0; i < 15; ++i) code->invoke(10);
        bool failed = false;
        try { managed_test::AllocationFailure inject(budget); code->invoke(3); }
        catch (const std::bad_alloc&) { failed = true; }
        catch (const std::runtime_error&) { failed = true; }
        int delivered = code->invoke(11);
        check(delivered >= 0 && delivered <= 16, "failed emission corrupted subscriber snapshot");
        if (failed) { if (!delivered) ++snapshot_failures; else ++partial_failures; }
        else { emitted = true; check(delivered == 16, "emission sweep success lost subscribers"); }
    }
    check(emitted && snapshot_failures, "snapshot allocation failure paths not reached");
    std::cout << "emit O" << (optimize ? 2 : 0) << " no_delivery_failures=" << snapshot_failures << " partial_failures=" << partial_failures << '\n';
    bool failed = false;
    try { code->invoke(6); } catch (const std::runtime_error&) { failed = true; }
    check(failed && code->invoke(7) == 101, "handler failure did not stop delivery/run cleanup/preserve earlier effects");
    code->invoke(4); code->invoke(9); collect(*runtime);
    check(runtime->snapshot().objects > 0, "global router did not stay alive");
    runtime->close();
    check(runtime->memory_snapshot().handles == 0, "Runtime shutdown retained global handles");
}
void reload_cache(const std::filesystem::path& root, const std::filesystem::path& evidence, bool optimize) {
    auto path = root / "tests/channel/reload.void";
    auto original = analyze(path);
    vd::Jit jit({evidence / (optimize ? "cache-o2" : "cache-o0"), std::string(vd::compiler_build_identity())});
    auto old = jit.compile(*original, optimize);
    check(jit.cache_status() == "miss" || jit.cache_status() == "hit" || jit.cache_status() == "unavailable", "invalid cache status");
    check(old->invoke(0) == 0 && old->invoke(1) == 1, "cached router setup failed");
    auto fresh = jit.compile(*original, optimize);
    check(jit.cache_status() == "hit" || jit.cache_status() == "unavailable", "object cache did not hit");
    check(fresh->invoke(1) == 0 && old->invoke(1) == 2, "cached application restored live subscribers");
    bool denied = false;
    std::thread foreign([&] { try { old->invoke(1); } catch (const std::exception&) { denied = true; } }); foreign.join();
    check(denied, "channel global invocation granted foreign owner authority");
    const auto source = vd::read_source(path);
    auto text = source.text(); text.replace(text.find("score += amount"), 15, "score += amount * 10");
    vd::ReloadSlot slot(old);
    auto changed = analyze(path, {{"/reload.void", vd::SourceFile(source.path(), text)}});
    check(jit.replace(slot, *changed, optimize), "compatible channel callback reload rejected");
    collect(*old->runtime_owner());
    check(slot.invoke(1) == 12 && old->invoke(1) == 13, "stored channel callback lost current/leased generation dispatch");
    fresh.reset(); old.reset();
    check(jit.live_generations() == 1, "channel token pinned an obsolete code generation");
    check(slot.invoke(1) == 23, "callback failed after original code retired");
    for (const auto& pair : {std::pair{"const amount: i32", "const amount: i64"},
                            std::pair{"var score = 0", "var score = 5"}}) {
        auto bad = text; bad.replace(bad.find(pair.first), std::strlen(pair.first), pair.second);
        // A signature change also needs a matching router type to remain valid source.
        if (std::string(pair.first) == "const amount: i32") {
            bad.replace(bad.find("router<i32>"), 11, "router<i64>");
            bad.replace(bad.find("score += amount * 10"), 20, "score += (amount as i32) * 10");
        }
        check(!jit.replace(slot, *analyze(path, {{"/reload.void", vd::SourceFile(source.path(), bad)}}), optimize),
              "incompatible channel reload was admitted");
    }
    check(slot.invoke(1) == 33 && slot.invoke(2) == 33 && slot.invoke(1) == 33, "rejected reload damaged persistent token");
}
void active_lease(const std::filesystem::path& root, bool optimize) {
    auto path = root / "tests/channel/reload.void"; auto source = vd::read_source(path);
    auto text = source.text(); text.replace(text.find("score += amount"), 15, "score += amount * 10");
    auto changed = analyze(path, {{"/reload.void", vd::SourceFile(source.path(), text)}});
    vd::Jit jit; auto old = jit.compile(*analyze(path), optimize); vd::ReloadSlot slot(old);
    managed_test::Probe paused;
    paused.before_construct = [&](std::size_t, const void*, std::size_t) { paused.reached.release(); paused.release.acquire(); };
    int original_result = -1, replacement_result = -1;
    auto leased = old;
    std::exception_ptr failure;
    std::thread owner([&, leased]() mutable {
        try {
            leased->invoke(0);
            { managed_test::Observe observe(paused); original_result = leased->invoke(1); }
            leased.reset();
            replacement_result = slot.invoke(1);
        } catch (...) { failure = std::current_exception(); }
    });
    paused.reached.acquire();
    bool replaced = jit.replace(slot, *changed, optimize);
    old.reset(); leased.reset();
    const bool active_retained = jit.live_generations() == 2;
    paused.release.release(); owner.join();
    if (failure) std::rethrow_exception(failure);
    check(replaced && active_retained && original_result == 1 && replacement_result == 11,
          "active channel snapshot/callback did not preserve its original code lease");
    check(jit.live_generations() == 1, "active channel callback pinned retired code after returning");
}
void native_lifetime(const std::filesystem::path& root, bool optimize) {
    auto result = vd::analyze_project(root / "tests/channel/native_lifetime.void", vd::native_fixture_project());
    if (!result.succeeded()) throw vd::CompilationError(result.diagnostics);
    for (int removal : {1, 2}) {
        vd::Jit jit; auto code = compile(jit, *result.program, optimize); auto runtime = code->runtime_owner();
        check(code->invoke(0) == 0 && code->invoke(3) == 66 && vd::native_fixture_live() == 1, "native capture setup failed");
        code->invoke(removal); collect(*runtime);
        check(code->invoke(3) == 66 && vd::native_fixture_live() == 1, "channel implicitly destroyed a subscriber native resource");
        runtime->close();
        check(vd::native_fixture_live() == 0, "Runtime did not drain its native resources");
    }
}
}
int main(int argc, char** argv) {
    try {
        check(argc == 3, "repository and evidence directory required");
        std::filesystem::path root = argv[1], evidence = argv[2]; std::filesystem::create_directories(evidence);
        ir(root, evidence);
        for (bool optimize : {false, true}) {
            semantics(root, optimize); faults(root, optimize); reload_cache(root, evidence, optimize); active_lease(root, optimize); native_lifetime(root, optimize);
            vd::Jit jit; auto program = analyze(root / "tests/channel/async.void");
            auto code = jit.compile_async(*program, optimize);
            check(code->invoke(0) == 7, "owner async router failed across await");
            vd::emit_async_object(program->lower(), evidence / (optimize ? "async-o2.o" : "async-o0.o"), optimize);
        }
        std::cout << checks << " channel host checks passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
