#include "void/backend.hpp"
#include "void/c_primitive.hpp"
#include "void/frontend.hpp"
#include "void/native_resource.hpp"
#include "void/runtime.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <semaphore>
#include <span>
#include <stdexcept>
#include <thread>

extern "C" vd::CPrimitiveThunk vd_c_primitive_dispatch_v3;
extern "C" std::int32_t g3_live();
extern "C" std::int32_t g3_releases();

namespace {
using namespace vd;
using namespace vd::ir;
namespace fs = std::filesystem;
constexpr auto key = "/native_resources.void::box";
constexpr std::uint32_t release_ordinal = 3;

void require(bool okay, std::string_view message) {
    if (!okay) throw std::runtime_error(std::string(message));
}
template<class Work>
void rejects(std::string_view label, std::string_view expected, Work work) {
    try { work(); }
    catch (const std::exception& error) {
        require(std::string_view(error.what()).find(expected) != std::string_view::npos,
                std::string(label) + " received " + error.what());
        std::cout << label << ": " << error.what() << '\n';
        return;
    }
    throw std::runtime_error(std::string(label) + " accepted");
}
std::string read(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    require(stream.is_open(), "cannot read input");
    return {std::istreambuf_iterator<char>(stream), {}};
}
void write(const fs::path& path, std::string_view text) {
    std::ofstream stream(path, std::ios::binary);
    require(bool(stream << text), "cannot write source probe");
}
auto analyze(const fs::path& path, const CPrimitiveAdmission::Owner& admission) {
    auto result = analyze_project(path, admission);
    if (!result.succeeded()) throw CompilationError(result.diagnostics);
    return result.program;
}

void descriptors(const std::string& descriptor) {
    auto changed = [&](std::string_view from, std::string_view to, std::string_view expected) {
        auto copy = descriptor;
        const auto position = copy.find(from);
        require(position != copy.npos, "descriptor marker missing");
        copy.replace(position, from.size(), to);
        rejects(to, expected, [&] { (void)CPrimitiveAdmission::admit(copy, vd_c_primitive_dispatch_v3); });
    };
    changed("3 3", "3 2", "unsupported descriptor");
    changed("pointer null", "integer null", "resource contract");
    changed("pointer null", "pointer minus-one", "resource contract");
    changed("shutdown-any", "shutdown-owner", "resource contract");
    changed("consume-always", "retry-on-error", "resource contract");
    changed("create close_box owner", "create missing owner", "terminal release signature");
    changed("bind close_box g3_close i32 close:box", "bind close_box g3_close i32 mut:box", "missing exact");
    changed("i32 close:box", "i64 close:box", "terminal release");
    changed("i32 close:box", "i32 close:box i32", "terminal release");
    changed("i32 borrow:box i32", "i32 borrow:other i32", "earlier admitted identity");
    changed("i32 borrow:box i32", "i32 borrow:box mut:box", "invalid native borrow");
    changed("i32 borrow:box i32", "owner:box borrow:box i32", "ownership transfer");
    changed("c-default", "vectorcall", "unsupported");
    changed("nonblocking no-callbacks", "retains-owner no-callbacks", "unsupported");
    changed("org.voidlang.g3-fixture", "org.voidlang.other", "provider rejected");
}
Function& function(Module& module, std::string_view suffix) {
    auto found = std::find_if(module.functions.begin(), module.functions.end(),
        [&](const auto& f) { return f.name.ends_with(suffix); });
    require(found != module.functions.end(), "typed fixture function missing");
    return *found;
}
void malformed(Module good) {
    good.analysis.reset();
    validate(good);
    auto test = [&](std::string_view label, std::string_view expected, auto change) {
        auto module = good;
        change(module);
        rejects(label, expected, [&] { validate(module); });
    };
    test("absent admission", "native resource identity", [](auto& m) { m.c_primitives.reset(); });
    test("wrong provider", "exact call signature", [](auto& m) {
        function(m, "::read").blocks[0].instructions.back().callee = "org.other::g3_read";
    });
    test("wrong release ordinal", "exact call signature", [](auto& m) {
        function(m, "::close_box").blocks[0].instructions.back().immediate = 0;
    });
    test("forged ownership movement", "exact call signature", [](auto& m) {
        function(m, "::close_box").blocks[0].instructions.back().initialization = true;
    });
    test("forged convention operand", "exact call signature", [](auto& m) {
        function(m, "::close_box").blocks[0].instructions.back().b = 1;
    });
    test("scope bypass", "requires the admitted direct import", [](auto& m) {
        m.source_imports[m.root_identity].clear();
    });
    test("raw bridge escape", "requires host admission", [](auto& m) {
        m.imports.emplace_back(c_resource_bridge);
    });
    test("raw owner bridge escape", "requires host admission", [](auto& m) {
        m.imports.emplace_back(c_owner_resource_bridge);
    });
    test("worker resource access", "require synchronous execution", [](auto& m) {
        function(m, "::entry").worker = true;
    });
    test("borrow as escaping callable", "direct-call only", [](auto& m) {
        auto& body = function(m, "::entry").blocks[0].instructions;
        Instruction escaped{Op::function_reference};
        escaped.callee = function(m, "::read").name;
        body.push_back(escaped);
    });
    test("wrapper control flow bypass", "invalid admitted C primitive wrapper", [](auto& m) {
        auto& f = function(m, "::close_box");
        f.blocks.push_back(f.blocks.front());
    });
    test("fabricated second owning wrapper", "forged native resource", [](auto& m) {
        auto& f = function(m, "::entry");
        auto& body = f.blocks.front().instructions;
        Instruction seal{Op::constant};
        seal.type = Type::u64;
        const auto argument = static_cast<unsigned>(body.size());
        body.push_back(seal);
        Instruction forged{Op::gc_construct};
        forged.type = function(m, "::read").parameters[0];
        forged.type.readonly = false;
        forged.arguments = {argument};
        body.push_back(forged);
    });
    test("integer-to-resource cast", "invalid conversion", [](auto& m) {
        auto& body = function(m, "::entry").blocks.front().instructions;
        const auto integer = static_cast<unsigned>(body.size());
        body.push_back({Op::constant});
        Instruction cast{Op::convert, integer};
        cast.type = function(m, "::close_box").parameters[0];
        body.push_back(cast);
    });
    test("uninitialized handle", "uninitialized C binding carrier", [](auto& m) {
        auto& f = function(m, "::entry");
        const auto index = static_cast<unsigned>(f.locals.size());
        f.locals.push_back(function(m, "::close_box").parameters[0]);
        f.blocks.front().instructions.push_back({Op::load, index});
    });
    test("eager global acquisition", "C primitive operation during global startup", [](auto& m) {
        auto& init = function(m, m.globals.front().initializer);
        auto& body = init.blocks.front().instructions;
        const auto arg = static_cast<unsigned>(body.size());
        body.push_back({Op::constant});
        Instruction call{Op::call};
        call.callee = function(m, "::create").name;
        call.arguments = {arg};
        body.push_back(std::move(call));
    });
}

extern "C" std::uint32_t alternate(const char* descriptor, std::uint32_t size, std::uint32_t ordinal,
                                   const std::uint64_t* args, std::uint32_t count, std::uint64_t* result) {
    return vd_c_primitive_dispatch_v3(descriptor, size, ordinal, args, count, result);
}

std::uint32_t bridge(CPrimitiveThunk* thunk, std::string_view descriptor, std::uint32_t ordinal,
                     std::span<const std::uint64_t> args, std::uint64_t* result, void** handle, CResourceRole role) {
    return vd_c_resource_dispatch_v3(thunk, descriptor.data(), static_cast<std::uint32_t>(descriptor.size()),
        ordinal, args.data(), static_cast<std::uint32_t>(args.size()), result, handle, key, release_ordinal, role);
}
void admit(Runtime& runtime) {
    const NominalDescriptor layout{key, "", "test resource", {8, {}}};
    runtime.admit_types({&layout, 1});
}
struct OwnerRoot {
    void* value = nullptr;
    static constexpr std::size_t zero = 0;
    Runtime::Slot slot{&value, sizeof(value), &zero, 1};
    Runtime::Frame frame;
    explicit OwnerRoot(Runtime& runtime) : frame(runtime, &slot, 1) {}
};
std::uint64_t duplicated_pointer = 0;
bool refuse_close = false;
extern "C" std::uint32_t duplicate_provider(const char* descriptor, std::uint32_t size, std::uint32_t ordinal,
    const std::uint64_t* args, std::uint32_t count, std::uint64_t* result) {
    if (ordinal == 0 && !count) {
        const auto status = vd_c_primitive_dispatch_v3(descriptor, size, ordinal, args, count, result);
        // A failed transport transfers nothing, even with a stale output word.
        *result = duplicated_pointer;
        return status;
    }
    if (ordinal == release_ordinal && refuse_close)
        return vd_c_primitive_dispatch_v3(descriptor, size, ordinal, args, 0, result);
    if (ordinal == 0 && duplicated_pointer) { *result = duplicated_pointer; return 0; }
    const auto status = vd_c_primitive_dispatch_v3(descriptor, size, ordinal, args, count, result);
    if (!status && ordinal == 0) duplicated_pointer = *result;
    if (!status && ordinal == release_ordinal) duplicated_pointer = 0;
    return status;
}
void runtime_denials(const std::string& descriptor) {
    Runtime runtime;
    Runtime::Attachment attach(runtime);
    admit(runtime);
    Runtime::Managed managed(runtime);
    OwnerRoot owner(runtime), other(runtime);
    const std::array<std::uint64_t, 1> create{24}, close{0};
    const std::array<std::uint64_t, 1> null_create{static_cast<std::uint32_t>(-1)};
    const std::array<std::uint64_t, 2> args{0, 0};
    std::uint64_t result = 0;
    const auto before = g3_releases();
    require(bridge(duplicate_provider, descriptor, 0, null_create, nullptr, &other.value, CResourceRole::acquire) == 0 &&
            !other.value && g3_live() == 0 && g3_releases() == before, "null acquired ownership");
    require(bridge(duplicate_provider, descriptor, 0, {}, nullptr, &other.value, CResourceRole::acquire) == 4 &&
            !other.value && g3_live() == 0 && g3_releases() == before, "refused acquisition acquired ownership");
    require(bridge(duplicate_provider, descriptor, 0, create, nullptr, &owner.value, CResourceRole::acquire) == 0 &&
            owner.value && g3_live() == 1, "first acquisition");
    require(bridge(duplicate_provider, descriptor, 0, {}, nullptr, &other.value, CResourceRole::acquire) == 4 &&
            !other.value && g3_live() == 1 && g3_releases() == before,
            "refused acquisition consumed the existing owner");
    require(bridge(duplicate_provider, descriptor, 0, create, nullptr, &other.value, CResourceRole::acquire) == 6 &&
            !other.value && g3_live() == 1 && g3_releases() == before, "duplicate native ownership");
    void* const identity = owner.value;
    owner.value = nullptr;
    require(runtime.collect() == Runtime::Collection::completed && runtime.contains(identity) && g3_live() == 1,
            "collection lost the ledger-owned wrapper");
    owner.value = identity;
    require(bridge(alternate, descriptor, 1, args, &result, &owner.value, CResourceRole::borrow) == 2, "wrong linked authority");
    require(bridge(alternate, descriptor, release_ordinal, close, &result, &owner.value, CResourceRole::close) == 2,
            "wrong linked release authority");
    auto wrong = descriptor;
    wrong.replace(wrong.find("org.voidlang.g3-fixture"), std::string_view("org.voidlang.g3-fixture").size(), "org.voidlang.wrong");
    require(bridge(duplicate_provider, wrong, 1, args, &result, &owner.value, CResourceRole::borrow) == 2, "wrong provider");
    std::atomic<unsigned> denied{0};
    std::thread outsider([&] {
        Runtime foreign;
        Runtime::Attachment attachment(foreign);
        Runtime::Managed active(foreign);
        OwnerRoot cross(foreign);
        cross.value = owner.value; // Deliberately forge a foreign root in the host-only proof.
        std::uint64_t observed = 0;
        denied = bridge(duplicate_provider, descriptor, 1, args, &observed, &cross.value, CResourceRole::borrow);
        cross.value = nullptr;
    });
    {
        Runtime::Native safe(runtime);
        outsider.join();
    }
    require(denied == 1, "cross-Runtime borrowed handle admitted");
    refuse_close = true;
    result = 123;
    const auto refusal = bridge(duplicate_provider, descriptor, release_ordinal, close,
                                &result, &owner.value, CResourceRole::close);
    refuse_close = false;
    require(refusal == 4 && result == 123 && g3_live() == 1 && g3_releases() == before,
            "transport refusal consumed ownership or returned a terminal status");
    require(bridge(duplicate_provider, descriptor, 0, create, nullptr, &other.value, CResourceRole::acquire) == 6 &&
            !other.value && bridge(duplicate_provider, descriptor, 1, args, &result, &owner.value, CResourceRole::borrow) == 0 &&
            result == 24, "transport refusal lost the exclusive claim or live owner");
    require(bridge(duplicate_provider, descriptor, release_ordinal, close, &result, &owner.value, CResourceRole::close) == 0 &&
            result == 0 && g3_live() == 0 && g3_releases() == before + 1, "terminal close");
    require(bridge(duplicate_provider, descriptor, 1, args, &result, &owner.value, CResourceRole::borrow) == 1 &&
            bridge(duplicate_provider, descriptor, release_ordinal, close, &result, &owner.value, CResourceRole::close) == 1,
            "stale alias or double close admitted");
    owner.value = nullptr;
    require(runtime.collect() == Runtime::Collection::completed && !runtime.contains(identity),
            "closed ledger wrapper remained rooted");
    std::cout << "duplicate native owner, foreign provider/Runtime and stale alias rejected\n";
    std::cout << "null and refused acquisition, refused release and retry preserved exact ownership\n";
}

void owner_shutdown(const std::string& descriptor) {
    Runtime runtime;
    runtime.start_application({});
    Runtime::Attachment attach(runtime);
    admit(runtime);
    const std::uint64_t value = 24;
    auto acquire = [&](void** slot) {
        return vd_c_resource_dispatch_owner_v3(vd_c_primitive_dispatch_v3, descriptor.data(),
            static_cast<std::uint32_t>(descriptor.size()), 0, &value, 1, nullptr, slot, key,
            release_ordinal, CResourceRole::acquire);
    };
    const auto before = g3_releases();
    {
        Runtime::Managed managed(runtime);
        OwnerRoot owner(runtime);
        require(acquire(&owner.value) == 0 && owner.value, "owner-affine acquisition failed");
    }
    std::exception_ptr failure;
    std::jthread foreign([&] {
        try {
        {
            Runtime::Attachment attachment(runtime);
            Runtime::Managed managed(runtime);
            OwnerRoot owner(runtime);
            require(acquire(&owner.value) == 3 && !owner.value, "foreign acquisition was accepted");
        }
        rejects("foreign shutdown", "application owner thread", [&] { (void)runtime.close(); });
        } catch (...) { failure = std::current_exception(); }
    });
    foreign.join();
    if (failure) std::rethrow_exception(failure);
    require(g3_live() == 1 && g3_releases() == before, "foreign shutdown touched native ownership");
    require(!runtime.close() && !runtime.close() && g3_live() == 0 && g3_releases() == before + 1,
        "owner shutdown did not release exactly once");
}

std::binary_semaphore entered(0), shutdown_start(0), resume(0);
std::atomic<bool> delayed_entered{false};
std::atomic<unsigned> pause_at{UINT32_MAX};
extern "C" std::uint32_t delayed(const char* descriptor, std::uint32_t size, std::uint32_t ordinal,
                                  const std::uint64_t* args, std::uint32_t count, std::uint64_t* result) {
    const auto pause = [] {
        delayed_entered = true;
        entered.release();
        resume.acquire();
    };
    if (ordinal == 1 && ordinal == pause_at.load()) pause();
    const auto status = vd_c_primitive_dispatch_v3(descriptor, size, ordinal, args, count, result);
    if (!status && ordinal == 0 && ordinal == pause_at.load()) pause();
    return status;
}
void shutdown_race(const std::string& descriptor, bool creating) {
    Runtime runtime;
    Runtime::Attachment attach(runtime);
    admit(runtime);
    Runtime::Managed managed(runtime);
    OwnerRoot owner(runtime);
    const std::array<std::uint64_t, 1> create{24}, close{0};
    const std::array<std::uint64_t, 2> read{0, 0};
    std::uint64_t result = 0;
    const auto before = g3_releases();
    if (!creating)
        require(bridge(delayed, descriptor, 0, create, nullptr, &owner.value, CResourceRole::acquire) == 0 && owner.value,
            "shutdown race setup");
    void* published = owner.value;
    delayed_entered = false;
    pause_at = creating ? 0 : 1;
    std::atomic<bool> completed{false}, observed_wait{false};
    std::atomic<unsigned> wrong_thread{0};
    std::thread shutdown([&] {
        entered.acquire();
        if (!creating) {
            {
                Runtime::Attachment other(runtime);
                Runtime::Managed active(runtime);
                OwnerRoot alias(runtime);
                alias.value = published;
                std::uint64_t ignored = 0;
                wrong_thread = bridge(delayed, descriptor, release_ordinal, close, &ignored, &alias.value, CResourceRole::close);
            }
        }
        shutdown_start.release();
        const auto error = runtime.close();
        completed = !error;
    });
    std::thread unblock([&] {
        shutdown_start.acquire();
        for (unsigned n = 0; n < 5000; ++n) {
            try { (void)NativeLibrary::get(runtime); }
            catch (const std::runtime_error&) {
                observed_wait = g3_live() == 1 && !completed && (creating || wrong_thread == 3);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        resume.release();
    });
    const auto status = creating ?
        bridge(delayed, descriptor, 0, create, nullptr, &owner.value, CResourceRole::acquire) :
        bridge(delayed, descriptor, 1, read, &result, &owner.value, CResourceRole::borrow);
    if (!delayed_entered) entered.release();
    {
        Runtime::Native safe(runtime);
        shutdown.join();
        unblock.join();
    }
    pause_at = UINT32_MAX;
    require(observed_wait && completed && g3_live() == 0 && g3_releases() == before + 1,
        "shutdown released live C call or failed to drain the native owner");
    require(creating ? status == 4 && !owner.value : status == 0 && result == 24,
        "shutdown changed the borrow result or published a failed acquisition");
    std::cout << (creating ? "creation-publication race" : "borrow vs wrong-thread close and shutdown") << " passed\n";
}

void failed_shutdown_release(const std::string& descriptor) {
    Runtime runtime;
    const auto before = g3_releases();
    {
        Runtime::Attachment attach(runtime);
        admit(runtime);
        Runtime::Managed managed(runtime);
        OwnerRoot root(runtime);
        const std::array<std::uint64_t, 1> create{static_cast<std::uint32_t>(-2)};
        require(bridge(vd_c_primitive_dispatch_v3, descriptor, 0, create, nullptr, &root.value,
                       CResourceRole::acquire) == 0 && root.value, "release-error fixture");
    }
    const auto error = runtime.close();
    require(error && error.os_code == 17 && g3_live() == 0 && g3_releases() == before + 1,
        "failed release did not consume the shutdown resource exactly once");
    require(!runtime.close() && g3_releases() == before + 1, "terminal error repeated native release");
    std::cout << "terminal provider error reported on shutdown after release\n";
}

void typed_stale_alias(Module module) {
    module.analysis.reset();
    auto& entry = function(module, "::entry");
    auto& create = function(module, "::create");
    auto& release = function(module, "::close_box");
    auto& read = function(module, "::read");
    Instruction acquire{Op::call};
    acquire.callee = create.name;
    acquire.type = create.result;
    acquire.arguments = {0};
    Instruction unwrap{Op::convert, 1};
    unwrap.type = release.parameters[0];
    Instruction close{Op::call};
    close.callee = release.name;
    close.arguments = {2};
    Instruction borrow{Op::call};
    borrow.callee = read.name;
    borrow.arguments = {2, 0};
    entry.locals.clear();
    entry.cleanup_entry.reset();
    entry.blocks = {{{{Op::constant}, acquire, unwrap, close, borrow}, {Terminator::Kind::return_value, 4}}};
    validate(module); // Liveness of an alias is intentionally dynamic.
    Jit jit;
    auto code = jit.compile(module, module.entry_symbol, false, {}, std::make_shared<Runtime>());
    rejects("direct typed IR stale alias", "closed, null or invalid", [&] { (void)code->invoke(0); });
    require(g3_live() == 0, "typed IR stale alias leaked an owner");
}

struct ReleaseObservation {
    std::weak_ptr<void> acquisition;
    unsigned calls = 0;
    bool before = false, after = false;
};
thread_local ReleaseObservation release_observation;

extern "C" std::uint32_t observe_release(const char* descriptor, std::uint32_t size, std::uint32_t ordinal,
    const std::uint64_t* args, std::uint32_t count, std::uint64_t* result) {
    if (ordinal == release_ordinal) {
        ++release_observation.calls;
        release_observation.before = !release_observation.acquisition.expired();
    }
    const auto status = vd_c_primitive_dispatch_v3(descriptor, size, ordinal, args, count, result);
    if (ordinal == release_ordinal)
        release_observation.after = !release_observation.acquisition.expired();
    if (ordinal == 0 && !status && *result)
        release_observation.acquisition = NativeCode::active_lease();
    return status;
}

void reload_resource(const fs::path& original, const fs::path& sandbox, const std::string& descriptor,
                     bool optimize, bool shutdown) {
    release_observation = {};
    const auto admission = CPrimitiveAdmission::admit(descriptor, observe_release);
    const auto path = sandbox / "resources.void";
    auto text = read(original);
    write(path, text);
    const auto before = analyze(path, admission);
    const auto change = text.find(" + 7");
    require(change != text.npos, "reload body marker missing");
    text.replace(change, 4, " + 8");
    write(path, text);
    const auto after = analyze(path, admission);
    auto incompatible = analyze(path, CPrimitiveAdmission::admit(descriptor, alternate));
    Jit jit;
    auto old = jit.compile(*before, optimize);
    ReloadSlot slot(old);
    require(slot.invoke(90) == 31 && g3_live() == 1, "owned resource created before reload");
    require(!jit.replace(slot, *incompatible, optimize), "incompatible linked authority replaced live program");
    require(jit.replace(slot, *after, optimize), "compatible body replacement rejected");
    old.reset();
    require(jit.live_generations() == 2 && slot.invoke(91) == 39, "live resource lost its original generation");
    const auto before_release = g3_releases();
    if (shutdown) require(!slot.acquire()->runtime_owner()->close(), "shutdown after reload failed");
    else require(slot.invoke(92) == 0, "explicit close after reload failed");
    require(release_observation.calls == 1 && release_observation.before && release_observation.after,
            "acquisition generation was unavailable within the terminal thunk");
    require(release_observation.acquisition.expired() && g3_live() == 0 &&
            g3_releases() == before_release + 1 && jit.live_generations() == 1,
            "terminal release did not consume the owner and retire its acquisition generation");
    std::cout << (shutdown ? "shutdown" : "explicit close")
              << " after reload retained the acquisition lease across the C release\n";
}
}

int main(int argc, char** argv) try {
    const bool emit = argc == 6 && std::string_view(argv[1]) == "--emit";
    require(emit || argc == 4, "expected descriptor, source and probe root");
    const auto descriptor = read(argv[emit ? 2 : 1]);
    const fs::path source = argv[emit ? 3 : 2];
    auto admission = CPrimitiveAdmission::admit(descriptor, vd_c_primitive_dispatch_v3);
    auto program = analyze(source, admission);
    if (emit) {
        emit_object(program->lower(Emission::entry), argv[4], std::string_view(argv[5]) == "optimized", true);
        return 0;
    }
    require(!analyze_project(source).succeeded(), "bare source acquired G3 authority");
    descriptors(descriptor);
    malformed(program->lower());
    runtime_denials(descriptor);
    owner_shutdown(descriptor);
    shutdown_race(descriptor, false);
    shutdown_race(descriptor, true);
    failed_shutdown_release(descriptor);
    typed_stale_alias(program->lower());
    const fs::path sandbox = argv[3];
    fs::create_directories(sandbox);
    for (bool optimize : {false, true}) {
        Jit jit;
        auto code = jit.compile(*program, optimize);
        const auto status = code->invoke(7);
        require(status == 104, "source lifecycle result " + std::to_string(status));
        rejects("closed alias", "closed, null or invalid", [&] { (void)code->invoke(-1); });
        rejects("double close", "closed, null or invalid", [&] { (void)code->invoke(-2); });
        rejects("null acquisition", "Null", [&] { (void)code->invoke(-3); });
        require(code->invoke(-4) == 22 && g3_live() == 1, "live shutdown fixture");
        require(!code->runtime_owner()->close() && g3_live() == 0, "Runtime shutdown leaked resource");
        reload_resource(source, sandbox, descriptor, optimize, false);
        reload_resource(source, sandbox, descriptor, optimize, true);
        std::cout << "G3 JIT " << (optimize ? "O2" : "normal") << " returned 104\n";
    }
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
