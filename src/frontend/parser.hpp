#pragma once

#include "lexer.hpp"
#include <map>

namespace vd::frontend {
struct Unit;

void parse_unit(Unit&, Diagnostics&, std::string_view module = {},
                const BuildConditions& = native_build_conditions(),
                std::vector<Token>* syntax = nullptr);
void parse_specialization(Unit&, Diagnostics&, std::vector<Token>, std::map<std::string, ir::Type>);
ir::Type parse_constraint(Unit&, Diagnostics&, const std::vector<Token>&,
                          const std::map<std::string, ir::Type>&);
}
