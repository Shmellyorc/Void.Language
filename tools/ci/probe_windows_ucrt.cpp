#include <cstdio>
#include <stdexcept>
#include <string>
#include <thread>

int main() {
    std::string result;
    std::thread worker([&] {
        try {
            throw std::runtime_error("VOID UCRT ABI probe");
        } catch (const std::exception& failure) {
            result = failure.what();
        }
    });
    worker.join();
    if (result != "VOID UCRT ABI probe") return 1;
    std::puts(result.c_str());
    return 0;
}
