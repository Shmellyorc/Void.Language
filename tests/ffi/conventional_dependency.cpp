#include <cstdio>
#include <chrono>
#include <thread>

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
extern "C" G4_DEP int g4_conventional_bias() { return bias; }
extern "C" G4_DEP void g4_conventional_delay() { std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
