#include <string>
#include <unistd.h>

extern "C" __attribute__((visibility("default"))) int void_adapter_probe() {
    auto pid = getpid();
    return pid > 0 && !std::to_string(pid).empty() ? 42 : 1;
}
