#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace vd::project {
// Runs the transport in its own process tree. Only the supplied environment
// and private working directory are visible to Git as configuration inputs.
int run_git_fetch(const std::filesystem::path& work, const std::vector<std::string>& args,
                  const std::vector<std::string>& environment,
                  std::chrono::steady_clock::time_point deadline);
}
