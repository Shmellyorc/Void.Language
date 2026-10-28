#include "void/os.hpp"
#include "void/detail/os_integer.hpp"
#include "void/os_abi.hpp"
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/cache.hpp"
#include "void/path.hpp"
#include "../managed/probe.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#ifdef __linux__
#include <pwd.h>
#endif
#endif
namespace {
unsigned checks = 0, serial = 0;
std::filesystem::path directory;
void check(bool yes, const char* message) { if (!yes) throw std::runtime_error(message); ++checks; }
auto analyze(const std::string& text) {
    auto file = directory / "main.void";
    std::ofstream(directory / ("snapshot" + std::to_string(++serial) + ".void"), std::ios::binary) << text;
    std::ofstream(file, std::ios::binary) << text;
    auto result = vd::analyze_project(file);
    if (!result.succeeded()) throw vd::CompilationError(result.diagnostics);
    check(result.program->warnings().empty(), "unexpected documentation warnings");
    return result.program;
}
struct Environment {
    std::string name, old; bool present;
    static void set(const char* name, const char* value) {
#ifdef _WIN32
        auto wide = vd::utf8_path(name).native();
        if (value) {
            auto contents = vd::utf8_path(value).native();
            if (!SetEnvironmentVariableW(wide.c_str(), contents.c_str())) throw std::runtime_error("environment fixture set");
        } else if (!SetEnvironmentVariableW(wide.c_str(), nullptr) && GetLastError() != ERROR_ENVVAR_NOT_FOUND) throw std::runtime_error("environment fixture unset");
#else
        if (value) { if (setenv(name, value, 1)) throw std::runtime_error("environment fixture set"); }
        else if (unsetenv(name)) throw std::runtime_error("environment fixture unset");
#endif
    }
    Environment(const char* key, const char* value) : name(key), present(std::getenv(key) != nullptr) {
        if (present) old = std::getenv(key); set(key, value);
    }
    ~Environment() { set(name.c_str(), present ? old.c_str() : nullptr); }
};
std::int64_t pid() {
#ifdef _WIN32
    return static_cast<std::int64_t>(GetCurrentProcessId());
#else
    return static_cast<std::int64_t>(getpid());
#endif
}
#ifdef __linux__
int passwd_mode = 0, passwd_calls = 0, image_mode = 0, image_calls = 0;
bool native_gc = false;
#endif
struct Reply {
    std::int64_t kind = 0, code = 0, number = 0;
    bool flag = false; void* text = nullptr; void* domain = nullptr;
};
constexpr std::array<std::size_t, 2> reference_offsets = {offsetof(Reply, text), offsetof(Reply, domain)};
constexpr vd::os::ReplyLayout geometry{sizeof(Reply), {offsetof(Reply,kind),offsetof(Reply,code),offsetof(Reply,number),offsetof(Reply,flag),offsetof(Reply,text),offsetof(Reply,domain)}};
void boundary() {
    using namespace vd;
    using namespace vd::os;
    Environment value("VOID_OS_HOST_VALUE", "owned snow \xe9\x9b\xaa");
    Runtime runtime(1); // Collection before each subsequent managed allocation, including domain publication.
    Runtime::Attachment attached(runtime); Runtime::Managed managed(runtime);
    Reply reply;
    Runtime::Slot slots[] = {{&reply, sizeof(reply), reference_offsets.data(), reference_offsets.size()}};
    Runtime::Frame rooted(runtime, slots, 1);
    void* name = nullptr; std::size_t zero = 0;
    Runtime::Slot name_slot{&name, sizeof(name), &zero, 1}; Runtime::Frame name_root(runtime, &name_slot, 1);
    runtime.string_into(&name, "VOID_OS_HOST_VALUE");
    RuntimeFailure failure;
    vd_os_call(&reply, &geometry, static_cast<unsigned>(Operation::environment), name, &failure);
    check(!failure.message && reply.kind == 0 && runtime.string_bytes(reply.text) == "owned snow \xe9\x9b\xaa", "owned env publication or forced GC");
    check(runtime.string_bytes(reply.domain).empty(), "non-native domain is not empty");
    Environment::set("VOID_OS_HOST_VALUE", "changed");
    check(runtime.string_bytes(reply.text) == "owned snow \xe9\x9b\xaa", "borrowed native environment escaped");
    auto before = runtime.snapshot().objects;
    std::int64_t process = -1;
    vd_os_call(&process, nullptr, static_cast<unsigned>(Operation::process_id), nullptr, &failure);
    check(!failure.message && process == pid() && runtime.snapshot().objects == before, "PID allocated managed strings or changed native bits");
    for (auto bits : {std::uint32_t(0), std::uint32_t(0x7fffffff), std::uint32_t(0x80000000), std::uint32_t(0xffffffff)}) {
        check(vd::os::detail::process_id_from_dword(bits) == static_cast<std::int64_t>(static_cast<std::uint64_t>(bits)), "unsigned DWORD widening");
    }
    for (unsigned mutation = 0; mutation < 9; ++mutation) {
        auto layout = geometry; auto op = static_cast<unsigned>(Operation::home); void* input = nullptr;
        if (mutation < 6) ++layout.fields[mutation];
        if (mutation == 6) ++layout.size;
        if (mutation == 7) op = 99;
        if (mutation == 8) input = name;

        failure = {};
        vd_os_call(&reply, &layout, op, input, &failure);
        check(failure.message != nullptr, "invalid ABI geometry accepted");
    }
    failure = {};
    vd_os_call(&reply, &geometry, static_cast<unsigned>(Operation::environment), nullptr, &failure);
    check(!failure.message && reply.kind == static_cast<std::int64_t>(ErrorKind::invalid_argument), "empty Runtime string caused ABI failure");
    unsigned failures = 0, successes = 0;
    for (int budget = 0; budget < 150; ++budget) {
        failure = {};
        {
            managed_test::AllocationFailure fault(budget, true);
            vd_os_call(&reply, &geometry, static_cast<unsigned>(Operation::environment), name, &failure);
        }
        if (failure.message) ++failures; else {
            ++successes; check(reply.kind == 0 && runtime.string_bytes(reply.text) == "changed", "fault published partial result");
        }
        if (failures && successes) break;
    }
    check(failures && successes, "native/managed allocation fault sweep did not cross failure boundary");
#ifdef __linux__
    image_mode = 3; image_calls = 0; failure = {};
    vd_os_call(&reply, &geometry, static_cast<unsigned>(Operation::executable), nullptr, &failure);
    image_mode = 0;
    check(native_gc && !failure.message, "OS query did not enter NativeSafe for concurrent collection");
    image_mode = 2; image_calls = 0; failure = {};
    vd_os_call(&reply, &geometry, static_cast<unsigned>(Operation::executable), nullptr, &failure);
    image_mode = 0;
    check(!failure.message && reply.kind == static_cast<std::int64_t>(ErrorKind::permission_denied) && reply.code == EACCES && runtime.string_bytes(reply.domain) == "errno", "native code/domain rooted publication");
#endif
    check(runtime.collect() == Runtime::Collection::completed, "final boundary collection");
}
void kernel() {
    using namespace vd::os;
    check(query(Operation::process_id).number == pid(), "native process ID differs");
    check(query(Operation::executable).kind == ErrorKind::success, "real native image unavailable");
    for (auto name : {"", "a=b", "=C:"}) check(query(Operation::environment, name).kind == ErrorKind::invalid_argument, "environment name validation");
    check(query(Operation::environment, std::string("a\0b",3)).kind == ErrorKind::invalid_argument, "embedded NUL accepted");
    check(query(Operation::environment, std::string("\xff",1)).kind == ErrorKind::invalid_utf8, "invalid UTF-8 input accepted");
    Environment missing("VOID_OS_NATIVE_ABSENT", nullptr);
    check(query(Operation::environment,"VOID_OS_NATIVE_ABSENT").kind == ErrorKind::not_found, "absent env is not not_found");
    Environment empty("VOID_OS_NATIVE_EMPTY", "");
    auto r = query(Operation::environment,"VOID_OS_NATIVE_EMPTY");
    check(r.kind == ErrorKind::success && r.text.empty(), "empty env conflated with absent");
    Environment unicode("VOID_OS_NATIVE_VALUE", " snow \xe9\x9b\xaa\n=a ");
    check(query(Operation::environment,"VOID_OS_NATIVE_VALUE").text == " snow \xe9\x9b\xaa\n=a ", "environment text rewritten");
    {
        const char* unicode_name = "VOID_OS_\xe9\x9b\xaa\xf0\x9f\x98\x80";
        Environment unicode_key(unicode_name,"unicode name value");
        check(query(Operation::environment,unicode_name).text == "unicode name value", "strict Unicode environment name");
    }
#ifdef _WIN32
    check(query(Operation::environment,"void_os_native_value").text == " snow \xe9\x9b\xaa\n=a ", "Windows environment case semantics");
#else
    {
        Environment lower("void_os_native_value",nullptr);
        check(query(Operation::environment,"void_os_native_value").kind == ErrorKind::not_found, "POSIX environment names became case insensitive");
    }
#endif
#ifdef __linux__
    {
        std::string large(2*1024*1024,'x');
        Environment huge("VOID_OS_NATIVE_HUGE",large.c_str());
        check(query(Operation::environment,"VOID_OS_NATIVE_HUGE").text == large, "large environment used stack storage or truncated");
    }
    Environment home("HOME", "");
    passwd_mode = 0;
    check(query(Operation::home).kind == ErrorKind::success, "real getpwuid_r fallback");
    for (int mode = 1; mode <= 5; ++mode) {
        passwd_mode = mode; passwd_calls = 0;
        auto result = query(Operation::home);
        if (mode == 1) check(result.text == "/passwd/fallback" && passwd_calls == 2, "ERANGE retry failed");
        if (mode == 2) check(result.kind == ErrorKind::unavailable && result.os_code == 0, "missing passwd record");
        if (mode == 3) check(result.kind == ErrorKind::permission_denied && result.os_code == EACCES && result.native_domain == "errno", "getpwuid_r return code lost");
        if (mode == 4) check(result.kind == ErrorKind::invalid_path, "relative passwd home accepted");
        if (mode == 5) check(result.kind == ErrorKind::invalid_utf8, "invalid passwd home accepted");
    }
    passwd_mode = 6;
    bool allocation_failed = false;
    try { (void)query(Operation::home); } catch (const std::bad_alloc&) { allocation_failed = true; }
    check(allocation_failed, "native ENOMEM became recoverable environmental error");
    passwd_mode = 0;
    image_mode = 1; image_calls = 0;
    auto image = query(Operation::executable); image_mode = 0;
    check(image.text == "/" + std::string(600,'x') + " (deleted)" && image_calls == 3, "readlink full-buffer retry/deleted suffix");
    image_mode = 2;
    auto denied = query(Operation::executable); image_mode = 0;
    check(denied.kind == ErrorKind::permission_denied && denied.os_code == EACCES && denied.native_domain == "errno", "image native error lost");
    image_mode = 4;
    auto absent = query(Operation::executable); image_mode = 0;
    check(absent.kind == ErrorKind::unavailable && absent.os_code == ENOENT, "missing procfs silently fell back");
    for (int mode : {5,6}) {
        image_mode = mode;
        auto result = query(Operation::executable); image_mode = 0;
        check(result.kind == (mode == 5 ? ErrorKind::unsupported : ErrorKind::system_failure) &&
              result.os_code == (mode == 5 ? ENOSYS : EIO) && result.native_domain == "errno", "explicit unsupported/system native diagnostic");
    }
    Environment bad("VOID_OS_NATIVE_VALUE", "\xff");
    check(query(Operation::environment,"VOID_OS_NATIVE_VALUE").kind == ErrorKind::invalid_utf8, "malformed native env repaired");
#endif
#ifdef _WIN32
    SetLastError(ERROR_ACCESS_DENIED);
    check(query(Operation::environment,"VOID_OS_NATIVE_EMPTY").kind == ErrorKind::success, "stale Win32 last error changed empty env result");
    {
        Environment long_value("VOID_OS_LONG", std::string(30000,'x').c_str());
        check(query(Operation::environment,"VOID_OS_LONG").text == std::string(30000,'x'), "growing Windows env buffer truncated");
    }
    const wchar_t malformed[] = {static_cast<wchar_t>(0xd800),0};
    check(SetEnvironmentVariableW(L"VOID_OS_BAD_UTF16",malformed), "malformed UTF-16 native fixture");
    auto bad_utf16 = query(Operation::environment,"VOID_OS_BAD_UTF16");
    SetEnvironmentVariableW(L"VOID_OS_BAD_UTF16",nullptr);
    check(bad_utf16.kind == ErrorKind::invalid_utf8 && bad_utf16.os_code == ERROR_NO_UNICODE_TRANSLATION && bad_utf16.native_domain == "win32", "unpaired native surrogate repaired or code lost");
    for (const auto& path : {std::string("C:/space # %/雪/"), std::string("//server/share/雪/"), std::string("//?/C:/")+std::string(400,'x'), std::string("//?/UNC/server/share/雪/")}) {
        auto checked = checked_path(path);
        check(checked.kind == ErrorKind::success && checked.text == path, "Windows native namespace rewritten");
    }
    // Preserve existing host apartments, including changed-mode initialization.
    for (DWORD apartment : {COINIT_APARTMENTTHREADED, COINIT_MULTITHREADED}) {
        auto status = CoInitializeEx(nullptr, apartment);
        check(SUCCEEDED(status), "native apartment fixture initialization");
        auto root = query(Operation::home);
        check(root.kind == ErrorKind::success, "Known Folder inside existing apartment");
        APTTYPE type; APTTYPEQUALIFIER qualifier;
        check(SUCCEEDED(CoGetApartmentType(&type,&qualifier)), "Known Folder uninitialized host apartment");
        check((apartment == COINIT_MULTITHREADED) == (type == APTTYPE_MTA), "Known Folder altered host apartment");
        CoUninitialize();
    }
#endif
}
void invalid(vd::ir::Module module) {
    bool denied = false;
    try { vd::ir::validate(module); } catch (const std::invalid_argument&) { denied = true; }
    check(denied,"forged OS IR accepted");
}
void ir_contracts() {
    {
        using namespace vd::ir;
        Module module; module.identity = "os-independent"; module.coordinated = true;
        Function function; function.module_identity = "/@stdlib/os.void";
        function.source_name = "native_pid"; function.name = function.module_identity + "::native_pid";
        function.parameters.clear(); function.result = Type::i64;
        Instruction query{Op::os_query}; query.type = Type::i64;
        query.immediate = static_cast<unsigned>(vd::os::Operation::process_id);
        function.blocks = {{{query},{Terminator::Kind::return_value,0}}}; module.functions = {function};
        validate(module); ++checks;
        for (const auto* owner : {"", "/main.void", "/os.void", "/@stdlib/os.void.fake"}) {
            auto forged = module; auto& fn = forged.functions[0]; fn.module_identity = owner;
            fn.name = fn.module_identity.empty() ? fn.source_name : fn.module_identity + "::" + fn.source_name;
            invalid(std::move(forged));
        }
        auto forged = module; forged.functions[0].blocks[0].instructions[0].operator_glyph = "+"; invalid(std::move(forged));
        forged = module; forged.functions[0].result = Type::i32;
        forged.functions[0].blocks[0].instructions[0].type = Type::i32;
        invalid(std::move(forged));
        auto reply = std::make_shared<ValueType>(); reply->identity = "/@stdlib/os.void::_reply";
        reply->name = "_reply"; reply->module_identity = "/@stdlib/os.void";
        reply->fields = {{"kind",Type::i64},{"os_code",Type::i64},{"number",Type::i64},
                         {"flag",Type::boolean},{"text",Type::string},{"native_domain",Type::string}};
        auto aggregate = module; aggregate.functions[0].result = Type(reply);
        auto& operation = aggregate.functions[0].blocks[0].instructions[0];
        operation.immediate = static_cast<unsigned>(vd::os::Operation::home); operation.type = Type(reply);
        validate(aggregate); ++checks;
        for (unsigned field = 0; field < 6; ++field) {
            auto mutated = aggregate; auto shape = std::make_shared<ValueType>(*reply);
            shape->fields[field].type = field < 3 ? FieldType(Type::boolean) : FieldType(Type::i64);
            mutated.functions[0].result = Type(shape); mutated.functions[0].blocks[0].instructions[0].type = Type(shape);
            invalid(std::move(mutated));
        }
        for (unsigned mutation = 0; mutation < 3; ++mutation) {
            auto mutated = aggregate; auto shape = std::make_shared<ValueType>(*reply);
            if (mutation == 0) shape->fields.pop_back();
            if (mutation == 1) shape->module_identity = "/os.void";
            if (mutation == 2) shape->identity = "/@stdlib/os.void::fake_reply";
            mutated.functions[0].result = Type(shape); mutated.functions[0].blocks[0].instructions[0].type = Type(shape);
            invalid(std::move(mutated));
        }
        auto wrong_input = aggregate;
        Instruction value{Op::constant}; value.type = Type::i64;
        auto env = operation; env.immediate = static_cast<unsigned>(vd::os::Operation::environment); env.arguments = {0};
        wrong_input.functions[0].blocks = {{{value,env},{Terminator::Kind::return_value,1}}};
        invalid(std::move(wrong_input));
    }
    auto program = analyze("use os\nfunc entry(n: i32) -> i32:\n    var value = os.env(\"VOID_OS_TEST\")\n    return (os.process_id() % 1000000) as i32\n");
    auto valid = program->lower(); vd::ir::validate(valid); ++checks;
    for (unsigned mutation = 0; mutation < 11; ++mutation) {
        auto forged = valid;
        if (mutation == 0) forged.coordinated = false;
        bool done = false;
        for (auto& function : forged.functions) for (auto& block : function.blocks) for (auto& i : block.instructions) {
            if (i.op != vd::ir::Op::os_query || done) continue;
            if (mutation == 1) function.module_identity = "/os.void";
            if (mutation == 2) i.immediate = 99;
            if (mutation == 3) i.a = 1;
            if (mutation == 4) i.b = 1;
            if (mutation == 5) i.callee = "fake";
            if (mutation == 6) i.initialization = true;
            if (mutation == 7) i.array_repeat = true;
            if (mutation == 8) i.type = vd::ir::Type::string;
            if (mutation == 9) i.receiver = vd::ir::Place{0,{}};
            if (mutation == 10) { if (i.arguments.empty()) i.arguments = {0}; else i.arguments.clear(); }
            done = true;
        }
        check(done,"canonical query missing from IR"); invalid(std::move(forged));
    }
    auto worker = analyze("use os\nfunc work(n: i32) -> i32:\n    return n\nasync func entry(n: i32) -> i32:\n    var task = spawn work(n)\n    var answer = await task\n    if n < 0:\n        var p = os.process_id()\n    return answer\n")->lower();
    std::string native;
    for (auto& fn : worker.functions) for (auto& block : fn.blocks) for (auto& i : block.instructions)
        if (i.op == vd::ir::Op::os_query && i.immediate == static_cast<unsigned>(vd::os::Operation::process_id)) native = fn.name;
    check(!native.empty(),"PID wrapper not emitted");
    unsigned changed = 0;
    for (auto& fn : worker.functions) if (fn.worker) {
        vd::ir::Instruction call{vd::ir::Op::call}; call.type = vd::ir::Type::i64; call.callee = native;
        fn.blocks[0].instructions.push_back(call); ++changed;
    }
    check(changed > 0,"worker fixture not lowered"); invalid(std::move(worker));
}
void lifecycle(bool optimize) {
    Environment value("VOID_OS_LIFECYCLE", "first");
    const std::string text = R"VOID(use os
var remembered = read_value()
func read_value() -> string:
    if os.env("VOID_OS_LIFECYCLE") is Ok(var text):
        return text
    return "missing"
func entry(n: i32) -> i32:
    if remembered == "first":
        if read_value() == "first":
            return 1
        return 2
    return 3
)VOID";
    auto program = analyze(text);
    vd::Jit jit({},vd::ir::Emission::entry); auto code = jit.compile(*program,optimize);
    check(code->invoke(0) == 1,"runtime global eager lookup");
    Environment::set("VOID_OS_LIFECYCLE","second");
    check(code->invoke(0) == 2,"explicit global not retained or fresh call cached");
    auto fresh = jit.compile(*program,optimize);
    check(fresh->invoke(0) == 3,"independent Runtime inherited global state");
    vd::ReloadSlot slot(code);
    auto changed = text; auto at = changed.find("return 2"); changed.replace(at,8,"return 12");
    check(jit.replace(slot,*analyze(changed),optimize),"compatible OS body reload rejected");
    check(slot.invoke(0) == 12,"reload reset stored discovery snapshot");
    auto owner = code->runtime_owner();
    { vd::Runtime::Attachment attached(*owner); vd::Runtime::Managed managed(*owner);
      check(owner->collect() == vd::Runtime::Collection::completed,"global forced collection"); }
    check(slot.invoke(0) == 12,"global path lost after GC");
    vd::Jit cache({directory/(optimize?"cache-o2":"cache-o0"),std::string(vd::compiler_build_identity())},vd::ir::Emission::entry);
    auto cold = cache.compile(*program,optimize);
    check(cold->invoke(0) == 3,"cache miss lookup state");
    auto miss = cache.cache_status();
    Environment::set("VOID_OS_LIFECYCLE","first");
    auto hit = cache.compile(*program,optimize);
    check(cache.cache_status() == (miss == "unavailable" ? "unavailable" : "hit"),"OS cache did not hit");
    check(hit->invoke(0) == 1 && cold->invoke(0) == 3,"cache serialized host state");
    owner->close(); check(owner->memory_snapshot().handles == 0,"global discovery handle leaked at close");
    auto pid_program = analyze("use os\nfunc entry(n: i32) -> i32:\n    return (os.process_id() % 1000000) as i32\n");
    for (unsigned index = 0; index < 2; ++index) check(jit.compile(*pid_program,optimize)->invoke(0) == pid()%1000000,"Runtime process identity differs");
}
void measure() {
    using Clock = std::chrono::steady_clock;
    auto ns = [](auto begin) { return std::chrono::duration<double,std::nano>(Clock::now()-begin).count(); };
    constexpr unsigned repeats = 2000;
    std::cout << "phase,operation,first_ns,repeat_mean_ns,repeats\n";
    for (unsigned operation = 0; operation < vd::os::operations.size(); ++operation) {
        auto selected = static_cast<vd::os::Operation>(operation);
        auto input = vd::os::operations[operation].text_argument ? "PATH" : "";
        auto start = Clock::now(); auto result = vd::os::query(selected,input); auto first = ns(start);
        start = Clock::now();
        for (unsigned n = 0; n < repeats; ++n) result = vd::os::query(selected,input);
        auto repeated = ns(start)/repeats;
        std::cout << "native," << vd::os::operations[operation].name << ',' << first << ',' << repeated << ',' << repeats << '\n';
    }
    {
        vd::Runtime runtime(4096); vd::Runtime::Attachment attached(runtime); vd::Runtime::Managed managed(runtime);
        Reply reply; vd::Runtime::Slot slot{&reply,sizeof(reply),reference_offsets.data(),reference_offsets.size()};
        vd::Runtime::Frame roots(runtime,&slot,1);
        void* input = nullptr; std::size_t zero = 0;
        vd::Runtime::Slot input_slot{&input,sizeof(input),&zero,1}; vd::Runtime::Frame input_root(runtime,&input_slot,1);
        runtime.string_into(&input,"PATH");
        for (unsigned operation : {4u,6u,8u,11u}) {
            vd::RuntimeFailure failure; std::int64_t pid_value = 0;
            auto call = [&] {
                bool scalar = operation == 11;
                vd_os_call(scalar ? static_cast<void*>(&pid_value) : static_cast<void*>(&reply),scalar ? nullptr : &geometry,
                           operation,operation == 8 ? input : nullptr,&failure);
                if (failure.message) throw std::runtime_error(failure.message);
            };
            auto start = Clock::now(); call(); auto first = ns(start);
            start = Clock::now(); for (unsigned n = 0; n < repeats; ++n) call(); auto repeated = ns(start)/repeats;
            std::cout << "bridge_publish," << vd::os::operations[operation].name << ',' << first << ',' << repeated << ',' << repeats << '\n';
        }
    }
    auto start = Clock::now();
    auto program = analyze("use os\nfunc entry(n: i32) -> i32:\n    var count: i64 = 0\n    for index in 0..<n:\n        if os.data_dir(\"com.game\") is Ok(var folder):\n            count += folder.byte_length\n    return (count % 1000000) as i32\n");
    auto analyzed = ns(start); vd::Jit jit({},vd::ir::Emission::entry);
    start = Clock::now(); auto code = jit.compile(*program,true); auto compiled = ns(start);
    start = Clock::now(); auto first = code->invoke(1); auto first_ns = ns(start);
    start = Clock::now(); auto repeated = code->invoke(repeats); auto repeated_ns = ns(start)/repeats;
    check(first > 0 && repeated == (first*static_cast<int>(repeats))%1000000,"source measurement checksum");
    std::cout << "source_analysis_ns," << analyzed << "\nsource_compile_o2_ns," << compiled
              << "\nsource_execute,data_dir," << first_ns << ',' << repeated_ns << ',' << repeats << '\n';
}
}
#ifdef __linux__
extern "C" int __real_getpwuid_r(uid_t,passwd*,char*,std::size_t,passwd**);
extern "C" int __wrap_getpwuid_r(uid_t uid,passwd* record,char* buffer,std::size_t size,passwd** output) {
    if (!passwd_mode) return __real_getpwuid_r(uid,record,buffer,size,output);
    ++passwd_calls; *output = nullptr;
    if (passwd_mode == 1 && passwd_calls == 1) return ERANGE;
    if (passwd_mode == 2) return 0;
    if (passwd_mode == 3) { errno = ENOENT; return EACCES; }
    if (passwd_mode == 6) return ENOMEM;
    const char* path = passwd_mode == 4 ? "relative" : passwd_mode == 5 ? "/bad\xff" : "/passwd/fallback";
    std::strcpy(buffer,path); record->pw_dir = buffer; *output = record; return 0;
}
extern "C" ssize_t __real_readlink(const char*,char*,std::size_t);
extern "C" ssize_t __wrap_readlink(const char* path,char* buffer,std::size_t size) {
    if (!image_mode || std::strcmp(path,"/proc/self/exe")) return __real_readlink(path,buffer,size);
    ++image_calls;
    if (image_mode == 2 || image_mode == 4) { errno = image_mode == 2 ? EACCES : ENOENT; return -1; }
    if (image_mode == 5 || image_mode == 6) { errno = image_mode == 5 ? ENOSYS : EIO; return -1; }
    if (image_mode == 3) {
        auto& runtime = vd::Runtime::current();
        std::thread collector([&] { vd::Runtime::Attachment attached(runtime); vd::Runtime::Managed managed(runtime);
            native_gc = runtime.collect() == vd::Runtime::Collection::completed; });
        collector.join();
        return __real_readlink(path,buffer,size);
    }
    auto text = "/" + std::string(600,'x') + " (deleted)";
    auto count = std::min(size,text.size()); std::memcpy(buffer,text.data(),count); return count;
}
#endif
int main(int argc,char** argv) {
    try {
        if (argc != 2 && argc != 3) throw std::runtime_error("work directory required");
        directory = std::filesystem::absolute(argv[1]); std::filesystem::create_directories(directory);
        if (argc == 3 && std::string(argv[2]) == "--measure") { measure(); return 0; }
        kernel(); boundary(); ir_contracts();
        for (bool optimize : {false,true}) lifecycle(optimize);
        std::cout << "OS native/ABI/GC/IR/globals/reload/cache: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
