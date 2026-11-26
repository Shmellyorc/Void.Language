#pragma once

#include "model.hpp"

namespace vd::frontend {

void check_string_pool(const std::map<std::string, std::unique_ptr<Unit>>&);
ir::Module lower_graph(const std::map<std::string, std::unique_ptr<Unit>>&, const std::string& entry,
                       std::shared_ptr<const project::LockedDependencies>);
}
