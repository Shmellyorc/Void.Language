#pragma once
#include "void/source.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <map>
#include <set>
namespace vd { class ValidatedAnalysis; }
#include <string>
#include <vector>
namespace vd::ir {
// Values are block-local instruction indexes; typed slots carry mutable state
// across control-flow edges. The defaults retain the original proof ABI.
struct ReferenceEffects;
struct ValueType;
struct TypeArena;
using ReferencePath = std::vector<unsigned>;
using ReferencePaths = std::set<ReferencePath>;
struct Type {
    enum Kind { i32, boolean, none, i8, i16, i64, u8, u16, u32, u64, f32, f64, aggregate, unresolved, invalid, managed_ref, null_value, interface_ref, prepared_receiver, string, fixed_array, growable_array, dictionary };
    Kind kind = i32;
    bool nullable = false, readonly = false;
    std::string preparation = {}; // Private prepared requirement identity; never user storage.
    bool construction = false; // Private place capability; never a managed reference value.
    // Compile-time authority only. Paths cross inline fields and stop at the
    // first managed reference. They never affect nominal identity or layout.
    ReferencePaths readonly_references;
    std::shared_ptr<const ValueType> definition;
    Type(Kind k = i32) : kind(k) {}
    explicit Type(std::shared_ptr<const ValueType> d, Kind k = aggregate);
    friend bool operator==(const Type&, const Type&);
    friend bool operator<(const Type& a, const Type& b) {
        if (a.kind != b.kind) return a.kind < b.kind;
        if (a.nullable!=b.nullable) return a.nullable<b.nullable;
        if (a.construction!=b.construction) return a.construction<b.construction;
        return key(a) < key(b);
    }
    static std::string key(const Type&);
};
// Descriptor edges never own another descriptor/table. A Type obtained from an
// edge reacquires the immutable table owner; public/query Types remain owning.
struct FieldType {
    Type::Kind kind = Type::i32;
    bool nullable = false, readonly = false;
    ReferencePaths readonly_references = {};
    std::weak_ptr<const ValueType> target;
    FieldType(Type::Kind k) : kind(k) {}
    FieldType(Type t);
    operator Type() const;
};
struct ValueField {
    std::string name; FieldType type; std::string owner = {};
    bool using_field = false, event = false;
    std::string visibility = "public";
    bool readonly = false;
    std::size_t source_offset = 0;
};
struct SumCase { std::string name; unsigned begin=0, count=0; };
struct ValueType {
    std::string identity, name;
    std::vector<ValueField> fields;
    std::size_t source_offset = 0;
    bool custom_equality = false;
    std::size_t custom_equality_offset = 0;
    bool managed = false;
    bool interface_type = false;
    std::vector<SumCase> cases = {}; // Disjoint, zero-inactive payload fields; field zero is the tag.
    bool result_type = false;
    bool callable = false;
    std::string native_provider = {}; // Compiled catalog identity, independent of source module spelling.
    bool native_resource = false; // Binding-private managed identity; no source payload/constructor.
    std::optional<FieldType> task_result = {}; // Private inferred task handle; no public constructor.
    std::vector<FieldType> call_parameters = {};
    std::optional<FieldType> call_result = {};
    std::string module_identity = {}, visibility = "public";
    // Effective logical fields retain ancestor indexes. The native payload is
    // { complete BasePayload, own fields... }; this edge never owns the arena.
    std::weak_ptr<const ValueType> base = {};
    mutable std::weak_ptr<const ValueType> self = {};
    mutable std::weak_ptr<TypeArena> arena = {};
    // Structural arrays use one element edge, never synthetic nominal fields.
    std::optional<FieldType> element = {};
    std::optional<std::uint64_t> extent = {};
    std::string display_name = {}; // Optional source-facing name; identity remains canonical.
};
struct TypeArena { std::vector<std::shared_ptr<const ValueType>> definitions; };
inline std::string Type::key(const Type& t) { return t.definition ? t.definition->identity : std::string{}; }
inline bool operator==(const Type& a, const Type& b) {
    return (a.kind!=Type::prepared_receiver || a.preparation==b.preparation) && a.kind == b.kind && a.nullable == b.nullable && a.construction == b.construction && (a.kind != Type::aggregate && a.kind != Type::unresolved && a.kind != Type::managed_ref && a.kind != Type::interface_ref && a.kind != Type::prepared_receiver && a.kind != Type::fixed_array && a.kind != Type::growable_array && a.kind != Type::dictionary ? true :
        a.definition && b.definition && a.definition->identity == b.definition->identity);
}
struct ConstantValue { std::uint64_t bits = 0; std::vector<ConstantValue> fields; std::string text = {};
    // A pure repeated default has one prototype (none for zero extent).
    std::optional<std::uint64_t> repeat = {};
};
// Immutable strings participate in tracing, never mutable-reference authority.
ReferencePaths reference_paths(Type type, bool include_strings = false);
ReferencePaths readonly_paths(Type type);
Type with_readonly_paths(Type type, const ReferencePaths& paths);
Type readonly_view(Type type);
Type field_type(Type owner, unsigned field);
Type with_field_type(Type owner, unsigned field, Type value);
bool capability_assignable(Type actual, Type expected);
bool subtype(Type actual, Type expected);
std::vector<unsigned> physical_field_path(Type owner, unsigned field);
std::string capability_key(Type type);
std::string reference_path_name(Type type,const ReferencePath& path);
std::string type_name(Type type);
inline constexpr unsigned array_element_path=~0u;
bool array_type(Type);
bool collection_type(Type);
Type dictionary_type(Type key,Type value);
Type dictionary_key(Type);
Type dictionary_value(Type);
std::optional<Type> dictionary_equality_owner(Type);
bool dictionary_key_supported(Type);
bool pattern_comparable(Type);
Type array_element(Type);
std::string array_identity(Type element, std::optional<std::uint64_t> extent);
bool scalar_array(Type);
bool scalar_fixed_array(Type);
// Logical scalar-byte lower bound, NOT a target layout or native storage estimate.
// Repetition contributes arithmetic, never per-element metadata or work.
struct CompactValueShape { unsigned depth = 0; std::uint64_t scalar_bytes = 0; std::size_t template_nodes = 0; };
CompactValueShape compact_value_shape(Type);
std::uint64_t checked_array_index(std::uint64_t bits, Type index, std::uint64_t extent);
enum class Op { math, random_xor, random_seed, global_cell, bitcast, native_call, async_cancel_children, async_cancel, async_group_enter, async_group_join, async_drain, async_call, await, gc_release, cleanup_call, function_reference, function_call, function_null, sum, sum_get, pattern_equal, type_test, type_cast, panic, argument, constant, add, multiply, less, call, subtract, divide,
    remainder, equal, not_equal, less_equal, greater, greater_equal, logical_not, load, store, convert, negate, aggregate, extract, insert,
    stage_begin, gc_construct, gc_allocate, gc_read, gc_write, gc_collect, gc_poll, gc_alive,
    upcast, virtual_call, base_init, stage_prefix, interface_pack, interface_prepare, interface_call, string_literal, string_concat, string_equal, string_count, string_index, string_encode, string_decode, text_case, scalar_display, scalar_format, format_unwrap, format_layout, scalar_parse, result_unwrap, memory_write, temporal_result, temporal_native, string_join, collection_enter, collection_leave, collection_append, collection_finish, os_query, console_endpoint, console_native, console_print, filesystem_native, filesystem_async, filesystem_control,
    fixed_array, array_get, array_set, fixed_snapshot, fixed_select, fixed_commit, array_count, list_push, list_remove, list_index, list_version, list_commit, list_check, array_capture, array_allocate, array_initialize, list_insert, list_clear, array_slice, dict_new, dict_get, dict_set, dict_contains, dict_remove, dict_clear, dict_count, dict_version, dict_check, dict_entry };
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
    bool array_repeat = false; // fixed_array only: pure prototype value, copied extent times.
    std::string operator_glyph = {}; // Exact source operator call proof, using ordinary call ABI.
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
    bool closure = false, bound_receiver = false;
    bool asynchronous = false;
    bool worker = false; // Private enqueue wrapper; synchronous reachable source body on a pool thread.
    Type task = Type::none; // Exact checked task shape for an async declaration.
    std::optional<unsigned> cleanup_entry;
    bool initializer = false;
    unsigned accessor = 0; // 1 getter, 2 setter; ordinary native method ABI.
    bool receiver = false, receiver_mutates = false; // First parameter aliases local slot zero.
    std::map<unsigned,unsigned> construction_origins = {}; // View slot -> owning slot/formal.
    std::string source_name = {}, module_identity = {}; // Empty for legacy hand-built proofs.
    std::string virtual_slot = {}; // Root declaration identity, shared by overrides.
    bool overriding = false;
    std::string method_name = {}, visibility = "public";
    // Private scalar owned-value proof; source/native activation is separate.
    bool fixed_ownership = false;
    std::string display_name = {}; // Debug/diagnostic spelling, independent of native identity.
    std::string lexical_owner = {}, lexical_parent = {};
    Type operator_owner = Type::none;
    std::string operator_glyph = {};
};
bool operator_arity(std::string_view glyph,std::size_t arity);
std::string operator_identity(Type owner,std::string_view glyph,const std::vector<Type>& operands);
void validate_operator_declaration(const Function&);
// Runtime globals are precise-rooted managed cells; order is eager startup order.
struct Global {
    std::string identity, module_identity, visibility;
    Type cell;
    std::string initializer, syntax;
    SourceLocation source;
};
struct Module {
    std::string identity;
    std::vector<std::string> imports; // Trusted C functions with i32(i32) ABI.
    std::vector<Function> functions;
    std::string entry_symbol = "entry";
    std::string source_path = {}; // Physical debug source; not part of stable identity.
    std::shared_ptr<const ValidatedAnalysis> analysis = {};
    std::string constant_contract = {}; // Exact canonical const declarations.
    bool application = false; // Explicit CLI application status policy; adapter retains i32(i32).
    bool async_root = false; // Explicit scheduler adapter, never implicit CLI behavior.
    std::string native_library = {};
    bool coordinated = false; // Requires a runtime owner; inferred for managed source.
    std::vector<Type> nominal_types = {}; // Complete validated generation universe.
    std::string root_identity = {};
    std::map<std::string,std::set<std::string>> source_imports = {};
    std::vector<Function> interface_requirements = {}; // Declaration headers, never executable bodies.
    struct Binding {
        Type view, interface;
        std::map<std::string,std::pair<std::vector<unsigned>,std::string>> methods;
        std::string key;
    };
    std::vector<Binding> interface_bindings = {};
    std::vector<std::string> strings = {}; // Owned unique UTF-8 byte templates.
    std::vector<Global> globals = {};
    // Non-executable generic declarations and their canonical concrete instances.
    // Access is rederived from these typed records, never a permission bit.
    struct GenericDeclaration { std::string module_identity; unsigned arity; bool nominal; };
    struct GenericInstance { std::string declaration; std::vector<Type> arguments; };
    std::map<std::string,GenericDeclaration> generic_declarations = {};
    std::map<std::string,GenericInstance> generic_instances = {};
};
std::string generic_identity(std::string_view,const std::vector<Type>&);
std::string generic_instance_identity(const Module&,const Module::GenericInstance&,const Function* = nullptr);
std::vector<Type> generic_operator_arguments(const Module&,const Function&);
inline constexpr std::size_t string_constant_limit = 16 * 1024 * 1024;
// CLI execution may emit only the closed entry graph. Embedders retain all
// source exports unless they explicitly choose this policy.
enum class Emission { all, entry };
void retain_entry(Module&);
inline constexpr std::size_t string_pool_limit = 64 * 1024 * 1024;
bool reference_value(Type);
bool async_boundary_safe(Type);
bool worker_transfer_safe(Type);
bool requires_initialization(Type);
struct MemberSelection {
    enum Status { missing, found, inaccessible, ambiguous } status = missing;
    std::vector<unsigned> path;
    std::string method;
    std::vector<std::pair<std::vector<unsigned>,std::string>> candidates;
};
bool field_accessible(const Module&,Type,unsigned,std::string_view scope,std::string_view lexical_owner = {});
MemberSelection select_member(const Module&,Type,std::string_view,std::string_view scope,std::size_t* work=nullptr,std::string_view lexical_owner = {});
bool method_accessible(const Module&,const Function&,std::string_view scope);
MemberSelection select_operator(const Module&,std::string_view,const std::vector<Type>&,std::string_view scope,std::string_view lexical_owner = {},const std::vector<Type>& generic_arguments = {});
void satisfy_constraint(const Module&,Type,Type,std::string_view scope);
Module::Binding structural_binding(const Module&,Type,Type,std::string_view scope,std::size_t* work=nullptr);
std::vector<Module::Binding> interface_universe(const Module&);
std::string binding_key(const Module::Binding&);
bool binding_requires_readonly(const Module::Binding&);
struct VirtualTarget { Type dynamic_type; std::string implementation; };
std::vector<VirtualTarget> virtual_targets(const Module&,const Function&);
std::string construction_name(const std::string& symbol, const std::vector<unsigned>& parameters);
using InstructionTypes = std::vector<std::vector<std::vector<Type>>>;
void validate(const Module& module, InstructionTypes* results = nullptr,std::map<std::string,ReferenceEffects>* verified_effects = nullptr);
Module architecture_proof(std::int32_t answer = 42);
}
