#pragma once
#include "void/ir.hpp"
#include <string>
namespace vd::numeric {
bool integer(ir::Type);
bool floating(ir::Type);
bool signed_integer(ir::Type);
bool number(ir::Type);
unsigned width(ir::Type);
struct Member { std::string_view name; bool callable; std::vector<std::string_view> parameters; std::string_view documentation; };
const std::vector<Member>& members();
const Member* member(std::string_view);
ir::Type primitive(std::string_view);
std::string symbol(ir::Type,std::string_view);
std::uint64_t limit(ir::Type,bool maximum);
std::uint64_t absolute(std::uint64_t,ir::Type);
std::uint64_t clamp(std::uint64_t,std::uint64_t lower,std::uint64_t upper,ir::Type);
// Compiler-owned typed bodies, emitted once per used canonical numeric callable.
void append_prelude(ir::Module&);
bool supports(ir::Op, ir::Type);
// Public numeric values are width-tagged IEEE/two's-complement bit patterns.
// LLVM support utilities are private implementation details, never IR types.
std::uint64_t literal(std::string text, ir::Type, bool explicit_rounding = false);
std::uint64_t convert(std::uint64_t, ir::Type from, ir::Type to);
std::uint64_t negate(std::uint64_t, ir::Type);
std::uint64_t binary(ir::Op, std::uint64_t, std::uint64_t, ir::Type);
}
