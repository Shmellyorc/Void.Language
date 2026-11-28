#include "void/application.hpp"
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
    const wchar_t* arguments[]{L"app.exe", L"\u96ea"};
    const auto captured = vd::capture_application_arguments(2, arguments, true);
    if (captured.error || captured.values.size() != 1 || captured.values.front() != "\xe9\x9b\xaa")
        return 2;
    std::puts(result.c_str());
    return 0;
}
