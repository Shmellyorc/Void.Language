#pragma once
#include "void/project_creation.hpp"
#include <functional>

namespace vd::project {
inline constexpr std::string_view lock_name = "void.lock";
inline constexpr std::size_t lock_byte_limit = 1024 * 1024;
inline constexpr std::size_t dependency_package_limit = 256;

std::string fold_project_name(std::string_view);
bool dependency_admin_directory(std::string_view);
bool valid_dependency_path(std::string_view);
bool project_path_alias(const std::filesystem::path&);
std::string read_project_input(const std::filesystem::path&, std::size_t limit);
std::optional<std::string> read_dependency_lock(const std::filesystem::path&);
bool publish_dependency_lock(const CreationRoot&, const std::optional<std::string>& previous,
                             std::string_view desired, const std::function<void()>& recheck);

enum class LockShape { valid, malformed, unsupported, conflicting };
LockShape dependency_lock_shape(std::string_view);
}
