#include "void/backend.hpp"
#include "void/runtime.hpp"
#include "void/runtime_abi.hpp"
#include "proof.hpp"
#include <future>
#include <semaphore>
#include <iostream>
#include <stdexcept>
namespace {
struct PollGate {
    std::binary_semaphore reached{0}, release{0};
    std::size_t polls = 0;
};
thread_local PollGate* poll_gate = nullptr;
}
// The test ABI object is the production abi.cpp, with ONLY this symbol renamed.
// No fake poll, native transition or substitute collector is used here.
extern "C" void vd_gc_real_poll() noexcept;
extern "C" void vd_gc_poll() noexcept {
    if (poll_gate) {
        ++poll_gate->polls;
        // Entry=1, first header=2, first body=3, backedge header=4.
        if (poll_gate->polls == 4) {
            if (!vd::Runtime::current().is_managed()) std::terminate();
            poll_gate->reached.release();
            // Outside every runtime lock. The observer releases us only after
            // the elected collector has published its request.
            poll_gate->release.acquire();
        }
    }
    vd_gc_real_poll();
}
namespace {
void numeric_loop(bool optimized) {
    auto runtime = std::make_shared<vd::Runtime>();
    vd::ir::Module module;
    module.identity = "numeric_runtime_loop";
    using namespace vd::ir;
    Function f; f.name = "entry"; f.locals = {Type::i32};
    f.blocks = {
        {{{Op::argument}, {Op::store,0,0}}, {Terminator::Kind::jump,0,1}},
        {{{Op::load,0}, {Op::constant,0,0,0}, {Op::greater,0,1}}, {Terminator::Kind::branch,2,2,3}},
        {{{Op::load,0}, {Op::constant,0,0,1}, {Op::subtract,0,1}, {Op::store,0,2}}, {Terminator::Kind::jump,0,1}},
        {{{Op::load,0}}, {Terminator::Kind::return_value,0}}
    };
    module.functions.push_back(f);
    vd::Jit jit;
    auto code = jit.compile(module,"entry",optimized,{},runtime);
    PollGate gate;
    constexpr std::int32_t iterations = 1000000;
    auto worker = std::async(std::launch::async,[&] {
        poll_gate = &gate;
        struct Clear { ~Clear() { poll_gate = nullptr; } } clear;
        return code->invoke(iterations);
    });
    // Release even on a failed assertion, before either future joins. This gate
    // deliberately holds a Managed participant, not a NativeSafe import.
    struct Release {
        PollGate& gate;
        bool released = false;
        void now() { if (!released) { released = true; gate.release.release(); } }
        ~Release() { now(); }
    } release{gate};
    if (!gate.reached.try_acquire_for(std::chrono::seconds(2)))
        throw std::runtime_error("numeric loop did not execute its backedge poll");
    auto collector = std::async(std::launch::async,[&] {
        vd::Runtime::Attachment a(*runtime); vd::Runtime::Managed m(*runtime);
        return runtime->collect(std::chrono::seconds(2));
    });
    const bool requested = runtime->wait_for_request(std::chrono::seconds(2));
    const auto pending = runtime->snapshot();
    release.now();
    if (collector.get() != vd::Runtime::Collection::completed || !requested ||
        !pending.requested || pending.threads != 2 || pending.parked != 1)
        throw std::runtime_error("numeric loop rendezvous did not overlap Managed execution");
    {
        vd::Runtime::Attachment a(*runtime); vd::Runtime::Managed m(*runtime);
        for (int i = 1; i < 100; ++i) if (runtime->collect(std::chrono::seconds(2)) != vd::Runtime::Collection::completed)
            throw std::runtime_error("numeric-only generated loop did not cooperate");
        vd::Runtime::Native n(*runtime);
        if (worker.get() != 0) throw std::runtime_error("numeric loop result");
    }
    const auto finished = runtime->snapshot();
    if (finished.cooperative_parks == 0) throw std::runtime_error("numeric loop never acknowledged a collection");
    if (finished.completed_epoch != 100 || finished.threads != 0 || finished.requested ||
        gate.polls != 2 * static_cast<std::size_t>(iterations) + 3)
        throw std::runtime_error("numeric loop poll coverage or rendezvous cleanup");
    std::cout << "numeric loop " << (optimized ? "O2" : "normal")
              << ": " << gate.polls << " polls, " << finished.completed_epoch
              << " collections, " << finished.cooperative_parks << " parks\n";
}
}
int main(int argc, char** argv) {
    try {
        auto module = runtime_test::proof();
        if (argc == 3) { vd::emit_runtime_object(module,argv[1],std::string(argv[2]) == "optimized"); return 0; }
        auto rejects = [](const vd::ir::Module& invalid) {
            try { vd::ir::validate(invalid); } catch (const std::invalid_argument&) { return; }
            throw std::runtime_error("invalid runtime IR accepted");
        };
        auto invalid = module; invalid.coordinated = false; rejects(invalid);
        invalid = module; invalid.functions[0].blocks[0].instructions[2].immediate = 9; rejects(invalid);
        invalid = module; invalid.functions[0].blocks[0].instructions[2].b = 0; rejects(invalid);
        for (bool optimized : {false,true}) {
            numeric_loop(optimized);
            auto runtime = std::make_shared<vd::Runtime>();
            vd::Jit jit;
            auto code = jit.compile(module,"entry",optimized,{},runtime);
            auto failure = jit.compile(module,"failure",optimized,{},runtime);
            auto receiver = jit.compile(module,"entry_receiver",optimized,{},runtime);
            for (int i = 0; i < 100; ++i) {
                if (code->invoke(i) != i*2+7) throw std::runtime_error("generated nested roots/handoff result");
                if (receiver->invoke(i) != i*2+1) throw std::runtime_error("nested receiver mutation/copy independence");
                bool failed = false;
                try { failure->invoke(i); } catch (const std::runtime_error& e) { failed = std::string(e.what()).find("zero") != std::string::npos; }
                if (!failed) throw std::runtime_error("generated checked failure lost");
                vd::Runtime::Attachment attachment(*runtime);
                vd::Runtime::Managed managed(*runtime);
                if (runtime->collect() != vd::Runtime::Collection::completed || runtime->snapshot().objects != 0)
                    throw std::runtime_error("generated frame/failure cleanup leaked");
            }
            // A distinct runtime is not a replacement for an existing binding.
            {
                vd::ReloadSlot slot(code);
                if (slot.replace(jit.compile(module,"entry",optimized,{},std::make_shared<vd::Runtime>())))
                    throw std::runtime_error("runtime-affinity mismatch accepted");
                if (slot.replace(jit.compile(runtime_test::proof(true),"entry",optimized,{},runtime)))
                    throw std::runtime_error("managed nominal layout mismatch accepted");
            }
            if (code->invoke(12) != 31) throw std::runtime_error("final generation result");
            // Heap descriptors are numeric owned copies, independent of LLVM
            // context/generation metadata even after every allocating lease dies.
            code.reset(); failure.reset(); receiver.reset();
            if (jit.live_generations() != 0 || runtime->snapshot().objects == 0) throw std::runtime_error("object/code lifetime proof precondition");
            vd::Runtime::Attachment a(*runtime); vd::Runtime::Managed m(*runtime);
            if (runtime->collect() != vd::Runtime::Collection::completed || runtime->snapshot().objects != 0)
                throw std::runtime_error("retired-generation object descriptor lifetime");
        }
        std::cout << "shared IR JIT roots: normal/O2, nested aggregates, temporaries, parameters, returns, loops, failure cleanup passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
