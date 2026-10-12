#include "void/backend.hpp"
#include "void/runtime.hpp"
#include "proof.hpp"
#include <future>
#include <latch>
#include <iostream>
#include <stdexcept>
namespace {
std::latch* signalled;
extern "C" std::int32_t signal_running(std::int32_t value) { signalled->count_down(); return value; }
void numeric_loop(bool optimized) {
    auto runtime = std::make_shared<vd::Runtime>();
    vd::ir::Module module;
    module.identity = "numeric_runtime_loop"; module.imports = {"signal_running"};
    using namespace vd::ir;
    Function f; f.name = "entry"; f.locals = {Type::i32};
    f.blocks = {
        {{{Op::argument}, {Op::call,0,0,0,"signal_running"}, {Op::store,0,1}}, {Terminator::Kind::jump,0,1}},
        {{{Op::load,0}, {Op::constant,0,0,0}, {Op::greater,0,1}}, {Terminator::Kind::branch,2,2,3}},
        {{{Op::load,0}, {Op::constant,0,0,1}, {Op::subtract,0,1}, {Op::store,0,2}}, {Terminator::Kind::jump,0,1}},
        {{{Op::load,0}}, {Terminator::Kind::return_value,0}}
    };
    module.functions.push_back(f);
    vd::Jit jit; jit.bind("signal_running",signal_running);
    auto code = jit.compile(module,"entry",optimized,{},runtime);
    std::latch entered(1); signalled = &entered;
    auto worker = std::async(std::launch::async,[&] { return code->invoke(1000000); });
    entered.wait();
    {
        vd::Runtime::Attachment a(*runtime); vd::Runtime::Managed m(*runtime);
        for (int i = 0; i < 100; ++i) if (runtime->collect(std::chrono::seconds(2)) != vd::Runtime::Collection::completed)
            throw std::runtime_error("numeric-only generated loop did not cooperate");
        vd::Runtime::Native n(*runtime);
        if (worker.get() != 0) throw std::runtime_error("numeric loop result");
    }
    if (runtime->snapshot().cooperative_parks == 0) throw std::runtime_error("numeric loop never acknowledged a collection");
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
