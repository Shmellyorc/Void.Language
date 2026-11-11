#include <filesystem>
#include <fstream>
#include <string>
#include <windows.h>

int main() {
    const auto capacity = GetEnvironmentVariableW(L"VOID_CREATION_SPAWN", nullptr, 0);
    if (!capacity) return 2;
    std::wstring marker(capacity, L'\0');
    const auto length = GetEnvironmentVariableW(L"VOID_CREATION_SPAWN", marker.data(), capacity);
    if (!length || length >= capacity) return 2;
    marker.resize(length);
    std::ofstream output(std::filesystem::path(marker), std::ios::binary);
    output << "native launch\n";
    output.close();
    return output ? 0 : 1;
}
