// Investigation harness only. Each process contains one explicitly sized worker.
#include "void/runtime.hpp"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <process.h>
#else
#include <csignal>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#endif
extern "C" {
int array_frame(int);
extern const std::uint64_t probe_static_bytes, probe_elements;
extern const std::uint64_t probe_slot_bytes, probe_frame_bytes, probe_pointer_bytes;
}
namespace {
constexpr unsigned overflow_exit = 90, unexpected_fault_exit = 91;
struct Request { unsigned depth; std::size_t bytes; bool unexpected_fault; int result = 70; };
struct Observation {
    std::uintptr_t previous = 0, minimum = std::numeric_limits<std::uintptr_t>::max(), maximum = 0;
    unsigned count = 0;
    bool invalid = false;
} observation; // Accessed only by the single worker, never by the fault handler.
#ifdef _WIN32
LONG WINAPI fault(EXCEPTION_POINTERS* info) {
    const bool overflow = info->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW;
    const char* text = overflow ?
        "{\"event\":\"stack_overflow\",\"evidence\":\"windows_exception\",\"exception_code\":3221225725}\n" :
        "{\"event\":\"unexpected_fault\",\"evidence\":\"non_stack_windows_exception\"}\n";
    DWORD count;
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), text, static_cast<DWORD>(std::char_traits<char>::length(text)), &count, nullptr);
    TerminateProcess(GetCurrentProcess(), overflow ? overflow_exit : unexpected_fault_exit);
    return EXCEPTION_EXECUTE_HANDLER;
}
#else
std::atomic<std::uintptr_t> guard_low{0}, guard_high{0};
static_assert(std::atomic<std::uintptr_t>::is_always_lock_free);
// Signal handler runs on a separate 64 KiB alternate stack. Only local integer
// formatting, lock-free reads, write and _exit occur here; no runtime or stdio.
void fault(int signal, siginfo_t* info, void*) {
    auto address = reinterpret_cast<std::uintptr_t>(info->si_addr);
    const bool overflow = address >= guard_low.load() && address < guard_high.load();
    const char* prefix = overflow ?
        "{\"event\":\"stack_overflow\",\"evidence\":\"fault_in_low_guard\",\"fault_address\":\"0x" :
        "{\"event\":\"unexpected_fault\",\"evidence\":\"fault_outside_low_guard\",\"fault_address\":\"0x";
    char text[256]; std::size_t n = 0;
    while (*prefix) text[n++] = *prefix++;
    const char* hex = "0123456789abcdef";
    for (unsigned i = sizeof(address) * 2; i; --i) text[n++] = hex[(address >> ((i - 1) * 4)) & 15];
    const char* suffix = signal == SIGSEGV ? "\",\"signal\":\"SIGSEGV\"}\n" : "\",\"signal\":\"SIGBUS\"}\n";
    while (*suffix) text[n++] = *suffix++;
    (void)!write(STDERR_FILENO, text, n);
    _exit(overflow ? overflow_exit : unexpected_fault_exit);
}
#endif
void run(Request& request) {
    std::uintptr_t low = 0, high = 0;
    std::size_t actual = 0;
#ifdef _WIN32
    ULONG_PTR system_low, system_high;
    GetCurrentThreadStackLimits(&system_low, &system_high);
    int marker;
    MEMORY_BASIC_INFORMATION region;
    if (!VirtualQuery(&marker, &region, sizeof(region))) throw std::runtime_error("VirtualQuery failed");
    low = reinterpret_cast<std::uintptr_t>(region.AllocationBase);
    auto cursor = low;
    while (VirtualQuery(reinterpret_cast<void*>(cursor), &region, sizeof(region)) &&
           reinterpret_cast<std::uintptr_t>(region.AllocationBase) == low) {
        if (!region.RegionSize || cursor > std::numeric_limits<std::uintptr_t>::max() - region.RegionSize)
            throw std::runtime_error("invalid stack reservation extent");
        cursor += region.RegionSize;
    }
    high = cursor; actual = high - low;
    ULONG guarantee = 65536;
    if (!SetThreadStackGuarantee(&guarantee)) throw std::runtime_error("SetThreadStackGuarantee failed");
    std::printf("{\"event\":\"stack_control\",\"control\":\"beginthreadex_reservation\",\"requested_bytes\":%zu,\"actual_bytes\":%zu,\"stack_low\":%llu,\"stack_high\":%llu,\"system_low\":%llu,\"system_high\":%llu,\"exception_guarantee_request\":65536,\"previous_guarantee_bytes\":%lu}\n",
        request.bytes, actual, static_cast<unsigned long long>(low), static_cast<unsigned long long>(high),
        static_cast<unsigned long long>(system_low), static_cast<unsigned long long>(system_high), guarantee);
#else
#ifdef __APPLE__
    actual = pthread_get_stacksize_np(pthread_self());
    high = reinterpret_cast<std::uintptr_t>(pthread_get_stackaddr_np(pthread_self()));
    low = high - actual;
#else
    pthread_attr_t attributes;
    if (pthread_getattr_np(pthread_self(), &attributes)) throw std::runtime_error("pthread_getattr_np failed");
    void* address = nullptr;
    const auto status = pthread_attr_getstack(&attributes, &address, &actual);
    pthread_attr_destroy(&attributes);
    if (status) throw std::runtime_error("pthread_attr_getstack failed");
    low = reinterpret_cast<std::uintptr_t>(address); high = low + actual;
#endif
    if (low != guard_high.load()) throw std::runtime_error("OS did not use the supplied worker stack");
    // Alt stack is distinct from the worker's measured usable stack.
    void* alternate = mmap(nullptr, 65536, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (alternate == MAP_FAILED) throw std::runtime_error("alternate stack mmap failed");
    stack_t signal_stack{}; signal_stack.ss_sp = alternate; signal_stack.ss_size = 65536;
    if (sigaltstack(&signal_stack, nullptr)) throw std::runtime_error("sigaltstack failed");
    struct sigaction action{}; action.sa_sigaction = fault; action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGSEGV, &action, nullptr) || sigaction(SIGBUS, &action, nullptr))
        throw std::runtime_error("sigaction failed");
    std::printf("{\"event\":\"stack_control\",\"control\":\"pthread_explicit_mapping\",\"requested_bytes\":%zu,\"actual_bytes\":%zu,\"stack_low\":%llu,\"stack_high\":%llu,\"guard_low\":%llu,\"guard_high\":%llu,\"alternate_signal_stack_bytes\":65536}\n",
        request.bytes, actual, static_cast<unsigned long long>(low), static_cast<unsigned long long>(high),
        static_cast<unsigned long long>(guard_low.load()), static_cast<unsigned long long>(guard_high.load()));
#endif
    std::fflush(stdout);
    if (actual != request.bytes) throw std::runtime_error("reported worker stack differs from requested size");
    if (probe_pointer_bytes != sizeof(void*) || probe_slot_bytes != sizeof(vd::Runtime::Slot) ||
        probe_frame_bytes != sizeof(vd::Runtime::Frame) || alignof(vd::Runtime::Slot) != 8 ||
        alignof(vd::Runtime::Frame) != 8 || offsetof(vd::Runtime::Slot, size) != 8 ||
        offsetof(vd::Runtime::Slot, offsets) != 16 || offsetof(vd::Runtime::Slot, count) != 24)
        throw std::runtime_error("native runtime root ABI differs from generated probe");
    std::printf("{\"event\":\"ready\",\"depth\":%u,\"static_bytes\":%llu,\"elements_per_home\":%llu,\"slot_bytes\":%zu,\"frame_bytes\":%zu}\n",
        request.depth, static_cast<unsigned long long>(probe_static_bytes), static_cast<unsigned long long>(probe_elements),
        sizeof(vd::Runtime::Slot), sizeof(vd::Runtime::Frame));
    std::fflush(stdout);
    if (request.unexpected_fault) {
#ifdef _WIN32
        RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
#else
        raise(SIGSEGV);
#endif
        throw std::runtime_error("unexpected-fault test returned");
    }
    vd::Runtime runtime;
    int result;
    {
        vd::Runtime::Attachment attachment(runtime);
        vd::Runtime::Managed managed(runtime);
        result = array_frame(static_cast<int>(request.depth));
    }
    auto state = runtime.snapshot();
    if (result != static_cast<int>(request.depth) || state.threads || state.objects)
        throw std::runtime_error("collection or balanced runtime cleanup failed");
    if (observation.count != request.depth || observation.invalid ||
        (request.depth > 1 && observation.minimum != observation.maximum))
        throw std::runtime_error("inconsistent native frame spacing");
    request.result = 0;
    std::printf("{\"event\":\"passed\",\"live_frames\":%d,\"balanced_cleanup\":true,\"observed_frames\":%u,\"min_stride_bytes\":%llu,\"max_stride_bytes\":%llu}\n",
        result, observation.count, static_cast<unsigned long long>(request.depth > 1 ? observation.minimum : 0),
        static_cast<unsigned long long>(observation.maximum));
    std::fflush(stdout);
#ifndef _WIN32
    stack_t disable{}; disable.ss_flags = SS_DISABLE;
    if (sigaltstack(&disable, nullptr) || munmap(alternate, 65536))
        throw std::runtime_error("alternate signal stack cleanup failed");
#endif
}
void guarded_run(Request& request) {
    try { run(request); }
    catch (const std::exception& error) {
        request.result = 70;
        std::fprintf(stderr, "probe setup/runtime error: %s\n", error.what());
    }
}
#ifdef _WIN32
unsigned __stdcall worker(void* argument) { guarded_run(*static_cast<Request*>(argument)); return 0; }
#else
void* worker(void* argument) { guarded_run(*static_cast<Request*>(argument)); return nullptr; }
#endif
}
extern "C" void probe_observe_frame(void* frame) noexcept {
    const auto address = reinterpret_cast<std::uintptr_t>(frame);
    if (observation.previous) {
        if (address >= observation.previous) observation.invalid = true;
        else {
            const auto stride = observation.previous - address;
            if (stride < observation.minimum) observation.minimum = stride;
            if (stride > observation.maximum) observation.maximum = stride;
        }
    }
    observation.previous = address;
    ++observation.count;
}
extern "C" int probe_collect() noexcept {
    try { return vd::Runtime::current().collect() == vd::Runtime::Collection::completed; }
    catch (const std::exception& error) {
        std::fprintf(stderr, "probe collection failed: %s\n", error.what()); return 0;
    }
}
int main(int argc, char** argv) {
    if (argc < 3 || argc > 4) return 2;
    try {
        Request request{static_cast<unsigned>(std::stoul(argv[1])), static_cast<std::size_t>(std::stoull(argv[2])), argc == 4};
        if (!request.depth || request.depth > 64 || request.bytes != 1048576 ||
            (argc == 4 && std::string(argv[3]) != "unexpected-fault")) return 2;
#ifdef _WIN32
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
        SetUnhandledExceptionFilter(fault);
        auto handle = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, static_cast<unsigned>(request.bytes), worker,
            &request, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr));
        if (!handle) throw std::runtime_error("_beginthreadex failed");
        auto waited = WaitForSingleObject(handle, 30000);
        CloseHandle(handle);
        if (waited != WAIT_OBJECT_0) {
            // The worker still owns request on our stack. Never return past it.
            TerminateProcess(GetCurrentProcess(), 72);
            return 72;
        }
#else
        rlimit core{0, 0}; if (setrlimit(RLIMIT_CORE, &core)) throw std::runtime_error("cannot disable core dumps");
        auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        // Larger than any probed native frame: a stack subtraction cannot jump
        // over this guard into an unrelated mapping. No source code is changed.
        constexpr std::size_t guard = 524288;
        if (!page || guard % page || request.bytes % page) throw std::runtime_error("unsupported native page size");
        const auto total = guard + request.bytes + page;
        void* mapping = mmap(nullptr, total, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
        if (mapping == MAP_FAILED) throw std::runtime_error("worker stack mmap failed");
        auto* stack = static_cast<char*>(mapping) + guard;
        if (mprotect(stack, request.bytes, PROT_READ | PROT_WRITE)) throw std::runtime_error("worker stack mprotect failed");
        guard_low.store(reinterpret_cast<std::uintptr_t>(mapping));
        guard_high.store(reinterpret_cast<std::uintptr_t>(stack));
        pthread_attr_t attributes;
        if (pthread_attr_init(&attributes)) throw std::runtime_error("pthread_attr_init failed");
        if (pthread_attr_setstack(&attributes, stack, request.bytes)) throw std::runtime_error("pthread_attr_setstack failed");
        pthread_t thread;
        const auto created = pthread_create(&thread, &attributes, worker, &request);
        pthread_attr_destroy(&attributes);
        if (created) throw std::runtime_error("pthread_create failed");
        if (pthread_join(thread, nullptr)) throw std::runtime_error("pthread_join failed");
        if (munmap(mapping, total)) throw std::runtime_error("worker stack cleanup failed");
#endif
        return request.result;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "probe launcher error: %s\n", error.what()); return 70;
    }
}
