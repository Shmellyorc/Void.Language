#include <cstdio>
#include <chrono>
#include <thread>

#ifdef VOID_D5_WINDOWS_MODULE_PROBE
#include <windows.h>
#endif

#ifdef _WIN32
#define G4_DEP __declspec(dllexport)
#else
#define G4_DEP __attribute__((visibility("default")))
#endif

static int bias;
struct Startup {
    Startup() {
        if (auto* log = std::fopen("native-init.log", "a")) {
            std::fputs("dependency initialized\n", log);
            std::fclose(log);
            bias = 7;
        }
    }
};
static Startup startup;
extern "C" G4_DEP int g4_conventional_bias() {
#ifdef VOID_D5_WINDOWS_MODULE_PROBE
    if (auto* log = std::fopen("native-modules.log", "wb")) {
        constexpr const wchar_t* names[] = {L"d4_dep.dll", L"d4_provider.dll", L"d4_adapter.dll"};
        for (const auto* name : names) {
            const auto module = GetModuleHandleW(name);
            wchar_t path[32768];
            const auto length = module ? GetModuleFileNameW(module, path, 32768) : 0;
            if (length && length < 32768) std::fwrite(path, sizeof(wchar_t), length, log);
            const wchar_t newline = L'\n';
            std::fwrite(&newline, sizeof(wchar_t), 1, log);
        }
        std::fclose(log);
    }
#endif
    return bias;
}
extern "C" G4_DEP void g4_conventional_delay() { std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
