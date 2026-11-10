#include "void/backend.hpp"
#include "void/c_primitive.hpp"
#include "void/frontend.hpp"
#include "void/runtime.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <iostream>
#include <iterator>
#include <semaphore>
#include <stdexcept>
#include <thread>

extern "C" vd::CPrimitiveThunk vd_c_primitive_dispatch_v2;

namespace {
using namespace vd;
using namespace vd::ir;

void require(bool okay, std::string_view message) {
    if (!okay) throw std::runtime_error(std::string(message));
}
template<class Test>
void reject(std::string_view label, std::string_view expected, Test&& test) {
    try { test(); }
    catch (const std::exception& failure) {
        require(std::string_view(failure.what()).find(expected) != std::string_view::npos,
            std::string(label) + " received " + failure.what());
        std::cout << label << ": " << failure.what() << '\n';
        return;
    }
    throw std::runtime_error(std::string(label) + " accepted");
}
std::string read(const char* path) {
    std::ifstream input(path, std::ios::binary);
    require(input.is_open(), "cannot read descriptor");
    return {std::istreambuf_iterator<char>(input), {}};
}
void bad_descriptors(const std::string& descriptor) {
    auto changed = [&](std::string_view from, std::string_view to, std::string_view expected) {
        auto copy = descriptor;
        auto position = copy.find(from);
        require(position != copy.npos, "descriptor marker missing");
        copy.replace(position, from.size(), to);
        reject(to, expected, [&] { (void)CPrimitiveAdmission::admit(copy, vd_c_primitive_dispatch_v2); });
    };
    changed("2 2", "2 1", "unsupported descriptor");
    changed("sample G2Sample 16 8 2", "sample G2Sample 16 4 2", "alignment");
    changed("packet G2Packet 32 8 4", "packet G2Packet 31 8 4", "size");
    changed("reading f64 8", "reading f64 4", "offset");
    changed("reading f64 8", "reading ptr 8", "unsupported");
    changed("reading f64 8", "reading u64 8", "provider rejected");
    changed("tag tag i32", "tag tag u32", "provider rejected");
    changed("array:u16:3:2", "array:u16:3:1", "stride");
    changed("array:u16:3:2", "array:u16:0:2", "extent");
    changed("array:u16:3:2", "array:u16:03:2", "canonical");
    changed("datum datum record:sample", "datum datum record:packet", "earlier");
    changed("c-default", "fastcall", "unsupported");
    changed("nonblocking no-callbacks", "retains-pointer no-callbacks", "unsupported");
    changed("out:1:2:64", "out:1:2:65537", "bounded");
    changed("out:1:2:64", "out:1:1:64", "offset/length");
    changed("out:1:2:64 c_size", "out:1:2:64 u64", "exact c_size");
    changed("in:1:2:64 c_size c_size out:4:5:64", "out:1:2:64 c_size c_size out:4:5:64", "span count");
    changed("bind fill g2_fill i64", "bind fill g2_fill i32", "result contract");
    changed("out:1:2:64", "inout:1:2:64", "provider rejected");
}

Function& function(Module& module, std::string_view suffix) {
    auto found = std::find_if(module.functions.begin(), module.functions.end(), [&](const auto& f) {
        return f.name.ends_with(suffix);
    });
    require(found != module.functions.end(), "typed fixture function missing");
    return *found;
}
void bad_ir(Module good) {
    good.analysis.reset();
    validate(good);
    auto test = [&](std::string_view name, std::string_view expected, auto mutate) {
        auto m = good;
        mutate(m);
        reject(name, expected, [&] { validate(m); });
    };
    test("absent host seal", "outside the admitted", [](auto& m) { m.c_primitives.reset(); });
    test("wrong provider", "exact call signature", [](auto& m) {
        function(m, "::packet_mix").blocks[0].instructions.back().callee = "org.other::g2_packet";
    });
    test("wrong ordinal", "exact call signature", [](auto& m) {
        function(m, "::packet_mix").blocks[0].instructions.back().immediate = 1;
    });
    test("record identity", "VOID IR:", [](auto& m) {
        function(m, "::packet_mix").parameters[1] = function(m, "::color").parameters[0];
    });
    test("span count and slot swap", "argument type or placement", [](auto& m) {
        function(m, "::fill").blocks[0].instructions.back().arguments[2] = 0;
    });
    test("readonly output", "VOID IR:", [](auto& m) {
        auto& parameter = function(m, "::fill").parameters[0];
        parameter = readonly_view(parameter);
    });
    test("foreign raw ABI use", "requires host admission", [](auto& m) {
        m.imports.emplace_back(c_span_bridge);
    });
    test("unauthorized direct import", "requires the admitted direct import", [](auto& m) {
        m.source_imports[m.root_identity].clear();
    });
    test("worker", "require synchronous execution", [](auto& m) {
        function(m, "::entry").worker = true;
    });
    test("raw pointer construction flag", "exact call signature", [](auto& m) {
        function(m, "::sum").blocks[0].instructions.back().initialization = true;
    });
    auto uninitialized = [&](std::string_view label, Type type) {
        test(label, "uninitialized C binding carrier", [&](auto& m) {
            auto& f = function(m, "::entry");
            const auto slot = static_cast<unsigned>(f.locals.size());
            f.locals.push_back(type);
            f.blocks.front().instructions.push_back({Op::load, slot});
        });
    };
    uninitialized("uninitialized record", function(good, "::packet_mix").parameters[1]);
    uninitialized("uninitialized byte list", function(good, "::fill").parameters[0]);
    uninitialized("uninitialized scalar field", Type::i32);
    test("incomplete inline array escapes", "incomplete C inline array", [](auto& m) {
        auto& f = function(m, "::entry");
        auto& block = f.blocks.front().instructions;
        const auto slot = static_cast<unsigned>(f.locals.size());
        const auto t = field_type(function(m, "::packet_mix").parameters[1], 1);
        f.locals.push_back(t);
        const auto pending = static_cast<unsigned>(block.size());
        Instruction allocate{Op::array_allocate};
        allocate.type = t;
        block.push_back(allocate);
        block.push_back({Op::store, slot, pending});
    });
}

std::binary_semaphore entered(0), collected(0);
std::atomic<bool> executing = false;
void* managed_object = nullptr;
extern "C" std::uint32_t held_scratch(const char*, std::uint32_t, std::uint32_t,
                                     const std::uint64_t* words, std::uint32_t, std::uint64_t* result) {
    executing = true;
    entered.release();
    collected.acquire();
    auto* bytes = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(words[0]));
    if (!bytes || bytes == managed_object || words[2] != 2 || bytes[0] != 0 || bytes[1] != 0) return 2;
    bytes[0] = 88;
    *result = 1;
    return 0;
}
void runtime_pressure(const std::string& descriptor) {
    Runtime runtime(1);
    Runtime::Attachment attach(runtime);
    Runtime::Managed managed(runtime);
    void* list = nullptr;
    constexpr std::size_t zero = 0;
    Runtime::Slot slot{&list, sizeof(list), &zero, 1};
    Runtime::Frame roots(runtime, &slot, 1);
    runtime.list_into(&list, ScalarEncoding::u8, 2);
    runtime.list_write(runtime.root_word(&list), ScalarEncoding::u8, 0, 9);
    runtime.list_write(runtime.root_word(&list), ScalarEncoding::u8, 1, 8);
    managed_object = list;
    const auto before = runtime.snapshot().completed_epoch;
    std::thread collector([&] {
        entered.acquire();
        if (executing) {
            Runtime::Attachment other(runtime);
            Runtime::Managed held(runtime);
            (void)runtime.collect();
        }
        collected.release();
    });
    CSpanRequest span{&list, 64, 0, 2, 1, 2};
    std::array<std::uint64_t, 4> words{0, 0, 2, 254};
    std::uint64_t result = 0;
    auto call = [&](CPrimitiveThunk* target) {
        return vd_c_span_dispatch_v2(target, descriptor.data(), static_cast<std::uint32_t>(descriptor.size()),
            4, words.data(), static_cast<std::uint32_t>(words.size()), &result, &span, 1);
    };
    const auto status = call(held_scratch);
    if (!executing) entered.release();
    collector.join();
    require(status == 0 && result == 1 && runtime.snapshot().completed_epoch > before, "GC did not complete during NativeSafe scratch lifetime");
    auto read_at = [&](unsigned i) { return runtime.list_read(runtime.root_word(&list), ScalarEncoding::u8, i); };
    require(read_at(0) == 88 && read_at(1) == 8, "partial copyback crossed initialized prefix");
    require(call(vd_c_primitive_dispatch_v2) == 5 && read_at(0) == 88 && read_at(1) == 8,
        "oversized provider result altered destination");
    words[3] = 255;
    require(call(vd_c_primitive_dispatch_v2) == 0 && read_at(0) == 88 && read_at(1) == 8,
        "negative provider result altered destination");
    words[2] = 65;
    require(call(vd_c_primitive_dispatch_v2) == 3, "capacity cap ignored");
    words[2] = 2;
    span.length_argument = span.offset_argument;
    require(call(vd_c_primitive_dispatch_v2) == 2, "conflicting buffer control slots admitted");
    span.length_argument = 2;
    span.root = &managed_object;
    require(call(vd_c_primitive_dispatch_v2) != 0, "unregistered buffer word admitted");
    std::cout << "NativeSafe collection, zeroed output scratch, partial commit, oversized and negative rollback passed\n";
}
}

int main(int argc, char** argv) try {
    require(argc == 3 || argc == 5, "expected descriptor and source, optionally AOT output and mode");
    const auto descriptor = read(argv[1]);
    auto admission = CPrimitiveAdmission::admit(descriptor, vd_c_primitive_dispatch_v2);
    auto result = analyze_project(argv[2], admission);
    if (!result.succeeded()) throw CompilationError(result.diagnostics);
    auto program = result.program;
    if (argc == 5) {
        emit_object(program->lower(Emission::entry), argv[3], std::string_view(argv[4]) == "optimized", true);
        return 0;
    }
    require(!analyze_project(argv[2]).succeeded(), "bare source acquired G2 admission");
    bad_descriptors(descriptor);
    bad_ir(program->lower());
    runtime_pressure(descriptor);
    for (bool optimize : {false, true}) {
        Jit jit;
        auto code = jit.compile(*program, optimize);
        const auto status = code->invoke(7);
        require(status == 103, "record/span source result " + std::to_string(status));
        const auto failed = [&](int value, std::string_view diagnostic) {
            reject(std::to_string(value), diagnostic, [&] { (void)code->invoke(value); });
        };
        failed(-1, "outside admitted extent");
        failed(-2, "overlapping C byte spans");
        failed(-3, "oversized written prefix");
        failed(-4, "outside admitted extent");
        std::cout << "G2 JIT " << (optimize ? "O2" : "normal") << " returned 103\n";
    }
    return 0;
} catch (const std::exception& failure) {
    std::cerr << failure.what() << '\n';
    return 1;
}
