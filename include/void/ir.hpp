#pragma once
#include "void/source.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace vd::ir {
// Values are block-local instruction indexes; typed slots carry mutable state
// across control-flow edges. The defaults retain the original proof ABI.
enum class Type { i32, boolean, none };
const char* type_name(Type type);
enum class Op { argument, constant, add, multiply, less, call, subtract, divide,
    remainder, equal, not_equal, less_equal, greater, greater_equal, logical_not, load, store };
struct Instruction {
    Op op;
    std::uint32_t a = 0, b = 0;
    std::int32_t immediate = 0;
    std::string callee = {};
    SourceLocation source = {};
    Type type = Type::i32; // Literal type; other result types derive from operands/signatures.
    std::vector<std::uint32_t> arguments = {};
    std::string source_excerpt = {}; // Snapshot of the source line and caret.
};
struct Terminator {
    enum class Kind { return_value, branch, jump, return_void, unreachable } kind = Kind::return_value;
    std::uint32_t value = 0, yes = 0, no = 0;
};
struct Block { std::vector<Instruction> instructions; Terminator end; };
struct Function {
    std::string name; SourceLocation source = {}; std::vector<Block> blocks;
    std::vector<Type> parameters = {Type::i32};
    Type result = Type::i32;
    std::vector<Type> locals = {};
    std::string source_name = {}, module_identity = {}; // Empty for legacy hand-built proofs.
};
struct Module {
    std::string identity;
    std::vector<std::string> imports; // Trusted C functions with i32(i32) ABI.
    std::vector<Function> functions;
    std::string entry_symbol = "entry";
    std::string source_path = {}; // Physical debug source; not part of stable identity.
    std::string constant_contract = {}; // Exact conservative restart token for source const changes.
};
void validate(const Module& module);
Module architecture_proof(std::int32_t answer = 42);
}
