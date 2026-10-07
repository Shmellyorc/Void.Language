#ifndef VOIDC_AST_H
#define VOIDC_AST_H

#include "lexer.h"

#include <stddef.h>
#include <stdint.h>

typedef struct VcArenaBlock VcArenaBlock;

typedef struct VcArena
{
    VcArenaBlock *first;
    VcArenaBlock *current;
} VcArena;

typedef enum VcAstKind
{
    VC_AST_COMPILATION_UNIT,
    VC_AST_USING_DECLARATION,
    VC_AST_NAMESPACE_DECLARATION,
    VC_AST_TYPE_DECLARATION,
    VC_AST_ENUM_MEMBER,
    VC_AST_METHOD_DECLARATION,
    VC_AST_FIELD_DECLARATION,
    VC_AST_PROPERTY_DECLARATION,
    VC_AST_PARAMETER,
    VC_AST_ATTRIBUTE,
    VC_AST_GENERIC_CONSTRAINT,

    VC_AST_BLOCK_STATEMENT,
    VC_AST_EXPRESSION_STATEMENT,
    VC_AST_RETURN_STATEMENT,
    VC_AST_THROW_STATEMENT,
    VC_AST_TRY_STATEMENT,
    VC_AST_CATCH_CLAUSE,
    VC_AST_LOCAL_DECLARATION,
    VC_AST_IF_STATEMENT,
    VC_AST_WHILE_STATEMENT,
    VC_AST_DO_WHILE_STATEMENT,
    VC_AST_FOR_STATEMENT,
    VC_AST_FOREACH_STATEMENT,
    VC_AST_USING_STATEMENT,
    VC_AST_LOCK_STATEMENT,
    VC_AST_FIXED_STATEMENT,
    VC_AST_SWITCH_STATEMENT,
    VC_AST_SWITCH_SECTION,
    VC_AST_SWITCH_LABEL,
    VC_AST_YIELD_RETURN_STATEMENT,
    VC_AST_YIELD_BREAK_STATEMENT,
    VC_AST_BREAK_STATEMENT,
    VC_AST_CONTINUE_STATEMENT,

    VC_AST_TYPE_RECEIVER_EXPRESSION,
    VC_AST_IDENTIFIER_EXPRESSION,
    VC_AST_LITERAL_EXPRESSION,
    VC_AST_MEMBER_ACCESS_EXPRESSION,
    VC_AST_CALL_EXPRESSION,
    VC_AST_INDEX_EXPRESSION,
    VC_AST_NEW_EXPRESSION,
    VC_AST_OBJECT_INITIALIZER_MEMBER,
    VC_AST_COLLECTION_INITIALIZER_ELEMENT,
    VC_AST_DEFAULT_EXPRESSION,
    VC_AST_TYPEOF_EXPRESSION,
    VC_AST_SIZEOF_EXPRESSION,
    VC_AST_STACKALLOC_EXPRESSION,
    VC_AST_CAST_EXPRESSION,
    VC_AST_TYPE_RELATION_EXPRESSION,
    VC_AST_PROPERTY_PATTERN_MEMBER,
    VC_AST_UNARY_EXPRESSION,
    VC_AST_AWAIT_EXPRESSION,
    VC_AST_BINARY_EXPRESSION,
    VC_AST_RANGE_EXPRESSION,
    VC_AST_CONDITIONAL_EXPRESSION,
    VC_AST_SWITCH_EXPRESSION,
    VC_AST_SWITCH_EXPRESSION_ARM,
    VC_AST_ASSIGNMENT_EXPRESSION,
    VC_AST_LAMBDA_EXPRESSION,
    VC_AST_COMPILER_CLOSURE_FRAME_EXPRESSION,
    VC_AST_COMPILER_CAPTURE_EXPRESSION,
    VC_AST_PARENTHESIZED_EXPRESSION
} VcAstKind;

typedef enum VcAstTypeKind
{
    VC_AST_TYPE_CLASS,
    VC_AST_TYPE_STRUCT,
    VC_AST_TYPE_INTERFACE,
    VC_AST_TYPE_DELEGATE,
    VC_AST_TYPE_ENUM
} VcAstTypeKind;

typedef enum VcAstLiteralKind
{
    VC_AST_LITERAL_NUMBER,
    VC_AST_LITERAL_STRING,
    VC_AST_LITERAL_CHARACTER,
    VC_AST_LITERAL_TRUE,
    VC_AST_LITERAL_FALSE,
    VC_AST_LITERAL_NULL
} VcAstLiteralKind;

typedef enum VcAstModifier
{
    VC_AST_MOD_PUBLIC    = 1u << 0,
    VC_AST_MOD_PRIVATE   = 1u << 1,
    VC_AST_MOD_PROTECTED = 1u << 2,
    VC_AST_MOD_INTERNAL  = 1u << 3,
    VC_AST_MOD_STATIC    = 1u << 4,
    VC_AST_MOD_SEALED    = 1u << 5,
    VC_AST_MOD_ABSTRACT  = 1u << 6,
    VC_AST_MOD_READONLY  = 1u << 7,
    VC_AST_MOD_UNSAFE    = 1u << 8,
    VC_AST_MOD_EXTERN    = 1u << 9,
    VC_AST_MOD_VIRTUAL   = 1u << 10,
    VC_AST_MOD_OVERRIDE  = 1u << 11,
    VC_AST_MOD_CONST     = 1u << 12,
    VC_AST_MOD_ASYNC     = 1u << 13,
    VC_AST_MOD_REF       = 1u << 14
} VcAstModifier;

typedef struct VcAstNode VcAstNode;
typedef struct VcAstTypeRef VcAstTypeRef;

typedef struct VcAstNodeList
{
    VcAstNode **items;
    size_t count;
    size_t capacity;
} VcAstNodeList;

typedef struct VcAstTypeList
{
    VcAstTypeRef **items;
    size_t count;
    size_t capacity;
} VcAstTypeList;

typedef struct VcAstStringList
{
    char **items;
    size_t count;
    size_t capacity;
} VcAstStringList;

struct VcAstTypeRef
{
    VcSourceLocation location;
    VcSourceSpan span;
    char *name;
    VcAstTypeList generic_arguments;
    bool is_function_pointer;
    size_t pointer_depth;
    size_t array_rank;
    size_t rectangular_rank;
    bool nullable;
    bool generic_constraint_parameter;
    /* Internal canonical argument provenance: the global namespace must not
       be rebound in a template's namespace after substitution. */
    bool is_global_qualified;
    const char *generic_parameter_origin;
};

struct VcAstNode
{
    VcAstKind kind;
    VcSourceLocation location;
    VcSourceSpan span;
    VcAstNodeList attributes;
    char *argument_name;

    /* Explicit type syntax; never a runtime value. Named dotted expressions
       remain unresolved syntax until value-first semantic lookup. */
    VcAstTypeRef *receiver_type;

    union
    {
        struct
        {
            VcAstNodeList declarations;
        } compilation_unit;

        struct
        {
            char *name;
            char *alias;
        } using_declaration;

        struct
        {
            char *name;
            bool file_scoped;
            VcAstNodeList declarations;
        } namespace_declaration;

        struct
        {
            VcAstTypeKind type_kind;
            uint32_t modifiers;
            char *name;
            VcAstStringList generic_parameters;
            VcAstNodeList generic_constraints;
            char *original_generic_name;
            VcAstTypeList generic_arguments;
            VcAstTypeList base_types;
            VcAstNodeList members;
        } type_declaration;

        struct
        {
            char *name;
            VcAstNode *value;
        } enum_member;

        struct
        {
            uint32_t modifiers;
            VcAstTypeRef *return_type;
            bool returns_ref;
            bool returns_ref_readonly;
            char *name;
            char *original_generic_name;
            const VcAstNode *runtime_source_method_node;
            bool runtime_hide_frame;
            VcAstStringList generic_parameters;
            VcAstNodeList generic_constraints;
            VcAstNodeList parameters;
            VcAstNodeList constructor_initializer_arguments;
            VcAstNode *body;
            bool is_constructor;
            bool has_base_constructor_initializer;
            bool has_this_constructor_initializer;
            bool is_operator;
            VcTokenKind operator_kind;
        } method_declaration;

        struct
        {
            uint32_t modifiers;
            VcAstTypeRef *type;
            char *name;
            VcAstNode *initializer;
            VcAstNode *event_add_body;
            VcAstNode *event_remove_body;
            bool has_event_add_accessor;
            bool has_event_remove_accessor;
            bool is_event;
            bool is_ref;
            bool ref_readonly;
        } field_declaration;

        struct
        {
            uint32_t modifiers;
            VcAstTypeRef *type;
            char *name;
            bool returns_ref;
            bool returns_ref_readonly;
            bool has_getter;
            bool has_setter;
            uint32_t getter_modifiers;
            uint32_t setter_modifiers;
            bool getter_auto;
            bool setter_auto;
            bool getter_expression;
            VcAstNode *getter_body;
            VcAstNode *setter_body;
            VcAstNode *initializer;
        } property_declaration;

        struct
        {
            VcTokenKind modifier;
            bool scoped;
            VcAstTypeRef *type;
            char *name;
            VcAstNode *default_value;
            bool extension_receiver;
        } parameter;

        struct
        {
            char *name;
            VcAstNodeList arguments;
        } attribute;

        struct
        {
            char *parameter;
            VcAstTypeRef *argument_type;
            VcAstTypeList type_constraints;
            bool requires_reference_type;
            bool requires_value_type;
            bool requires_unmanaged_type;
            bool requires_constructor;
        } generic_constraint;

        struct
        {
            VcAstNodeList statements;
        } block_statement;

        struct
        {
            VcAstNode *expression;
        } expression_statement;

        struct
        {
            VcAstNode *expression;
            bool is_ref;
        } return_statement;

        struct
        {
            VcAstNode *expression;
        } throw_statement;

        struct
        {
            VcAstNode *try_block;
            VcAstNodeList catches;
            VcAstNode *finally_block;
        } try_statement;

        struct
        {
            VcAstTypeRef *type;
            char *name;
            VcAstNode *body;
            bool catch_all;
        } catch_clause;

        struct
        {
            VcAstTypeRef *type;
            char *name;
            VcAstNode *initializer;
            bool is_var;
            bool is_ref;
            bool ref_readonly;
            bool scoped;
            bool is_using_resource;
            bool is_await_using_resource;
        } local_declaration;

        struct
        {
            VcAstNode *condition;
            VcAstNode *then_statement;
            VcAstNode *else_statement;
        } if_statement;

        struct
        {
            VcAstNode *condition;
            VcAstNode *body;
        } while_statement;

        struct
        {
            VcAstNode *initializer;
            VcAstNode *condition;
            VcAstNode *increment;
            VcAstNode *body;
        } for_statement;

        struct
        {
            VcAstTypeRef *type;
            char *name;
            bool is_var;
            bool is_await;
            bool is_ref;
            bool ref_readonly;
            VcAstNode *collection;
            VcAstNode *body;
            VcAstNode *move_next_await_protocol;
            VcAstNode *dispose_await_protocol;
        } foreach_statement;

        struct
        {
            VcAstNode *declaration;
            VcAstNode *expression;
            VcAstNode *body;
            VcAstNode *dispose_await_protocol;
            bool is_declaration;
            bool is_await;
        } using_statement;

        struct
        {
            VcAstNode *expression;
            VcAstNode *body;
        } lock_statement;

        struct
        {
            VcAstTypeRef *type;
            char *name;
            VcAstNode *initializer;
            VcAstNode *body;
        } fixed_statement;

        struct
        {
            VcAstNode *expression;
            VcAstNodeList sections;
        } switch_statement;

        struct
        {
            VcAstNodeList labels;
            VcAstNodeList statements;
        } switch_section;

        struct
        {
            bool is_default;
            bool is_pattern;
            VcAstNode *value;
            VcAstNode *pattern;
            VcAstNode *guard;
        } switch_label;

        struct
        {
            VcAstNode *expression;
        } yield_statement;

        struct
        {
            char *name;
        } identifier_expression;

        struct
        {
            VcAstLiteralKind literal_kind;
            char *text;
        } literal_expression;

        struct
        {
            VcAstNode *target;
            char *member;
            char *constrained_static_parameter;
            /* Target-typed generic method-group inference reuses the ordinary
               monomorphizer by recording requested arguments on this member. */
            VcAstTypeList generic_arguments;
            VcAstTypeList specialized_generic_arguments;
            char *original_generic_name;
            bool null_conditional;
            bool null_conditional_direct;
        } member_access_expression;

        struct
        {
            VcAstNode *callee;
            VcAstTypeList generic_arguments;
            VcAstTypeList specialized_generic_arguments;
            VcAstNodeList arguments;
            char *original_generic_name;
            bool had_explicit_function_pointer_type_argument;
        } call_expression;

        struct
        {
            VcAstNode *target;
            VcAstNode *index;
            VcAstNodeList indices;
            bool null_conditional;
            bool null_conditional_direct;
        } index_expression;

        struct
        {
            VcAstTypeRef *type;
            bool target_typed;
            bool is_array;
            bool has_initializer;
            size_t implicit_array_rank;
            VcAstNode *array_length;
            VcAstNodeList array_lengths;
            VcAstNodeList arguments;
            VcAstNodeList initializers;
        } new_expression;

        struct
        {
            char *member;
            VcAstNode *value;
        } object_initializer_member;

        struct
        {
            VcAstNodeList arguments;
        } collection_initializer_element;

        struct
        {
            VcAstTypeRef *type;
        } default_expression;

        struct
        {
            VcAstTypeRef *type;
        } typeof_expression;

        struct
        {
            VcAstTypeRef *type;
        } sizeof_expression;

        struct
        {
            VcAstTypeRef *type;
            VcAstNode *count;
        } stackalloc_expression;

        struct
        {
            VcAstTypeRef *type;
            VcAstNode *expression;
        } cast_expression;

        struct
        {
            VcTokenKind operator_kind;
            VcAstNode *expression;
            VcAstTypeRef *type;
            const char *pattern_name;
            VcAstNode *pattern_constant;
            VcTokenKind pattern_operator_kind;
            VcTokenKind pattern_logical_kind;
            VcAstNode *pattern_left;
            VcAstNode *pattern_right;
            VcAstNodeList pattern_properties;
            bool pattern_property;
        } type_relation_expression;

        struct
        {
            const char *name;
            VcAstNode *pattern;
        } property_pattern_member;

        struct
        {
            VcTokenKind operator_kind;
            VcAstNode *operand;
            VcAstTypeRef *inline_out_type;
            VcSourceSpan inline_out_inference_span;
            const char *constrained_static_parameter;
            bool inline_out_inferred;
            bool inline_out_discard;
            bool postfix;
        } unary_expression;

        struct
        {
            VcAstNode *operand;
            VcAstNode *awaiter_inference_call;
        } await_expression;

        struct
        {
            VcTokenKind operator_kind;
            bool is_ref;
            VcAstNode *left;
            VcAstNode *right;
            const char *left_constrained_static_parameter;
            const char *right_constrained_static_parameter;
        } binary_expression;

        struct
        {
            VcAstNode *start;
            VcAstNode *end;
        } range_expression;

        struct
        {
            VcAstNode *condition;
            VcAstNode *when_true;
            VcAstNode *when_false;
        } conditional_expression;

        struct
        {
            VcAstNode *expression;
            VcAstNodeList arms;
        } switch_expression;

        struct
        {
            bool is_discard;
            VcAstNode *pattern;
            VcAstNode *guard;
            VcAstNode *result;
        } switch_expression_arm;

        struct
        {
            VcTokenKind operator_kind;
            bool is_ref;
            VcAstNode *left;
            VcAstNode *right;
            const char *left_constrained_static_parameter;
            const char *right_constrained_static_parameter;
        } assignment_expression;

        struct
        {
            VcAstStringList parameters;
            VcAstNode *body;
            bool expression_body;
            bool is_async;
            bool lowered_async_forwarder;
        } lambda_expression;

        struct
        {
            VcAstNodeList captures;
        } compiler_closure_frame_expression;

        struct
        {
            VcAstNode *target;
            VcAstNode *source_expression;
            const VcAstNode *source_lambda_node;
            const VcAstNode *declaration_node;
            const VcAstNode *owner_method_node;
            char *name;
            VcAstTypeRef *type;
            bool captures_this;
        } compiler_capture_expression;

        struct
        {
            VcAstNode *expression;
        } parenthesized_expression;
    } as;
};

typedef struct VcAstTree
{
    VcArena arena;
    VcAstNode *root;
} VcAstTree;

typedef enum VcAstRectangularInitializerIssueKind
{
    VC_AST_RECTANGULAR_INITIALIZER_ISSUE_NONE = 0,
    VC_AST_RECTANGULAR_INITIALIZER_ISSUE_INCONSISTENT_LENGTH,
    VC_AST_RECTANGULAR_INITIALIZER_ISSUE_MISSING_GROUP,
    VC_AST_RECTANGULAR_INITIALIZER_ISSUE_EXTRA_GROUP
} VcAstRectangularInitializerIssueKind;

typedef struct VcAstRectangularInitializerIssue
{
    VcAstRectangularInitializerIssueKind kind;
    VcSourceLocation location;
    size_t dimension;
} VcAstRectangularInitializerIssue;

void vc_ast_tree_init(VcAstTree *tree);
void vc_ast_tree_destroy(VcAstTree *tree);

void *vc_ast_alloc(VcAstTree *tree, size_t size);
char *vc_ast_copy_text(VcAstTree *tree, const char *text, size_t length);
VcAstNode *vc_ast_new_node(VcAstTree *tree, VcAstKind kind, VcSourceLocation location);
VcAstTypeRef *vc_ast_new_type(VcAstTree *tree, VcSourceLocation location);
bool vc_ast_tree_contains(const VcAstTree *tree, const void *pointer);

bool vc_ast_node_list_push(VcAstTree *tree, VcAstNodeList *list, VcAstNode *node);
bool vc_ast_type_list_push(VcAstTree *tree, VcAstTypeList *list, VcAstTypeRef *type);
bool vc_ast_string_list_push(VcAstTree *tree, VcAstStringList *list, char *value);

bool vc_ast_rectangular_initializer_shape(
    const VcAstNodeList *items,
    VcSourceLocation location,
    size_t rank,
    size_t *lengths,
    VcAstRectangularInitializerIssue *issue);

void vc_ast_dump(const VcAstTree *tree, const VcSource *source);

bool vc_ast_character_scalar(const char *text, uint32_t *value);

bool vc_ast_receiver_name(const VcAstNode *node, char *name, size_t size);

#endif
