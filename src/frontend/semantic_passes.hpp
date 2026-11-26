#pragma once

#include "model.hpp"

namespace vd::frontend {

void analyze_capabilities(std::map<std::string, std::unique_ptr<Unit>>&, Diagnostics&);
void analyze_construction(std::map<std::string, std::unique_ptr<Unit>>&, Diagnostics&);
void validate_inheritance(std::map<std::string, std::unique_ptr<Unit>>&, Unit& root, Diagnostics&);
}
