#pragma once

#include "lexer.hpp"
#include "void/c_primitive.hpp"
#include "void/numeric.hpp"
#include "void/reference_capabilities.hpp"
#include <functional>
#include <list>
#include <map>
#include <type_traits>

namespace vd::frontend {
using Type = ir::Type;
inline const Type error_type = Type::invalid; // AST-only; the lowering gate excludes it.
struct Expr;
struct Statement;
struct Function;
struct Unit;
struct CompileEvaluation;
struct EditorProbe {
    std::string module;
    std::size_t offset;
    EditorAssistance result;
    bool operator_context = false;
    std::map<std::string,std::set<std::string>> candidate_targets;
    std::map<std::string,unsigned> candidate_occurrences;
    unsigned operator_occurrences = 0;
    std::string prefix;
    // A receiver needing effects is completed only after the ordinary compiler
    // stabilizes reference profiles. The unfinished activation remains opaque.
    Expr* receiver = nullptr;
    Function* function = nullptr;
    Unit* owner = nullptr;
    bool finalizing = false, readonly_storage = false, receiver_addressable = true;
};
struct EditorProbeFound {};
struct EditorReceiverFound {
    Statement* statement;
    std::unique_ptr<Expr> receiver;
    Token site;
    bool readonly_storage, addressable;
};

struct TypeUse { Token token; Type type; std::optional<Token> qualifier; };
struct Pattern {
    Pattern() = default;
    // vectors of owning nodes can appear copyable to library traits even though
    // instantiating their copy bodies fails. Make relocation move-only explicitly.
    Pattern(const Pattern&) = delete;
    Pattern& operator=(const Pattern&) = delete;
    Pattern(Pattern&&) = default;
    Pattern& operator=(Pattern&&) = default;
    enum class Kind { wildcard, capture, value, range, constructor, list, dictionary } kind=Kind::wildcard;
    Token token{TokenKind::name,"",0};std::string spelling;
    Token variant_token{TokenKind::name,"",0}; // Written final component, including whitespace around dots.
    std::unique_ptr<TypeUse> enum_type_use;
    std::vector<Pattern> children;std::vector<std::unique_ptr<Expr>> values;std::vector<Token> fields;
    std::vector<unsigned> indexes;
    bool constant_binding=false;
    std::vector<Type> type_arguments;
    Type type=Type::none,narrowed=Type::none;unsigned slot=0,tag=0;bool rest=false,exclusive=false,nullable=false,annotated_type=false,editor_incomplete_arm=false,derived_case=false,invocation=false;
};
struct Expr {
    bool event=false, asynchronous_call=false;
    std::string event_owner,native_symbol;
    Type environment=Type::none;
    Type constructed_type=Type::none, initializer_result=Type::none;
    enum class Kind { global, spawned, task_cancel, awaited, lambda, capture_reference, function_reference, function_call, function_null, test, match, choice, has, sum, propagate, literal, name, call, unary, binary, conversion, field, construction, method_call, base_initialization, array_literal, array_index, array_default, array_slice, dictionary_literal, return_pack, editor_hole } kind;
    Function* compile_target = nullptr; // Selected ordinary declaration; never native execution.
    Token token;
    Token qualifier_token{TokenKind::name, "", 0};
    std::unique_ptr<TypeUse> enum_type_use;
    std::string op, qualifier, target, source_target;
    std::vector<Pattern> patterns;
    unsigned constant_pattern_frame=0;
    std::vector<Type> type_arguments;
    std::uint64_t value = 0;
    ir::ConstantValue aggregate_value;
    std::vector<Token> names;
    std::vector<std::unique_ptr<Expr>> omitted;
    std::string initializer, display_adapter;
    std::vector<unsigned> field_indexes, argument_indexes;
    std::optional<unsigned> c_callback_argument;
    std::string literal_text;
    std::optional<std::string> format_specification;
    std::vector<std::unique_ptr<Expr>> parts;
    Type type = Type::none;
    unsigned slot = 0;
    bool receiver_addressable = false, receiver_readonly = false, receiver_global = false, implicit = false;
    std::string virtual_slot;
    bool super_call = false, interface_call = false, property = false, receiver_writes = false, selected_receiver = false, setter_mutates = false;
    std::string setter;
    Expr(Kind k, Token t) : kind(k), token(std::move(t)) {}
    Expr(const Expr&) = delete;
    Expr& operator=(const Expr&) = delete;
    Expr(Expr&&) = default;
    Expr& operator=(Expr&&) = default;
};
// Owned, trivia-free specification syntax; runtime-producing helper bodies remain
// ordinary reloadable code. Written expressions and exact selected targets stay distinct.
std::string format_specification_syntax(const Expr&);

struct Statement {
    std::size_t source_end=0;
    enum class Kind { scoped, deferred, match, variable, assignment, expression, returned, conditional, loop, broken, continued, no_op, destructure } kind;
    Token token;
    std::optional<Pattern> pattern;
    std::string name, op, constant_syntax, compound_target;
    Token compound_token{TokenKind::symbol,"",0};
    std::optional<Type> annotation;
    std::unique_ptr<Expr> expression;
    std::unique_ptr<Expr> index;
    std::unique_ptr<Expr> selection;
    std::unique_ptr<Expr> range_end;
    std::unique_ptr<Expr> compound_conversion;
    unsigned compound_slot=0;
    Token binder{TokenKind::name,"",0}, index_binder{TokenKind::name,"",0};
    unsigned index_slot=0;
    bool reverse=false, exclusive=false, exhaustive=false, asynchronous_iteration=false;
    std::vector<Token> fields;
    std::vector<bool> implicit_fields;
    std::vector<unsigned> field_indexes;
    struct Binding { Token token; unsigned slot = 0; };
    std::vector<Binding> bindings;
    Type assigned_type = Type::none;
    std::vector<Statement> body, otherwise;
    unsigned slot = 0, flow = 0;
    bool readonly = false, invalid = false, using_field = false, event = false, task_group = false, editor_checked = false;
    std::string visibility = "public";
    std::string cleanup_target; // Selected nonvirtual destructor for the compiler-owned with handler.
    Statement(Kind k, Token t) : kind(k), token(std::move(t)) {}
};
struct Parameter {
    Token token; Type type; unsigned slot = 0;
    std::unique_ptr<Expr> default_expression = {};
    std::optional<ir::ConstantValue> default_value = {};
    std::string default_syntax = {};
    std::size_t default_begin = 0, default_end = 0;
};
struct ConstructionContext {
    std::set<unsigned> parameters;
    std::map<unsigned,unsigned> aliases;
    std::map<const Expr*,std::set<unsigned>> calls;
};
struct GenericParameter { Token token; std::optional<Type> constraint; std::vector<Token> constraint_syntax; };
struct ReturnComponent { Token site; std::optional<Token> name; Type type; };
struct Function {
    struct Capture {unsigned outer_slot;Type cell;Token token;bool snapshot=false;};
    bool collection_adapter=false, positional_only=false;
    bool json_adapter=false, enum_adapter=false;
    bool compile_time=false, compile_checked=false, compile_failed=false, analyzing=false;
    Unit* compile_owner=nullptr;
    bool global_initializer=false;
    std::optional<Parameter> static_owner; // Type context only; never an ABI receiver.
    bool anonymous=false, deferred=false, bound_receiver=false, asynchronous=false, display_adapter=false, display_has_string=false, display_has_format=false;
    Type task=Type::none;
    unsigned binding_ordinal=0;
    Type cleanup_type=Type::none;
    std::list<Function> lambdas;
    std::vector<Capture> captures;
    std::map<unsigned,Type> captured_locals;
    std::map<unsigned,unsigned> capture_inputs;
    std::vector<GenericParameter> generics;
    std::vector<Token> syntax;
    std::vector<TypeUse> type_uses;
    std::map<std::string,Type> substitutions;
    std::map<std::string,std::string> substitution_sources;
    std::string generic_origin, concrete_name, concrete_method_name, display_name;
    std::vector<Type> generic_arguments;
    std::vector<RelatedLocation> instantiation_context;
    unsigned specialization_depth=0;
    std::string native_symbol;
    std::optional<unsigned> c_primitive;
    bool analyzed=false, prepared=false, defaults_prepared=false, initializer_prepared=false;
    std::map<std::set<unsigned>,ConstructionContext> construction_contexts;
    std::string construction_contract;
    Token token;
    bool file_private = false, parse_failed = false;
    std::string symbol, visibility = "public";
    std::string lexical_owner, lexical_parent;
    Type operator_owner=Type::none;
    std::string operator_glyph, operator_pattern;
    std::vector<Parameter> parameters;
    std::optional<Parameter> receiver;
    bool receiver_mutates = false, initializer = false, base_propagates = false;
    unsigned accessor = 0; // 1 getter, 2 setter; bodies use the ordinary method checker.
    bool virtual_method = false, overriding = false, interface_requirement = false;
    std::string virtual_slot;
    std::unique_ptr<Expr> base_prelude;
    std::string body_syntax, initialization_contract, capability_contract;
    ir::ReferenceEffects reference_effects, virtual_effects;
    bool virtual_mutates=false;
    std::vector<std::unique_ptr<Expr>> initialization_defaults;
    ir::ConstantValue initialization_value;
    Type result = Type::none;
    std::vector<ReturnComponent> returns;
    std::vector<Statement> body;
    struct Binding { Token token; unsigned slot; bool readonly; };
    std::vector<Binding> editor_bindings;
    std::size_t source_end = 0;
    std::vector<Type> locals;
};
// Value construction contexts enforce source quarantine using ordinary value
// copies. Only managed private places need a distinct IR passing convention.
std::set<unsigned> managed_construction_parameters(const Function&, const std::set<unsigned>&);
std::string random_public_name(const Function&);
bool random_numeric_function(const Function&);
Type editor_pattern_type(Type);
std::string editor_return_type(const Function&, bool symbolic = false);
std::string editor_signature(const Function&);

struct Global {
    Token token;
    std::string identity, visibility, syntax;
    Type static_owner=Type::none;
    bool readonly=false;
    Function* initializer = nullptr;
    Type type = Type::none, cell = Type::none;
    unsigned state = 0;
};
struct Constant {
    Statement declaration;
    bool file_private = false;
    std::string visibility = "public";
    unsigned state = 0;
    Type type = Type::none;
    std::uint64_t value = 0;
    ir::ConstantValue aggregate_value;
};
struct GenericApplication {Token token;std::string name;std::shared_ptr<ir::ValueType> definition;std::vector<Type> arguments;Type resolved=Type::none;};
struct ArraySyntax {
    Token token;
    Type element;
    std::shared_ptr<ir::ValueType> definition;
    std::unique_ptr<Expr> extent;
    Type resolved = Type::none;
    unsigned state = 0;
    bool template_syntax=false;
    std::optional<Type> key={};
};
struct FunctionSyntax {Token token;std::vector<Type> parameters;Type result;std::shared_ptr<ir::ValueType> definition;Type resolved=Type::none;};
struct ResultSyntax { Token token; Type success, failure; std::shared_ptr<ir::ValueType> definition; Type resolved=Type::none; };
struct Unit;
struct Structure {
    std::size_t source_end=0;
    std::vector<Token> case_tokens;
    std::vector<std::unique_ptr<Expr>> numeric_expressions;
    unsigned numeric_state=0;
    Structure() = default;
    // Fields own noncopyable AST nodes. Container copy traits do not inspect
    // vector<Statement>'s copy body; never let reallocation select that path.
    Structure(const Structure&) = delete;
    Structure& operator=(const Structure&) = delete;
    Structure(Structure&&) = default;
    Structure& operator=(Structure&&) = default;
    Token token;
    std::vector<GenericParameter> generics;
    std::vector<Token> syntax;
    std::vector<TypeUse> type_uses;
    std::vector<Type> type_arguments;
    Structure* generic_origin=nullptr;
    bool hierarchy_checked=false;
    unsigned specialization_depth=0;
    std::shared_ptr<ir::ValueType> definition;
    std::string visibility = "public";
    std::vector<Statement> fields, static_fields;
    bool statics_registered=false;
    std::optional<Type> base_type;
    Token base_token{TokenKind::name,"",0};
    std::vector<std::pair<Unit*,Statement*>> effective_fields;
    ir::ConstantValue defaults;
    unsigned layout_state = 0, default_state = 0, layout_depth = 1;
    std::size_t expanded_fields = 0;
    Function* initializer = nullptr;
    std::list<Function> operators, requirements;
};
static_assert(!std::is_copy_constructible_v<Pattern> && !std::is_copy_assignable_v<Pattern>);
static_assert(std::is_move_constructible_v<Pattern> && std::is_move_assignable_v<Pattern>);
static_assert(std::is_same_v<decltype(std::move_if_noexcept(std::declval<Pattern&>())),Pattern&&>);
static_assert(!std::is_copy_constructible_v<Expr> && !std::is_copy_assignable_v<Expr>);
static_assert(std::is_move_constructible_v<Expr> && std::is_move_assignable_v<Expr>);
static_assert(std::is_same_v<decltype(std::move_if_noexcept(std::declval<Expr&>())),Expr&&>);
static_assert(std::is_move_constructible_v<Statement> && std::is_move_assignable_v<Statement>);
static_assert(!std::is_copy_constructible_v<Function> && !std::is_copy_assignable_v<Function>);
static_assert(std::is_move_constructible_v<Function> && std::is_move_assignable_v<Function>);
static_assert(!std::is_copy_constructible_v<Statement> && !std::is_copy_assignable_v<Statement>);
static_assert(!std::is_copy_constructible_v<Structure> && !std::is_copy_assignable_v<Structure>);
static_assert(std::is_move_constructible_v<Structure> && std::is_move_assignable_v<Structure>);
Documentation source_documentation(const SourceFile&, std::size_t offset);
Documentation numeric_documentation(const numeric::Member&);
void validate_documentation(const SourceFile&, const Function&, Diagnostics&);
bool standard_module(std::string_view);
bool same_library(std::string_view, std::string_view);
bool source_visible(std::string_view visibility, std::string_view owner, std::string_view scope);
struct Import {
    Token path, alias;
    std::string target;
    bool explicit_alias = false;
};
std::string import_alias(const Import&);
struct Unit {
    CPrimitiveAdmission::Owner c_primitives;
    std::string c_provider;
    BuildConditions conditions=native_build_conditions();
    std::vector<ConditionalRegion> conditional_regions;
    EditorProbe* editor=nullptr;
    std::function<void(Type,Type,const SourceFile&,const Token&)> prepare_relationships;
    SourceFile source;
    std::string identity;
    std::vector<Import> imports;
    std::list<Function> functions, generic_functions;
    std::vector<Constant> constants;
    std::list<Global> globals;
    std::map<std::string, Global*> global_names, static_fields;
    std::map<std::string, Function*> static_methods;
    std::function<void(Global&)> resolve_global;
    std::list<Structure> structures, generic_structures;
    std::list<FunctionSyntax> function_types;
    std::list<ResultSyntax> results;
    std::list<ArraySyntax> arrays;
    std::list<GenericApplication> applications;
    std::vector<TypeUse> type_uses;
    std::map<std::string,Function*> specialized_functions, display_adapters, json_adapters, enum_equalities, enum_metadata;
    std::map<std::string,Structure*> specialized_structures;
    std::map<std::string, Structure*> structure_names;
    std::map<std::string, Constant*> constant_names;
    std::map<std::string, Function*> declarations, methods;
    std::map<std::string, std::vector<Unit*>> qualifiers;
    std::set<std::string> explicit_qualifiers;
    std::set<std::string> failed_qualifiers, invalid_declarations;
    std::vector<Type> synthetic_types;
    std::map<std::string,AnalyzedDeclaration> builtins;
    std::function<Type(Type,Type)> make_result;
    std::function<Type(Type)> make_option;
    std::function<Type(Type)> make_nullable;
    std::map<std::string,Type> nullable_types; // Resolved annotations remain available to immutable tooling.
    std::function<Type(Type,const Token&)> resolve_callable;
    std::function<Type(const std::vector<Type>&,Type)> make_callable;
    std::function<Type(Type)> make_task;
    std::function<Type(const std::string&,Type,bool)> make_capture_record;
    std::function<Type()> make_cleanup;
    std::function<Type(Type,const Token&)> resolve_result;
    std::function<Type(Type,const Token&)> resolve_array;
    std::function<Type(Type,Type)> make_dictionary;
    std::function<Type(Type,const Token&)> resolve_application;
    std::function<Function&(Unit&,Unit&,Function&,const std::vector<Type>&,const Token&,unsigned,Type)> specialize_function;
    std::function<Structure&(Unit&,Unit&,Structure&,const std::vector<Type>&,const Token&)> specialize_structure;
    bool application_body = false;
    Type async_application_task = Type::none;
    bool failed_import = false;
    bool parse_failed = false;
    std::function<void(Structure&)> resolve_numeric;
    CompileEvaluation* compile_evaluation=nullptr; // Validation-scoped, cleared before publication.
    std::shared_ptr<ir::Module> member_catalog;
    Unit(SourceFile s, std::string id) : source(std::move(s)), identity(std::move(id)) {}
};
inline std::string constant_pattern_identity(const std::string& module,const Token& token) {
    return module+"::$constant_pattern::"+std::to_string(token.offset);
}
bool application_main(const Function&);
const Function* application_entry(const Unit&);

// Flow sets describe possible exits from a statement, independently of LLVM.
inline constexpr unsigned next_flow = 1, return_flow = 2, break_flow = 4, continue_flow = 8;
inline bool always(const Expr* e) { return !e || (e->kind == Expr::Kind::literal && e->type == Type::boolean && e->value == 1); }
ir::Op operation(const std::string&);
bool checked_initializer_default(Type);
}
