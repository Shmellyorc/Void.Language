#pragma once
// Optional compiler observations. Records belong to one invocation, never to
// validated source, cache identities, generated code or the Runtime ABI.
#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <string_view>

namespace vd::observation {
using Clock = std::chrono::steady_clock;
enum class Phase : unsigned {
    frontend, project_capture, dependency_verification, source_loading,
    lex_parse, semantic_analysis, analysis_assembly, authorization_wait,
    typed_ir, llvm_ir, llvm_verification, llvm_optimization,
    native_object_emission, jit_materialization, cache_lookup, cache_store,
    toolchain_validation, native_link_driver, publication, package_assembly, count
};
inline constexpr std::array<std::string_view, static_cast<unsigned>(Phase::count)> names{
    "frontend", "project_capture", "dependency_verification", "source_loading",
    "lex_parse", "semantic_analysis", "analysis_assembly", "authorization_wait",
    "typed_ir", "llvm_ir", "llvm_verification", "llvm_optimization",
    "native_object_emission", "jit_materialization", "cache_lookup", "cache_store",
    "toolchain_validation", "native_link_driver", "publication", "package_assembly"
};
// Optional editor details are separate from the versioned CLI phase schema.
enum class EditorDetail : unsigned { inputs, candidates, formatting, publication, count };
inline constexpr std::array<std::string_view, static_cast<unsigned>(EditorDetail::count)> editor_names{
    "input_freshness", "candidates", "formatting", "response_publication"
};
struct Record {
    Clock::duration duration{};
    std::uint64_t count = 0;
    unsigned depth = 0;
    bool partial = false, skipped = false;
    Clock::time_point start{};
};
struct Sink {
    std::array<Record, names.size()> records{};
    std::array<Record, editor_names.size()> editor_records{};
    bool detailed = false;
    // Only the CLI installs this noexcept callback; compiler libraries default
    // to no sink. There is no output ownership or terminal logic in the phases.
    void (*enter)(Phase, void*) noexcept = nullptr;
    void* context = nullptr;
};
// A scoped, thread-local binding borrows invocation storage. It is deliberately
// not inherited by worker threads; combined ORC timings use the caller's wall
// interval. Nothing persists between library calls or concurrent invocations.
inline thread_local Sink* active = nullptr;
class Binding {
    Sink* previous;
public:
    explicit Binding(Sink* sink) noexcept : previous(active) { active = sink; }
    ~Binding() { active = previous; }
    Binding(const Binding&) = delete;
    Binding& operator=(const Binding&) = delete;
};
class Scope {
    Sink* sink;
    Record* record = nullptr;
    int exceptions = 0;
    bool failed = false;
public:
    explicit Scope(Phase value) noexcept : sink(active) {
        if (!sink) return;
        if (sink->enter) sink->enter(value, sink->context);
        if (sink->detailed) {
            record = &sink->records[static_cast<unsigned>(value)];
            if (record->depth++ == 0) record->start = Clock::now();
            exceptions = std::uncaught_exceptions();
        }
    }
    explicit Scope(EditorDetail value) noexcept : sink(active) {
        if (sink && sink->detailed) {
            record = &sink->editor_records[static_cast<unsigned>(value)];
            if (record->depth++ == 0) record->start = Clock::now();
            exceptions = std::uncaught_exceptions();
        }
    }
    void fail() noexcept { failed = true; }
    void finish() noexcept {
        if (!sink) return;
        if (record) {
            record->partial |= failed || std::uncaught_exceptions() > exceptions;
            if (--record->depth == 0) {
                record->duration += Clock::now() - record->start;
                ++record->count;
            }
        }
        sink = nullptr;
    }
    ~Scope() { finish(); }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};
inline void skipped(Phase phase) noexcept {
    if (active && active->detailed) active->records[static_cast<unsigned>(phase)].skipped = true;
}
inline std::int64_t microseconds(Clock::duration value) noexcept {
    return std::chrono::round<std::chrono::microseconds>(value).count();
}
}
