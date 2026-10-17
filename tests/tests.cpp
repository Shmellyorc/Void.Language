#include "void/backend.hpp"
#include "void/heap.hpp"
#include "void/native.hpp"
#include "void/reference_capabilities.hpp"
#include "native_fixture.h"
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
void session_tests(const std::filesystem::path& root);
void tooling_tests(const std::filesystem::path& root);
void analysis_tests(const std::filesystem::path& root);
void numeric_tests(const std::filesystem::path& root);
void frontend_tests(const std::filesystem::path& root);
void module_tests(const std::filesystem::path& output);
namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class F> void rejects(F operation) {
    bool rejected = false;
    try { operation(); } catch (const std::exception&) { rejected = true; }
    require(rejected, "invalid input accepted");
}
void core() {
    // Logical reference paths cross inline values but stop at heap identity.
    // Self-referential managed graphs must not recursively expand the shape.
    using namespace vd::ir;
    auto node=std::make_shared<ValueType>(); node->identity="shape::node"; node->name="node"; node->managed=true;
    Type ref(node,Type::managed_ref); ref.nullable=true;
    node->fields={{"next",ref},{"scalar",Type::i32}};
    auto pair=std::make_shared<ValueType>(); pair->identity="shape::pair"; pair->name="pair";
    pair->fields={{"left",ref},{"right",ref},{"scalar",Type::i32}};
    Type pair_type(pair);
    auto outer=std::make_shared<ValueType>(); outer->identity="shape::outer"; outer->name="outer";
    outer->fields={{"nested",pair_type},{"direct",ref}};
    Type outer_type(outer);
    require(reference_paths(outer_type)==ReferencePaths{{0,0},{0,1},{1}},"finite nested reference shape");
    auto mixed=with_readonly_paths(outer_type,{{0,1}});
    require(mixed==outer_type && !(mixed<outer_type) && !(outer_type<mixed),"capabilities changed nominal identity");
    require(readonly_paths(field_type(field_type(mixed,0),1))==ReferencePaths{{}},"nested readonly projection");
    require(readonly_paths(field_type(field_type(mixed,0),0)).empty(),"unrelated reference authority lost");
    require(!capability_assignable(mixed,outer_type) && capability_assignable(outer_type,mixed),"readonly capability assignability");
    auto replaced=with_field_type(mixed,0,pair_type);
    require(readonly_paths(replaced).empty(),"aggregate reference profile replacement");
    auto deep=readonly_view(outer_type);
    require(readonly_paths(deep)==reference_paths(outer_type),"deep readonly aggregate view");
    require(readonly_paths(field_type(readonly_view(ref),0))==ReferencePaths{{}},"deep readonly heap edge");
    auto stage=ref; stage.nullable=false; stage.construction=true;
    require(reference_paths(stage).empty(),"construction view became managed reference leaf");
    require(fact_type(readonly_view(stage),{}).readonly && !capability_assignable(readonly_view(stage),stage),"construction-place readonly capability was discarded");
    Module capability_ir; capability_ir.identity="capability-proof"; capability_ir.coordinated=true;
    Function pack; pack.name="pack"; pack.parameters={readonly_view(ref)}; pack.result=pair_type;
    pack.blocks={{{{Op::argument},{Op::constant,0,0,0,{}, {},ref},{Op::constant,0,0,1},
        {Op::aggregate,0,0,0,{}, {},pair_type,{0,1,2}}},{Terminator::Kind::return_value,3}}};
    capability_ir.functions={pack};
    rejects([&] { validate(capability_ir); }); // Aggregate result cannot launder its first leaf.
    capability_ir.functions[0].result=with_readonly_paths(pair_type,{{0}});
    InstructionTypes capability_types; validate(capability_ir,&capability_types);
    require(readonly_paths(capability_types[0][0][3])==ReferencePaths{{0}},"IR aggregate failed to derive leaf capabilities");
    auto forged=capability_ir;
    forged.functions[0].locals={pair_type};
    forged.functions[0].blocks[0].instructions.push_back({Op::store,0,3});
    rejects([&] { validate(forged); }); // A declared mutable slot cannot accept the carrier.
    Function select; select.name="select"; select.parameters={pair_type}; select.locals={pair_type};
    select.receiver=true; select.result=ref;
    select.blocks={{{{Op::load,0},{Op::extract,0}},{Terminator::Kind::return_value,1}}};
    Function caller; caller.name="caller"; caller.parameters={readonly_view(pair_type)};
    caller.locals={readonly_view(pair_type),ref}; caller.result=Type::i32;
    Instruction invocation{Op::call}; invocation.callee="select"; invocation.receiver=Place{0,{}};
    caller.blocks={{{{Op::argument},{Op::store,0,0},invocation,{Op::store,1,2},{Op::constant,0,0,1}},
        {Terminator::Kind::return_value,4}}};
    capability_ir.functions={select,caller};
    rejects([&] { validate(capability_ir); }); // A readonly receiver must qualify a method's returned reference.
    capability_ir.functions[1].locals[1]=readonly_view(ref);
    validate(capability_ir);
    Function weaken;weaken.name="weaken";weaken.parameters={ref};weaken.result=readonly_view(ref);
    weaken.blocks={{{{Op::argument}},{Terminator::Kind::return_value,0}}};
    Function dropped;dropped.name="dropped";dropped.parameters={ref};dropped.locals={ref};dropped.result=Type::i32;
    dropped.blocks={{{{Op::argument},{Op::call,0,0,0,"weaken",{}, {},{0}},{Op::store,0,1},{Op::constant,0,0,0}},
        {Terminator::Kind::return_value,3}}};
    auto interface_ir=capability_ir;interface_ir.functions={weaken,dropped};
    rejects([&] {validate(interface_ir);});
    interface_ir.functions[1].locals[0]=readonly_view(ref);validate(interface_ir);
    auto invalid_shape=capability_ir;
    invalid_shape.functions[1].locals[0].readonly_references.insert({9});
    rejects([&] { validate(invalid_shape); });
    auto heap_shape=std::make_shared<ValueType>();heap_shape->identity="shape::heap";heap_shape->name="heap";heap_shape->managed=true;
    heap_shape->fields={{"payload",pair_type}};
    Type heap_ref(heap_shape,Type::managed_ref);
    Function publish;publish.name="publish";publish.parameters={readonly_view(pair_type)};publish.result=heap_ref;
    publish.blocks={{{{Op::argument},{Op::gc_construct,0,0,0,{}, {},heap_ref,{0}}},{Terminator::Kind::return_value,1}}};
    capability_ir.functions={publish};
    rejects([&] { validate(capability_ir); }); // Qualification cannot be hidden in a heap payload.
    publish.parameters={heap_ref,readonly_view(pair_type)};publish.result=Type::i32;
    publish.blocks={{{{Op::argument},{Op::argument,1},{Op::gc_write,0,1},{Op::constant,0,0,0}},
        {Terminator::Kind::return_value,3}}};capability_ir.functions={publish};
    rejects([&] { validate(capability_ir); });
    Function read;read.name="read";read.parameters={readonly_view(heap_ref)};read.result=ref;
    read.blocks={{{{Op::argument},{Op::gc_read,0},{Op::extract,1}},{Terminator::Kind::return_value,2}}};
    capability_ir.functions={read};
    rejects([&] { validate(capability_ir); }); // Readonly must cross the heap -> inline -> reference path.
    capability_ir.functions[0].result=readonly_view(ref);validate(capability_ir);
    auto required_ref=ref;required_ref.nullable=false;
    auto required_value=std::make_shared<ValueType>();required_value->identity="shape::required";required_value->name="required";
    required_value->fields={{"child",required_ref}};
    Function null_nested;null_nested.name="null_nested";null_nested.parameters={};null_nested.result=Type(required_value);
    null_nested.blocks={{{{Op::constant,0,0,0,{}, {},required_ref},{Op::aggregate,0,0,0,{}, {},Type(required_value),{0}}},
        {Terminator::Kind::return_value,1}}};capability_ir.functions={null_nested};
    rejects([&] { validate(capability_ir); });
    null_nested.locals={required_ref};
    null_nested.blocks[0].instructions[0]={Op::load,0};capability_ir.functions={null_nested};
    rejects([&] { validate(capability_ir); }); // Zeroed root storage is not a valid nonnullable default.
    Function boxed_stage;boxed_stage.name="boxed_stage";boxed_stage.parameters={stage};boxed_stage.locals={stage};boxed_stage.construction_origins={{0,0}};boxed_stage.result=pair_type;
    boxed_stage.blocks={{{{Op::argument},{Op::constant,0,0,0,{}, {},ref},{Op::constant,0,0,0},
        {Op::aggregate,0,0,0,{}, {},pair_type,{0,1,2}}},{Terminator::Kind::return_value,3}}};capability_ir.functions={boxed_stage};
    rejects([&] { validate(capability_ir); });
    vd::SourceFile source("/main.void", "func main():\r\n    return 42\r\n");
    auto pos = source.location(18);
    require(pos.line == 2 && pos.column == 5, "CRLF source coordinates");
    require(source.diagnostic(18, "example").find("/main.void:2:5: error: example") == 0, "source diagnostic");
    rejects([&] { source.location(1000); });
    auto module = vd::ir::architecture_proof();
    vd::ir::validate(module);
    module.functions[0].blocks[0].instructions[2].a = 999;
    rejects([&] { vd::ir::validate(module); });
    module = vd::ir::architecture_proof();
    module.functions[1].blocks[0].end.value = 0;
    rejects([&] { vd::ir::validate(module); });
    module = vd::ir::architecture_proof();
    module.functions.push_back(module.functions.front());
    rejects([&] { vd::ir::validate(module); });
    module = vd::ir::architecture_proof();
    module.functions[1].blocks[2].instructions[1].callee = "missing";
    rejects([&] { vd::ir::validate(module); });
}
void jit() {
    vd::Jit engine;
    for (bool optimized : {false, true}) {
        auto code = engine.compile(vd::ir::architecture_proof(), "entry", optimized);
        require(code->invoke(0) == 42 && code->invoke(10) == 52 && code->invoke(-1) == 7, "JIT branch/call result");
    }
    for(bool optimized:{false,true}) {
        vd::Jit linked;
        auto provider=linked.compile(vd::ir::architecture_proof(40),"entry",optimized);
        // A separate LLVM module imports an existing non-entry native export.
        // Keep the provider lease alive for the consumer's execution.
        vd::ir::Function caller{"entry",{"cross-module.void",1,1},{{
            {{vd::ir::Op::argument},{vd::ir::Op::call,0,0,0,"calculate__v1"}},
            {vd::ir::Terminator::Kind::return_value,1}}}};
        vd::ir::Module consumer{"/consumer.void",{"calculate__v1"},{caller}};
        auto code=linked.compile(consumer,"entry",optimized);
        require(code->invoke(2)==42,"cross-LLVM-module export resolution failed");
    }
    require(engine.live_generations() == 0, "JIT code not retired");
    rejects([&] { engine.compile(vd::ir::architecture_proof(), "missing"); });
    // A lease owns the execution session even after its compiler host is destroyed.
    std::shared_ptr<const vd::NativeCode> survivor;
    { vd::Jit temporary; survivor = temporary.compile(vd::ir::architecture_proof(), "entry"); }
    require(survivor->invoke(0) == 42, "JIT session lifetime");
}
std::mutex gate_mutex;
std::condition_variable gate_cv;
bool entered = false, released = false;
extern "C" std::int32_t blocked_native(std::int32_t value) {
    std::unique_lock lock(gate_mutex);
    entered = true; gate_cv.notify_all();
    gate_cv.wait(lock, [] { return released; });
    return value;
}
void reload() {
    vd::Jit engine;
    engine.bind("blocked_native", &blocked_native);
    auto old_module = vd::ir::architecture_proof(42);
    old_module.imports = {"blocked_native"};
    old_module.functions[0].blocks[0].instructions.push_back({vd::ir::Op::call, 2, 0, 0, "blocked_native"});
    old_module.functions[0].blocks[0].end.value = 3;
    vd::ReloadSlot slot(engine.compile(old_module, "entry"));
    auto pending = std::async(std::launch::async, [&] { return slot.invoke(0); });
    struct ReleaseOnExit {
        ~ReleaseOnExit() {
            { std::lock_guard lock(gate_mutex); released = true; }
            gate_cv.notify_all();
        }
    } release_on_exit;
    {
        std::unique_lock lock(gate_mutex);
        if (!gate_cv.wait_for(lock, std::chrono::seconds(10), [] { return entered; })) {
            released = true; gate_cv.notify_all();
            throw std::runtime_error("old native call did not enter");
        }
    }
    auto next = engine.compile(vd::ir::architecture_proof(100), "entry");
    require(slot.replace(next), "compatible body rejected");
    require(slot.invoke(0) == 100 && engine.live_generations() == 2, "active generation retired early");
    {
        std::lock_guard lock(gate_mutex); released = true;
    }
    gate_cv.notify_all();
    require(pending.get() == 42, "active old call changed behavior");
    require(engine.live_generations() == 1, "old code not retired after return");
    for (int field = 0; field < 3; ++field) {
        vd::Compatibility contract{"/architecture.void::entry"};
        if (field == 0) contract.layout = 1;
        if (field == 1) contract.captures = 1;
        if (field == 2) contract.constants = 1;
        auto bad = engine.compile(vd::ir::architecture_proof(999), "entry", true, contract);
        require(!slot.replace(bad) && slot.invoke(0) == 100, "incompatible edit published");
    }
    rejects([&] { engine.compile(vd::ir::architecture_proof(), "entry", true, {"/architecture.void::entry", "c:i64(i64)"}); });
    auto invalid = vd::ir::architecture_proof();
    invalid.functions[0].blocks.clear();
    rejects([&] { engine.compile(invalid, "entry"); });
    require(slot.invoke(0) == 100, "invalid source disturbed active code");
    next.reset();
    std::atomic<bool> bad_result = false;
    std::atomic<unsigned> calls = 0;
    std::atomic<bool> stop = false;
    std::vector<std::thread> readers;
    // Declare the guard before spawning: exceptions during either thread creation
    // or JIT compilation must stop and join every successfully started caller.
    struct JoinReaders {
        std::atomic<bool>& stop;
        std::vector<std::thread>& readers;
        void finish() {
            stop.store(true);
            for (auto& reader : readers) if (reader.joinable()) reader.join();
        }
        ~JoinReaders() { finish(); }
    } join_readers{stop, readers};
    for (int i = 0; i < 4; ++i) readers.emplace_back([&] {
        while (!stop.load()) {
            int answer = slot.invoke(0);
            ++calls;
            if (answer != 100 && answer != 42 && answer != 43) bad_result = true;
        }
    });
    for (int i = 0; i < 100; ++i)
        require(slot.replace(engine.compile(vd::ir::architecture_proof(42 + i % 2), "entry")), "stress replacement rejected");
    join_readers.finish();
    require(calls > 0 && !bad_result && engine.live_generations() == 1, "reload stress failure");
    // Exercise the same cleanup path when compilation fails mid-stress.
    stop = false;
    readers.clear();
    bool injected_failure = false;
    try {
        JoinReaders cleanup{stop, readers};
        readers.emplace_back([&] { while (!stop.load()) std::this_thread::yield(); });
        throw std::runtime_error("injected development compile failure");
    } catch (const std::runtime_error&) { injected_failure = true; }
    require(injected_failure && stop.load() && !readers.front().joinable(), "reader exception cleanup failed");
}
thread_local vd::Heap* probe_heap = nullptr;
struct Node { void* child; std::int32_t payload; };
const vd::TypeLayout node_layout{sizeof(Node), {offsetof(Node, child)}};
extern "C" void* vd_probe_allocate() {
    auto* p = probe_heap->allocate(node_layout);
    std::int32_t payload = 123;
    std::memcpy(static_cast<char*>(p) + offsetof(Node, payload), &payload, sizeof(payload));
    auto* child = probe_heap->allocate(node_layout);
    probe_heap->store(p, offsetof(Node, child), child);
    probe_heap->store(child, offsetof(Node, child), p);
    return p;
}
extern "C" void vd_probe_enter(void** slot) { probe_heap->add_root(slot); }
extern "C" void vd_probe_leave(void** slot) { probe_heap->remove_root(slot); }
extern "C" void vd_probe_poll() {
    auto* a = probe_heap->allocate(node_layout);
    auto* b = probe_heap->allocate(node_layout);
    probe_heap->store(a, 0, b); probe_heap->store(b, 0, a);
    probe_heap->collect();
}
extern "C" std::int32_t vd_probe_inspect(void* p) {
    if (!probe_heap->contains(p) || probe_heap->size() != 2) return -1;
    std::int32_t value = 0;
    std::memcpy(&value, static_cast<char*>(p) + offsetof(Node, payload), sizeof(value));
    return value;
}
void gc() {
    vd::Heap heap;
    rejects([&] { heap.allocate({1, {0}}); });
    rejects([&] { heap.allocate({32, {0, 1}}); });
    {
        auto* parent = heap.allocate({32, {16}}); // Nested inline value reference.
        auto* child = heap.allocate(node_layout);
        heap.store(parent, 16, child);
        vd::Root root(heap, parent);
        heap.collect();
        require(heap.contains(child) && heap.size() == 2, "inline value reference not traced");
        root.reset(nullptr);
        heap.collect();
        require(heap.size() == 0, "dead graph retained");
    }
    auto* valid = heap.allocate(node_layout);
    {
        vd::Root good(heap, valid);
        vd::Root invalid(heap, &heap);
        rejects([&] { heap.collect(); });
        require(heap.contains(valid), "invalid root caused a partial sweep");
    }
    heap.collect();
    require(heap.size() == 0, "failed collection left stale mark bits");
    vd::Jit engine;
    engine.bind("vd_probe_allocate", &vd_probe_allocate);
    engine.bind("vd_probe_enter", &vd_probe_enter);
    engine.bind("vd_probe_leave", &vd_probe_leave);
    engine.bind("vd_probe_poll", &vd_probe_poll);
    engine.bind("vd_probe_inspect", &vd_probe_inspect);
    probe_heap = &heap;
    for (bool optimized : {false, true}) {
        auto code = engine.compile_root_probe(optimized);
        for (int n = 0; n < 20; ++n) {
            require(code->invoke(500) == 123, "generated root lost across safepoints");
            heap.collect();
            require(heap.size() == 0, "unrooted cycle not reclaimed");
        }
    }
    probe_heap = nullptr;
}
struct CallbackContext {
    vd::ReloadSlot& slot;
    vd::Root captured;
    vd::Heap& heap;
};
extern "C" std::int32_t callback(void* context, std::int32_t value) {
    auto& capture = *static_cast<CallbackContext*>(context);
    if (!capture.heap.contains(capture.captured.get())) return -999;
    return capture.slot.invoke(value);
}
void native() {
    vd::Jit engine;
    engine.bind("fixture_twice", &fixture_twice);
    auto module = vd::ir::architecture_proof();
    module.imports = {"fixture_twice"};
    module.functions[0].blocks[0] = {{{vd::ir::Op::argument}, {vd::ir::Op::call, 0, 0, 0, "fixture_twice"}},
                                    {vd::ir::Terminator::Kind::return_value, 1}};
    require(engine.compile(module, "entry")->invoke(21) == 42, "JIT to real C call");
    auto missing = module; missing.imports = {"missing_native"};
    missing.functions[0].blocks[0].instructions[1].callee = "missing_native";
    rejects([&] { engine.compile(missing, "entry"); });
    auto text = vd::native_utf8("VOID \xc3\xa9 \xf0\x9f\x8c\x8d");
    require(fixture_text_length(text.c_str()) == text.size(), "UTF-8 borrowed native text");
    rejects([] { vd::native_utf8(std::string("a\0b", 3)); });
    for (auto bad : {"\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82", "\x80"})
        rejects([&] { vd::native_utf8(bad); });
    vd::ReloadSlot slot(engine.compile(vd::ir::architecture_proof(), "entry"));
    vd::Heap captured_heap;
    using Owner = std::unique_ptr<fixture_resource, decltype(&fixture_destroy)>;
    {
        Owner resource(fixture_create(), &fixture_destroy);
        require(resource != nullptr && fixture_resources() == 1, "opaque resource creation");
        CallbackContext context{slot, vd::Root(captured_heap, captured_heap.allocate(node_layout)), captured_heap};
        fixture_register(resource.get(), &callback, &context);
        captured_heap.collect();
        require(captured_heap.size() == 1, "native callback capture lost its root");
        require(fixture_invoke(resource.get(), 0) == 42, "C callback into JIT");
        require(slot.replace(engine.compile(vd::ir::architecture_proof(99), "entry")), "callback replacement");
        require(fixture_invoke(resource.get(), 0) == 99, "callback lost stable identity");
        fixture_unregister(resource.get());
        require(fixture_invoke(resource.get(), 0) == -1, "callback unregister");
    }
    require(fixture_resources() == 0, "native resource not released exactly once");
    captured_heap.collect();
    require(captured_heap.size() == 0, "native callback capture retained after unregister and scope exit");
}
}
int main(int argc, char** argv) {
    try {
        if (argc < 2) throw std::runtime_error("test suite required");
        std::string suite = argv[1];
        if (suite == "sessions" && argc == 3) session_tests(argv[2]);
        else if (suite == "tooling" && argc == 3) tooling_tests(argv[2]);
        else if (suite == "analysis" && argc == 3) analysis_tests(argv[2]);
        else if (suite == "numeric" && argc == 3) numeric_tests(argv[2]);
        else if (suite == "frontend" && argc == 3) frontend_tests(argv[2]);
        else if (suite == "modules" && argc == 3) module_tests(argv[2]);
        else if (argc != 2) throw std::runtime_error("unexpected suite arguments");
        else if (suite == "core") core(); else if (suite == "jit") jit();
        else if (suite == "reload") reload(); else if (suite == "gc") gc();
        else if (suite == "native") native(); else throw std::runtime_error("unknown suite");
        std::cout << suite << ": passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
