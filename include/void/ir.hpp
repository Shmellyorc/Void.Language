#pragma once
#include "void/source.hpp"
#include <cstdint>
#include <memory>
#include <optional>
namespace vd { class ValidatedAnalysis; }
#include <string>
#include <vector>
namespace vd::ir {
// Values are block-local instruction indexes; typed slots carry mutable state
// across control-flow edges. The defaults retain the original proof ABI.
struct ValueType;
struct TypeArena;
struct Type {
    enum Kind { i32, boolean, none, i8, i16, i64, u8, u16, u32, u64, f32, f64, aggregate, unresolved, invalid, managed_ref, null_value };
    Kind kind = i32;
    bool nullable = false, readonly = false;
    std::shared_ptr<const ValueType> definition;
    Type(Kind k = i32) : kind(k) {}
    explicit Type(std::shared_ptr<const ValueType> d, Kind k = aggregate);
    friend bool operator==(const Type&, const Type&);
    friend bool operator<(const Type& a, const Type& b) {
        if (a.kind != b.kind) return a.kind < b.kind;
        if (a.nullable!=b.nullable) return a.nullable<b.nullable;
        return key(a) < key(b);
    }
    static std::string key(const Type&);
};
// Descriptor edges never own another descriptor/table. A Type obtained from an
// edge reacquires the immutable table owner; public/query Types remain owning.
struct FieldType {
    Type::Kind kind = Type::i32;
    bool nullable = false, readonly = false;
    std::weak_ptr<const ValueType> target;
    FieldType(Type::Kind k) : kind(k) {}
    FieldType(Type t);
    operator Type() const;
};
struct ValueField { std::string name; FieldType type; };
struct ValueType {
    std::string identity, name;
    std::vector<ValueField> fields;
    std::size_t source_offset = 0;
    bool managed = false;
    mutable std::weak_ptr<const ValueType> self = {};
    mutable std::weak_ptr<TypeArena> arena = {};
};
struct TypeArena { std::vector<std::shared_ptr<const ValueType>> definitions; };
inline std::string Type::key(const Type& t) { return t.definition ? t.definition->identity : std::string{}; }
inline bool operator==(const Type& a, const Type& b) {
    return a.kind == b.kind && a.nullable == b.nullable && (a.kind != Type::aggregate && a.kind != Type::unresolved && a.kind != Type::managed_ref ? true :
        a.definition && b.definition && a.definition->identity == b.definition->identity);
}
struct ConstantValue { std::uint64_t bits = 0; std::vector<ConstantValue> fields; };
std::string type_name(Type type);
enum class Op { argument, constant, add, multiply, less, call, subtract, divide,
    remainder, equal, not_equal, less_equal, greater, greater_equal, logical_not, load, store, convert, negate, aggregate, extract, insert,
    gc_construct, gc_allocate, gc_read, gc_write, gc_collect, gc_poll, gc_alive };
// A validated local aggregate storage path; never a source-visible reference.
struct Place { std::uint32_t slot = 0; std::vector<unsigned> fields; };
struct Instruction {
    Op op;
    std::uint32_t a = 0, b = 0;
    std::int64_t immediate = 0;
    std::string callee = {};
    SourceLocation source = {};
    Type type = Type::i32; // Literal type; other result types derive from operands/signatures.
    std::vector<std::uint32_t> arguments = {};
    std::optional<Place> receiver = {};
    bool initialization = false; // Only compiler-validated construction invokes init.
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
    bool initializer = false;
    bool receiver = false, receiver_mutates = false; // First parameter aliases local slot zero.
    std::string source_name = {}, module_identity = {}; // Empty for legacy hand-built proofs.
};
struct Module {
    std::string identity;
    std::vector<std::string> imports; // Trusted C functions with i32(i32) ABI.
    std::vector<Function> functions;
    std::string entry_symbol = "entry";
    std::string source_path = {}; // Physical debug source; not part of stable identity.
    std::shared_ptr<const ValidatedAnalysis> analysis = {};
    std::string constant_contract = {}; // Exact canonical const declarations.
    bool coordinated = false; // Requires a runtime owner; inferred for managed source.
};
using InstructionTypes = std::vector<std::vector<std::vector<Type>>>;
void validate(const Module& module, InstructionTypes* results = nullptr);
Module architecture_proof(std::int32_t answer = 42);
}
