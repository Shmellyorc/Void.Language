#pragma once
#include "void/ir.hpp"
#include <string>
namespace vd::numeric {
bool integer(ir::Type);
bool floating(ir::Type);
bool signed_integer(ir::Type);
bool number(ir::Type);
unsigned width(ir::Type);
bool supports(ir::Op, ir::Type);
// Public numeric values are width-tagged IEEE/two's-complement bit patterns.
// LLVM support utilities are private implementation details, never IR types.
std::uint64_t literal(std::string text, ir::Type, bool explicit_rounding = false);
std::uint64_t convert(std::uint64_t, ir::Type from, ir::Type to);
std::uint64_t negate(std::uint64_t, ir::Type);
std::uint64_t binary(ir::Op, std::uint64_t, std::uint64_t, ir::Type);
}
