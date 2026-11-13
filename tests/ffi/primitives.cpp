#include "void/backend.hpp"
#include "void/c_primitive.hpp"
#include "void/cache.hpp"
#include "void/runtime.hpp"
#include "void/frontend.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

extern "C" vd::CPrimitiveThunk vd_c_primitive_dispatch_v1;
extern "C" long g1_clong(long);
extern "C" bool g1_invert(bool);
extern "C" float g1_single(float, float);
extern "C" double g1_double(double, double);
extern "C" std::uint64_t g1_mixed(std::int8_t, std::uint16_t, std::int32_t, float, std::uint64_t, double,
    std::int64_t, float, std::uint32_t, double, std::uint8_t, std::int16_t, float, double);

namespace {
using namespace vd;
using namespace vd::ir;
using fs_path = std::filesystem::path;

void require(bool okay, std::string_view why) {
    if (!okay) throw std::runtime_error(std::string(why));
}
std::string read(const fs_path& path) {
    std::ifstream in(path, std::ios::binary);
    require(in.is_open(), "cannot open fixture input");
    return {std::istreambuf_iterator<char>(in), {}};
}
void write(const fs_path& path, std::string_view value) {
    std::ofstream out(path, std::ios::binary);
    require(bool(out << value), "cannot write isolated source");
}
template<class Test>
void reject(std::string_view label, std::string_view expected, Test&& test) {
    try { test(); }
    catch (const std::invalid_argument& failure) {
        require(std::string_view(failure.what()).find(expected) != std::string_view::npos,
            std::string(label) + " received " + failure.what());
        std::cout << label << ": " << failure.what() << '\n';
        return;
    }
    throw std::runtime_error(std::string(label) + " accepted");
}
std::shared_ptr<const ValidatedAnalysis> analyze(const fs_path& path, const CPrimitiveAdmission::Owner& admission) {
    auto result = analyze_project(path, admission);
    if (!result.succeeded()) throw CompilationError(result.diagnostics);
    return result.program;
}

void descriptors(const std::string& descriptor) {
    auto changed = [&](std::string_view from, std::string_view to, std::string_view why) {
        auto copy = descriptor;
        const auto position = copy.find(from);
        require(position != copy.npos, "descriptor mutation marker missing");
        copy.replace(position, from.size(), to);
        reject(to, why, [&] { (void)CPrimitiveAdmission::admit(copy, vd_c_primitive_dispatch_v1); });
    };
    changed("void-c-primitive 1 1", "void-c-primitive 2 1", "unsupported");
    changed("void-c-primitive 1 1", "void-c-primitive 1 2", "unsupported");
    changed("call c-default", "call vectorcall", "unsupported");
    changed("effects direct-sync nonblocking no-callbacks", "effects direct-sync blocking no-callbacks", "unsupported");
    changed("c_size:u64", "c_size:i64", "unsupported");
    changed("c_bool:bool", "c_bool:u8", "unsupported");
    changed("bind signed8 g1_signed8 i8 i8", "bind signed8 g1_signed8 i8 ptr", "unsupported C primitive");
    changed("bind signed8 g1_signed8 i8 i8", "bind signed8 g1_signed8 i8 record", "unsupported C primitive");
    changed("bind signed8 g1_signed8 i8 i8", "bind signed8 g1_signed8 i8 void", "unsupported C primitive");
    changed("bind signed8 g1_signed8 i8 i8", "bind signed8 g1_signed8 i8 u8", "provider rejected");
    changed("bind signed8 g1_signed8 i8 i8", "bind signed8 g1_missing i8 i8", "provider rejected");
    changed("bind signed8 g1_signed8 i8 i8", "bind signed16 g1_signed8 i8 i8", "duplicate");
    changed("bind signed8 g1_signed8 i8 i8", "bind signed8 g1_signed16 i8 i8", "duplicate");
    changed("bind signed8 g1_signed8 i8 i8", "extra signed8 g1_signed8 i8 i8", "binding record");
    changed("provider org.voidlang.g1-fixture", "provider org.voidlang.forged", "provider rejected");
    changed("source /native_primitives.void", "source /other.void", "provider rejected");
    changed("source /native_primitives.void", "source /../other.void", "canonical");
    changed("target x86_64-", "target aarch64-", "unsupported target");
    auto altered = descriptor;
    const auto artifact = altered.find("\nfixture ") + std::string_view("\nfixture ").size();
    altered[artifact] = altered[artifact] == 'a' ? 'b' : 'a';
    reject("artifact identity", "provider rejected", [&] { (void)CPrimitiveAdmission::admit(altered, vd_c_primitive_dispatch_v1); });
    reject("descriptor bounds", "descriptor size", [&] {
        (void)CPrimitiveAdmission::admit(std::string(16385, 'x'), vd_c_primitive_dispatch_v1);
    });
    auto large = descriptor.substr(0, descriptor.find("bind "));
    for (unsigned index = 0; index < c_conventional_binding_limit; ++index)
        large += "bind value" + std::to_string(index) + " export" + std::to_string(index) + " i32\n";
    const auto adapter = CPrimitiveAdmission::generate_adapter(large, "large_provider.h");
    require(adapter.find("export1023") != std::string::npos, "large conventional descriptor truncated");
    reject("conventional binding bound", "bounded primitive binding", [&] {
        (void)CPrimitiveAdmission::generate_adapter(large + "bind overflow overflow i32\n", "large_provider.h");
    });
    reject("conventional descriptor bound", "descriptor size", [&] {
        (void)CPrimitiveAdmission::generate_adapter(std::string(c_conventional_descriptor_limit + 1, 'x'), "large_provider.h");
    });
    reject("unavailable fixture", "transport is unavailable", [&] { (void)CPrimitiveAdmission::admit(descriptor, nullptr); });
    std::uint64_t word = 2, result = 0;
    auto transport = [&](std::uint32_t index, const std::uint64_t* words, unsigned count, std::uint64_t* out) {
        return vd_c_primitive_dispatch_v1(descriptor.data(), static_cast<std::uint32_t>(descriptor.size()),
                                          index, words, count, out);
    };
    require(transport(11, &word, 1, &result) != 0, "noncanonical Boolean admitted");
    require(transport(100, nullptr, 0, nullptr) != 0, "unknown ordinal admitted");
    require(transport(0, &word, 0, &result) != 0 && transport(0, &word, 1, nullptr) != 0,
        "malformed transport count or output admitted");
}

Module hand_built(const CPrimitiveAdmission::Owner& admission) {
    Module m;
    m.identity = m.root_identity = "/hand.void";
    m.entry_symbol = "/hand.void::entry";
    m.coordinated = true;
    m.c_primitives = admission;
    m.source_imports[m.root_identity].insert(admission->module());
    m.source_imports[admission->module()];
    Function entry;
    entry.name = m.entry_symbol;
    entry.source_name = "entry";
    entry.module_identity = m.root_identity;
    Function wrapper;
    wrapper.module_identity = admission->module();
    wrapper.source_name = "signed32";
    wrapper.name = wrapper.module_identity + "::" + wrapper.source_name;
    Instruction direct{Op::call};
    direct.callee = wrapper.name;
    direct.arguments = {0};
    entry.blocks = {{{{Op::argument, 0}, direct}, {Terminator::Kind::return_value, 1}}};
    Instruction native{Op::c_primitive_call};
    native.callee = admission->provider() + "::g1_signed32";
    native.arguments = {0};
    native.immediate = 2;
    wrapper.blocks = {{{{Op::argument, 0}, native}, {Terminator::Kind::return_value, 1}}};
    m.functions = {entry, wrapper};
    return m;
}
void typed_ir(const CPrimitiveAdmission::Owner& admission) {
    auto good = hand_built(admission);
    validate(good);
    for (bool optimize : {false, true}) {
        Jit jit;
        auto code = jit.compile(good, good.entry_symbol, optimize, {}, std::make_shared<Runtime>(256));
        require(code->invoke(-701) == -701, "hand-built typed IR round trip");
    }
    auto test = [&](std::string_view label, std::string_view expected, auto mutate) {
        auto m = good;
        auto& f = m.functions.back();
        mutate(m, f, f.blocks[0].instructions[1]);
        reject(label, expected, [&] { validate(m); });
    };
    const std::string_view abi = "invalid C primitive descriptor";
    test("missing independent admission", "outside the admitted", [](auto& m, auto&, auto&) { m.c_primitives.reset(); });
    test("wrong provider", abi, [](auto&, auto&, auto& i) { i.callee = "org.voidlang.other::g1_signed32"; });
    test("wrong export", abi, [](auto&, auto&, auto& i) { i.callee = "org.voidlang.g1-fixture::g1_missing"; });
    test("wrong ordinal", abi, [](auto&, auto&, auto& i) { i.immediate = 1; });
    test("negative ordinal", "outside the admitted", [](auto&, auto&, auto& i) { i.immediate = -1; });
    test("missing operand", abi, [](auto&, auto&, auto& i) { i.arguments.clear(); });
    test("extra operand", abi, [](auto&, auto&, auto& i) { i.arguments.push_back(0); });
    test("argument signedness", abi, [](auto& m, auto& f, auto&) { m.functions[0].parameters[0] = f.parameters[0] = Type::u32; });
    test("argument width", abi, [](auto& m, auto& f, auto&) { m.functions[0].parameters[0] = f.parameters[0] = Type::i64; });
    test("return signedness", abi, [](auto&, auto&, auto& i) { i.type = Type::u32; });
    test("return count", abi, [](auto&, auto&, auto& i) { i.type = Type::none; });
    test("aggregate result", abi, [](auto&, auto&, auto& i) { i.type = Type::aggregate; });
    test("pointer-like result", abi, [](auto&, auto&, auto& i) { i.type = Type::managed_ref; });
    test("nondefault ABI operand", abi, [](auto&, auto&, auto& i) { i.b = 1; });
    test("receiver", abi, [](auto&, auto&, auto& i) { i.receiver = Place{}; });
    test("unadmitted scope", "requires the admitted direct import", [](auto& m, auto&, auto&) {
        m.source_imports[m.root_identity].clear();
    });
    test("unadmitted body", "outside the admitted", [](auto& m, auto& f, auto&) {
        f.module_identity = "/forged.void"; f.name = "/forged.void::signed32";
        m.functions[0].blocks[0].instructions[1].callee = f.name;
    });
    test("plain import bypass", "requires host admission", [](auto& m, auto&, auto&) {
        m.imports.emplace_back(c_primitive_transport);
    });
    test("first class binding", "direct-call only", [](auto& m, auto&, auto&) {
        m.functions[0].blocks[0].instructions[1].op = Op::function_reference;
    });
    test("async activation", "require synchronous execution", [](auto& m, auto&, auto&) {
        m.functions[0].asynchronous = true;
    });
    test("runtime-free bypass", "invalid C primitive module admission", [](auto& m, auto&, auto&) { m.coordinated = false; });
    reject("ORC untrusted symbol bypass", "per-generation admission", [] {
        Jit jit;
        jit.bind(std::string(c_primitive_transport), vd_c_primitive_dispatch_v1);
    });
}
}

int main(int argc, char** argv) try {
    const bool emit = argc == 6 && std::string_view(argv[1]) == "--emit";
    require(emit || argc == 4, "expected descriptor, source and isolated fixture directory");
    const auto descriptor = read(argv[emit ? 2 : 1]);
    const fs_path source = argv[emit ? 3 : 2];
    auto admission = vd::CPrimitiveAdmission::admit(descriptor, vd_c_primitive_dispatch_v1);
    const auto program = analyze(source, admission);
    if (emit) {
        vd::emit_object(program->lower(vd::ir::Emission::entry), argv[4], std::string_view(argv[5]) == "optimized", true);
        return 0;
    }
    const fs_path sandbox = argv[3];
    std::filesystem::create_directories(sandbox);
    require(!vd::analyze_project(source).succeeded(), "bare source gained native authority");
    const auto probe = sandbox / "probe.void";
    write(probe, "use \"/native_primitives.void\" as c\nfunc entry(value: i32) -> i32:\n    var escaped = c.signed32\n    return value\n");
    require(!vd::analyze_project(probe, admission).succeeded(), "binding escaped as a source function value");
    write(probe, "use \"/native_primitives.void\" as c\nfunc entry(value: i32) -> i32:\n    return c.signed32(value)\n");
    const auto collision = sandbox / "native_primitives.void";
    write(collision, "func signed32(value: i32) -> i32:\n    return 0\n");
    const auto denied = vd::analyze_project(probe, admission);
    std::filesystem::remove(collision);
    require(!denied.succeeded() && !denied.diagnostics.empty(), "physical source shadowed the admitted module");
    write(probe, "use \"/native_primitives.void\" as c\nasync func entry(value: i32) -> i32:\n    return c.signed32(value)\n");
    const auto async = vd::analyze_project(probe, admission);
    require(!async.succeeded() && !async.diagnostics.empty(), "G1 admitted an async graph");
    descriptors(descriptor);
    typed_ir(admission);
    const auto long_width = sizeof(long) * 8;
    require(admission->bindings()[9].parameters[0] == (long_width == 32 ? Type::i32 : Type::i64), "wrong C long profile");
    require(g1_clong(-631) == -631 && g1_invert(false) && !g1_invert(true) &&
        g1_single(1.25f, -3.0f) == -0.5f && g1_double(5.5, -1.5) == 2.5 &&
        g1_mixed(-111, 60001, -200000001, 1.25f, UINT64_MAX, -2.5, -800000000000000001LL,
            5.5f, 4000000001U, -7.75, 251, -30123, -0.5f, 17.5) == 17293822569102704641ULL,
        "direct C ABI oracle disagreed");
    for (bool optimize : {false, true}) {
        vd::Jit jit({sandbox, std::string(vd::compiler_build_identity())});
        auto code = jit.compile(*program, optimize);
        auto other = jit.compile(*program, optimize);
        require(jit.cache_status() == "unavailable" && jit.live_generations() == 2, "G1 cache/generation boundary");
        const auto result = code->invoke(7);
        require(result == 73, "VOID C primitive matrix returned " + std::to_string(result));
        require(other->invoke(-10) == 56, "second generation did not dispatch");
        other.reset();
        require(jit.live_generations() == 1, "native thunk generation lease not retired");
        std::cout << "JIT " << (optimize ? "O2" : "normal") << " returned " << result
                  << "; direct C oracle agreed; long=" << long_width << " bits\n";
    }
    return 0;
} catch (const std::exception& failure) {
    std::cerr << failure.what() << '\n';
    return 1;
}
