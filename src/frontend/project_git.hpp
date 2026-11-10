#pragma once
#include "project_files.hpp"
#include <set>

namespace vd::project {
struct GitSnapshot {
    std::filesystem::path tree;
    std::map<std::string, std::string, std::less<>> files;
    std::set<std::string, std::less<>> directories;
    std::string proof;
    std::string digest;
    std::size_t bytes = 0;
};

std::string git_store_key(const GitPin&);
std::filesystem::path git_snapshot_path(const std::filesystem::path&, const GitPin&);
std::optional<GitSnapshot> read_git_snapshot(const std::filesystem::path& pin_directory, const GitPin&);
GitSnapshot acquire_git_snapshot(const std::filesystem::path& root, const GitPin&);
void publish_git_snapshot(const CreationRoot&, const GitPin&, const GitSnapshot&);
}
