#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace vd::os {
// Private same-build compiler/Runtime contract. No native handles cross this boundary.
inline constexpr std::string_view source_module = "/@stdlib/os.void";
enum class Operation : unsigned {
    data_root, config_root, cache_root, state_root, home, temp, executable,
    resources, environment, platform, architecture, process_id, bundle
};
struct Contract { std::string_view name; bool text_argument = false; };
inline constexpr std::array operations = {
    Contract{"_os_data_root"}, Contract{"_os_config_root"}, Contract{"_os_cache_root"},
    Contract{"_os_state_root"}, Contract{"_os_home"}, Contract{"_os_temp"},
    Contract{"_os_executable"}, Contract{"_os_resources"}, Contract{"_os_env", true},
    Contract{"_os_platform"}, Contract{"_os_architecture"}, Contract{"_os_process_id"},
    Contract{"_os_bundle"}
};
enum class ErrorKind : std::int64_t {
    success, not_found, invalid_argument, invalid_path, invalid_utf8,
    permission_denied, unavailable, unsupported, system_failure
};
struct Result {
    ErrorKind kind = ErrorKind::success;
    std::int64_t os_code = 0;
    std::int64_t number = 0;
    bool flag = false;
    std::string text;
    std::string native_domain;
};
Result query(Operation, std::string_view input = {});
Result checked_path(std::string, bool executable = false);
Result native_error(std::int64_t, std::string, ErrorKind = ErrorKind::system_failure);
#ifdef __APPLE__
Result apple_query(Operation);
#endif
}
