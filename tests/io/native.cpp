#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/detail/filesystem_hooks.hpp"
#include "../managed/probe.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <semaphore>
#include <stdexcept>
#include <thread>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}

namespace {
#ifndef IO_AOT_PROBE
void closing_during_open(const char* source) {
    auto program = vd::analyze_project(source);
    if (!program.succeeded()) throw vd::CompilationError(program.diagnostics);
    for (bool optimize : {false, true}) {
        vd::Jit jit;
        auto code = jit.compile_async(*program.program, optimize);
        auto runtime = code->runtime_owner();
        runtime->async_worker_limit(1);
        std::binary_semaphore entered{0};
        std::binary_semaphore release{0};
        std::atomic<unsigned> closes{0};
        auto hooks = std::make_shared<vd::filesystem::testing::Hooks>();
        hooks->before = [&](vd::filesystem::Operation operation) {
            if (operation == vd::filesystem::Operation::close) ++closes;
            if (operation == vd::filesystem::Operation::open) {
                entered.release();
                release.acquire();
            }
        };
        auto invocation = std::async(std::launch::async, [&] {
            vd::filesystem::testing::Observe observe(hooks);
            try {
                return std::to_string(code->invoke(7));
            } catch (const std::runtime_error& failure) {
                return std::string(failure.what());
            }
        });
        require(entered.try_acquire_for(std::chrono::seconds(15)), "opening did not enter native work");
        auto shutdown = std::async(std::launch::async, [&] { return runtime->close(); });
        bool drained = shutdown.wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout;
        release.release();
        auto status = shutdown.get();
        auto outcome = invocation.get();
        require(drained, "Runtime shutdown returned before an entered open drained");
        require(!status && closes == 1, "shutdown/open rollback did not close exactly once");
        bool checked_failure = outcome.find("shutdown") != std::string::npos ||
                               outcome.find("allocation") != std::string::npos ||
                               outcome.find("publication") != std::string::npos;
        if (outcome != "7" && !checked_failure) {
            throw std::runtime_error("shutdown/open result lost its checked failure: " + outcome);
        }
        std::cout << (optimize ? "O2" : "JIT")
                  << ": Runtime shutdown drained an entered open and retired its acquisition once\n";
    }
}

void close_failures(const char* source, bool shutdown) {
    auto program = vd::analyze_project(source);
    if (!program.succeeded()) throw vd::CompilationError(program.diagnostics);
    for (bool optimize : {false, true}) {
        vd::Jit jit;
        auto code = jit.compile(*program.program, optimize);
        auto runtime = code->runtime_owner();
        unsigned closes = 0;
        auto hooks = std::make_shared<vd::filesystem::testing::Hooks>();
        hooks->close_error = 5;
        hooks->before = [&](vd::filesystem::Operation operation) {
            if (operation == vd::filesystem::Operation::close) ++closes;
        };
        vd::filesystem::testing::Observe observe(hooks);
        require(code->invoke(7) == 7, "source close failure or forgotten resource result");
        {
            vd::Runtime::Attachment attachment(*runtime);
            if (shutdown) {
                {
                    vd::Runtime::Managed managed(*runtime);
                    require(runtime->collect() == vd::Runtime::Collection::completed &&
                            runtime->snapshot().objects > 0, "forgotten file lost its ledger root");
                }
                auto error = runtime->close();
                require(error.kind == vd::console::ErrorKind::io_failure && error.os_code == 5,
                        "Runtime shutdown lost a native close diagnostic");
                require(!runtime->close(), "repeated shutdown retained a retired file");
            } else {
                vd::Runtime::Managed managed(*runtime);
                require(runtime->collect() == vd::Runtime::Collection::completed &&
                        runtime->snapshot().objects == 0, "closed file retained its root");
            }
        }
        require(closes == 1, "source or shutdown closed a native handle more than once");
        std::cout << (optimize ? "O2" : "JIT") << ": "
                  << (shutdown ? "forgotten-file shutdown" : "recoverable close")
                  << " failure retained its diagnostic, retired once and released roots\n";
    }
}
#endif

void exercise(const std::shared_ptr<vd::Runtime>& runtime,
              const std::function<std::int32_t()>& invoke, const char* label, bool cancelled = false) {
    std::ofstream("blocked file.bin", std::ios::binary) << "payload";
    runtime->async_worker_limit(1);
    auto owner = std::this_thread::get_id();
    std::binary_semaphore entered{0};
    std::binary_semaphore release{0};
    std::atomic<bool> held{false};
    std::atomic<bool> valid_worker{true};
    std::atomic<bool> gate_timeout{false};
    bool owner_progress = false;
    unsigned closes = 0;
    auto hooks = std::make_shared<vd::filesystem::testing::Hooks>();
    hooks->before = [&](vd::filesystem::Operation operation) {
        if (operation == vd::filesystem::Operation::close) ++closes;
        if (operation != vd::filesystem::Operation::read_bytes || held.exchange(true)) return;
        valid_worker = std::this_thread::get_id() != owner && !vd::Runtime::current().is_managed();
        entered.release();
        if (valid_worker && !release.try_acquire_for(std::chrono::seconds(15))) {
            gate_timeout = true;
        }
    };
    managed_test::Probe probe;
    probe.inspect_roots = true;
    probe.frames.reserve(8192);
    probe.before_construct = [&](std::size_t, const void* payload, std::size_t bytes) {
        if (bytes != sizeof(std::int32_t)) return;
        std::int32_t value = 0;
        std::memcpy(&value, payload, sizeof(value));
        if (value != 1234567) return;
        require(std::this_thread::get_id() == owner && runtime->is_managed(), "marker ran off owner");
        struct Unblock {
            std::binary_semaphore& release;
            ~Unblock() { release.release(); }
        } unblock{release};
        {
            vd::Runtime::Native safe(*runtime);
            require(entered.try_acquire_for(std::chrono::seconds(10)), "native read did not enter");
        }
        owner_progress = true;
    };
    {
        vd::filesystem::testing::Observe observe(hooks);
        managed_test::Observe source_probe(probe);
        std::string diagnostic;
        try {
            require(invoke() == 7, "blocked-file result");
        } catch (const std::runtime_error& failure) {
            diagnostic = failure.what();
        }
        require(cancelled ? diagnostic.find("Async task cancelled") != std::string::npos : diagnostic.empty(),
                diagnostic.empty() ? "entered native file cancellation disappeared" : diagnostic.c_str());
    }
    require(held && valid_worker && owner_progress && !gate_timeout, "real file job blocked the owner executor");
    require(probe.roots_valid && probe.commits_valid && probe.frames.empty(), "blocked-file roots");
    require(closes == 1, "native file close did not drain and retire exactly once");
    {
        vd::Runtime::Attachment attachment(*runtime);
        vd::Runtime::Managed managed(*runtime);
        require(runtime->collect() == vd::Runtime::Collection::completed &&
                runtime->snapshot().objects == 0, "blocked-file retained roots");
    }
    std::cout << label << ": owner ran another ready activation while "
                 "a real file read was held on the single bounded worker\n";
    std::filesystem::remove("blocked file.bin");
}
}

#ifdef IO_AOT_PROBE
extern "C" std::int32_t io_probe_entry(std::int32_t, vd::RuntimeFailure*);
extern "C" const vd::RuntimeTypeRecord __vd_types[];
extern "C" const std::size_t __vd_type_count;
extern "C" const vd::RuntimeWitnessRecord __vd_witnesses[];
extern "C" const std::size_t __vd_witness_count;
#endif

int main(int argc, char** argv) {
    try {
#ifdef IO_AOT_PROBE
        (void)argc;
        (void)argv;
        auto runtime = std::make_shared<vd::Runtime>();
        vd::RuntimeFailure admission;
        vd_runtime_admit_metadata(runtime.get(), __vd_types, __vd_type_count,
                                  __vd_witnesses, __vd_witness_count, &admission);
        require(!admission.message, "Native probe metadata admission");
        exercise(runtime, [&] {
            vd::RuntimeFailure failure;
            std::int32_t answer;
            {
                vd::Runtime::Attachment attachment(*runtime);
                vd::Runtime::Managed managed(*runtime);
                answer = io_probe_entry(7, &failure);
            }
            if (failure.message) throw std::runtime_error(failure.message);
            return answer;
        }, "linked AOT");
#else
        if (argc == 3) {
            if (std::string(argv[1]) == "--opening-shutdown") {
                closing_during_open(argv[2]);
                return 0;
            }
            close_failures(argv[2], std::string(argv[1]) == "--shutdown");
            return 0;
        }
        require(argc == 2, "responsive source required");
        auto program = vd::analyze_project(argv[1]);
        if (!program.succeeded()) throw vd::CompilationError(program.diagnostics);
        for (bool optimize : {false, true}) {
            vd::Jit jit;
            auto code = jit.compile_async(*program.program, optimize);
            bool cancelled = std::filesystem::path(argv[1]).stem() == "cancelled";
            exercise(code->runtime_owner(), [&] { return code->invoke(7); }, optimize ? "O2" : "JIT", cancelled);
        }
#endif
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
