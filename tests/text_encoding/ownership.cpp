#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/runtime.hpp"
#include "../managed/probe.hpp"

#include <filesystem>
#include <thread>
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

// Exercise the private ABI with aliased output/input slots and a collector
// already requesting the world. The replacement also aliases the input.
void translation_ownership() {
    vd::Runtime runtime(1);
    vd::Runtime::Attachment attachment(runtime);
    vd::Runtime::Managed managed(runtime);
    void *value = nullptr, *keys = nullptr, *replacements = nullptr;
    constexpr std::size_t zero = 0;
    vd::Runtime::Slot slots[] = {{&value,sizeof(value),&zero,1}, {&keys,sizeof(keys),&zero,1},
                                 {&replacements,sizeof(replacements),&zero,1}};
    vd::Runtime::Frame frame(runtime, slots, 3);
    std::string source = "a" + std::string(32768, 'b');
    runtime.string_into(&value, source);
    runtime.list_into(&keys, vd::ScalarEncoding::i32, 1);
    runtime.list_write(runtime.root_word(&keys), vd::ScalarEncoding::i32, 0, 'a');
    auto schema_key = "void.array.element.v1/" + std::to_string(vd::ir::Type::string) + ":";
    vd::NativeArrayField field{"",0,0,12,0};
    vd::NativeArrayElement element{schema_key.c_str(),sizeof(void*),alignof(void*),1,&field};
    auto layout = runtime.native_array_layout(element, 1);
    runtime.array_new(&replacements, layout, true);
    runtime.array_write(runtime.root_word(&replacements), layout, true, 0, &value, true);
    auto original = value;
    auto invoke = [&](void** out, void** k, std::int64_t budget, vd::RuntimeFailure& failure) {
        return vd_text_translate(out, &value, k, &replacements, &element, budget, &failure);
    };
    vd::RuntimeFailure failure;
    void* unrooted = nullptr;
    invoke(&unrooted, &keys, 0, failure);
    require(failure.message && !unrooted, "unrooted translation output accepted");
    failure = {};
    void* unrooted_input = value;
    vd_text_translate(&value, &unrooted_input, &keys, &replacements, &element, 0, &failure);
    require(failure.message && value == original, "unrooted translation input accepted");
    failure = {};
    invoke(&value, &replacements, 0, failure);
    require(failure.message && value == original, "wrong translation array schema accepted");
    failure = {};
    require(invoke(&value, &keys, 10, failure) == 3 && !failure.message && value == original,
            "bounded rejection published into aliased input");

    const std::int64_t size = 2 * source.size() - 1;
    std::string expected = source + source.substr(1);
    vd::Runtime::Collection collected{};
    std::thread collector([&] {
        vd::Runtime::Attachment other(runtime);
        vd::Runtime::Managed collecting(runtime);
        collected = runtime.collect();
    });
    while (!runtime.snapshot().requested) std::this_thread::yield();
    int status = invoke(&value, &keys, size, failure);
    // A pending collector must also be able to finish if the call failed early.
    runtime.poll();
    collector.join();
    require(collected == vd::Runtime::Collection::completed && !failure.message && !status &&
            runtime.string_bytes(value) == expected, "translation failed with a concurrent collector");

    int rejected = 0;
    for (int n = 0; n < 2048; ++n) {
        // Restore from the managed array root, including after a collection.
        runtime.array_read(&value, runtime.root_word(&replacements), layout, true, 0);
        failure = {};
        {
            managed_test::AllocationFailure injected(n, true);
            status = invoke(&value, &keys, size, failure);
        }
        if (!failure.message) {
            require(!status && runtime.string_bytes(value) == expected && rejected,
                    "allocation sweep did not publish exactly one complete result");
            std::cout << rejected << " translation ABI allocation failures preserved aliased output\n";
            return;
        }
        ++rejected;
        require(value == original && runtime.string_bytes(value) == source,
                "translation allocation failure modified input");
        require(runtime.collect() == vd::Runtime::Collection::completed, "translation leaked a private root");
        runtime.list_write(runtime.root_word(&keys), vd::ScalarEncoding::i32, 0, 'a');
        runtime.array_write(runtime.root_word(&replacements), layout, true, 0, &value);
    }
    throw std::runtime_error("translation allocation sweep did not reach success");
}

void execute(const vd::ValidatedAnalysis& program, bool optimized) {
    vd::Jit jit;
    auto code = jit.compile(program, optimized);
    const auto runtime = code->runtime_owner();
    unsigned failures = 0;
    for (int scenario = 0; scenario < 20; ++scenario) {
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
        translation_ownership();
        for (bool optimized : {false, true}) {
            execute(*analysis.program, optimized);
        }
        std::cout << checks << " ownership/root checks passed\n";
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
