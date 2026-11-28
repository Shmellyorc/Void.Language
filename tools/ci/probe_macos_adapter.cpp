#include "void/application.hpp"
#include <unistd.h>

extern "C" __attribute__((visibility("default"))) int void_adapter_probe() {
    const char* arguments[]{"./app", "\xe9\x9b\xaa"};
    const auto captured = vd::capture_application_arguments(2, arguments, false);
    return getpid() > 0 && !captured.error && captured.values.size() == 1 &&
        captured.values.front() == arguments[1] ? 42 : 1;
}
