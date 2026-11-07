#include "void/application.hpp"
#include "void/runtime.hpp"
#include "void/utf8.hpp"
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace vd {
namespace {
constexpr std::size_t max_count = 4096, max_bytes = 1024 * 1024;
}
ApplicationArguments checked_application_arguments(std::vector<std::string> values) {
    ApplicationArguments result;
    if (values.size() > max_count) { result.error = 2; return result; }
    std::size_t bytes = 0;
    for (const auto& value : values) {
        if (value.size() > max_bytes - bytes || value.find('\0') != std::string::npos) {
            result.error = 2; return result;
        }
        if (!valid_utf8(value)) { result.error = 4; return result; }
        bytes += value.size();
    }
    result.error = 0; result.values = std::move(values); return result;
}
ApplicationArguments capture_application_arguments(int argc, const void* argv, bool wide) {
    if (argc < 1 || argc > static_cast<int>(max_count + 1) || !argv) return {2, {}};
    std::vector<std::string> values;
    values.reserve(argc - 1);
    std::size_t bytes = 0;
    for (int n = 1; n < argc; ++n) {
        std::string value;
#ifdef _WIN32
        if (wide) {
            const auto* text = static_cast<wchar_t* const*>(argv)[n];
            if (!text) return {2, {}};
            std::size_t length = 0;
            while (length <= max_bytes && text[length]) ++length;
            if (length > max_bytes) return {2, {}};
            const auto count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text,
                static_cast<int>(length), nullptr, 0, nullptr, nullptr);
            if (length && !count) return {4, {}};
            value.resize(count);
            if (count && !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text,
                static_cast<int>(length), value.data(), count, nullptr, nullptr)) return {4, {}};
        } else
#else
        if (wide) return {2, {}};
#endif
        {
            const auto* text = static_cast<char* const*>(argv)[n];
            if (!text) return {2, {}};
            std::size_t length = 0;
            while (length <= max_bytes - bytes && text[length]) ++length;
            if (length > max_bytes - bytes) return {2, {}};
            value.assign(text, length);
        }
        if (value.size() > max_bytes - bytes) return {2, {}};
        bytes += value.size(); values.push_back(std::move(value));
    }
    return checked_application_arguments(std::move(values));
}
void Runtime::application_arguments(ApplicationArguments value) {
    std::lock_guard lock(mutex_);
    if (application_arguments_set_ || global_owner_ != std::thread::id{})
        throw std::logic_error("Application arguments require admission before globals");
    if (!value.error) value = checked_application_arguments(std::move(value.values));
    application_arguments_ = std::move(value); application_arguments_set_ = true;
}
}
