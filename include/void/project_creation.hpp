#pragma once
#include "void/project.hpp"
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace vd::project {
struct CreationOptions {
    Kind kind = Kind::application;
    bool new_directory = false, nested = false;
    std::optional<std::string> name, version, project_id, application_id, package_id, package_name, source;
};
struct CreationRoot {
    std::filesystem::path path;
    std::string identity;
};
struct CreationPlan {
    CreationRoot parent; // init root, or new's existing parent
    std::filesystem::path root;
    std::optional<std::string> starter;
    std::shared_ptr<const Manifest> manifest;
    bool nested = false;
};
class CreationCollision : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
using CreationEntropy = std::error_code (*)(void*, std::size_t);
CreationRoot inspect_creation_root(const std::filesystem::path&);
bool creation_manifest_present(const std::filesystem::path&);
CreationPlan plan_creation(const std::filesystem::path&, const CreationOptions&, CreationEntropy entropy = nullptr);
using CreationCheckpoint = void (*)(const std::filesystem::path&);
void publish_creation(const CreationPlan&, CreationCheckpoint before_publish = nullptr);
struct CreationInventory {
    CreationRoot root;
    bool manifest_present = false, complete = false;
    std::size_t eligible_count = 0;
    std::vector<std::string> sources; // canonical VFS identities
};
CreationInventory discover_creation(const std::filesystem::path& workspace, const std::filesystem::path& active);
}
