#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/runtime.hpp"
#include "void/async.hpp"
#include "void/http_resource.hpp"
#include "../../managed/probe.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
int main(int argc, char** argv) {
    try {
        if (argc < 3) throw std::runtime_error("mode and HTTP source required");
        auto analysis = vd::analyze_project(argv[2]);
        if (!analysis.succeeded()) throw vd::CompilationError(analysis.diagnostics);
        if (std::string_view(argv[1]) == "--emit-async") {
            if (argc != 5) throw std::runtime_error("HTTP source, output object and optimization required");
            vd::emit_async_object(analysis.program->lower(), argv[3], std::string_view(argv[4]) == "optimized");
            return 0;
        }
        if (argc != 3) throw std::runtime_error("Unexpected HTTP runner arguments");
        for (bool optimize : {false, true}) {
            vd::Jit jit;
            auto runtime = std::make_shared<vd::Runtime>(256);
            runtime->async_worker_limit(1);
            auto module = analysis.program->lower();
            const auto mode = std::string_view(argv[1]);
            module.async_root = mode == "--async" || mode == "--faults-async";
            auto code = jit.compile(module, module.entry_symbol, optimize, {}, runtime);
            if (mode == "--faults" || mode == "--faults-async") {
                if (code->invoke(7) != 7) throw std::runtime_error("HTTP fault fixture warmup failed");
                bool succeeded = false; unsigned failures = 0;
                for (int budget = 0; budget < 4096 && !succeeded; ++budget) {
                    managed_test::Probe probe; probe.inspect_roots = true; probe.frames.reserve(8192);
                    std::cerr << "HTTP allocation budget " << budget << (optimize ? " O2\n" : " normal\n");
                    {
                        managed_test::Observe observe(probe);
                        try { managed_test::AllocationFailure fault(budget); succeeded = code->invoke(7) == 7; }
                        catch (const std::exception&) { ++failures; }
                    }
                    if (!probe.roots_valid || !probe.commits_valid || !probe.frames.empty()) throw std::runtime_error("HTTP allocation failure root/commit imbalance");
                    vd::Runtime::Attachment attached(*runtime); vd::Runtime::Managed managed(*runtime); runtime->collect();
                    const auto native = vd::http::runtime_driver(*runtime)->driver().snapshot();
                    if (runtime->snapshot().objects || runtime->memory_snapshot().handles > 1 || native.clients || native.transfers)
                        throw std::runtime_error("HTTP allocation failure leaked source/native resources");
                }
                if (!succeeded || !failures || code->invoke(7) != 7) throw std::runtime_error("HTTP allocation sweep did not recover");
                { vd::Runtime::Attachment attached(*runtime); vd::Runtime::Managed managed(*runtime); runtime->collect(); }
                std::cout << (optimize ? "O2" : "normal") << " HTTP: " << failures << " checked allocation failures recovered\n";
                continue;
            }
            managed_test::Probe probe; probe.collect_before = true; probe.inspect_roots = true; probe.frames.reserve(8192);
            std::size_t samples = 0, minimum = static_cast<std::size_t>(-1), maximum = 0, tasks = 0, handles = 0;
            probe.before_construct = [&](std::size_t count, const void*, std::size_t) {
                if (count < 512 || count % 256) return;
                if (runtime->collect() != vd::Runtime::Collection::completed) std::terminate();
                const auto memory = runtime->memory_snapshot(); const auto active = vd::async_snapshot();
                ++samples; minimum = std::min(minimum, memory.heap.payload_bytes); maximum = std::max(maximum, memory.heap.payload_bytes);
                tasks = std::max(tasks, active.tasks); handles = std::max(handles, memory.handles);
                std::cout << "{\"source_http_sample\":{\"constructions\":" << count << ",\"heap_payload\":" << memory.heap.payload_bytes
                    << ",\"tasks\":" << active.tasks << ",\"groups\":" << active.groups << ",\"work\":" << active.work
                    << ",\"handles\":" << memory.handles << "}}\n";
            };
            {
                managed_test::Observe observe(probe);
                if (code->invoke(7) != 7) throw std::runtime_error("HTTP source returned a failed assertion");
            }
            if (!probe.roots_valid || !probe.commits_valid || !probe.frames.empty()) throw std::runtime_error("HTTP precise root/commit imbalance");
            if (module.async_root && (samples < 4 || maximum - minimum > 524288 || tasks > 64 || handles > 64))
                throw std::runtime_error("HTTP source pull lifetime window: samples=" + std::to_string(samples) +
                    ", payload=" + std::to_string(minimum) + ".." + std::to_string(maximum) +
                    ", tasks=" + std::to_string(tasks) + ", handles=" + std::to_string(handles));
            {
                vd::Runtime::Attachment attached(*runtime); vd::Runtime::Managed managed(*runtime);
                runtime->collect();
                if (runtime->snapshot().objects) throw std::runtime_error("HTTP source retained resource/result roots after drain: " + std::to_string(runtime->snapshot().objects));
                // The idle Runtime driver has one registered null Handle until
                // Runtime shutdown. It roots no managed value; count its actual
                // retirement separately from source result/resource retirement.
                { vd::Runtime::Native safe(*runtime);
                  if (runtime->close()) throw std::runtime_error("HTTP source Runtime shutdown failed"); }
                if (runtime->memory_snapshot().handles) throw std::runtime_error("HTTP native ledger retained Handles after Runtime shutdown");
            }
            std::cout << (optimize ? "O2" : "normal") << " HTTP source, one worker, forced GC and root retirement passed\n";
        }
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
