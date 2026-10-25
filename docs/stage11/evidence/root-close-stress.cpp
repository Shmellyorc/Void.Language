#include "void/runtime.hpp"
#include <atomic>
#include <thread>
#include <memory>
#include <iostream>
#include <stdexcept>
unsigned checks = 0;
void check(bool okay, const char* message) { if (!okay) throw std::runtime_error(message); ++checks; }
void close_during_root_setup() {
    auto runtime = std::make_shared<vd::Runtime>(1);
    std::atomic<bool> completed = false;
    vd::RuntimeFailure failure;
    int state = 0;
    std::thread owner([&] {
        vd::Runtime::Attachment attachment(*runtime);
        vd::Runtime::Managed managed(*runtime);
        state = runtime->globals_begin("root-close-regression", 32768, failure);
        completed = true;
    });
    while (!runtime->memory_snapshot().handles && !completed.load()) std::this_thread::yield();
    runtime->close();
    owner.join();
    check(state == 1 || (state == -1 && failure.message), "shutdown during global root setup returned an invalid state");
    check(runtime->memory_snapshot().handles == 0, "shutdown during global root setup retained persistent handles");
}
int main() { for (unsigned n = 0; n < 128; ++n) close_during_root_setup(); std::cout << checks << " root-close stress checks passed\n"; }
