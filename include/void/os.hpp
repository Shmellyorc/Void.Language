#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace vd::os {
// Private same-build compiler/Runtime contract. No native handles cross this boundary.
inline constexpr std::string_view source_module = "/@stdlib/os.void";
inline constexpr std::string_view source_prefix = "/@stdlib/os.void::";
struct LocationAdapter { std::string_view facade, implementation; };
inline constexpr std::array location_adapters = {
    LocationAdapter{"data_dir", "_app_data_dir"}, LocationAdapter{"config_dir", "_app_config_dir"},
    LocationAdapter{"cache_dir", "_app_cache_dir"}, LocationAdapter{"state_dir", "_app_state_dir"}
};
inline const LocationAdapter* location_adapter(std::string_view symbol, bool facade) {
    if (!symbol.starts_with(source_prefix)) return nullptr;
    symbol.remove_prefix(source_prefix.size());
    for (const auto& candidate : location_adapters)
        if (symbol == (facade ? candidate.facade : candidate.implementation)) return &candidate;
    return nullptr;
}
enum class Operation : unsigned {
    data_root, config_root, cache_root, state_root, home, temp, executable,
    resources, environment, platform, architecture, process_id, bundle, application_id
};
struct Contract { std::string_view name; bool text_argument = false; };
inline constexpr std::array operations = {
    Contract{"_os_data_root"}, Contract{"_os_config_root"}, Contract{"_os_cache_root"},
    Contract{"_os_state_root"}, Contract{"_os_home"}, Contract{"_os_temp"},
    Contract{"_os_executable"}, Contract{"_os_resources"}, Contract{"_os_env", true},
    Contract{"_os_platform"}, Contract{"_os_architecture"}, Contract{"_os_process_id"},
    Contract{"_os_bundle"}, Contract{"_os_application_id"}
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
