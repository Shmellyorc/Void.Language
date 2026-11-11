#pragma once
#include "void/project_creation.hpp"
#include <functional>
#include <map>

#ifdef _WIN32
namespace vd::native_windows { struct DirectoryReservation; }
#endif

namespace vd::project {
inline constexpr std::string_view lock_name = "void.lock";
inline constexpr std::size_t lock_byte_limit = 1024 * 1024;
inline constexpr std::size_t dependency_package_limit = 256;
inline constexpr std::string_view git_store_name = "git-deps";

std::string fold_project_name(std::string_view);
bool dependency_admin_directory(std::string_view);
bool valid_dependency_path(std::string_view);
bool valid_git_subdir(std::string_view);
bool project_path_alias(const std::filesystem::path&);
std::string read_project_input(const std::filesystem::path&, std::size_t limit);

// Keep borrowed stage authority within the synchronous publisher callback.
struct ProjectDirectoryReader {
    std::function<CreationRoot(const std::filesystem::path&)> inspect = inspect_creation_root;
    std::function<std::string(const std::filesystem::path&, std::size_t)> read = read_project_input;
#ifdef _WIN32
    const native_windows::DirectoryReservation* reservation = nullptr;
#endif
};
using VerifyProjectDirectory = std::function<void(const std::filesystem::path&, const ProjectDirectoryReader&)>;

std::optional<std::string> read_dependency_lock(const std::filesystem::path&);
bool publish_dependency_lock(const CreationRoot&, const std::optional<std::string>& previous,
                             std::string_view desired, const std::function<void()>& recheck);
void publish_git_snapshot(const CreationRoot&, const std::string& key,
                          const std::map<std::string, std::string_view>& files,
                          const VerifyProjectDirectory& verify);
void publish_project_directory(const CreationRoot&, const std::string& leaf,
                               const std::map<std::string, std::string_view>& files,
                               const VerifyProjectDirectory& verify,
                               const std::function<void()>& recheck = {},
                               const ProjectDirectoryReader& parent_reader = {});
void publish_application_directory(const CreationRoot&, const std::string& leaf,
                                   const VerifyProjectDirectory& prepare,
                                   const std::function<void(const std::filesystem::path&)>& verify_replacement);

enum class LockShape { valid, malformed, unsupported, conflicting };
LockShape dependency_lock_shape(std::string_view);
}
