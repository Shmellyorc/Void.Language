#pragma once
#include <filesystem>

namespace vd::project {
enum class ResolveMode { create, refresh, locked };
enum class ResolveResult { no_dependencies, verified, created, refreshed };
// Resolve local source metadata and verify or publish the selected root's lock.
ResolveResult resolve_dependencies(const std::filesystem::path&, ResolveMode);
}
