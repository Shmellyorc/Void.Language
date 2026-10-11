#pragma once
#include "void/source.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace vd::ir {
// Architecture-proof IR: every function has signature i32(i32).
// Local IDs are instruction indexes. Values may only be used in their block.
enum class Op { argument, constant, add, multiply, less, call };
struct Instruction {
    Op op;
    std::uint32_t a = 0, b = 0;
    std::int32_t immediate = 0;
    std::string callee = {};
    SourceLocation source = {};
};
struct Terminator {
    enum class Kind { return_value, branch } kind = Kind::return_value;
    std::uint32_t value = 0, yes = 0, no = 0;
};
struct Block { std::vector<Instruction> instructions; Terminator end; };
struct Function { std::string name; SourceLocation source = {}; std::vector<Block> blocks; };
struct Module {
    std::string identity;
    std::vector<std::string> imports; // Trusted C functions with i32(i32) ABI.
    std::vector<Function> functions;
};
void validate(const Module& module);
Module architecture_proof(std::int32_t answer = 42);
}
