#include "async_lower.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VC_ASYNC_PREFIX "__voidc$async$"

typedef struct VcAsyncLocal
{
    const VcAstNode *declaration;
    VcAstTypeRef *type;
    char *name;
    bool promoted;
} VcAsyncLocal;

typedef struct VcAsyncAwait
{
    VcAstNode *operand;
    VcAstTypeRef *awaiter_type;
    VcAstTypeRef *result_type;
    const VcAstNode *result_declaration;
    VcAstNode *assignment_target;
    bool return_result;
    char *field_name;
    const VcSemanticBinding *protocol_binding;
} VcAsyncAwait;

typedef struct VcAsyncCatch
{
    const VcAstNode *clause;
    VcAstTypeRef *type;
    char *field_name;
} VcAsyncCatch;

typedef struct VcAsyncResource
{
    VcAstTypeRef *type;
    char *field_name;
} VcAsyncResource;

typedef struct VcAsyncSpill
{
    VcAstTypeRef *type;
    char *field_name;
} VcAsyncSpill;

typedef struct VcAsyncForeach
{
    const VcAstNode *statement;
    VcAstTypeRef *collection_type;
    VcAstTypeRef *item_type;
    VcAstTypeRef *enumerator_type;
    const VcSemanticBinding *move_next_await_binding;
    const VcSemanticBinding *dispose_await_binding;
    bool async_foreach;
    bool array_foreach;
    size_t array_rank;
    bool has_dispose;
} VcAsyncForeach;

typedef enum VcAsyncTerminator
{
    VC_ASYNC_GOTO,
    VC_ASYNC_BRANCH,
    VC_ASYNC_SWITCH,
    VC_ASYNC_AWAIT_START,
    VC_ASYNC_AWAIT_RESUME,
    VC_ASYNC_RETURN,
    VC_ASYNC_CATCH_DISPATCH,
    VC_ASYNC_FINISH
} VcAsyncTerminator;

typedef struct VcAsyncBlock
{
    int state;
    VcSourceLocation location;
    VcAstNodeList statements;
    VcAsyncTerminator terminator;
    VcAstNode *expression;
    int target;
    int false_target;
    const VcAstNode *switch_source;
    int *switch_targets;
    size_t switch_target_count;
    size_t await_index;
    int exception_target;
    const VcAstNode *try_source;
    int *catch_targets;
    size_t catch_target_count;
} VcAsyncBlock;

typedef struct VcAsyncContext
{
    VcAstTree *tree;
    const VcAstNode *owner;
    VcAstNode *method;
    const VcSemanticModel *semantic;
    const VcSemanticMethod *semantic_method;
    VcAsyncLocal *locals;
    size_t local_count;
    size_t local_capacity;
    VcAsyncAwait *awaits;
    size_t await_count;
    size_t await_capacity;
    VcAsyncCatch *catches;
    size_t catch_count;
    size_t catch_capacity;
    VcAsyncResource *resources;
    size_t resource_count;
    size_t resource_capacity;
    VcAsyncSpill *spills;
    size_t spill_count;
    size_t spill_capacity;
    VcAsyncForeach *foreaches;
    size_t foreach_count;
    size_t foreach_capacity;
    VcAsyncBlock *blocks;
    size_t block_count;
    size_t block_capacity;
    int finish_state;
    int return_state;
    bool has_exception_regions;
    const VcSemanticLambda *rewrite_lambda;
    char *error;
    size_t error_size;
} VcAsyncContext;

static void context_error(VcAsyncContext *context, const char *format, ...)
{
    if (context->error == NULL || context->error_size == 0 || context->error[0] != '\0')
        return;
    va_list args;
    va_start(args, format);
    vsnprintf(context->error, context->error_size, format, args);
    va_end(args);
}

static char *copy_text(VcAstTree *tree, const char *text)
{
    return vc_ast_copy_text(tree, text, strlen(text));
}

static VcAstTypeRef *named_type(VcAstTree *tree, VcSourceLocation location, const char *name)
{
    VcAstTypeRef *type = vc_ast_new_type(tree, location);
    if (type == NULL)
        return NULL;
    type->name = copy_text(tree, name);
    return type->name != NULL ? type : NULL;
}

static VcAstTypeRef *clone_type(VcAstTree *tree, const VcAstTypeRef *source)
{
    if (source == NULL)
        return NULL;
    VcAstTypeRef *copy = vc_ast_new_type(tree, source->location);
    if (copy == NULL)
        return NULL;
    copy->name = source->name;
    copy->is_function_pointer = source->is_function_pointer;
    copy->pointer_depth = source->pointer_depth;
    copy->array_rank = source->array_rank;
    copy->rectangular_rank = source->rectangular_rank;
    copy->nullable = source->nullable;
    copy->generic_constraint_parameter = source->generic_constraint_parameter;
    copy->generic_parameter_origin = source->generic_parameter_origin;
    for (size_t i = 0; i < source->generic_arguments.count; i++)
    {
        VcAstTypeRef *argument = clone_type(tree, source->generic_arguments.items[i]);
        if (argument == NULL || !vc_ast_type_list_push(tree, &copy->generic_arguments, argument))
            return NULL;
    }
    return copy;
}

static VcAstTypeRef *generic_type(VcAstTree *tree, VcSourceLocation location,
    const char *name, const VcAstTypeRef *argument)
{
    VcAstTypeRef *type = named_type(tree, location, name);
    VcAstTypeRef *argument_copy = clone_type(tree, argument);
    if (type == NULL || argument_copy == NULL ||
        !vc_ast_type_list_push(tree, &type->generic_arguments, argument_copy))
        return NULL;
    return type;
}

static VcAstNode *identifier(VcAstTree *tree, VcSourceLocation location, const char *name)
{
    VcAstNode *node = vc_ast_new_node(tree, VC_AST_IDENTIFIER_EXPRESSION, location);
    if (node == NULL)
        return NULL;
    node->as.identifier_expression.name = copy_text(tree, name);
    return node->as.identifier_expression.name != NULL ? node : NULL;
}

static VcAstNode *number_literal(VcAstTree *tree, VcSourceLocation location, int value)
{
    char text[32];
    const int written = snprintf(text, sizeof(text), "%d", value);
    if (written < 0 || (size_t)written >= sizeof(text))
        return NULL;
    VcAstNode *node = vc_ast_new_node(tree, VC_AST_LITERAL_EXPRESSION, location);
    if (node == NULL)
        return NULL;
    node->as.literal_expression.literal_kind = VC_AST_LITERAL_NUMBER;
    node->as.literal_expression.text = copy_text(tree, text);
    return node->as.literal_expression.text != NULL ? node : NULL;
}

static VcAstNode *bool_literal(VcAstTree *tree, VcSourceLocation location, bool value)
{
    VcAstNode *node = vc_ast_new_node(tree, VC_AST_LITERAL_EXPRESSION, location);
    if (node == NULL)
        return NULL;
    node->as.literal_expression.literal_kind = value ? VC_AST_LITERAL_TRUE : VC_AST_LITERAL_FALSE;
    node->as.literal_expression.text = copy_text(tree, value ? "true" : "false");
    return node->as.literal_expression.text != NULL ? node : NULL;
}

static VcAstNode *default_expression(VcAstTree *tree, VcSourceLocation location, const VcAstTypeRef *type)
{
    VcAstNode *node = vc_ast_new_node(tree, VC_AST_DEFAULT_EXPRESSION, location);
    if (node == NULL)
        return NULL;
    node->as.default_expression.type = clone_type(tree, type);
    return node->as.default_expression.type != NULL ? node : NULL;
}

static VcAstTypeRef *type_from_semantic_model(VcAstTree *tree,
    const VcSemanticModel *semantic, VcSemanticType type, VcSourceLocation location)
{
    if (tree == NULL || semantic == NULL)
        return NULL;
    if (vc_semantic_type_is_array(type))
    {
        VcAstTypeRef *element = type_from_semantic_model(tree, semantic,
            vc_semantic_array_element_type(semantic, type), location);
        if (element == NULL)
            return NULL;
        const size_t rank = vc_semantic_array_rank(semantic, type);
        if (rank == 1)
            element->array_rank++;
        else if (element->array_rank == 0 && element->rectangular_rank == 0)
            element->rectangular_rank = rank;
        else
            return NULL;
        return element;
    }
    if (vc_semantic_type_is_nullable(type))
    {
        VcAstTypeRef *underlying = type_from_semantic_model(tree, semantic,
            vc_semantic_nullable_underlying_type(semantic, type), location);
        if (underlying == NULL || underlying->nullable)
            return NULL;
        underlying->nullable = true;
        return underlying;
    }
    if (vc_semantic_type_is_pointer(type))
    {
        VcAstTypeRef *element = type_from_semantic_model(tree, semantic,
            vc_semantic_pointer_element_type(semantic, type), location);
        if (element == NULL)
            return NULL;
        element->pointer_depth++;
        return element;
    }
    const char *name = vc_semantic_type_name(semantic, type);
    if (name == NULL || name[0] == '<')
        return NULL;
    return named_type(tree, location, name);
}

static VcAstTypeRef *type_from_semantic(VcAsyncContext *context, VcSemanticType type,
    VcSourceLocation location)
{
    return context == NULL ? NULL : type_from_semantic_model(
        context->tree, context->semantic, type, location);
}

static VcAstNode *unary(VcAstTree *tree, VcSourceLocation location, VcTokenKind op, VcAstNode *operand)
{
    VcAstNode *node = vc_ast_new_node(tree, VC_AST_UNARY_EXPRESSION, location);
    if (node == NULL)
        return NULL;
    node->as.unary_expression.operator_kind = op;
    node->as.unary_expression.operand = operand;
    node->as.unary_expression.postfix = false;
    return node;
}

static VcAstNode *member_access(VcAstTree *tree, VcSourceLocation location,
    VcAstNode *target, const char *member)
{
    VcAstNode *node = vc_ast_new_node(tree, VC_AST_MEMBER_ACCESS_EXPRESSION, location);
    if (node == NULL)
        return NULL;
    node->as.member_access_expression.target = target;
    node->as.member_access_expression.member = copy_text(tree, member);
    return node->as.member_access_expression.member != NULL ? node : NULL;
}

static VcAstNode *this_member(VcAstTree *tree, VcSourceLocation location, const char *member)
{
    VcAstNode *self = identifier(tree, location, "this");
    return self != NULL ? member_access(tree, location, self, member) : NULL;
}

static VcAstNode *call(VcAstTree *tree, VcSourceLocation location, VcAstNode *callee)
{
    VcAstNode *node = vc_ast_new_node(tree, VC_AST_CALL_EXPRESSION, location);
    if (node == NULL)
        return NULL;
    node->as.call_expression.callee = callee;
    return node;
}

static VcAstNode *member_call(VcAstTree *tree, VcSourceLocation location,
    VcAstNode *target, const char *member)
{
    VcAstNode *access = member_access(tree, location, target, member);
    return access != NULL ? call(tree, location, access) : NULL;
}

static VcAstNode *expression_statement(VcAstTree *tree, VcSourceLocation location,
    VcAstNode *expression)
{
    VcAstNode *node = vc_ast_new_node(tree, VC_AST_EXPRESSION_STATEMENT, location);
    if (node == NULL)
        return NULL;
    node->as.expression_statement.expression = expression;
    return node;
}

static VcAstNode *return_statement(VcAstTree *tree, VcSourceLocation location, VcAstNode *expression)
{
    VcAstNode *node = vc_ast_new_node(tree, VC_AST_RETURN_STATEMENT, location);
    if (node == NULL)
        return NULL;
    node->as.return_statement.expression = expression;
    return node;
}

static VcAstNode *continue_statement(VcAstTree *tree, VcSourceLocation location)
{
    return vc_ast_new_node(tree, VC_AST_CONTINUE_STATEMENT, location);
}

static VcAstNode *break_statement(VcAstTree *tree, VcSourceLocation location)
{
    return vc_ast_new_node(tree, VC_AST_BREAK_STATEMENT, location);
}

static VcAstNode *block(VcAstTree *tree, VcSourceLocation location)
{
    return vc_ast_new_node(tree, VC_AST_BLOCK_STATEMENT, location);
}

static bool block_push(VcAstTree *tree, VcAstNode *target, VcAstNode *statement)
{
    return target != NULL && statement != NULL &&
        vc_ast_node_list_push(tree, &target->as.block_statement.statements, statement);
}

static VcAstNode *assignment_target(VcAstTree *tree, VcSourceLocation location,
    VcAstNode *left, VcAstNode *right)
{
    VcAstNode *node = vc_ast_new_node(tree, VC_AST_ASSIGNMENT_EXPRESSION, location);
    if (node == NULL)
        return NULL;
    node->as.assignment_expression.operator_kind = VC_TOKEN_EQUAL;
    node->as.assignment_expression.left = left;
    node->as.assignment_expression.right = right;
    return node;
}

static VcAstNode *this_member_assignment(VcAstTree *tree, VcSourceLocation location,
    const char *member, VcAstNode *value)
{
    VcAstNode *left = this_member(tree, location, member);
    return left != NULL ? assignment_target(tree, location, left, value) : NULL;
}

static VcAstNode *binary(VcAstTree *tree, VcSourceLocation location, VcTokenKind op,
    VcAstNode *left, VcAstNode *right)
{
    VcAstNode *node = vc_ast_new_node(tree, VC_AST_BINARY_EXPRESSION, location);
    if (node == NULL)
        return NULL;
    node->as.binary_expression.operator_kind = op;
    node->as.binary_expression.left = left;
    node->as.binary_expression.right = right;
    return node;
}

static VcAstNode *new_expression(VcAstTree *tree, VcSourceLocation location, const char *type_name)
{
    VcAstNode *node = vc_ast_new_node(tree, VC_AST_NEW_EXPRESSION, location);
    if (node == NULL)
        return NULL;
    node->as.new_expression.type = named_type(tree, location, type_name);
    return node->as.new_expression.type != NULL ? node : NULL;
}

static VcAstNode *new_generic_expression(VcAstTree *tree, VcSourceLocation location,
    const char *type_name, const VcAstTypeRef *argument)
{
    VcAstNode *node = vc_ast_new_node(tree, VC_AST_NEW_EXPRESSION, location);
    if (node == NULL)
        return NULL;
    node->as.new_expression.type = generic_type(tree, location, type_name, argument);
    return node->as.new_expression.type != NULL ? node : NULL;
}

static VcAstNode *new_expression_with_type(VcAstTree *tree, VcSourceLocation location,
    const VcAstTypeRef *type)
{
    VcAstNode *node = vc_ast_new_node(tree, VC_AST_NEW_EXPRESSION, location);
    if (node == NULL)
        return NULL;
    node->as.new_expression.type = clone_type(tree, type);
    return node->as.new_expression.type != NULL ? node : NULL;
}

static VcAstNode *make_parameter(VcAstTree *tree, VcSourceLocation location,
    const char *name, VcAstTypeRef *type)
{
    VcAstNode *parameter = vc_ast_new_node(tree, VC_AST_PARAMETER, location);
    if (parameter == NULL)
        return NULL;
    parameter->as.parameter.modifier = VC_TOKEN_EOF;
    parameter->as.parameter.type = type;
    parameter->as.parameter.name = copy_text(tree, name);
    return parameter->as.parameter.name != NULL ? parameter : NULL;
}

static bool add_field(VcAstTree *tree, VcAstNode *type, VcSourceLocation location,
    const char *name, VcAstTypeRef *field_type)
{
    VcAstNode *field = vc_ast_new_node(tree, VC_AST_FIELD_DECLARATION, location);
    if (field == NULL)
        return false;
    field->as.field_declaration.modifiers = VC_AST_MOD_PRIVATE;
    field->as.field_declaration.type = field_type;
    field->as.field_declaration.name = copy_text(tree, name);
    return field->as.field_declaration.name != NULL &&
        vc_ast_node_list_push(tree, &type->as.type_declaration.members, field);
}

static const VcSemanticMethod *semantic_method_for_node(
    const VcSemanticModel *semantic, const VcAstNode *method)
{
    if (semantic == NULL || method == NULL)
        return NULL;
    for (size_t i = 0; i < semantic->method_count; i++)
        if (semantic->methods[i].node == method)
            return &semantic->methods[i];
    return NULL;
}

static bool expression_contains_kind(const VcAstNode *node, VcAstKind kind);

static bool statement_contains_kind(const VcAstNode *node, VcAstKind kind)
{
    if (node == NULL)
        return false;
    if (node->kind == kind)
        return true;
    switch (node->kind)
    {
        case VC_AST_BLOCK_STATEMENT:
            for (size_t i = 0; i < node->as.block_statement.statements.count; i++)
                if (statement_contains_kind(node->as.block_statement.statements.items[i], kind)) return true;
            return false;
        case VC_AST_EXPRESSION_STATEMENT:
            return expression_contains_kind(node->as.expression_statement.expression, kind);
        case VC_AST_RETURN_STATEMENT:
            return expression_contains_kind(node->as.return_statement.expression, kind);
        case VC_AST_THROW_STATEMENT:
            return expression_contains_kind(node->as.throw_statement.expression, kind);
        case VC_AST_TRY_STATEMENT:
            if (statement_contains_kind(node->as.try_statement.try_block, kind)) return true;
            for (size_t i = 0; i < node->as.try_statement.catches.count; i++)
                if (statement_contains_kind(node->as.try_statement.catches.items[i], kind)) return true;
            return statement_contains_kind(node->as.try_statement.finally_block, kind);
        case VC_AST_CATCH_CLAUSE:
            return statement_contains_kind(node->as.catch_clause.body, kind);
        case VC_AST_LOCAL_DECLARATION:
            return expression_contains_kind(node->as.local_declaration.initializer, kind);
        case VC_AST_IF_STATEMENT:
            return expression_contains_kind(node->as.if_statement.condition, kind) ||
                statement_contains_kind(node->as.if_statement.then_statement, kind) ||
                statement_contains_kind(node->as.if_statement.else_statement, kind);
        case VC_AST_WHILE_STATEMENT:
        case VC_AST_DO_WHILE_STATEMENT:
            return expression_contains_kind(node->as.while_statement.condition, kind) ||
                statement_contains_kind(node->as.while_statement.body, kind);
        case VC_AST_FOR_STATEMENT:
            return statement_contains_kind(node->as.for_statement.initializer, kind) ||
                expression_contains_kind(node->as.for_statement.condition, kind) ||
                expression_contains_kind(node->as.for_statement.increment, kind) ||
                statement_contains_kind(node->as.for_statement.body, kind);
        case VC_AST_FOREACH_STATEMENT:
            return (kind == VC_AST_AWAIT_EXPRESSION && node->as.foreach_statement.is_await) ||
                expression_contains_kind(node->as.foreach_statement.collection, kind) ||
                statement_contains_kind(node->as.foreach_statement.body, kind);
        case VC_AST_USING_STATEMENT:
            return (kind == VC_AST_AWAIT_EXPRESSION && node->as.using_statement.is_await) ||
                statement_contains_kind(node->as.using_statement.declaration, kind) ||
                expression_contains_kind(node->as.using_statement.expression, kind) ||
                statement_contains_kind(node->as.using_statement.body, kind);
        case VC_AST_LOCK_STATEMENT:
            return expression_contains_kind(node->as.lock_statement.expression, kind) ||
                statement_contains_kind(node->as.lock_statement.body, kind);
        case VC_AST_FIXED_STATEMENT:
            return expression_contains_kind(node->as.fixed_statement.initializer, kind) ||
                statement_contains_kind(node->as.fixed_statement.body, kind);
        case VC_AST_SWITCH_STATEMENT:
            if (expression_contains_kind(node->as.switch_statement.expression, kind)) return true;
            for (size_t i = 0; i < node->as.switch_statement.sections.count; i++)
                if (statement_contains_kind(node->as.switch_statement.sections.items[i], kind)) return true;
            return false;
        case VC_AST_SWITCH_SECTION:
            for (size_t i = 0; i < node->as.switch_section.labels.count; i++)
                if (statement_contains_kind(node->as.switch_section.labels.items[i], kind)) return true;
            for (size_t i = 0; i < node->as.switch_section.statements.count; i++)
                if (statement_contains_kind(node->as.switch_section.statements.items[i], kind)) return true;
            return false;
        case VC_AST_SWITCH_LABEL:
            return expression_contains_kind(node->as.switch_label.value, kind) ||
                expression_contains_kind(node->as.switch_label.pattern, kind) ||
                expression_contains_kind(node->as.switch_label.guard, kind);
        case VC_AST_YIELD_RETURN_STATEMENT:
            return expression_contains_kind(node->as.yield_statement.expression, kind);
        default:
            return false;
    }
}

static bool expression_contains_kind(const VcAstNode *node, VcAstKind kind)
{
    if (node == NULL)
        return false;
    if (node->kind == kind)
        return true;
    switch (node->kind)
    {
        case VC_AST_MEMBER_ACCESS_EXPRESSION:
            return expression_contains_kind(node->as.member_access_expression.target, kind);
        case VC_AST_CALL_EXPRESSION:
            if (expression_contains_kind(node->as.call_expression.callee, kind)) return true;
            for (size_t i = 0; i < node->as.call_expression.arguments.count; i++)
                if (expression_contains_kind(node->as.call_expression.arguments.items[i], kind)) return true;
            return false;
        case VC_AST_INDEX_EXPRESSION:
            if (expression_contains_kind(node->as.index_expression.target, kind) ||
                expression_contains_kind(node->as.index_expression.index, kind)) return true;
            for (size_t i = 0; i < node->as.index_expression.indices.count; i++)
                if (expression_contains_kind(node->as.index_expression.indices.items[i], kind)) return true;
            return false;
        case VC_AST_NEW_EXPRESSION:
            if (expression_contains_kind(node->as.new_expression.array_length, kind)) return true;
            for (size_t i = 0; i < node->as.new_expression.array_lengths.count; i++)
                if (expression_contains_kind(node->as.new_expression.array_lengths.items[i], kind)) return true;
            for (size_t i = 0; i < node->as.new_expression.arguments.count; i++)
                if (expression_contains_kind(node->as.new_expression.arguments.items[i], kind)) return true;
            for (size_t i = 0; i < node->as.new_expression.initializers.count; i++)
                if (expression_contains_kind(node->as.new_expression.initializers.items[i], kind)) return true;
            return false;
        case VC_AST_OBJECT_INITIALIZER_MEMBER:
            return expression_contains_kind(node->as.object_initializer_member.value, kind);
        case VC_AST_COLLECTION_INITIALIZER_ELEMENT:
            for (size_t i = 0; i < node->as.collection_initializer_element.arguments.count; i++)
                if (expression_contains_kind(node->as.collection_initializer_element.arguments.items[i], kind)) return true;
            return false;
        case VC_AST_STACKALLOC_EXPRESSION:
            return expression_contains_kind(node->as.stackalloc_expression.count, kind);
        case VC_AST_CAST_EXPRESSION:
            return expression_contains_kind(node->as.cast_expression.expression, kind);
        case VC_AST_TYPE_RELATION_EXPRESSION:
            if (expression_contains_kind(node->as.type_relation_expression.expression, kind) ||
                expression_contains_kind(node->as.type_relation_expression.pattern_constant, kind) ||
                expression_contains_kind(node->as.type_relation_expression.pattern_left, kind) ||
                expression_contains_kind(node->as.type_relation_expression.pattern_right, kind)) return true;
            for (size_t i = 0; i < node->as.type_relation_expression.pattern_properties.count; i++)
                if (expression_contains_kind(node->as.type_relation_expression.pattern_properties.items[i], kind)) return true;
            return false;
        case VC_AST_PROPERTY_PATTERN_MEMBER:
            return expression_contains_kind(node->as.property_pattern_member.pattern, kind);
        case VC_AST_UNARY_EXPRESSION:
            return expression_contains_kind(node->as.unary_expression.operand, kind);
        case VC_AST_AWAIT_EXPRESSION:
            return expression_contains_kind(node->as.await_expression.operand, kind);
        case VC_AST_RANGE_EXPRESSION:
            return expression_contains_kind(node->as.range_expression.start, kind) ||
                expression_contains_kind(node->as.range_expression.end, kind);
        case VC_AST_BINARY_EXPRESSION:
            return expression_contains_kind(node->as.binary_expression.left, kind) ||
                expression_contains_kind(node->as.binary_expression.right, kind);
        case VC_AST_CONDITIONAL_EXPRESSION:
            return expression_contains_kind(node->as.conditional_expression.condition, kind) ||
                expression_contains_kind(node->as.conditional_expression.when_true, kind) ||
                expression_contains_kind(node->as.conditional_expression.when_false, kind);
        case VC_AST_SWITCH_EXPRESSION:
            if (expression_contains_kind(node->as.switch_expression.expression, kind)) return true;
            for (size_t i = 0; i < node->as.switch_expression.arms.count; i++)
                if (expression_contains_kind(node->as.switch_expression.arms.items[i], kind)) return true;
            return false;
        case VC_AST_SWITCH_EXPRESSION_ARM:
            return expression_contains_kind(node->as.switch_expression_arm.pattern, kind) ||
                expression_contains_kind(node->as.switch_expression_arm.guard, kind) ||
                expression_contains_kind(node->as.switch_expression_arm.result, kind);
        case VC_AST_ASSIGNMENT_EXPRESSION:
            return expression_contains_kind(node->as.assignment_expression.left, kind) ||
                expression_contains_kind(node->as.assignment_expression.right, kind);
        case VC_AST_LAMBDA_EXPRESSION:
            return statement_contains_kind(node->as.lambda_expression.body, kind) ||
                expression_contains_kind(node->as.lambda_expression.body, kind);
        case VC_AST_COMPILER_CLOSURE_FRAME_EXPRESSION:
            for (size_t i = 0; i < node->as.compiler_closure_frame_expression.captures.count; i++)
                if (expression_contains_kind(
                        node->as.compiler_closure_frame_expression.captures.items[i], kind)) return true;
            return false;
        case VC_AST_COMPILER_CAPTURE_EXPRESSION:
            return expression_contains_kind(node->as.compiler_capture_expression.target, kind);
        case VC_AST_PARENTHESIZED_EXPRESSION:
            return expression_contains_kind(node->as.parenthesized_expression.expression, kind);
        default:
            return false;
    }
}


static bool expression_contains_node(const VcAstNode *node, const VcAstNode *wanted);

static bool statement_contains_node(const VcAstNode *node, const VcAstNode *wanted)
{
    if (node == NULL)
        return false;
    if (node == wanted)
        return true;
    switch (node->kind)
    {
        case VC_AST_BLOCK_STATEMENT:
            for (size_t i = 0; i < node->as.block_statement.statements.count; i++)
                if (statement_contains_node(node->as.block_statement.statements.items[i], wanted)) return true;
            return false;
        case VC_AST_EXPRESSION_STATEMENT:
            return expression_contains_node(node->as.expression_statement.expression, wanted);
        case VC_AST_RETURN_STATEMENT:
            return expression_contains_node(node->as.return_statement.expression, wanted);
        case VC_AST_THROW_STATEMENT:
            return expression_contains_node(node->as.throw_statement.expression, wanted);
        case VC_AST_TRY_STATEMENT:
            if (statement_contains_node(node->as.try_statement.try_block, wanted)) return true;
            for (size_t i = 0; i < node->as.try_statement.catches.count; i++)
                if (statement_contains_node(node->as.try_statement.catches.items[i], wanted)) return true;
            return statement_contains_node(node->as.try_statement.finally_block, wanted);
        case VC_AST_CATCH_CLAUSE:
            return statement_contains_node(node->as.catch_clause.body, wanted);
        case VC_AST_LOCAL_DECLARATION:
            return expression_contains_node(node->as.local_declaration.initializer, wanted);
        case VC_AST_IF_STATEMENT:
            return expression_contains_node(node->as.if_statement.condition, wanted) ||
                statement_contains_node(node->as.if_statement.then_statement, wanted) ||
                statement_contains_node(node->as.if_statement.else_statement, wanted);
        case VC_AST_WHILE_STATEMENT:
        case VC_AST_DO_WHILE_STATEMENT:
            return expression_contains_node(node->as.while_statement.condition, wanted) ||
                statement_contains_node(node->as.while_statement.body, wanted);
        case VC_AST_FOR_STATEMENT:
            return statement_contains_node(node->as.for_statement.initializer, wanted) ||
                expression_contains_node(node->as.for_statement.condition, wanted) ||
                expression_contains_node(node->as.for_statement.increment, wanted) ||
                statement_contains_node(node->as.for_statement.body, wanted);
        case VC_AST_FOREACH_STATEMENT:
            return expression_contains_node(node->as.foreach_statement.collection, wanted) ||
                statement_contains_node(node->as.foreach_statement.body, wanted);
        case VC_AST_USING_STATEMENT:
            return statement_contains_node(node->as.using_statement.declaration, wanted) ||
                expression_contains_node(node->as.using_statement.expression, wanted) ||
                statement_contains_node(node->as.using_statement.body, wanted);
        case VC_AST_LOCK_STATEMENT:
            return expression_contains_node(node->as.lock_statement.expression, wanted) ||
                statement_contains_node(node->as.lock_statement.body, wanted);
        case VC_AST_FIXED_STATEMENT:
            return expression_contains_node(node->as.fixed_statement.initializer, wanted) ||
                statement_contains_node(node->as.fixed_statement.body, wanted);
        case VC_AST_SWITCH_STATEMENT:
            if (expression_contains_node(node->as.switch_statement.expression, wanted)) return true;
            for (size_t i = 0; i < node->as.switch_statement.sections.count; i++)
                if (statement_contains_node(node->as.switch_statement.sections.items[i], wanted)) return true;
            return false;
        case VC_AST_SWITCH_SECTION:
            for (size_t i = 0; i < node->as.switch_section.labels.count; i++)
                if (statement_contains_node(node->as.switch_section.labels.items[i], wanted)) return true;
            for (size_t i = 0; i < node->as.switch_section.statements.count; i++)
                if (statement_contains_node(node->as.switch_section.statements.items[i], wanted)) return true;
            return false;
        case VC_AST_SWITCH_LABEL:
            return expression_contains_node(node->as.switch_label.value, wanted) ||
                expression_contains_node(node->as.switch_label.pattern, wanted) ||
                expression_contains_node(node->as.switch_label.guard, wanted);
        case VC_AST_YIELD_RETURN_STATEMENT:
            return expression_contains_node(node->as.yield_statement.expression, wanted);
        default:
            return false;
    }
}

static bool expression_contains_node(const VcAstNode *node, const VcAstNode *wanted)
{
    if (node == NULL)
        return false;
    if (node == wanted)
        return true;
    switch (node->kind)
    {
        case VC_AST_MEMBER_ACCESS_EXPRESSION:
            return expression_contains_node(node->as.member_access_expression.target, wanted);
        case VC_AST_CALL_EXPRESSION:
            if (expression_contains_node(node->as.call_expression.callee, wanted)) return true;
            for (size_t i = 0; i < node->as.call_expression.arguments.count; i++)
                if (expression_contains_node(node->as.call_expression.arguments.items[i], wanted)) return true;
            return false;
        case VC_AST_INDEX_EXPRESSION:
            if (expression_contains_node(node->as.index_expression.target, wanted) ||
                expression_contains_node(node->as.index_expression.index, wanted)) return true;
            for (size_t i = 0; i < node->as.index_expression.indices.count; i++)
                if (expression_contains_node(node->as.index_expression.indices.items[i], wanted)) return true;
            return false;
        case VC_AST_NEW_EXPRESSION:
            if (expression_contains_node(node->as.new_expression.array_length, wanted)) return true;
            for (size_t i = 0; i < node->as.new_expression.array_lengths.count; i++)
                if (expression_contains_node(node->as.new_expression.array_lengths.items[i], wanted)) return true;
            for (size_t i = 0; i < node->as.new_expression.arguments.count; i++)
                if (expression_contains_node(node->as.new_expression.arguments.items[i], wanted)) return true;
            for (size_t i = 0; i < node->as.new_expression.initializers.count; i++)
                if (expression_contains_node(node->as.new_expression.initializers.items[i], wanted)) return true;
            return false;
        case VC_AST_OBJECT_INITIALIZER_MEMBER:
            return expression_contains_node(node->as.object_initializer_member.value, wanted);
        case VC_AST_COLLECTION_INITIALIZER_ELEMENT:
            for (size_t i = 0; i < node->as.collection_initializer_element.arguments.count; i++)
                if (expression_contains_node(node->as.collection_initializer_element.arguments.items[i], wanted)) return true;
            return false;
        case VC_AST_STACKALLOC_EXPRESSION:
            return expression_contains_node(node->as.stackalloc_expression.count, wanted);
        case VC_AST_CAST_EXPRESSION:
            return expression_contains_node(node->as.cast_expression.expression, wanted);
        case VC_AST_TYPE_RELATION_EXPRESSION:
            if (expression_contains_node(node->as.type_relation_expression.expression, wanted) ||
                expression_contains_node(node->as.type_relation_expression.pattern_constant, wanted) ||
                expression_contains_node(node->as.type_relation_expression.pattern_left, wanted) ||
                expression_contains_node(node->as.type_relation_expression.pattern_right, wanted)) return true;
            for (size_t i = 0; i < node->as.type_relation_expression.pattern_properties.count; i++)
                if (expression_contains_node(node->as.type_relation_expression.pattern_properties.items[i], wanted)) return true;
            return false;
        case VC_AST_PROPERTY_PATTERN_MEMBER:
            return expression_contains_node(node->as.property_pattern_member.pattern, wanted);
        case VC_AST_UNARY_EXPRESSION:
            return expression_contains_node(node->as.unary_expression.operand, wanted);
        case VC_AST_AWAIT_EXPRESSION:
            return expression_contains_node(node->as.await_expression.operand, wanted);
        case VC_AST_RANGE_EXPRESSION:
            return expression_contains_node(node->as.range_expression.start, wanted) ||
                expression_contains_node(node->as.range_expression.end, wanted);
        case VC_AST_BINARY_EXPRESSION:
            return expression_contains_node(node->as.binary_expression.left, wanted) ||
                expression_contains_node(node->as.binary_expression.right, wanted);
        case VC_AST_CONDITIONAL_EXPRESSION:
            return expression_contains_node(node->as.conditional_expression.condition, wanted) ||
                expression_contains_node(node->as.conditional_expression.when_true, wanted) ||
                expression_contains_node(node->as.conditional_expression.when_false, wanted);
        case VC_AST_SWITCH_EXPRESSION:
            if (expression_contains_node(node->as.switch_expression.expression, wanted)) return true;
            for (size_t i = 0; i < node->as.switch_expression.arms.count; i++)
                if (expression_contains_node(node->as.switch_expression.arms.items[i], wanted)) return true;
            return false;
        case VC_AST_SWITCH_EXPRESSION_ARM:
            return expression_contains_node(node->as.switch_expression_arm.pattern, wanted) ||
                expression_contains_node(node->as.switch_expression_arm.guard, wanted) ||
                expression_contains_node(node->as.switch_expression_arm.result, wanted);
        case VC_AST_ASSIGNMENT_EXPRESSION:
            return expression_contains_node(node->as.assignment_expression.left, wanted) ||
                expression_contains_node(node->as.assignment_expression.right, wanted);
        case VC_AST_LAMBDA_EXPRESSION:
            return statement_contains_node(node->as.lambda_expression.body, wanted) ||
                expression_contains_node(node->as.lambda_expression.body, wanted);
        case VC_AST_COMPILER_CLOSURE_FRAME_EXPRESSION:
            for (size_t i = 0; i < node->as.compiler_closure_frame_expression.captures.count; i++)
                if (expression_contains_node(
                        node->as.compiler_closure_frame_expression.captures.items[i], wanted)) return true;
            return false;
        case VC_AST_COMPILER_CAPTURE_EXPRESSION:
            return expression_contains_node(node->as.compiler_capture_expression.target, wanted);
        case VC_AST_PARENTHESIZED_EXPRESSION:
            return expression_contains_node(node->as.parenthesized_expression.expression, wanted);
        default:
            return false;
    }
}

static bool identifier_named(const VcAstNode *node, const char *name)
{
    if (node == NULL)
        return false;
    if (node->kind == VC_AST_IDENTIFIER_EXPRESSION)
        return node->as.identifier_expression.name != NULL &&
            strcmp(node->as.identifier_expression.name, name) == 0;
    return false;
}

static bool expression_contains_identifier(const VcAstNode *node, const char *name);

static bool statement_contains_identifier(const VcAstNode *node, const char *name)
{
    if (node == NULL)
        return false;
    if (identifier_named(node, name))
        return true;
    switch (node->kind)
    {
        case VC_AST_BLOCK_STATEMENT:
            for (size_t i = 0; i < node->as.block_statement.statements.count; i++)
                if (statement_contains_identifier(node->as.block_statement.statements.items[i], name)) return true;
            return false;
        case VC_AST_EXPRESSION_STATEMENT:
            return expression_contains_identifier(node->as.expression_statement.expression, name);
        case VC_AST_RETURN_STATEMENT:
            return expression_contains_identifier(node->as.return_statement.expression, name);
        case VC_AST_THROW_STATEMENT:
            return expression_contains_identifier(node->as.throw_statement.expression, name);
        case VC_AST_TRY_STATEMENT:
            if (statement_contains_identifier(node->as.try_statement.try_block, name)) return true;
            for (size_t i = 0; i < node->as.try_statement.catches.count; i++)
                if (statement_contains_identifier(node->as.try_statement.catches.items[i], name)) return true;
            return statement_contains_identifier(node->as.try_statement.finally_block, name);
        case VC_AST_CATCH_CLAUSE:
            return statement_contains_identifier(node->as.catch_clause.body, name);
        case VC_AST_LOCAL_DECLARATION:
            return expression_contains_identifier(node->as.local_declaration.initializer, name);
        case VC_AST_IF_STATEMENT:
            return expression_contains_identifier(node->as.if_statement.condition, name) ||
                statement_contains_identifier(node->as.if_statement.then_statement, name) ||
                statement_contains_identifier(node->as.if_statement.else_statement, name);
        case VC_AST_WHILE_STATEMENT:
        case VC_AST_DO_WHILE_STATEMENT:
            return expression_contains_identifier(node->as.while_statement.condition, name) ||
                statement_contains_identifier(node->as.while_statement.body, name);
        case VC_AST_FOR_STATEMENT:
            return statement_contains_identifier(node->as.for_statement.initializer, name) ||
                expression_contains_identifier(node->as.for_statement.condition, name) ||
                expression_contains_identifier(node->as.for_statement.increment, name) ||
                statement_contains_identifier(node->as.for_statement.body, name);
        case VC_AST_FOREACH_STATEMENT:
            return expression_contains_identifier(node->as.foreach_statement.collection, name) ||
                statement_contains_identifier(node->as.foreach_statement.body, name);
        case VC_AST_USING_STATEMENT:
            return statement_contains_identifier(node->as.using_statement.declaration, name) ||
                expression_contains_identifier(node->as.using_statement.expression, name) ||
                statement_contains_identifier(node->as.using_statement.body, name);
        case VC_AST_LOCK_STATEMENT:
            return expression_contains_identifier(node->as.lock_statement.expression, name) ||
                statement_contains_identifier(node->as.lock_statement.body, name);
        case VC_AST_FIXED_STATEMENT:
            return expression_contains_identifier(node->as.fixed_statement.initializer, name) ||
                statement_contains_identifier(node->as.fixed_statement.body, name);
        case VC_AST_SWITCH_STATEMENT:
            if (expression_contains_identifier(node->as.switch_statement.expression, name)) return true;
            for (size_t i = 0; i < node->as.switch_statement.sections.count; i++)
                if (statement_contains_identifier(node->as.switch_statement.sections.items[i], name)) return true;
            return false;
        case VC_AST_SWITCH_SECTION:
            for (size_t i = 0; i < node->as.switch_section.labels.count; i++)
                if (statement_contains_identifier(node->as.switch_section.labels.items[i], name)) return true;
            for (size_t i = 0; i < node->as.switch_section.statements.count; i++)
                if (statement_contains_identifier(node->as.switch_section.statements.items[i], name)) return true;
            return false;
        case VC_AST_SWITCH_LABEL:
            return expression_contains_identifier(node->as.switch_label.value, name) ||
                expression_contains_identifier(node->as.switch_label.pattern, name) ||
                expression_contains_identifier(node->as.switch_label.guard, name);
        case VC_AST_YIELD_RETURN_STATEMENT:
            return expression_contains_identifier(node->as.yield_statement.expression, name);
        default:
            return false;
    }
}

static bool expression_contains_identifier(const VcAstNode *node, const char *name)
{
    if (node == NULL)
        return false;
    if (identifier_named(node, name))
        return true;
    switch (node->kind)
    {
        case VC_AST_MEMBER_ACCESS_EXPRESSION:
            return expression_contains_identifier(node->as.member_access_expression.target, name);
        case VC_AST_CALL_EXPRESSION:
            if (expression_contains_identifier(node->as.call_expression.callee, name)) return true;
            for (size_t i = 0; i < node->as.call_expression.arguments.count; i++)
                if (expression_contains_identifier(node->as.call_expression.arguments.items[i], name)) return true;
            return false;
        case VC_AST_INDEX_EXPRESSION:
            if (expression_contains_identifier(node->as.index_expression.target, name) ||
                expression_contains_identifier(node->as.index_expression.index, name)) return true;
            for (size_t i = 0; i < node->as.index_expression.indices.count; i++)
                if (expression_contains_identifier(node->as.index_expression.indices.items[i], name)) return true;
            return false;
        case VC_AST_NEW_EXPRESSION:
            if (expression_contains_identifier(node->as.new_expression.array_length, name)) return true;
            for (size_t i = 0; i < node->as.new_expression.array_lengths.count; i++)
                if (expression_contains_identifier(node->as.new_expression.array_lengths.items[i], name)) return true;
            for (size_t i = 0; i < node->as.new_expression.arguments.count; i++)
                if (expression_contains_identifier(node->as.new_expression.arguments.items[i], name)) return true;
            for (size_t i = 0; i < node->as.new_expression.initializers.count; i++)
                if (expression_contains_identifier(node->as.new_expression.initializers.items[i], name)) return true;
            return false;
        case VC_AST_OBJECT_INITIALIZER_MEMBER:
            return expression_contains_identifier(node->as.object_initializer_member.value, name);
        case VC_AST_COLLECTION_INITIALIZER_ELEMENT:
            for (size_t i = 0; i < node->as.collection_initializer_element.arguments.count; i++)
                if (expression_contains_identifier(node->as.collection_initializer_element.arguments.items[i], name)) return true;
            return false;
        case VC_AST_STACKALLOC_EXPRESSION:
            return expression_contains_identifier(node->as.stackalloc_expression.count, name);
        case VC_AST_CAST_EXPRESSION:
            return expression_contains_identifier(node->as.cast_expression.expression, name);
        case VC_AST_TYPE_RELATION_EXPRESSION:
            if (expression_contains_identifier(node->as.type_relation_expression.expression, name) ||
                expression_contains_identifier(node->as.type_relation_expression.pattern_constant, name) ||
                expression_contains_identifier(node->as.type_relation_expression.pattern_left, name) ||
                expression_contains_identifier(node->as.type_relation_expression.pattern_right, name)) return true;
            for (size_t i = 0; i < node->as.type_relation_expression.pattern_properties.count; i++)
                if (expression_contains_identifier(node->as.type_relation_expression.pattern_properties.items[i], name)) return true;
            return false;
        case VC_AST_PROPERTY_PATTERN_MEMBER:
            return expression_contains_identifier(node->as.property_pattern_member.pattern, name);
        case VC_AST_UNARY_EXPRESSION:
            return expression_contains_identifier(node->as.unary_expression.operand, name);
        case VC_AST_AWAIT_EXPRESSION:
            return expression_contains_identifier(node->as.await_expression.operand, name);
        case VC_AST_RANGE_EXPRESSION:
            return expression_contains_identifier(node->as.range_expression.start, name) ||
                expression_contains_identifier(node->as.range_expression.end, name);
        case VC_AST_BINARY_EXPRESSION:
            return expression_contains_identifier(node->as.binary_expression.left, name) ||
                expression_contains_identifier(node->as.binary_expression.right, name);
        case VC_AST_CONDITIONAL_EXPRESSION:
            return expression_contains_identifier(node->as.conditional_expression.condition, name) ||
                expression_contains_identifier(node->as.conditional_expression.when_true, name) ||
                expression_contains_identifier(node->as.conditional_expression.when_false, name);
        case VC_AST_SWITCH_EXPRESSION:
            if (expression_contains_identifier(node->as.switch_expression.expression, name)) return true;
            for (size_t i = 0; i < node->as.switch_expression.arms.count; i++)
                if (expression_contains_identifier(node->as.switch_expression.arms.items[i], name)) return true;
            return false;
        case VC_AST_SWITCH_EXPRESSION_ARM:
            return expression_contains_identifier(node->as.switch_expression_arm.pattern, name) ||
                expression_contains_identifier(node->as.switch_expression_arm.guard, name) ||
                expression_contains_identifier(node->as.switch_expression_arm.result, name);
        case VC_AST_ASSIGNMENT_EXPRESSION:
            return expression_contains_identifier(node->as.assignment_expression.left, name) ||
                expression_contains_identifier(node->as.assignment_expression.right, name);
        case VC_AST_LAMBDA_EXPRESSION:
            return statement_contains_identifier(node->as.lambda_expression.body, name) ||
                expression_contains_identifier(node->as.lambda_expression.body, name);
        case VC_AST_COMPILER_CLOSURE_FRAME_EXPRESSION:
            for (size_t i = 0; i < node->as.compiler_closure_frame_expression.captures.count; i++)
                if (expression_contains_identifier(
                        node->as.compiler_closure_frame_expression.captures.items[i], name)) return true;
            return false;
        case VC_AST_COMPILER_CAPTURE_EXPRESSION:
            return expression_contains_identifier(
                node->as.compiler_capture_expression.target, name);
        case VC_AST_PARENTHESIZED_EXPRESSION:
            return expression_contains_identifier(node->as.parenthesized_expression.expression, name);
        default:
            return false;
    }
}


static const VcSemanticLambda *semantic_lambda_for_node(
    const VcAsyncContext *context, const VcAstNode *node)
{
    if (context == NULL || context->semantic == NULL || node == NULL)
        return NULL;
    for (size_t i = 0; i < context->semantic->lambda_count; i++)
        if (context->semantic->lambdas[i].node == node)
            return &context->semantic->lambdas[i];
    return NULL;
}

static const VcSemanticCapture *rewrite_lambda_capture(
    const VcAsyncContext *context, const char *name)
{
    if (context == NULL || context->rewrite_lambda == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < context->rewrite_lambda->capture_count; i++)
    {
        const VcSemanticCapture *capture = &context->rewrite_lambda->captures[i];
        if (capture->name != NULL && strcmp(capture->name, name) == 0)
            return capture;
    }
    return NULL;
}

static size_t parameter_index_for_identifier(const VcAsyncContext *context, const VcAstNode *node)
{
    const VcSemanticBinding *binding = vc_semantic_binding(context->semantic, node);
    const VcAstNode *declaration = binding != NULL ? binding->declaration_node : NULL;
    if (declaration == NULL && node != NULL && node->kind == VC_AST_IDENTIFIER_EXPRESSION)
    {
        const VcSemanticCapture *capture = rewrite_lambda_capture(
            context, node->as.identifier_expression.name);
        if (capture != NULL)
            declaration = capture->declaration_node;
    }
    if (declaration == NULL)
        return (size_t)-1;
    for (size_t i = 0; i < context->method->as.method_declaration.parameters.count; i++)
        if (context->method->as.method_declaration.parameters.items[i] == declaration)
            return i;
    return (size_t)-1;
}

static bool declaration_is_instance_member(const VcAstNode *declaration)
{
    if (declaration == NULL)
        return false;
    uint32_t modifiers = 0;
    switch (declaration->kind)
    {
        case VC_AST_FIELD_DECLARATION:
            modifiers = declaration->as.field_declaration.modifiers;
            break;
        case VC_AST_PROPERTY_DECLARATION:
            modifiers = declaration->as.property_declaration.modifiers;
            break;
        case VC_AST_METHOD_DECLARATION:
            if (declaration->as.method_declaration.is_constructor)
                return false;
            modifiers = declaration->as.method_declaration.modifiers;
            break;
        default:
            return false;
    }
    return (modifiers & VC_AST_MOD_STATIC) == 0;
}

static bool binding_is_instance_owner_member(
    const VcAsyncContext *context, const VcSemanticBinding *binding)
{
    if (binding == NULL || context->semantic_method == NULL ||
        !context->semantic_method->has_owner_struct)
        return false;

    const size_t owner_index = context->semantic_method->owner_struct_index;
    if (binding->has_method && binding->method_index < context->semantic->method_count)
    {
        const VcSemanticMethod *method = &context->semantic->methods[binding->method_index];
        return method->has_owner_struct && method->owner_struct_index == owner_index &&
            !method->is_static;
    }

    if (binding->has_field && binding->struct_index == owner_index &&
        owner_index < context->semantic->struct_count)
    {
        const VcSemanticStruct *owner = &context->semantic->structs[owner_index];
        return binding->field_index < owner->field_count &&
            !owner->fields[binding->field_index].is_static;
    }

    if (declaration_is_instance_member(binding->declaration_node))
    {
        const VcAstNode *owner_node = context->semantic->structs[owner_index].node;
        if (owner_node != NULL && owner_node->kind == VC_AST_TYPE_DECLARATION)
        {
            for (size_t i = 0; i < owner_node->as.type_declaration.members.count; i++)
                if (owner_node->as.type_declaration.members.items[i] == binding->declaration_node)
                    return true;
        }
    }

    return false;
}

static bool binding_is_static_owner_member(
    const VcAsyncContext *context, const VcSemanticBinding *binding)
{
    if (binding == NULL || context->semantic_method == NULL ||
        !context->semantic_method->has_owner_struct)
        return false;

    const size_t owner_index = context->semantic_method->owner_struct_index;
    if (binding->has_method && binding->method_index < context->semantic->method_count)
    {
        const VcSemanticMethod *method = &context->semantic->methods[binding->method_index];
        return method->has_owner_struct && method->owner_struct_index == owner_index &&
            method->is_static;
    }

    if (binding->has_field && binding->struct_index == owner_index &&
        owner_index < context->semantic->struct_count)
    {
        const VcSemanticStruct *owner = &context->semantic->structs[owner_index];
        return binding->field_index < owner->field_count &&
            owner->fields[binding->field_index].is_static;
    }

    if (binding->declaration_node != NULL &&
        owner_index < context->semantic->struct_count)
    {
        const VcAstNode *owner_node = context->semantic->structs[owner_index].node;
        if (owner_node != NULL && owner_node->kind == VC_AST_TYPE_DECLARATION)
        {
            for (size_t i = 0; i < owner_node->as.type_declaration.members.count; i++)
            {
                if (owner_node->as.type_declaration.members.items[i] != binding->declaration_node)
                    continue;
                uint32_t modifiers = 0;
                switch (binding->declaration_node->kind)
                {
                    case VC_AST_FIELD_DECLARATION:
                        modifiers = binding->declaration_node->as.field_declaration.modifiers;
                        break;
                    case VC_AST_PROPERTY_DECLARATION:
                        modifiers = binding->declaration_node->as.property_declaration.modifiers;
                        break;
                    case VC_AST_METHOD_DECLARATION:
                        modifiers = binding->declaration_node->as.method_declaration.modifiers;
                        break;
                    default:
                        return false;
                }
                return (modifiers & VC_AST_MOD_STATIC) != 0;
            }
        }
    }
    return false;
}

static VcAstNode *owner_type_receiver(VcAsyncContext *context, VcSourceLocation location)
{
    return identifier(context->tree, location, context->owner->as.type_declaration.name);
}

static VcAstNode *owner_receiver(VcAsyncContext *context, VcSourceLocation location)
{
    return this_member(context->tree, location, "_this");
}

static VcAsyncLocal *local_for_declaration(VcAsyncContext *context, const VcAstNode *declaration);
static const VcAsyncLocal *local_for_identifier(const VcAsyncContext *context, const VcAstNode *identifier_node);
static const VcAsyncCatch *catch_for_identifier(const VcAsyncContext *context, const VcAstNode *identifier_node);

static bool rewrite_expression(VcAsyncContext *context, VcAstNode *node);

static bool rewrite_statement(VcAsyncContext *context, VcAstNode *node)
{
    if (node == NULL)
        return true;
    switch (node->kind)
    {
        case VC_AST_BLOCK_STATEMENT:
            for (size_t i = 0; i < node->as.block_statement.statements.count; i++)
                if (!rewrite_statement(context, node->as.block_statement.statements.items[i])) return false;
            return true;
        case VC_AST_EXPRESSION_STATEMENT:
            return rewrite_expression(context, node->as.expression_statement.expression);
        case VC_AST_RETURN_STATEMENT:
            return rewrite_expression(context, node->as.return_statement.expression);
        case VC_AST_THROW_STATEMENT:
            return rewrite_expression(context, node->as.throw_statement.expression);
        case VC_AST_TRY_STATEMENT:
            if (!rewrite_statement(context, node->as.try_statement.try_block)) return false;
            for (size_t i = 0; i < node->as.try_statement.catches.count; i++)
                if (!rewrite_statement(context, node->as.try_statement.catches.items[i])) return false;
            return rewrite_statement(context, node->as.try_statement.finally_block);
        case VC_AST_CATCH_CLAUSE:
            return rewrite_statement(context, node->as.catch_clause.body);
        case VC_AST_LOCAL_DECLARATION:
        {
            VcAsyncLocal *local = local_for_declaration(context, node);
            VcAstNode *initializer = node->as.local_declaration.initializer;
            if (!rewrite_expression(context, initializer))
                return false;
            if (local == NULL)
                return true;
            if (initializer == NULL)
            {
                node->kind = VC_AST_BLOCK_STATEMENT;
                memset(&node->as, 0, sizeof(node->as));
                return true;
            }
            VcAstNode *target = local->promoted
                ? this_member(context->tree, node->location, local->name)
                : identifier(context->tree, node->location, local->name);
            VcAstNode *assign = target != NULL
                ? assignment_target(context->tree, node->location, target, initializer)
                : NULL;
            if (assign == NULL)
                return false;
            node->kind = VC_AST_EXPRESSION_STATEMENT;
            memset(&node->as, 0, sizeof(node->as));
            node->as.expression_statement.expression = assign;
            return true;
        }
        case VC_AST_IF_STATEMENT:
            return rewrite_expression(context, node->as.if_statement.condition) &&
                rewrite_statement(context, node->as.if_statement.then_statement) &&
                rewrite_statement(context, node->as.if_statement.else_statement);
        case VC_AST_WHILE_STATEMENT:
        case VC_AST_DO_WHILE_STATEMENT:
            return rewrite_expression(context, node->as.while_statement.condition) &&
                rewrite_statement(context, node->as.while_statement.body);
        case VC_AST_FOR_STATEMENT:
            return rewrite_statement(context, node->as.for_statement.initializer) &&
                rewrite_expression(context, node->as.for_statement.condition) &&
                rewrite_expression(context, node->as.for_statement.increment) &&
                rewrite_statement(context, node->as.for_statement.body);
        case VC_AST_FOREACH_STATEMENT:
            return rewrite_expression(context, node->as.foreach_statement.collection) &&
                rewrite_statement(context, node->as.foreach_statement.body);
        case VC_AST_USING_STATEMENT:
            return rewrite_statement(context, node->as.using_statement.declaration) &&
                rewrite_expression(context, node->as.using_statement.expression) &&
                rewrite_statement(context, node->as.using_statement.body);
        case VC_AST_LOCK_STATEMENT:
            return rewrite_expression(context, node->as.lock_statement.expression) &&
                rewrite_statement(context, node->as.lock_statement.body);
        case VC_AST_FIXED_STATEMENT:
            return rewrite_expression(context, node->as.fixed_statement.initializer) &&
                rewrite_statement(context, node->as.fixed_statement.body);
        case VC_AST_SWITCH_STATEMENT:
            if (!rewrite_expression(context, node->as.switch_statement.expression)) return false;
            for (size_t i = 0; i < node->as.switch_statement.sections.count; i++)
                if (!rewrite_statement(context, node->as.switch_statement.sections.items[i])) return false;
            return true;
        case VC_AST_SWITCH_SECTION:
            for (size_t i = 0; i < node->as.switch_section.labels.count; i++)
                if (!rewrite_statement(context, node->as.switch_section.labels.items[i])) return false;
            for (size_t i = 0; i < node->as.switch_section.statements.count; i++)
                if (!rewrite_statement(context, node->as.switch_section.statements.items[i])) return false;
            return true;
        case VC_AST_SWITCH_LABEL:
            return rewrite_expression(context, node->as.switch_label.value) &&
                rewrite_expression(context, node->as.switch_label.pattern) &&
                rewrite_expression(context, node->as.switch_label.guard);
        case VC_AST_YIELD_RETURN_STATEMENT:
            return rewrite_expression(context, node->as.yield_statement.expression);
        default:
            return true;
    }
}

static bool rewrite_expression(VcAsyncContext *context, VcAstNode *node)
{
    if (node == NULL)
        return true;
    switch (node->kind)
    {
        case VC_AST_IDENTIFIER_EXPRESSION:
        {
            const char *name = node->as.identifier_expression.name;
            if (name == NULL || strcmp(name, "base") == 0)
                return true;
            if (strcmp(name, "__voidc_async_self") == 0)
            {
                node->as.identifier_expression.name = copy_text(context->tree, "this");
                return node->as.identifier_expression.name != NULL;
            }
            if (strcmp(name, "this") == 0)
            {
                VcAstNode *receiver = owner_receiver(context, node->location);
                if (receiver == NULL)
                    return false;
                *node = *receiver;
                return true;
            }

            const size_t parameter_index = parameter_index_for_identifier(context, node);
            if (parameter_index != (size_t)-1)
            {
                char field_name[48];
                const int written = snprintf(field_name, sizeof(field_name), "__arg%zu", parameter_index);
                if (written < 0 || (size_t)written >= sizeof(field_name))
                    return false;
                VcAstNode *field = this_member(context->tree, node->location, field_name);
                if (field == NULL)
                    return false;
                *node = *field;
                return true;
            }

            const VcSemanticBinding *binding = vc_semantic_binding(context->semantic, node);
            const VcAsyncCatch *catch_local = catch_for_identifier(context, node);
            if (catch_local != NULL)
            {
                VcAstNode *field = this_member(context->tree, node->location, catch_local->field_name);
                if (field == NULL)
                    return false;
                *node = *field;
                return true;
            }
            const VcAsyncLocal *local = local_for_identifier(context, node);
            if (local != NULL)
            {
                if (local->promoted)
                {
                    VcAstNode *field = this_member(context->tree, node->location, local->name);
                    if (field == NULL)
                        return false;
                    *node = *field;
                }
                else
                    node->as.identifier_expression.name = local->name;
                return true;
            }
            if ((context->method->as.method_declaration.modifiers & VC_AST_MOD_STATIC) == 0 &&
                binding_is_instance_owner_member(context, binding))
            {
                VcAstNode *receiver = owner_receiver(context, node->location);
                char *member = copy_text(context->tree, name);
                if (receiver == NULL || member == NULL)
                    return false;
                node->kind = VC_AST_MEMBER_ACCESS_EXPRESSION;
                node->as.member_access_expression.target = receiver;
                node->as.member_access_expression.member = member;
                node->as.member_access_expression.constrained_static_parameter = NULL;
                node->as.member_access_expression.null_conditional = false;
                node->as.member_access_expression.null_conditional_direct = false;
                return true;
            }
            if (binding_is_static_owner_member(context, binding))
            {
                VcAstNode *receiver = owner_type_receiver(context, node->location);
                char *member = copy_text(context->tree, name);
                if (receiver == NULL || member == NULL)
                    return false;
                node->kind = VC_AST_MEMBER_ACCESS_EXPRESSION;
                node->as.member_access_expression.target = receiver;
                node->as.member_access_expression.member = member;
                node->as.member_access_expression.constrained_static_parameter = NULL;
                node->as.member_access_expression.null_conditional = false;
                node->as.member_access_expression.null_conditional_direct = false;
            }
            return true;
        }
        case VC_AST_MEMBER_ACCESS_EXPRESSION:
        {
            VcAstNode *target = node->as.member_access_expression.target;
            const char *member = node->as.member_access_expression.member;
            if (target != NULL && target->kind == VC_AST_IDENTIFIER_EXPRESSION &&
                target->as.identifier_expression.name != NULL &&
                strcmp(target->as.identifier_expression.name, "this") == 0 && member != NULL &&
                (strncmp(member, "__arg", 5) == 0 ||
                 strncmp(member, "__voidc_async_", 14) == 0))
                return true;
            return rewrite_expression(context, target);
        }
        case VC_AST_CALL_EXPRESSION:
        {
            VcAstNode *callee = node->as.call_expression.callee;
            const VcSemanticBinding *binding = vc_semantic_binding(context->semantic, node);
            bool rewritten_callee = false;
            if (callee != NULL && callee->kind == VC_AST_IDENTIFIER_EXPRESSION)
            {
                VcAstNode *receiver = NULL;
                if ((context->method->as.method_declaration.modifiers & VC_AST_MOD_STATIC) == 0 &&
                    binding_is_instance_owner_member(context, binding))
                    receiver = owner_receiver(context, callee->location);
                else if (binding_is_static_owner_member(context, binding))
                    receiver = owner_type_receiver(context, callee->location);

                if (receiver != NULL)
                {
                    char *member = copy_text(context->tree, callee->as.identifier_expression.name);
                    if (member == NULL)
                        return false;
                    callee->kind = VC_AST_MEMBER_ACCESS_EXPRESSION;
                    callee->as.member_access_expression.target = receiver;
                    callee->as.member_access_expression.member = member;
                    callee->as.member_access_expression.constrained_static_parameter = NULL;
                    callee->as.member_access_expression.null_conditional = false;
                    callee->as.member_access_expression.null_conditional_direct = false;
                    rewritten_callee = true;
                }
            }
            if (!rewritten_callee && !rewrite_expression(context, callee)) return false;
            for (size_t i = 0; i < node->as.call_expression.arguments.count; i++)
                if (!rewrite_expression(context, node->as.call_expression.arguments.items[i])) return false;
            return true;
        }
        case VC_AST_INDEX_EXPRESSION:
            if (!rewrite_expression(context, node->as.index_expression.target) ||
                !rewrite_expression(context, node->as.index_expression.index)) return false;
            for (size_t i = 0; i < node->as.index_expression.indices.count; i++)
                if (!rewrite_expression(context, node->as.index_expression.indices.items[i])) return false;
            return true;
        case VC_AST_NEW_EXPRESSION:
            if (!rewrite_expression(context, node->as.new_expression.array_length)) return false;
            for (size_t i = 0; i < node->as.new_expression.array_lengths.count; i++)
                if (!rewrite_expression(context, node->as.new_expression.array_lengths.items[i])) return false;
            for (size_t i = 0; i < node->as.new_expression.arguments.count; i++)
                if (!rewrite_expression(context, node->as.new_expression.arguments.items[i])) return false;
            for (size_t i = 0; i < node->as.new_expression.initializers.count; i++)
                if (!rewrite_expression(context, node->as.new_expression.initializers.items[i])) return false;
            return true;
        case VC_AST_OBJECT_INITIALIZER_MEMBER:
            return rewrite_expression(context, node->as.object_initializer_member.value);
        case VC_AST_COLLECTION_INITIALIZER_ELEMENT:
            for (size_t i = 0; i < node->as.collection_initializer_element.arguments.count; i++)
                if (!rewrite_expression(context, node->as.collection_initializer_element.arguments.items[i])) return false;
            return true;
        case VC_AST_STACKALLOC_EXPRESSION:
            return rewrite_expression(context, node->as.stackalloc_expression.count);
        case VC_AST_CAST_EXPRESSION:
            return rewrite_expression(context, node->as.cast_expression.expression);
        case VC_AST_TYPE_RELATION_EXPRESSION:
            if (!rewrite_expression(context, node->as.type_relation_expression.expression) ||
                !rewrite_expression(context, node->as.type_relation_expression.pattern_constant) ||
                !rewrite_expression(context, node->as.type_relation_expression.pattern_left) ||
                !rewrite_expression(context, node->as.type_relation_expression.pattern_right)) return false;
            for (size_t i = 0; i < node->as.type_relation_expression.pattern_properties.count; i++)
                if (!rewrite_expression(context, node->as.type_relation_expression.pattern_properties.items[i])) return false;
            return true;
        case VC_AST_PROPERTY_PATTERN_MEMBER:
            return rewrite_expression(context, node->as.property_pattern_member.pattern);
        case VC_AST_UNARY_EXPRESSION:
            return rewrite_expression(context, node->as.unary_expression.operand);
        case VC_AST_AWAIT_EXPRESSION:
            return rewrite_expression(context, node->as.await_expression.operand);
        case VC_AST_RANGE_EXPRESSION:
            return rewrite_expression(context, node->as.range_expression.start) &&
                rewrite_expression(context, node->as.range_expression.end);
        case VC_AST_BINARY_EXPRESSION:
            return rewrite_expression(context, node->as.binary_expression.left) &&
                rewrite_expression(context, node->as.binary_expression.right);
        case VC_AST_CONDITIONAL_EXPRESSION:
            return rewrite_expression(context, node->as.conditional_expression.condition) &&
                rewrite_expression(context, node->as.conditional_expression.when_true) &&
                rewrite_expression(context, node->as.conditional_expression.when_false);
        case VC_AST_SWITCH_EXPRESSION:
            if (!rewrite_expression(context, node->as.switch_expression.expression)) return false;
            for (size_t i = 0; i < node->as.switch_expression.arms.count; i++)
                if (!rewrite_expression(context, node->as.switch_expression.arms.items[i])) return false;
            return true;
        case VC_AST_SWITCH_EXPRESSION_ARM:
            return rewrite_expression(context, node->as.switch_expression_arm.pattern) &&
                rewrite_expression(context, node->as.switch_expression_arm.guard) &&
                rewrite_expression(context, node->as.switch_expression_arm.result);
        case VC_AST_ASSIGNMENT_EXPRESSION:
            return rewrite_expression(context, node->as.assignment_expression.left) &&
                rewrite_expression(context, node->as.assignment_expression.right);
        case VC_AST_LAMBDA_EXPRESSION:
        {
            const VcSemanticLambda *previous = context->rewrite_lambda;
            const VcSemanticLambda *current = semantic_lambda_for_node(context, node);
            if (current != NULL)
                context->rewrite_lambda = current;
            const bool ok = node->as.lambda_expression.expression_body
                ? rewrite_expression(context, node->as.lambda_expression.body)
                : rewrite_statement(context, node->as.lambda_expression.body);
            context->rewrite_lambda = previous;
            return ok;
        }
        case VC_AST_COMPILER_CLOSURE_FRAME_EXPRESSION:
            for (size_t i = 0; i < node->as.compiler_closure_frame_expression.captures.count; i++)
                if (!rewrite_expression(context,
                        node->as.compiler_closure_frame_expression.captures.items[i])) return false;
            return true;
        case VC_AST_COMPILER_CAPTURE_EXPRESSION:
            return rewrite_expression(context, node->as.compiler_capture_expression.target);
        case VC_AST_PARENTHESIZED_EXPRESSION:
            return rewrite_expression(context, node->as.parenthesized_expression.expression);
        default:
            return true;
    }
}

static bool make_state_constructor(VcAsyncContext *context, VcAstNode *state_type,
    const char *state_name, bool captures_this, const VcAstTypeRef *result_type, int initial_state)
{
    const VcSourceLocation location = context->method->location;
    VcAstNode *constructor = vc_ast_new_node(context->tree, VC_AST_METHOD_DECLARATION, location);
    VcAstNode *body = block(context->tree, location);
    if (constructor == NULL || body == NULL)
        return false;
    constructor->as.method_declaration.modifiers = VC_AST_MOD_PUBLIC;
    constructor->as.method_declaration.name = copy_text(context->tree, state_name);
    constructor->as.method_declaration.is_constructor = true;
    constructor->as.method_declaration.body = body;
    if (constructor->as.method_declaration.name == NULL)
        return false;

    if (captures_this)
    {
        VcAstTypeRef *owner_type = named_type(context->tree, location,
            context->owner->as.type_declaration.name);
        VcAstNode *parameter = make_parameter(context->tree, location, "__owner", owner_type);
        VcAstNode *assign = this_member_assignment(context->tree, location, "_this",
            identifier(context->tree, location, "__owner"));
        if (owner_type == NULL || parameter == NULL || assign == NULL ||
            !vc_ast_node_list_push(context->tree, &constructor->as.method_declaration.parameters, parameter) ||
            !block_push(context->tree, body, expression_statement(context->tree, location, assign)))
            return false;
    }

    for (size_t i = 0; i < context->method->as.method_declaration.parameters.count; i++)
    {
        VcAstNode *source = context->method->as.method_declaration.parameters.items[i];
        char parameter_name[48];
        char field_name[48];
        const int p_written = snprintf(parameter_name, sizeof(parameter_name), "__value%zu", i);
        const int f_written = snprintf(field_name, sizeof(field_name), "__arg%zu", i);
        if (p_written < 0 || (size_t)p_written >= sizeof(parameter_name) ||
            f_written < 0 || (size_t)f_written >= sizeof(field_name))
            return false;
        VcAstNode *parameter = make_parameter(context->tree, source->location,
            parameter_name, source->as.parameter.type);
        VcAstNode *assign = this_member_assignment(context->tree, source->location,
            field_name, identifier(context->tree, source->location, parameter_name));
        if (parameter == NULL || assign == NULL ||
            !vc_ast_node_list_push(context->tree, &constructor->as.method_declaration.parameters, parameter) ||
            !block_push(context->tree, body,
                expression_statement(context->tree, source->location, assign)))
            return false;
    }

    VcAstNode *completion = result_type == NULL
        ? new_expression(context->tree, location, "TaskCompletionSource")
        : new_generic_expression(context->tree, location, "TaskCompletionSource", result_type);
    VcAstNode *completion_assign = this_member_assignment(context->tree, location,
        "_completion", completion);
    VcAstNode *state_assign = this_member_assignment(context->tree, location,
        "_state", number_literal(context->tree, location, initial_state));
    if (completion == NULL || completion_assign == NULL || state_assign == NULL ||
        !block_push(context->tree, body,
            expression_statement(context->tree, location, completion_assign)) ||
        !block_push(context->tree, body,
            expression_statement(context->tree, location, state_assign)) ||
        !vc_ast_node_list_push(context->tree, &state_type->as.type_declaration.members, constructor))
        return false;
    return true;
}

static VcAsyncLocal *local_for_declaration(VcAsyncContext *context, const VcAstNode *declaration)
{
    if (context == NULL || declaration == NULL)
        return NULL;
    for (size_t i = 0; i < context->local_count; i++)
        if (context->locals[i].declaration == declaration)
            return &context->locals[i];
    return NULL;
}

static const VcAsyncLocal *local_for_identifier(
    const VcAsyncContext *context, const VcAstNode *identifier_node)
{
    if (context == NULL || context->semantic == NULL || identifier_node == NULL)
        return NULL;
    const VcSemanticBinding *binding = vc_semantic_binding(context->semantic, identifier_node);
    const VcAstNode *declaration = binding != NULL ? binding->declaration_node : NULL;
    if (declaration == NULL && identifier_node->kind == VC_AST_IDENTIFIER_EXPRESSION)
    {
        const VcSemanticCapture *capture = rewrite_lambda_capture(
            context, identifier_node->as.identifier_expression.name);
        if (capture != NULL)
            declaration = capture->declaration_node;
    }
    if (declaration == NULL)
        return NULL;
    for (size_t i = 0; i < context->local_count; i++)
        if (context->locals[i].declaration == declaration)
            return &context->locals[i];
    return NULL;
}

static const VcAsyncCatch *catch_for_identifier(
    const VcAsyncContext *context, const VcAstNode *identifier_node)
{
    if (context == NULL || context->semantic == NULL || identifier_node == NULL)
        return NULL;
    const VcSemanticBinding *binding = vc_semantic_binding(context->semantic, identifier_node);
    if (binding == NULL || binding->declaration_node == NULL)
        return NULL;
    for (size_t i = 0; i < context->catch_count; i++)
        if (context->catches[i].clause == binding->declaration_node)
            return &context->catches[i];
    return NULL;
}

static bool add_async_local(VcAsyncContext *context, VcAstNode *declaration)
{
    if (declaration == NULL || declaration->kind != VC_AST_LOCAL_DECLARATION)
        return true;
    if (local_for_declaration(context, declaration) != NULL)
        return true;

    VcAstTypeRef *type = NULL;
    if (!declaration->as.local_declaration.is_var && declaration->as.local_declaration.type != NULL)
        type = clone_type(context->tree, declaration->as.local_declaration.type);
    else
    {
        const VcSemanticBinding *binding = vc_semantic_binding(context->semantic, declaration);
        if ((binding == NULL || binding->type == VC_SEM_TYPE_ERROR ||
                binding->type == VC_SEM_TYPE_UNKNOWN || binding->type == VC_SEM_TYPE_VOID) &&
            declaration->as.local_declaration.initializer != NULL)
            binding = vc_semantic_binding(context->semantic, declaration->as.local_declaration.initializer);
        if (binding != NULL)
            type = type_from_semantic(context, binding->type, declaration->location);
    }
    if (type == NULL)
    {
        context_error(context, "async local '%s' type could not be represented in the state machine",
            declaration->as.local_declaration.name != NULL ? declaration->as.local_declaration.name : "<unknown>");
        return false;
    }

    if (context->local_count == context->local_capacity)
    {
        const size_t capacity = context->local_capacity == 0 ? 8 : context->local_capacity * 2;
        VcAsyncLocal *locals = realloc(context->locals, capacity * sizeof(*locals));
        if (locals == NULL)
        {
            context_error(context, "out of memory while collecting async locals");
            return false;
        }
        context->locals = locals;
        context->local_capacity = capacity;
    }

    char generated[64];
    const int written = snprintf(generated, sizeof(generated), "__voidc_async_local_%zu", context->local_count);
    if (written < 0 || (size_t)written >= sizeof(generated))
        return false;
    char *name = copy_text(context->tree, generated);
    if (name == NULL)
        return false;
    context->locals[context->local_count++] = (VcAsyncLocal){declaration, type, name, false};
    return true;
}

static VcAsyncForeach *async_foreach_for_statement(VcAsyncContext *context,
    const VcAstNode *statement)
{
    if (context == NULL || statement == NULL)
        return NULL;
    for (size_t i = 0; i < context->foreach_count; i++)
        if (context->foreaches[i].statement == statement)
            return &context->foreaches[i];
    return NULL;
}

static VcAsyncForeach *add_async_foreach(VcAsyncContext *context, VcAstNode *statement)
{
    VcAsyncForeach *existing = async_foreach_for_statement(context, statement);
    if (existing != NULL)
        return existing;
    const VcSemanticBinding *binding = vc_semantic_binding(context->semantic, statement);
    const VcSemanticBinding *collection_binding = statement != NULL
        ? vc_semantic_binding(context->semantic, statement->as.foreach_statement.collection)
        : NULL;
    if (binding == NULL || !binding->has_foreach || collection_binding == NULL)
    {
        context_error(context, "async foreach binding is unavailable");
        return NULL;
    }
    VcAstTypeRef *collection_type = type_from_semantic(context, collection_binding->type,
        statement->location);
    VcAstTypeRef *item_type = type_from_semantic(context, binding->foreach_item_type,
        statement->location);
    VcAstTypeRef *enumerator_type = binding->foreach_array ? NULL
        : type_from_semantic(context, binding->foreach_enumerator_type, statement->location);
    const VcSemanticBinding *move_next_await_binding = NULL;
    const VcSemanticBinding *dispose_await_binding = NULL;
    if (binding->foreach_async)
    {
        if (binding->foreach_move_next_method_index >= context->semantic->method_count ||
            binding->foreach_dispose_method_index >= context->semantic->method_count)
        {
            context_error(context, "async foreach protocol binding is unavailable");
            return NULL;
        }
        if (statement->as.foreach_statement.move_next_await_protocol == NULL ||
            statement->as.foreach_statement.dispose_await_protocol == NULL)
        {
            context_error(context, "async foreach await protocol metadata is unavailable");
            return NULL;
        }
        move_next_await_binding = vc_semantic_binding(context->semantic,
            statement->as.foreach_statement.move_next_await_protocol);
        dispose_await_binding = vc_semantic_binding(context->semantic,
            statement->as.foreach_statement.dispose_await_protocol);
        if (move_next_await_binding == NULL || !move_next_await_binding->has_await_protocol ||
            move_next_await_binding->type != VC_SEM_TYPE_BOOL ||
            dispose_await_binding == NULL || !dispose_await_binding->has_await_protocol ||
            dispose_await_binding->type != VC_SEM_TYPE_VOID)
        {
            context_error(context, "async foreach generalized await protocol binding is unavailable");
            return NULL;
        }
    }
    if (collection_type == NULL || item_type == NULL ||
        (!binding->foreach_array && enumerator_type == NULL))
    {
        context_error(context, "async foreach types could not be represented in the state machine");
        return NULL;
    }
    if (context->foreach_count == context->foreach_capacity)
    {
        const size_t capacity = context->foreach_capacity == 0 ? 4 : context->foreach_capacity * 2;
        VcAsyncForeach *items = realloc(context->foreaches, capacity * sizeof(*items));
        if (items == NULL)
        {
            context_error(context, "out of memory while collecting async foreach state");
            return NULL;
        }
        context->foreaches = items;
        context->foreach_capacity = capacity;
    }
    const size_t index = context->foreach_count++;
    context->foreaches[index] = (VcAsyncForeach){
        statement, collection_type, item_type, enumerator_type,
        move_next_await_binding, dispose_await_binding, binding->foreach_async,
        binding->foreach_array, binding->foreach_array
            ? vc_semantic_array_rank(context->semantic, collection_binding->type) : 0,
        binding->foreach_has_dispose
    };
    return &context->foreaches[index];
}

static VcAsyncLocal *add_promoted_async_local(VcAsyncContext *context,
    const VcAstNode *declaration, const VcAstTypeRef *type, const char *prefix)
{
    VcAsyncLocal *existing = local_for_declaration(context, declaration);
    if (existing != NULL)
    {
        existing->promoted = true;
        return existing;
    }
    if (context == NULL || declaration == NULL || type == NULL || prefix == NULL)
        return NULL;
    if (context->local_count == context->local_capacity)
    {
        const size_t capacity = context->local_capacity == 0 ? 8 : context->local_capacity * 2;
        VcAsyncLocal *locals = realloc(context->locals, capacity * sizeof(*locals));
        if (locals == NULL)
        {
            context_error(context, "out of memory while collecting async foreach locals");
            return NULL;
        }
        context->locals = locals;
        context->local_capacity = capacity;
    }
    char generated[96];
    const int written = snprintf(generated, sizeof(generated), "__voidc_async_%s_%zu",
        prefix, context->local_count);
    if (written < 0 || (size_t)written >= sizeof(generated))
        return NULL;
    char *name = copy_text(context->tree, generated);
    VcAstTypeRef *type_copy = clone_type(context->tree, type);
    if (name == NULL || type_copy == NULL)
        return NULL;
    const size_t index = context->local_count++;
    context->locals[index] = (VcAsyncLocal){declaration, type_copy, name, true};
    return &context->locals[index];
}

static bool collect_async_pattern_locals(VcAsyncContext *context, VcAstNode *pattern)
{
    if (pattern == NULL)
        return true;
    if (pattern->kind == VC_AST_PROPERTY_PATTERN_MEMBER)
        return collect_async_pattern_locals(context,
            pattern->as.property_pattern_member.pattern);
    if (pattern->kind != VC_AST_TYPE_RELATION_EXPRESSION)
        return true;

    if (pattern->as.type_relation_expression.pattern_name != NULL)
    {
        const VcSemanticBinding *binding = vc_semantic_binding(context->semantic, pattern);
        if (binding == NULL || !binding->has_type_relation ||
            binding->relation_target_type == VC_SEM_TYPE_ERROR ||
            binding->relation_target_type == VC_SEM_TYPE_UNKNOWN ||
            binding->relation_target_type == VC_SEM_TYPE_VOID)
        {
            context_error(context, "async pattern local type could not be resolved");
            return false;
        }
        VcAstTypeRef *type = type_from_semantic(context,
            binding->relation_target_type, pattern->location);
        if (type == NULL || add_promoted_async_local(context, pattern, type, "pattern") == NULL)
        {
            context_error(context, "async pattern local type could not be represented");
            return false;
        }
    }

    if (!collect_async_pattern_locals(context,
            pattern->as.type_relation_expression.pattern_left) ||
        !collect_async_pattern_locals(context,
            pattern->as.type_relation_expression.pattern_right))
        return false;
    for (size_t i = 0; i < pattern->as.type_relation_expression.pattern_properties.count; i++)
        if (!collect_async_pattern_locals(context,
                pattern->as.type_relation_expression.pattern_properties.items[i]))
            return false;
    return true;
}

static bool collect_async_switch_section_pattern_locals(
    VcAsyncContext *context, VcAstNode *section)
{
    if (section == NULL || section->kind != VC_AST_SWITCH_SECTION)
        return true;
    for (size_t i = 0; i < section->as.switch_section.labels.count; i++)
    {
        VcAstNode *label = section->as.switch_section.labels.items[i];
        if (label != NULL && label->kind == VC_AST_SWITCH_LABEL &&
            label->as.switch_label.is_pattern &&
            !collect_async_pattern_locals(context, label->as.switch_label.pattern))
            return false;
    }
    return true;
}

static VcAstNode *synthetic_local_marker(VcAsyncContext *context, VcSourceLocation location)
{
    return vc_ast_new_node(context->tree, VC_AST_LOCAL_DECLARATION, location);
}

static VcAstNode *foreach_array_length_call(VcAsyncContext *context,
    VcSourceLocation location, const char *collection_name, size_t dimension)
{
    VcAstNode *call_node = member_call(context->tree, location,
        this_member(context->tree, location, collection_name), "GetLength");
    VcAstNode *dimension_node = number_literal(context->tree, location, (int)dimension);
    if (call_node == NULL || dimension_node == NULL ||
        !vc_ast_node_list_push(context->tree, &call_node->as.call_expression.arguments,
            dimension_node))
        return NULL;
    return call_node;
}

static VcAstNode *foreach_array_item_expression(VcAsyncContext *context,
    VcSourceLocation location, const char *collection_name, const char *index_name,
    size_t rank)
{
    VcAstNode *target = this_member(context->tree, location, collection_name);
    VcAstNode *index_node = vc_ast_new_node(context->tree, VC_AST_INDEX_EXPRESSION, location);
    if (target == NULL || index_node == NULL || rank == 0)
        return NULL;
    index_node->as.index_expression.target = target;

    if (rank == 1)
    {
        index_node->as.index_expression.index = this_member(context->tree, location, index_name);
        return index_node->as.index_expression.index != NULL ? index_node : NULL;
    }

    for (size_t dimension = 0; dimension < rank; dimension++)
    {
        VcAstNode *value = this_member(context->tree, location, index_name);
        if (value == NULL)
            return NULL;
        VcAstNode *stride = NULL;
        for (size_t trailing = dimension + 1; trailing < rank; trailing++)
        {
            VcAstNode *length = foreach_array_length_call(context, location,
                collection_name, trailing);
            if (length == NULL)
                return NULL;
            stride = stride == NULL ? length
                : binary(context->tree, location, VC_TOKEN_STAR, stride, length);
            if (stride == NULL)
                return NULL;
        }
        if (stride != NULL)
        {
            value = binary(context->tree, location, VC_TOKEN_SLASH, value, stride);
            if (value == NULL)
                return NULL;
        }
        VcAstNode *length = foreach_array_length_call(context, location,
            collection_name, dimension);
        if (length == NULL)
            return NULL;
        value = binary(context->tree, location, VC_TOKEN_PERCENT, value, length);
        if (value == NULL ||
            !vc_ast_node_list_push(context->tree, &index_node->as.index_expression.indices, value))
            return NULL;
    }
    index_node->as.index_expression.index = index_node->as.index_expression.indices.items[0];
    return index_node;
}

static bool collect_async_locals(VcAsyncContext *context, VcAstNode *node)
{
    if (node == NULL)
        return true;
    switch (node->kind)
    {
        case VC_AST_BLOCK_STATEMENT:
            for (size_t i = 0; i < node->as.block_statement.statements.count; i++)
            {
                VcAstNode *statement = node->as.block_statement.statements.items[i];
                if (statement != NULL && statement->kind == VC_AST_LOCAL_DECLARATION)
                {
                    if (!add_async_local(context, statement)) return false;
                    continue;
                }
                if (statement_contains_kind(statement, VC_AST_AWAIT_EXPRESSION) &&
                    !collect_async_locals(context, statement))
                    return false;
            }
            return true;
        case VC_AST_IF_STATEMENT:
            return collect_async_locals(context, node->as.if_statement.then_statement) &&
                collect_async_locals(context, node->as.if_statement.else_statement);
        case VC_AST_WHILE_STATEMENT:
        case VC_AST_DO_WHILE_STATEMENT:
            return collect_async_locals(context, node->as.while_statement.body);
        case VC_AST_FOR_STATEMENT:
            if (node->as.for_statement.initializer != NULL &&
                node->as.for_statement.initializer->kind == VC_AST_LOCAL_DECLARATION &&
                !add_async_local(context, node->as.for_statement.initializer))
                return false;
            return collect_async_locals(context, node->as.for_statement.body);
        case VC_AST_SWITCH_STATEMENT:
            for (size_t i = 0; i < node->as.switch_statement.sections.count; i++)
                if (!collect_async_locals(context, node->as.switch_statement.sections.items[i])) return false;
            return true;
        case VC_AST_SWITCH_SECTION:
            if (!collect_async_switch_section_pattern_locals(context, node))
                return false;
            for (size_t i = 0; i < node->as.switch_section.statements.count; i++)
            {
                VcAstNode *statement = node->as.switch_section.statements.items[i];
                if (statement != NULL && statement->kind == VC_AST_LOCAL_DECLARATION)
                {
                    if (!add_async_local(context, statement)) return false;
                    continue;
                }
                if (statement_contains_kind(statement, VC_AST_AWAIT_EXPRESSION) &&
                    !collect_async_locals(context, statement))
                    return false;
            }
            return true;
        case VC_AST_TRY_STATEMENT:
            if (!collect_async_locals(context, node->as.try_statement.try_block)) return false;
            for (size_t i = 0; i < node->as.try_statement.catches.count; i++)
                if (!collect_async_locals(context, node->as.try_statement.catches.items[i])) return false;
            return collect_async_locals(context, node->as.try_statement.finally_block);
        case VC_AST_CATCH_CLAUSE:
            return collect_async_locals(context, node->as.catch_clause.body);
        case VC_AST_USING_STATEMENT:
            if (node->as.using_statement.declaration != NULL &&
                !add_async_local(context, node->as.using_statement.declaration))
                return false;
            return collect_async_locals(context, node->as.using_statement.body);
        case VC_AST_FOREACH_STATEMENT:
            return collect_async_locals(context, node->as.foreach_statement.body);
        case VC_AST_LOCK_STATEMENT:
            return collect_async_locals(context, node->as.lock_statement.body);
        case VC_AST_FIXED_STATEMENT:
            return collect_async_locals(context, node->as.fixed_statement.body);
        default:
            return true;
    }
}

static VcAsyncBlock *new_async_block(VcAsyncContext *context, VcSourceLocation location,
    int exception_target)
{
    if (context->block_count == context->block_capacity)
    {
        const size_t capacity = context->block_capacity == 0 ? 16 : context->block_capacity * 2;
        VcAsyncBlock *blocks = realloc(context->blocks, capacity * sizeof(*blocks));
        if (blocks == NULL)
        {
            context_error(context, "out of memory while building async state machine");
            return NULL;
        }
        context->blocks = blocks;
        context->block_capacity = capacity;
    }
    VcAsyncBlock *result = &context->blocks[context->block_count];
    memset(result, 0, sizeof(*result));
    result->state = (int)context->block_count;
    result->location = location;
    result->target = -1;
    result->false_target = -1;
    result->await_index = (size_t)-1;
    result->exception_target = exception_target;
    context->block_count++;
    return result;
}

static VcAstNode *async_null_literal(VcAstTree *tree, VcSourceLocation location)
{
    VcAstNode *node = vc_ast_new_node(tree, VC_AST_LITERAL_EXPRESSION, location);
    if (node == NULL)
        return NULL;
    node->as.literal_expression.literal_kind = VC_AST_LITERAL_NULL;
    node->as.literal_expression.text = copy_text(tree, "null");
    return node->as.literal_expression.text != NULL ? node : NULL;
}

static VcAsyncCatch *async_catch_for_clause(VcAsyncContext *context, const VcAstNode *clause)
{
    if (context == NULL || clause == NULL)
        return NULL;
    for (size_t i = 0; i < context->catch_count; i++)
        if (context->catches[i].clause == clause)
            return &context->catches[i];
    return NULL;
}

static VcAsyncCatch *add_async_catch(VcAsyncContext *context, const VcAstNode *clause)
{
    VcAsyncCatch *existing = async_catch_for_clause(context, clause);
    if (existing != NULL)
        return existing;
    const VcSemanticBinding *binding = vc_semantic_binding(context->semantic, clause);
    if (binding == NULL)
    {
        context_error(context, "async catch type could not be resolved");
        return NULL;
    }
    VcAstTypeRef *type = type_from_semantic(context, binding->type, clause->location);
    if (type == NULL)
    {
        context_error(context, "async catch type could not be represented in the state machine");
        return NULL;
    }
    if (context->catch_count == context->catch_capacity)
    {
        const size_t capacity = context->catch_capacity == 0 ? 4 : context->catch_capacity * 2;
        VcAsyncCatch *items = realloc(context->catches, capacity * sizeof(*items));
        if (items == NULL)
        {
            context_error(context, "out of memory while collecting async catches");
            return NULL;
        }
        context->catches = items;
        context->catch_capacity = capacity;
    }
    char generated[64];
    const int written = snprintf(generated, sizeof(generated), "__voidc_async_catch_%zu",
        context->catch_count);
    if (written < 0 || (size_t)written >= sizeof(generated))
        return NULL;
    char *field_name = copy_text(context->tree, generated);
    if (field_name == NULL)
        return NULL;
    const size_t index = context->catch_count++;
    context->catches[index] = (VcAsyncCatch){clause, type, field_name};
    return &context->catches[index];
}

static VcAsyncResource *add_async_resource(VcAsyncContext *context,
    const VcAstNode *using_statement)
{
    const VcSemanticBinding *binding = vc_semantic_binding(context->semantic, using_statement);
    if (binding == NULL || !binding->has_using)
    {
        context_error(context, "async using resource could not be resolved");
        return NULL;
    }
    VcAstTypeRef *type = type_from_semantic(context, binding->using_resource_type,
        using_statement->location);
    if (type == NULL)
    {
        context_error(context, "async using resource type could not be represented in the state machine");
        return NULL;
    }
    if (context->resource_count == context->resource_capacity)
    {
        const size_t capacity = context->resource_capacity == 0 ? 4 : context->resource_capacity * 2;
        VcAsyncResource *items = realloc(context->resources, capacity * sizeof(*items));
        if (items == NULL)
        {
            context_error(context, "out of memory while collecting async using resources");
            return NULL;
        }
        context->resources = items;
        context->resource_capacity = capacity;
    }
    char generated[64];
    const int written = snprintf(generated, sizeof(generated), "__voidc_async_using_%zu",
        context->resource_count);
    if (written < 0 || (size_t)written >= sizeof(generated))
        return NULL;
    char *field_name = copy_text(context->tree, generated);
    if (field_name == NULL)
        return NULL;
    const size_t index = context->resource_count++;
    context->resources[index] = (VcAsyncResource){type, field_name};
    return &context->resources[index];
}

static VcAstNode *async_state_member(VcAsyncContext *context,
    VcSourceLocation location, const char *member)
{
    VcAstNode *self = identifier(context->tree, location, "__voidc_async_self");
    return self != NULL ? member_access(context->tree, location, self, member) : NULL;
}

static VcAsyncSpill *add_async_spill(VcAsyncContext *context,
    VcAstTypeRef *type, VcSourceLocation location)
{
    if (context == NULL || type == NULL)
        return NULL;
    if (context->spill_count == context->spill_capacity)
    {
        const size_t capacity = context->spill_capacity == 0 ? 4 : context->spill_capacity * 2;
        VcAsyncSpill *items = realloc(context->spills, capacity * sizeof(*items));
        if (items == NULL)
        {
            context_error(context, "out of memory while collecting async expression spills");
            return NULL;
        }
        context->spills = items;
        context->spill_capacity = capacity;
    }
    char generated[64];
    const int written = snprintf(generated, sizeof(generated), "__voidc_async_spill_%zu",
        context->spill_count);
    if (written < 0 || (size_t)written >= sizeof(generated))
        return NULL;
    char *field_name = copy_text(context->tree, generated);
    if (field_name == NULL)
        return NULL;
    const size_t index = context->spill_count++;
    context->spills[index] = (VcAsyncSpill){type, field_name};
    (void)location;
    return &context->spills[index];
}

static VcAstTypeRef *async_expression_type(VcAsyncContext *context,
    const VcAstNode *expression)
{
    const VcSemanticBinding *binding = vc_semantic_binding(context->semantic, expression);
    if (binding == NULL)
        return NULL;
    return type_from_semantic(context, binding->type, expression->location);
}

static bool async_expression_is_generated_spill(const VcAstNode *expression)
{
    if (expression == NULL || expression->kind != VC_AST_MEMBER_ACCESS_EXPRESSION ||
        expression->as.member_access_expression.target == NULL ||
        expression->as.member_access_expression.target->kind != VC_AST_IDENTIFIER_EXPRESSION)
        return false;
    const char *name = expression->as.member_access_expression.target->as.identifier_expression.name;
    const char *member = expression->as.member_access_expression.member;
    return name != NULL && strcmp(name, "__voidc_async_self") == 0 &&
        member != NULL && strncmp(member, "__voidc_async_spill_", 20) == 0;
}

static bool async_spill_expression(VcAsyncContext *context, VcAstNode **expression,
    VcAstNodeList *prefix)
{
    if (expression == NULL || *expression == NULL || prefix == NULL)
        return false;
    if (async_expression_is_generated_spill(*expression))
        return true;

    VcAstTypeRef *type = async_expression_type(context, *expression);
    if (type == NULL)
    {
        context_error(context, "async expression spill type could not be resolved");
        return false;
    }
    VcAsyncSpill *spill = add_async_spill(context, type, (*expression)->location);
    if (spill == NULL)
        return false;

    VcAstNode *target = async_state_member(context, (*expression)->location, spill->field_name);
    VcAstNode *assign = target != NULL
        ? assignment_target(context->tree, (*expression)->location, target, *expression)
        : NULL;
    VcAstNode *statement = assign != NULL
        ? expression_statement(context->tree, (*expression)->location, assign)
        : NULL;
    VcAstNode *replacement = async_state_member(context,
        (*expression)->location, spill->field_name);
    if (statement == NULL || replacement == NULL ||
        !vc_ast_node_list_push(context->tree, prefix, statement))
        return false;
    *expression = replacement;
    return true;
}

static bool async_expression_is_simple_value(const VcAstNode *expression)
{
    if (expression == NULL)
        return true;
    return expression->kind == VC_AST_IDENTIFIER_EXPRESSION ||
        expression->kind == VC_AST_LITERAL_EXPRESSION ||
        async_expression_is_generated_spill(expression);
}

static VcAstNode *async_assignment_statement(VcAsyncContext *context,
    VcSourceLocation location, VcAstNode *left, VcAstNode *right)
{
    VcAstNode *assignment = assignment_target(context->tree, location, left, right);
    return assignment != NULL
        ? expression_statement(context->tree, location, assignment)
        : NULL;
}

static bool async_append_spill_assignment(VcAsyncContext *context,
    VcAstNodeList *prefix, const VcAsyncSpill *spill, VcAstNode *value,
    VcSourceLocation location)
{
    VcAstNode *target = async_state_member(context, location, spill->field_name);
    VcAstNode *statement = target != NULL
        ? async_assignment_statement(context, location, target, value)
        : NULL;
    return statement != NULL && vc_ast_node_list_push(context->tree, prefix, statement);
}

static VcAstNode *async_if_statement(VcAsyncContext *context,
    VcSourceLocation location, VcAstNode *condition, VcAstNode *then_statement,
    VcAstNode *else_statement)
{
    VcAstNode *node = vc_ast_new_node(context->tree, VC_AST_IF_STATEMENT, location);
    if (node == NULL)
        return NULL;
    node->as.if_statement.condition = condition;
    node->as.if_statement.then_statement = then_statement;
    node->as.if_statement.else_statement = else_statement;
    return node;
}

static bool normalize_async_foundation_expression(VcAsyncContext *context,
    VcAstNode **expression, VcAstNodeList *prefix);

static bool async_make_lazy_binary(VcAsyncContext *context, VcAstNode **expression,
    VcAstNodeList *prefix)
{
    VcAstNode *node = *expression;
    const VcTokenKind op = node->as.binary_expression.operator_kind;
    if (!normalize_async_foundation_expression(context,
            &node->as.binary_expression.left, prefix))
        return false;

    VcAstTypeRef *result_type = async_expression_type(context, node);
    if (result_type == NULL)
    {
        context_error(context, "async short-circuit spill type could not be resolved");
        return false;
    }
    VcAsyncSpill *result = add_async_spill(context, result_type, node->location);
    if (result == NULL || !async_append_spill_assignment(context, prefix, result,
            node->as.binary_expression.left, node->location))
        return false;

    VcAstNodeList right_prefix = {0};
    if (!normalize_async_foundation_expression(context,
            &node->as.binary_expression.right, &right_prefix))
        return false;
    VcAstNode *right_block = block(context->tree, node->location);
    if (right_block == NULL)
        return false;
    for (size_t i = 0; i < right_prefix.count; i++)
        if (!block_push(context->tree, right_block, right_prefix.items[i]))
            return false;
    VcAstNode *right_target = async_state_member(context, node->location, result->field_name);
    VcAstNode *right_assign = right_target != NULL
        ? async_assignment_statement(context, node->location, right_target,
            node->as.binary_expression.right)
        : NULL;
    if (right_assign == NULL || !block_push(context->tree, right_block, right_assign))
        return false;

    VcAstNode *result_read = async_state_member(context, node->location, result->field_name);
    VcAstNode *condition = NULL;
    if (op == VC_TOKEN_AMPERSAND_AMPERSAND)
        condition = result_read;
    else if (op == VC_TOKEN_PIPE_PIPE)
        condition = result_read != NULL
            ? unary(context->tree, node->location, VC_TOKEN_BANG, result_read)
            : NULL;
    else
        condition = result_read != NULL
            ? binary(context->tree, node->location, VC_TOKEN_EQUAL_EQUAL,
                result_read, async_null_literal(context->tree, node->location))
            : NULL;
    VcAstNode *branch = condition != NULL
        ? async_if_statement(context, node->location, condition, right_block, NULL)
        : NULL;
    VcAstNode *replacement = async_state_member(context, node->location, result->field_name);
    if (branch == NULL || replacement == NULL ||
        !vc_ast_node_list_push(context->tree, prefix, branch))
        return false;
    *expression = replacement;
    return true;
}

static bool async_make_conditional_expression(VcAsyncContext *context,
    VcAstNode **expression, VcAstNodeList *prefix)
{
    VcAstNode *node = *expression;
    if (!normalize_async_foundation_expression(context,
            &node->as.conditional_expression.condition, prefix))
        return false;

    VcAstTypeRef *result_type = async_expression_type(context, node);
    if (result_type == NULL)
    {
        context_error(context, "async conditional spill type could not be resolved");
        return false;
    }
    VcAsyncSpill *result = add_async_spill(context, result_type, node->location);
    if (result == NULL)
        return false;

    VcAstNodeList true_prefix = {0};
    VcAstNodeList false_prefix = {0};
    if (!normalize_async_foundation_expression(context,
            &node->as.conditional_expression.when_true, &true_prefix) ||
        !normalize_async_foundation_expression(context,
            &node->as.conditional_expression.when_false, &false_prefix))
        return false;

    VcAstNode *true_block = block(context->tree, node->location);
    VcAstNode *false_block = block(context->tree, node->location);
    if (true_block == NULL || false_block == NULL)
        return false;
    for (size_t i = 0; i < true_prefix.count; i++)
        if (!block_push(context->tree, true_block, true_prefix.items[i])) return false;
    for (size_t i = 0; i < false_prefix.count; i++)
        if (!block_push(context->tree, false_block, false_prefix.items[i])) return false;

    VcAstNode *true_target = async_state_member(context, node->location, result->field_name);
    VcAstNode *false_target = async_state_member(context, node->location, result->field_name);
    VcAstNode *true_assign = true_target != NULL
        ? async_assignment_statement(context, node->location, true_target,
            node->as.conditional_expression.when_true)
        : NULL;
    VcAstNode *false_assign = false_target != NULL
        ? async_assignment_statement(context, node->location, false_target,
            node->as.conditional_expression.when_false)
        : NULL;
    if (true_assign == NULL || false_assign == NULL ||
        !block_push(context->tree, true_block, true_assign) ||
        !block_push(context->tree, false_block, false_assign))
        return false;

    VcAstNode *branch = async_if_statement(context, node->location,
        node->as.conditional_expression.condition, true_block, false_block);
    VcAstNode *replacement = async_state_member(context, node->location, result->field_name);
    if (branch == NULL || replacement == NULL ||
        !vc_ast_node_list_push(context->tree, prefix, branch))
        return false;
    *expression = replacement;
    return true;
}

static VcAstNode *async_switch_expression_arm_body(VcAsyncContext *context,
    VcAstNode *arm, const char *result_field, const char *matched_field)
{
    VcAstNodeList result_prefix = {0};
    if (!normalize_async_foundation_expression(context,
            &arm->as.switch_expression_arm.result, &result_prefix))
        return NULL;

    VcAstNode *result_block = block(context->tree, arm->location);
    if (result_block == NULL)
        return NULL;
    for (size_t i = 0; i < result_prefix.count; i++)
        if (!block_push(context->tree, result_block, result_prefix.items[i]))
            return NULL;
    VcAstNode *result_target = async_state_member(context, arm->location, result_field);
    VcAstNode *result_assignment = result_target != NULL
        ? async_assignment_statement(context, arm->location, result_target,
            arm->as.switch_expression_arm.result)
        : NULL;
    VcAstNode *matched_target = async_state_member(context, arm->location, matched_field);
    VcAstNode *matched_assignment = matched_target != NULL
        ? async_assignment_statement(context, arm->location, matched_target,
            bool_literal(context->tree, arm->location, true))
        : NULL;
    if (result_assignment == NULL || matched_assignment == NULL ||
        !block_push(context->tree, result_block, result_assignment) ||
        !block_push(context->tree, result_block, matched_assignment))
        return NULL;

    if (arm->as.switch_expression_arm.guard == NULL)
        return result_block;

    VcAstNodeList guard_prefix = {0};
    if (!normalize_async_foundation_expression(context,
            &arm->as.switch_expression_arm.guard, &guard_prefix))
        return NULL;
    VcAstNode *guard_block = block(context->tree, arm->location);
    if (guard_block == NULL)
        return NULL;
    for (size_t i = 0; i < guard_prefix.count; i++)
        if (!block_push(context->tree, guard_block, guard_prefix.items[i]))
            return NULL;
    VcAstNode *guard_branch = async_if_statement(context, arm->location,
        arm->as.switch_expression_arm.guard, result_block, NULL);
    if (guard_branch == NULL || !block_push(context->tree, guard_block, guard_branch))
        return NULL;
    return guard_block;
}

static VcAstNode *async_switch_expression_pattern_dispatch(VcAsyncContext *context,
    VcAstNode *arm, VcAstNode *source, VcAstNode *arm_body)
{
    VcAstNode *switch_node = vc_ast_new_node(context->tree,
        VC_AST_SWITCH_STATEMENT, arm->location);
    VcAstNode *section = vc_ast_new_node(context->tree,
        VC_AST_SWITCH_SECTION, arm->location);
    VcAstNode *label = vc_ast_new_node(context->tree,
        VC_AST_SWITCH_LABEL, arm->location);
    if (switch_node == NULL || section == NULL || label == NULL)
        return NULL;

    switch_node->as.switch_statement.expression = source;
    label->as.switch_label.is_default = false;
    label->as.switch_label.is_pattern = true;
    label->as.switch_label.pattern = arm->as.switch_expression_arm.pattern;
    if (!vc_ast_node_list_push(context->tree, &section->as.switch_section.labels, label) ||
        !vc_ast_node_list_push(context->tree, &section->as.switch_section.statements, arm_body) ||
        !vc_ast_node_list_push(context->tree, &section->as.switch_section.statements,
            break_statement(context->tree, arm->location)) ||
        !vc_ast_node_list_push(context->tree, &switch_node->as.switch_statement.sections, section))
        return NULL;
    return switch_node;
}

static bool async_make_switch_expression(VcAsyncContext *context,
    VcAstNode **expression, VcAstNodeList *prefix)
{
    VcAstNode *node = *expression;
    VcAstTypeRef *result_type = async_expression_type(context, node);
    VcAstTypeRef *bool_type = named_type(context->tree, node->location, "bool");
    if (result_type == NULL || bool_type == NULL)
    {
        context_error(context, "async switch-expression spill type could not be resolved");
        return false;
    }
    VcAsyncSpill *result = add_async_spill(context, result_type, node->location);
    if (result == NULL)
        return false;
    const char *result_field = result->field_name;
    VcAsyncSpill *matched = add_async_spill(context, bool_type, node->location);
    if (matched == NULL)
        return false;
    const char *matched_field = matched->field_name;

    if (!normalize_async_foundation_expression(context,
            &node->as.switch_expression.expression, prefix) ||
        !async_spill_expression(context, &node->as.switch_expression.expression, prefix))
        return false;
    if (!async_expression_is_generated_spill(node->as.switch_expression.expression))
        return false;
    const char *source_field = node->as.switch_expression.expression->as.member_access_expression.member;

    VcAstNode *matched_target = async_state_member(context, node->location, matched_field);
    VcAstNode *initialize_matched = matched_target != NULL
        ? async_assignment_statement(context, node->location, matched_target,
            bool_literal(context->tree, node->location, false))
        : NULL;
    if (initialize_matched == NULL ||
        !vc_ast_node_list_push(context->tree, prefix, initialize_matched))
        return false;

    for (size_t i = 0; i < node->as.switch_expression.arms.count; i++)
    {
        VcAstNode *arm = node->as.switch_expression.arms.items[i];
        if (arm == NULL || arm->kind != VC_AST_SWITCH_EXPRESSION_ARM)
            return false;
        VcAstNode *arm_body = async_switch_expression_arm_body(context, arm,
            result_field, matched_field);
        if (arm_body == NULL)
            return false;

        VcAstNode *gate_body = block(context->tree, arm->location);
        if (gate_body == NULL)
            return false;
        if (arm->as.switch_expression_arm.is_discard)
        {
            if (!block_push(context->tree, gate_body, arm_body))
                return false;
        }
        else
        {
            VcAstNode *source_read = async_state_member(context, arm->location, source_field);
            VcAstNode *dispatch = source_read != NULL
                ? async_switch_expression_pattern_dispatch(context, arm, source_read, arm_body)
                : NULL;
            if (dispatch == NULL || !block_push(context->tree, gate_body, dispatch))
                return false;
        }

        VcAstNode *matched_read = async_state_member(context, arm->location, matched_field);
        VcAstNode *not_matched = matched_read != NULL
            ? unary(context->tree, arm->location, VC_TOKEN_BANG, matched_read)
            : NULL;
        VcAstNode *gate = not_matched != NULL
            ? async_if_statement(context, arm->location, not_matched, gate_body, NULL)
            : NULL;
        if (gate == NULL || !vc_ast_node_list_push(context->tree, prefix, gate))
            return false;
    }

    VcAstNode *replacement = async_state_member(context, node->location, result_field);
    if (replacement == NULL)
        return false;
    *expression = replacement;
    return true;
}

static bool async_normalize_call(VcAsyncContext *context, VcAstNode *node,
    VcAstNodeList *prefix)
{
    bool argument_has_await = false;
    for (size_t i = 0; i < node->as.call_expression.arguments.count; i++)
        argument_has_await = argument_has_await || expression_contains_kind(
            node->as.call_expression.arguments.items[i], VC_AST_AWAIT_EXPRESSION);

    if (!normalize_async_foundation_expression(context,
            &node->as.call_expression.callee, prefix))
        return false;

    if (argument_has_await)
    {
        VcAstNode *callee = node->as.call_expression.callee;
        if (callee != NULL && callee->kind == VC_AST_MEMBER_ACCESS_EXPRESSION &&
            !async_expression_is_simple_value(callee->as.member_access_expression.target))
        {
            if (!async_spill_expression(context,
                    &callee->as.member_access_expression.target, prefix))
                return false;
        }
        else if (callee != NULL && callee->kind != VC_AST_IDENTIFIER_EXPRESSION &&
            callee->kind != VC_AST_MEMBER_ACCESS_EXPRESSION &&
            !async_expression_is_generated_spill(callee))
        {
            if (!async_spill_expression(context, &node->as.call_expression.callee, prefix))
                return false;
        }
    }

    for (size_t i = 0; i < node->as.call_expression.arguments.count; i++)
    {
        const bool current_has_await = expression_contains_kind(
            node->as.call_expression.arguments.items[i], VC_AST_AWAIT_EXPRESSION);
        if (current_has_await)
        {
            for (size_t j = 0; j < i; j++)
                if (!async_expression_is_generated_spill(
                        node->as.call_expression.arguments.items[j]) &&
                    !async_spill_expression(context,
                        &node->as.call_expression.arguments.items[j], prefix))
                    return false;
        }
        if (!normalize_async_foundation_expression(context,
                &node->as.call_expression.arguments.items[i], prefix))
            return false;
    }
    return true;
}

static bool async_normalize_new(VcAsyncContext *context, VcAstNode *node,
    VcAstNodeList *prefix)
{
    if (expression_contains_kind(node->as.new_expression.array_length, VC_AST_AWAIT_EXPRESSION) &&
        !normalize_async_foundation_expression(context,
            &node->as.new_expression.array_length, prefix))
        return false;
    for (size_t i = 0; i < node->as.new_expression.array_lengths.count; i++)
    {
        const bool current_has_await = expression_contains_kind(
            node->as.new_expression.array_lengths.items[i], VC_AST_AWAIT_EXPRESSION);
        if (current_has_await)
        {
            for (size_t j = 0; j < i; j++)
                if (!async_expression_is_generated_spill(node->as.new_expression.array_lengths.items[j]) &&
                    !async_spill_expression(context,
                        &node->as.new_expression.array_lengths.items[j], prefix))
                    return false;
        }
        if (!normalize_async_foundation_expression(context,
                &node->as.new_expression.array_lengths.items[i], prefix))
            return false;
    }
    for (size_t i = 0; i < node->as.new_expression.arguments.count; i++)
    {
        const bool current_has_await = expression_contains_kind(
            node->as.new_expression.arguments.items[i], VC_AST_AWAIT_EXPRESSION);
        if (current_has_await)
        {
            for (size_t j = 0; j < i; j++)
                if (!async_expression_is_generated_spill(node->as.new_expression.arguments.items[j]) &&
                    !async_spill_expression(context,
                        &node->as.new_expression.arguments.items[j], prefix))
                    return false;
        }
        if (!normalize_async_foundation_expression(context,
                &node->as.new_expression.arguments.items[i], prefix))
            return false;
    }
    for (size_t i = 0; i < node->as.new_expression.initializers.count; i++)
    {
        if (expression_contains_kind(node->as.new_expression.initializers.items[i],
                VC_AST_AWAIT_EXPRESSION))
        {
            context_error(context,
                "await in object or collection initializer elements is not supported by the async expression completion stage");
            return false;
        }
    }
    return true;
}

static bool async_normalize_assignment_target(VcAsyncContext *context,
    VcAstNode **expression, VcAstNodeList *prefix, bool capture_for_later_await)
{
    if (expression == NULL || *expression == NULL)
        return true;
    VcAstNode *node = *expression;
    switch (node->kind)
    {
        case VC_AST_MEMBER_ACCESS_EXPRESSION:
            if (!normalize_async_foundation_expression(context,
                    &node->as.member_access_expression.target, prefix))
                return false;
            if (capture_for_later_await &&
                !async_expression_is_simple_value(node->as.member_access_expression.target) &&
                !async_spill_expression(context,
                    &node->as.member_access_expression.target, prefix))
                return false;
            return true;

        case VC_AST_INDEX_EXPRESSION:
            if (!normalize_async_foundation_expression(context,
                    &node->as.index_expression.target, prefix))
                return false;
            if (capture_for_later_await && !async_spill_expression(context,
                    &node->as.index_expression.target, prefix))
                return false;
            if (node->as.index_expression.indices.count != 0)
            {
                for (size_t i = 0; i < node->as.index_expression.indices.count; i++)
                {
                    if (!normalize_async_foundation_expression(context,
                            &node->as.index_expression.indices.items[i], prefix))
                        return false;
                    if (capture_for_later_await && !async_spill_expression(context,
                            &node->as.index_expression.indices.items[i], prefix))
                        return false;
                }
                node->as.index_expression.index = node->as.index_expression.indices.items[0];
            }
            else if (node->as.index_expression.index != NULL)
            {
                if (!normalize_async_foundation_expression(context,
                        &node->as.index_expression.index, prefix))
                    return false;
                if (capture_for_later_await && !async_spill_expression(context,
                        &node->as.index_expression.index, prefix))
                    return false;
            }
            return true;

        default:
            return normalize_async_foundation_expression(context, expression, prefix);
    }
}

static bool normalize_async_foundation_expression(VcAsyncContext *context,
    VcAstNode **expression, VcAstNodeList *prefix)
{
    if (expression == NULL || *expression == NULL ||
        !expression_contains_kind(*expression, VC_AST_AWAIT_EXPRESSION))
        return true;

    VcAstNode *node = *expression;
    switch (node->kind)
    {
        case VC_AST_AWAIT_EXPRESSION:
        {
            if (!normalize_async_foundation_expression(context,
                    &node->as.await_expression.operand, prefix))
                return false;
            const VcSemanticBinding *binding = vc_semantic_binding(context->semantic, node);
            if (binding == NULL || binding->type == VC_SEM_TYPE_VOID ||
                binding->type == VC_SEM_TYPE_ERROR)
            {
                context_error(context,
                    "nested await expression requires a value-producing awaiter");
                return false;
            }
            VcAstTypeRef *result_type = type_from_semantic(context,
                binding->type, node->location);
            if (result_type == NULL)
            {
                context_error(context,
                    "async await spill could not materialize await result type");
                return false;
            }
            VcAsyncSpill *spill = add_async_spill(context, result_type, node->location);
            if (spill == NULL)
                return false;
            VcAstNode *target = async_state_member(context, node->location, spill->field_name);
            VcAstNode *assign = target != NULL
                ? assignment_target(context->tree, node->location, target, node)
                : NULL;
            VcAstNode *statement = assign != NULL
                ? expression_statement(context->tree, node->location, assign)
                : NULL;
            VcAstNode *replacement = async_state_member(context,
                node->location, spill->field_name);
            if (statement == NULL || replacement == NULL ||
                !vc_ast_node_list_push(context->tree, prefix, statement))
                return false;
            *expression = replacement;
            return true;
        }

        case VC_AST_MEMBER_ACCESS_EXPRESSION:
            return normalize_async_foundation_expression(context,
                &node->as.member_access_expression.target, prefix);

        case VC_AST_CALL_EXPRESSION:
            return async_normalize_call(context, node, prefix);

        case VC_AST_INDEX_EXPRESSION:
        {
            bool later_await = false;
            if (node->as.index_expression.indices.count != 0)
            {
                for (size_t i = 0; i < node->as.index_expression.indices.count; i++)
                    later_await = later_await || expression_contains_kind(
                        node->as.index_expression.indices.items[i], VC_AST_AWAIT_EXPRESSION);
            }
            else
                later_await = expression_contains_kind(
                    node->as.index_expression.index, VC_AST_AWAIT_EXPRESSION);
            if (!normalize_async_foundation_expression(context,
                    &node->as.index_expression.target, prefix))
                return false;
            if (later_await && !async_spill_expression(context,
                    &node->as.index_expression.target, prefix))
                return false;
            if (node->as.index_expression.indices.count != 0)
            {
                for (size_t i = 0; i < node->as.index_expression.indices.count; i++)
                {
                    const bool current_has_await = expression_contains_kind(
                        node->as.index_expression.indices.items[i], VC_AST_AWAIT_EXPRESSION);
                    if (current_has_await)
                        for (size_t j = 0; j < i; j++)
                            if (!async_expression_is_generated_spill(node->as.index_expression.indices.items[j]) &&
                                !async_spill_expression(context,
                                    &node->as.index_expression.indices.items[j], prefix))
                                return false;
                    if (!normalize_async_foundation_expression(context,
                            &node->as.index_expression.indices.items[i], prefix))
                        return false;
                }
                node->as.index_expression.index = node->as.index_expression.indices.items[0];
            }
            else if (node->as.index_expression.index != NULL &&
                !normalize_async_foundation_expression(context,
                    &node->as.index_expression.index, prefix))
                return false;
            return true;
        }

        case VC_AST_NEW_EXPRESSION:
            return async_normalize_new(context, node, prefix);

        case VC_AST_STACKALLOC_EXPRESSION:
            return normalize_async_foundation_expression(context,
                &node->as.stackalloc_expression.count, prefix);

        case VC_AST_CAST_EXPRESSION:
            return normalize_async_foundation_expression(context,
                &node->as.cast_expression.expression, prefix);

        case VC_AST_TYPE_RELATION_EXPRESSION:
            return normalize_async_foundation_expression(context,
                       &node->as.type_relation_expression.expression, prefix) &&
                normalize_async_foundation_expression(context,
                       &node->as.type_relation_expression.pattern_constant, prefix);

        case VC_AST_PROPERTY_PATTERN_MEMBER:
            return normalize_async_foundation_expression(context,
                &node->as.property_pattern_member.pattern, prefix);

        case VC_AST_UNARY_EXPRESSION:
            return normalize_async_foundation_expression(context,
                &node->as.unary_expression.operand, prefix);

        case VC_AST_RANGE_EXPRESSION:
        {
            const bool start_has_await = expression_contains_kind(
                node->as.range_expression.start, VC_AST_AWAIT_EXPRESSION);
            const bool end_has_await = expression_contains_kind(
                node->as.range_expression.end, VC_AST_AWAIT_EXPRESSION);
            if (start_has_await &&
                !normalize_async_foundation_expression(context,
                    &node->as.range_expression.start, prefix))
                return false;
            if (end_has_await)
            {
                if (!async_spill_expression(context,
                        &node->as.range_expression.start, prefix))
                    return false;
                if (!normalize_async_foundation_expression(context,
                        &node->as.range_expression.end, prefix))
                    return false;
            }
            return true;
        }

        case VC_AST_BINARY_EXPRESSION:
        {
            if (node->as.binary_expression.operator_kind == VC_TOKEN_AMPERSAND_AMPERSAND ||
                node->as.binary_expression.operator_kind == VC_TOKEN_PIPE_PIPE ||
                node->as.binary_expression.operator_kind == VC_TOKEN_QUESTION_QUESTION)
                return async_make_lazy_binary(context, expression, prefix);

            const bool left_has_await = expression_contains_kind(
                node->as.binary_expression.left, VC_AST_AWAIT_EXPRESSION);
            const bool right_has_await = expression_contains_kind(
                node->as.binary_expression.right, VC_AST_AWAIT_EXPRESSION);
            if (left_has_await &&
                !normalize_async_foundation_expression(context,
                    &node->as.binary_expression.left, prefix))
                return false;
            if (right_has_await)
            {
                if (!async_spill_expression(context,
                        &node->as.binary_expression.left, prefix))
                    return false;
                if (!normalize_async_foundation_expression(context,
                        &node->as.binary_expression.right, prefix))
                    return false;
            }
            return true;
        }

        case VC_AST_CONDITIONAL_EXPRESSION:
            return async_make_conditional_expression(context, expression, prefix);

        case VC_AST_ASSIGNMENT_EXPRESSION:
        {
            const bool right_has_await = expression_contains_kind(
                node->as.assignment_expression.right, VC_AST_AWAIT_EXPRESSION);
            if (!async_normalize_assignment_target(context,
                    &node->as.assignment_expression.left, prefix, right_has_await))
                return false;
            return normalize_async_foundation_expression(context,
                &node->as.assignment_expression.right, prefix);
        }

        case VC_AST_PARENTHESIZED_EXPRESSION:
            return normalize_async_foundation_expression(context,
                &node->as.parenthesized_expression.expression, prefix);

        case VC_AST_SWITCH_EXPRESSION:
            return async_make_switch_expression(context, expression, prefix);

        case VC_AST_SWITCH_EXPRESSION_ARM:
            context_error(context,
                "switch-expression arm cannot be lowered independently");
            return false;

        case VC_AST_LAMBDA_EXPRESSION:
            /* Await inside a nested lambda belongs to that lambda's own async semantics. */
            return true;

        default:
            context_error(context,
                "await expression position is not supported by the async expression completion stage");
            return false;
    }
}

static bool append_async_await(VcAsyncContext *context, VcAstNode *operand,
    VcAstTypeRef *awaiter_type, VcAstTypeRef *result_type,
    const VcAstNode *result_declaration, VcAstNode *assignment_target,
    bool return_result, const VcSemanticBinding *protocol_binding,
    size_t *await_index)
{
    if (awaiter_type == NULL ||
        ((result_declaration != NULL || assignment_target != NULL || return_result) &&
         result_type == NULL))
    {
        context_error(context, "async method '%s' cannot materialize awaiter type",
            context->method->as.method_declaration.name);
        return false;
    }

    if (context->await_count == context->await_capacity)
    {
        const size_t capacity = context->await_capacity == 0 ? 4 : context->await_capacity * 2;
        VcAsyncAwait *awaits = realloc(context->awaits, capacity * sizeof(*awaits));
        if (awaits == NULL)
        {
            context_error(context, "out of memory while collecting async awaits");
            return false;
        }
        context->awaits = awaits;
        context->await_capacity = capacity;
    }
    const size_t index = context->await_count++;
    char generated[64];
    const int written = snprintf(generated, sizeof(generated), "_awaiter%zu", index);
    if (written < 0 || (size_t)written >= sizeof(generated))
        return false;
    char *field_name = copy_text(context->tree, generated);
    if (field_name == NULL)
        return false;
    context->awaits[index] = (VcAsyncAwait){
        operand, awaiter_type, result_type, result_declaration, assignment_target,
        return_result, field_name, protocol_binding
    };
    *await_index = index;
    return true;
}

static bool add_bound_async_await(VcAsyncContext *context, VcAstNode *operand,
    const VcSemanticBinding *await_binding, const VcAstNode *result_declaration,
    VcAstNode *assignment_target, bool return_result, VcSourceLocation location,
    size_t *await_index)
{
    if (operand == NULL || await_binding == NULL || !await_binding->has_await_protocol)
    {
        context_error(context,
            "async method '%s' could not resolve the custom awaiter protocol",
            context->method->as.method_declaration.name);
        return false;
    }

    VcAstTypeRef *awaiter_type = type_from_semantic(context,
        await_binding->awaiter_type, location);
    if (awaiter_type == NULL)
    {
        context_error(context,
            "async method '%s' could not materialize custom awaiter type",
            context->method->as.method_declaration.name);
        return false;
    }

    VcAstTypeRef *result_type = await_binding->type == VC_SEM_TYPE_VOID ? NULL :
        type_from_semantic(context, await_binding->type, location);
    if ((result_declaration != NULL || assignment_target != NULL || return_result) &&
        result_type == NULL)
    {
        context_error(context,
            "async method '%s' cannot consume a value from a void awaiter",
            context->method->as.method_declaration.name);
        return false;
    }

    return append_async_await(context, operand, awaiter_type, result_type,
        result_declaration, assignment_target, return_result, await_binding, await_index);
}

static bool add_async_await(VcAsyncContext *context, VcAstNode *await_expression,
    const VcAstNode *result_declaration, VcAstNode *assignment_target,
    bool return_result, size_t *await_index)
{
    if (await_expression == NULL || await_expression->kind != VC_AST_AWAIT_EXPRESSION ||
        await_expression->as.await_expression.operand == NULL)
    {
        context_error(context, "async method '%s' received an invalid await expression",
            context->method->as.method_declaration.name);
        return false;
    }
    const VcSemanticBinding *await_binding = vc_semantic_binding(
        context->semantic, await_expression);
    return add_bound_async_await(context, await_expression->as.await_expression.operand,
        await_binding, result_declaration, assignment_target, return_result,
        await_expression->location, await_index);
}

static int compile_async_statement(VcAsyncContext *context, VcAstNode *statement,
    int continuation, int break_target, int continue_target, int return_target,
    int exception_target);

static int compile_async_cleanup(VcAsyncContext *context, VcAstNode *cleanup,
    int continuation, int return_target, int exception_target);

static int compile_async_block_list(VcAsyncContext *context, const VcAstNodeList *statements,
    int continuation, int break_target, int continue_target, int return_target,
    int exception_target)
{
    int entry = continuation;
    for (size_t i = statements->count; i > 0; i--)
    {
        entry = compile_async_statement(context, statements->items[i - 1], entry,
            break_target, continue_target, return_target, exception_target);
        if (entry < 0)
            return -1;
    }
    return entry;
}

static int simple_async_goto_block(VcAsyncContext *context, VcAstNode *statement,
    int continuation, int exception_target)
{
    VcAsyncBlock *state = new_async_block(context, statement->location, exception_target);
    if (state == NULL)
        return -1;
    state->terminator = VC_ASYNC_GOTO;
    state->target = continuation;
    if (!vc_ast_node_list_push(context->tree, &state->statements, statement))
    {
        context_error(context, "out of memory while storing async statement");
        return -1;
    }
    return state->state;
}

static int compile_async_await_index(VcAsyncContext *context, size_t await_index,
    int continuation, VcSourceLocation location, int exception_target)
{
    VcAsyncBlock *resume = new_async_block(context, location, exception_target);
    if (resume == NULL)
        return -1;
    resume->terminator = VC_ASYNC_AWAIT_RESUME;
    resume->await_index = await_index;
    resume->target = continuation;
    const int resume_state = resume->state;

    VcAsyncBlock *start = new_async_block(context, location, exception_target);
    if (start == NULL)
        return -1;
    start->terminator = VC_ASYNC_AWAIT_START;
    start->await_index = await_index;
    start->target = resume_state;
    return start->state;
}

static int compile_async_await(VcAsyncContext *context, VcAstNode *await_expression,
    const VcAstNode *result_declaration, VcAstNode *assignment_target,
    bool return_result, int continuation, VcSourceLocation location, int exception_target)
{
    size_t await_index = 0;
    if (!add_async_await(context, await_expression, result_declaration, assignment_target,
            return_result, &await_index))
        return -1;
    return compile_async_await_index(context, await_index, continuation, location,
        exception_target);
}

static int compile_async_bound_await(VcAsyncContext *context, VcAstNode *operand,
    const VcSemanticBinding *await_binding, VcAstNode *assignment_target,
    int continuation, VcSourceLocation location, int exception_target)
{
    size_t await_index = 0;
    if (!add_bound_async_await(context, operand, await_binding, NULL, assignment_target,
            false, location, &await_index))
        return -1;
    return compile_async_await_index(context, await_index, continuation, location,
        exception_target);
}

static int compile_async_foreach_dispose_await(VcAsyncContext *context,
    const char *enumerator_field, const VcSemanticBinding *await_binding,
    int continuation, int exception_target, VcSourceLocation location)
{
    VcAstNode *dispose_call = member_call(context->tree, location,
        this_member(context->tree, location, enumerator_field), "DisposeAsync");
    if (dispose_call == NULL)
        return -1;
    return compile_async_bound_await(context, dispose_call, await_binding,
        NULL, continuation, location, exception_target);
}

static int compile_async_using_dispose_await(VcAsyncContext *context,
    const char *resource_field, const VcSemanticBinding *await_binding, bool skip_if_null,
    int continuation, int exception_target, VcSourceLocation location)
{
    VcAstNode *dispose_call = member_call(context->tree, location,
        this_member(context->tree, location, resource_field), "DisposeAsync");
    if (dispose_call == NULL)
        return -1;
    const int dispose_entry = compile_async_bound_await(context, dispose_call, await_binding,
        NULL, continuation, location, exception_target);
    if (dispose_entry < 0 || !skip_if_null)
        return dispose_entry;

    VcAsyncBlock *branch = new_async_block(context, location, exception_target);
    if (branch == NULL)
        return -1;
    branch->terminator = VC_ASYNC_BRANCH;
    branch->expression = binary(context->tree, location, VC_TOKEN_BANG_EQUAL,
        this_member(context->tree, location, resource_field),
        async_null_literal(context->tree, location));
    if (branch->expression == NULL)
        return -1;
    branch->target = dispose_entry;
    branch->false_target = continuation;
    return branch->state;
}

static int compile_async_using_cleanup(VcAsyncContext *context, bool is_async,
    VcAstNode *cleanup_statement, const char *resource_field,
    const VcSemanticBinding *dispose_await_binding, bool skip_if_null,
    int continuation, int return_target, int exception_target, VcSourceLocation location)
{
    if (is_async)
        return compile_async_using_dispose_await(context, resource_field,
            dispose_await_binding, skip_if_null, continuation, exception_target, location);
    return compile_async_cleanup(context, cleanup_statement, continuation, return_target,
        exception_target);
}

static bool expression_is_direct_await(VcAstNode *expression, VcAstNode **operand)
{
    if (expression == NULL || expression->kind != VC_AST_AWAIT_EXPRESSION ||
        expression->as.await_expression.operand == NULL)
        return false;
    if (operand != NULL)
        *operand = expression->as.await_expression.operand;
    return true;
}

static bool wrap_async_normalized_statement(VcAsyncContext *context,
    VcAstNode **statement_slot, VcAstNodeList *prefix)
{
    if (prefix == NULL || prefix->count == 0)
        return true;
    if (statement_slot == NULL || *statement_slot == NULL)
        return false;
    VcAstNode *original = *statement_slot;
    VcAstNode *wrapper = block(context->tree, original->location);
    if (wrapper == NULL)
        return false;
    for (size_t i = 0; i < prefix->count; i++)
        if (!block_push(context->tree, wrapper, prefix->items[i]))
            return false;
    if (!block_push(context->tree, wrapper, original))
        return false;
    *statement_slot = wrapper;
    return true;
}

static bool normalize_async_foundation_statement(VcAsyncContext *context,
    VcAstNode **statement_slot)
{
    if (statement_slot == NULL || *statement_slot == NULL)
        return true;
    VcAstNode *statement = *statement_slot;
    switch (statement->kind)
    {
        case VC_AST_BLOCK_STATEMENT:
            for (size_t i = 0; i < statement->as.block_statement.statements.count; i++)
                if (!normalize_async_foundation_statement(context,
                        &statement->as.block_statement.statements.items[i]))
                    return false;
            return true;

        case VC_AST_LOCAL_DECLARATION:
        {
            VcAstNode *operand = NULL;
            VcAstNode *initializer = statement->as.local_declaration.initializer;
            if (initializer == NULL || expression_is_direct_await(initializer, &operand) ||
                !expression_contains_kind(initializer, VC_AST_AWAIT_EXPRESSION))
                return true;
            VcAstNodeList prefix = {0};
            if (!normalize_async_foundation_expression(context,
                    &statement->as.local_declaration.initializer, &prefix))
                return false;
            return wrap_async_normalized_statement(context, statement_slot, &prefix);
        }

        case VC_AST_EXPRESSION_STATEMENT:
        {
            VcAstNode *expression = statement->as.expression_statement.expression;
            VcAstNode *operand = NULL;
            if (expression == NULL || expression_is_direct_await(expression, &operand) ||
                !expression_contains_kind(expression, VC_AST_AWAIT_EXPRESSION))
                return true;
            if (expression->kind == VC_AST_ASSIGNMENT_EXPRESSION)
            {
                if (expression_contains_kind(expression->as.assignment_expression.left,
                        VC_AST_AWAIT_EXPRESSION))
                    return true;
                if (expression_is_direct_await(expression->as.assignment_expression.right,
                        &operand))
                {
                    VcAstNodeList target_prefix = {0};
                    if (!async_normalize_assignment_target(context,
                            &expression->as.assignment_expression.left, &target_prefix, true))
                        return false;
                    return target_prefix.count == 0
                        ? true
                        : wrap_async_normalized_statement(context, statement_slot, &target_prefix);
                }
                VcAstNodeList prefix = {0};
                if (!normalize_async_foundation_expression(context,
                        &expression->as.assignment_expression.right, &prefix))
                    return false;
                return wrap_async_normalized_statement(context, statement_slot, &prefix);
            }
            VcAstNodeList prefix = {0};
            if (!normalize_async_foundation_expression(context,
                    &statement->as.expression_statement.expression, &prefix))
                return false;
            return wrap_async_normalized_statement(context, statement_slot, &prefix);
        }

        case VC_AST_RETURN_STATEMENT:
        {
            VcAstNode *operand = NULL;
            VcAstNode *expression = statement->as.return_statement.expression;
            if (expression == NULL || expression_is_direct_await(expression, &operand) ||
                !expression_contains_kind(expression, VC_AST_AWAIT_EXPRESSION))
                return true;
            VcAstNodeList prefix = {0};
            if (!normalize_async_foundation_expression(context,
                    &statement->as.return_statement.expression, &prefix))
                return false;
            return wrap_async_normalized_statement(context, statement_slot, &prefix);
        }

        case VC_AST_IF_STATEMENT:
        {
            if (!normalize_async_foundation_statement(context,
                    &statement->as.if_statement.then_statement) ||
                !normalize_async_foundation_statement(context,
                    &statement->as.if_statement.else_statement))
                return false;
            if (!expression_contains_kind(statement->as.if_statement.condition,
                    VC_AST_AWAIT_EXPRESSION))
                return true;
            VcAstNodeList prefix = {0};
            if (!normalize_async_foundation_expression(context,
                    &statement->as.if_statement.condition, &prefix))
                return false;
            return wrap_async_normalized_statement(context, statement_slot, &prefix);
        }

        case VC_AST_WHILE_STATEMENT:
        {
            if (!normalize_async_foundation_statement(context,
                    &statement->as.while_statement.body))
                return false;
            if (!expression_contains_kind(statement->as.while_statement.condition,
                    VC_AST_AWAIT_EXPRESSION))
                return true;
            VcAstNodeList prefix = {0};
            if (!normalize_async_foundation_expression(context,
                    &statement->as.while_statement.condition, &prefix))
                return false;
            VcAstNode *loop_body = block(context->tree, statement->location);
            if (loop_body == NULL)
                return false;
            for (size_t i = 0; i < prefix.count; i++)
                if (!block_push(context->tree, loop_body, prefix.items[i])) return false;
            VcAstNode *negated = unary(context->tree, statement->location, VC_TOKEN_BANG,
                statement->as.while_statement.condition);
            VcAstNode *stop = async_if_statement(context, statement->location, negated,
                break_statement(context->tree, statement->location), NULL);
            if (stop == NULL || !block_push(context->tree, loop_body, stop) ||
                !block_push(context->tree, loop_body, statement->as.while_statement.body))
                return false;
            statement->as.while_statement.condition = bool_literal(context->tree,
                statement->location, true);
            statement->as.while_statement.body = loop_body;
            return statement->as.while_statement.condition != NULL;
        }

        case VC_AST_DO_WHILE_STATEMENT:
        {
            if (!normalize_async_foundation_statement(context,
                    &statement->as.while_statement.body))
                return false;
            if (!expression_contains_kind(statement->as.while_statement.condition,
                    VC_AST_AWAIT_EXPRESSION))
                return true;
            VcAstNodeList condition_prefix = {0};
            if (!normalize_async_foundation_expression(context,
                    &statement->as.while_statement.condition, &condition_prefix))
                return false;
            VcAstTypeRef *bool_type = named_type(context->tree, statement->location, "bool");
            VcAsyncSpill *first = bool_type != NULL
                ? add_async_spill(context, bool_type, statement->location)
                : NULL;
            if (first == NULL)
                return false;

            VcAstNode *wrapper = block(context->tree, statement->location);
            VcAstNode *loop = vc_ast_new_node(context->tree,
                VC_AST_WHILE_STATEMENT, statement->location);
            VcAstNode *loop_body = block(context->tree, statement->location);
            VcAstNode *condition_body = block(context->tree, statement->location);
            if (wrapper == NULL || loop == NULL || loop_body == NULL || condition_body == NULL)
                return false;
            VcAstNode *first_target = async_state_member(context, statement->location,
                first->field_name);
            VcAstNode *initialize_first = first_target != NULL
                ? async_assignment_statement(context, statement->location, first_target,
                    bool_literal(context->tree, statement->location, true))
                : NULL;
            if (initialize_first == NULL || !block_push(context->tree, wrapper, initialize_first))
                return false;
            for (size_t i = 0; i < condition_prefix.count; i++)
                if (!block_push(context->tree, condition_body, condition_prefix.items[i])) return false;
            VcAstNode *negated_condition = unary(context->tree, statement->location, VC_TOKEN_BANG,
                statement->as.while_statement.condition);
            VcAstNode *stop = async_if_statement(context, statement->location,
                negated_condition, break_statement(context->tree, statement->location), NULL);
            if (stop == NULL || !block_push(context->tree, condition_body, stop))
                return false;
            VcAstNode *first_read = async_state_member(context, statement->location,
                first->field_name);
            VcAstNode *not_first = first_read != NULL
                ? unary(context->tree, statement->location, VC_TOKEN_BANG, first_read)
                : NULL;
            VcAstNode *condition_gate = not_first != NULL
                ? async_if_statement(context, statement->location, not_first, condition_body, NULL)
                : NULL;
            VcAstNode *clear_target = async_state_member(context, statement->location,
                first->field_name);
            VcAstNode *clear_first = clear_target != NULL
                ? async_assignment_statement(context, statement->location, clear_target,
                    bool_literal(context->tree, statement->location, false))
                : NULL;
            if (condition_gate == NULL || clear_first == NULL ||
                !block_push(context->tree, loop_body, condition_gate) ||
                !block_push(context->tree, loop_body, clear_first) ||
                !block_push(context->tree, loop_body, statement->as.while_statement.body))
                return false;
            loop->as.while_statement.condition = bool_literal(context->tree,
                statement->location, true);
            loop->as.while_statement.body = loop_body;
            if (loop->as.while_statement.condition == NULL || !block_push(context->tree, wrapper, loop))
                return false;
            *statement_slot = wrapper;
            return true;
        }

        case VC_AST_FOR_STATEMENT:
        {
            if (!normalize_async_foundation_statement(context,
                    &statement->as.for_statement.initializer) ||
                !normalize_async_foundation_statement(context,
                    &statement->as.for_statement.body))
                return false;
            const bool condition_has_await = expression_contains_kind(
                statement->as.for_statement.condition, VC_AST_AWAIT_EXPRESSION);
            const bool increment_has_await = expression_contains_kind(
                statement->as.for_statement.increment, VC_AST_AWAIT_EXPRESSION);
            if (!condition_has_await && !increment_has_await)
                return true;

            VcAstNodeList condition_prefix = {0};
            VcAstNodeList increment_prefix = {0};
            if (condition_has_await && !normalize_async_foundation_expression(context,
                    &statement->as.for_statement.condition, &condition_prefix))
                return false;
            VcAstNode *increment_operand = NULL;
            if (increment_has_await &&
                !expression_is_direct_await(statement->as.for_statement.increment, &increment_operand) &&
                !normalize_async_foundation_expression(context,
                    &statement->as.for_statement.increment, &increment_prefix))
                return false;

            VcAstTypeRef *bool_type = named_type(context->tree, statement->location, "bool");
            VcAsyncSpill *first = bool_type != NULL
                ? add_async_spill(context, bool_type, statement->location)
                : NULL;
            if (first == NULL)
                return false;
            VcAstNode *wrapper = block(context->tree, statement->location);
            VcAstNode *loop = vc_ast_new_node(context->tree,
                VC_AST_WHILE_STATEMENT, statement->location);
            VcAstNode *loop_body = block(context->tree, statement->location);
            VcAstNode *increment_body = block(context->tree, statement->location);
            if (wrapper == NULL || loop == NULL || loop_body == NULL || increment_body == NULL)
                return false;
            if (statement->as.for_statement.initializer != NULL &&
                !block_push(context->tree, wrapper, statement->as.for_statement.initializer))
                return false;
            VcAstNode *first_target = async_state_member(context, statement->location,
                first->field_name);
            VcAstNode *initialize_first = first_target != NULL
                ? async_assignment_statement(context, statement->location, first_target,
                    bool_literal(context->tree, statement->location, true))
                : NULL;
            if (initialize_first == NULL || !block_push(context->tree, wrapper, initialize_first))
                return false;
            for (size_t i = 0; i < increment_prefix.count; i++)
                if (!block_push(context->tree, increment_body, increment_prefix.items[i])) return false;
            if (statement->as.for_statement.increment != NULL &&
                !block_push(context->tree, increment_body,
                    expression_statement(context->tree,
                        statement->as.for_statement.increment->location,
                        statement->as.for_statement.increment)))
                return false;
            VcAstNode *first_read = async_state_member(context, statement->location,
                first->field_name);
            VcAstNode *not_first = first_read != NULL
                ? unary(context->tree, statement->location, VC_TOKEN_BANG, first_read)
                : NULL;
            VcAstNode *increment_gate = not_first != NULL
                ? async_if_statement(context, statement->location, not_first, increment_body, NULL)
                : NULL;
            VcAstNode *clear_target = async_state_member(context, statement->location,
                first->field_name);
            VcAstNode *clear_first = clear_target != NULL
                ? async_assignment_statement(context, statement->location, clear_target,
                    bool_literal(context->tree, statement->location, false))
                : NULL;
            if (increment_gate == NULL || clear_first == NULL ||
                !block_push(context->tree, loop_body, increment_gate) ||
                !block_push(context->tree, loop_body, clear_first))
                return false;
            for (size_t i = 0; i < condition_prefix.count; i++)
                if (!block_push(context->tree, loop_body, condition_prefix.items[i])) return false;
            if (statement->as.for_statement.condition != NULL)
            {
                VcAstNode *negated = unary(context->tree, statement->location, VC_TOKEN_BANG,
                    statement->as.for_statement.condition);
                VcAstNode *stop = async_if_statement(context, statement->location, negated,
                    break_statement(context->tree, statement->location), NULL);
                if (stop == NULL || !block_push(context->tree, loop_body, stop))
                    return false;
            }
            if (!block_push(context->tree, loop_body, statement->as.for_statement.body))
                return false;
            loop->as.while_statement.condition = bool_literal(context->tree,
                statement->location, true);
            loop->as.while_statement.body = loop_body;
            if (loop->as.while_statement.condition == NULL || !block_push(context->tree, wrapper, loop))
                return false;
            *statement_slot = wrapper;
            return true;
        }

        case VC_AST_TRY_STATEMENT:
            if (!normalize_async_foundation_statement(context,
                    &statement->as.try_statement.try_block))
                return false;
            for (size_t i = 0; i < statement->as.try_statement.catches.count; i++)
                if (!normalize_async_foundation_statement(context,
                        &statement->as.try_statement.catches.items[i]))
                    return false;
            return normalize_async_foundation_statement(context,
                &statement->as.try_statement.finally_block);

        case VC_AST_CATCH_CLAUSE:
            return normalize_async_foundation_statement(context,
                &statement->as.catch_clause.body);

        case VC_AST_USING_STATEMENT:
            return normalize_async_foundation_statement(context,
                       &statement->as.using_statement.declaration) &&
                normalize_async_foundation_statement(context,
                       &statement->as.using_statement.body);

        case VC_AST_SWITCH_STATEMENT:
        {
            for (size_t i = 0; i < statement->as.switch_statement.sections.count; i++)
                if (!normalize_async_foundation_statement(context,
                        &statement->as.switch_statement.sections.items[i]))
                    return false;
            if (!expression_contains_kind(statement->as.switch_statement.expression,
                    VC_AST_AWAIT_EXPRESSION))
                return true;
            VcAstNodeList prefix = {0};
            if (!normalize_async_foundation_expression(context,
                    &statement->as.switch_statement.expression, &prefix))
                return false;
            return wrap_async_normalized_statement(context, statement_slot, &prefix);
        }

        case VC_AST_SWITCH_SECTION:
            for (size_t i = 0; i < statement->as.switch_section.statements.count; i++)
                if (!normalize_async_foundation_statement(context,
                        &statement->as.switch_section.statements.items[i]))
                    return false;
            return true;

        case VC_AST_THROW_STATEMENT:
        {
            if (statement->as.throw_statement.expression == NULL ||
                !expression_contains_kind(statement->as.throw_statement.expression,
                    VC_AST_AWAIT_EXPRESSION))
                return true;
            VcAstNodeList prefix = {0};
            if (!normalize_async_foundation_expression(context,
                    &statement->as.throw_statement.expression, &prefix))
                return false;
            return wrap_async_normalized_statement(context, statement_slot, &prefix);
        }

        case VC_AST_FOREACH_STATEMENT:
        {
            if (!normalize_async_foundation_statement(context,
                    &statement->as.foreach_statement.body))
                return false;
            if (!statement->as.foreach_statement.is_await &&
                !statement_contains_kind(statement, VC_AST_AWAIT_EXPRESSION) &&
                !statement_contains_kind(statement, VC_AST_RETURN_STATEMENT))
                return true;
            if (add_async_foreach(context, statement) == NULL)
                return false;
            if (!expression_contains_kind(statement->as.foreach_statement.collection,
                    VC_AST_AWAIT_EXPRESSION))
                return true;
            VcAstNodeList prefix = {0};
            if (!normalize_async_foundation_expression(context,
                    &statement->as.foreach_statement.collection, &prefix))
                return false;
            return wrap_async_normalized_statement(context, statement_slot, &prefix);
        }

        /* Suspension while holding a monitor remains illegal. */
        case VC_AST_LOCK_STATEMENT:
        case VC_AST_FIXED_STATEMENT:
        default:
            return true;
    }
}

static bool async_nested_control_requires_split(const VcAstNode *statement,
    int break_target, int continue_target)
{
    return statement_contains_kind(statement, VC_AST_AWAIT_EXPRESSION) ||
        statement_contains_kind(statement, VC_AST_RETURN_STATEMENT) ||
        (break_target >= 0 && statement_contains_kind(statement, VC_AST_BREAK_STATEMENT)) ||
        (continue_target >= 0 && statement_contains_kind(statement, VC_AST_CONTINUE_STATEMENT));
}

static VcAstNode *make_async_dispose_statement(VcAsyncContext *context,
    VcSourceLocation location, const char *field_name, bool skip_if_null)
{
    VcAstNode *call_node = member_call(context->tree, location,
        this_member(context->tree, location, field_name), "Dispose");
    VcAstNode *call_statement = call_node != NULL
        ? expression_statement(context->tree, location, call_node)
        : NULL;
    if (!skip_if_null)
        return call_statement;

    VcAstNode *condition = binary(context->tree, location, VC_TOKEN_BANG_EQUAL,
        this_member(context->tree, location, field_name),
        async_null_literal(context->tree, location));
    VcAstNode *if_node = vc_ast_new_node(context->tree, VC_AST_IF_STATEMENT, location);
    if (condition == NULL || if_node == NULL || call_statement == NULL)
        return NULL;
    if_node->as.if_statement.condition = condition;
    if_node->as.if_statement.then_statement = call_statement;
    return if_node;
}

static int compile_async_cleanup(VcAsyncContext *context, VcAstNode *cleanup,
    int continuation, int return_target, int exception_target)
{
    if (cleanup == NULL)
        return continuation;
    return compile_async_statement(context, cleanup, continuation, -1, -1,
        return_target, exception_target);
}

static bool rewrite_async_rethrows(VcAsyncContext *context, VcAstNode *node,
    const char *catch_field)
{
    if (node == NULL)
        return true;
    switch (node->kind)
    {
        case VC_AST_BLOCK_STATEMENT:
            for (size_t i = 0; i < node->as.block_statement.statements.count; i++)
                if (!rewrite_async_rethrows(context,
                        node->as.block_statement.statements.items[i], catch_field)) return false;
            return true;
        case VC_AST_THROW_STATEMENT:
            if (node->as.throw_statement.expression == NULL && catch_field != NULL)
            {
                VcAstNode *self = identifier(context->tree, node->location,
                    "__voidc_async_self");
                node->as.throw_statement.expression = self != NULL
                    ? member_access(context->tree, node->location, self, catch_field)
                    : NULL;
                return node->as.throw_statement.expression != NULL;
            }
            return true;
        case VC_AST_TRY_STATEMENT:
            if (!rewrite_async_rethrows(context, node->as.try_statement.try_block, catch_field))
                return false;
            /* Nested catch bodies establish their own rethrow exception and are rewritten
               when that catch is compiled. A nested finally remains in the lexical catch. */
            return rewrite_async_rethrows(context, node->as.try_statement.finally_block, catch_field);
        case VC_AST_IF_STATEMENT:
            return rewrite_async_rethrows(context, node->as.if_statement.then_statement, catch_field) &&
                rewrite_async_rethrows(context, node->as.if_statement.else_statement, catch_field);
        case VC_AST_WHILE_STATEMENT:
        case VC_AST_DO_WHILE_STATEMENT:
            return rewrite_async_rethrows(context, node->as.while_statement.body, catch_field);
        case VC_AST_FOR_STATEMENT:
            return rewrite_async_rethrows(context, node->as.for_statement.initializer, catch_field) &&
                rewrite_async_rethrows(context, node->as.for_statement.body, catch_field);
        case VC_AST_USING_STATEMENT:
            return rewrite_async_rethrows(context, node->as.using_statement.body, catch_field);
        case VC_AST_LOCK_STATEMENT:
            return rewrite_async_rethrows(context, node->as.lock_statement.body, catch_field);
        case VC_AST_FIXED_STATEMENT:
            return rewrite_async_rethrows(context, node->as.fixed_statement.body, catch_field);
        case VC_AST_FOREACH_STATEMENT:
            return rewrite_async_rethrows(context, node->as.foreach_statement.body, catch_field);
        case VC_AST_SWITCH_STATEMENT:
            for (size_t i = 0; i < node->as.switch_statement.sections.count; i++)
                if (!rewrite_async_rethrows(context,
                        node->as.switch_statement.sections.items[i], catch_field)) return false;
            return true;
        case VC_AST_SWITCH_SECTION:
            for (size_t i = 0; i < node->as.switch_section.statements.count; i++)
                if (!rewrite_async_rethrows(context,
                        node->as.switch_section.statements.items[i], catch_field)) return false;
            return true;
        default:
            return true;
    }
}

static int compile_async_try(VcAsyncContext *context, VcAstNode *statement,
    int continuation, int break_target, int continue_target, int return_target,
    int exception_target)
{
    VcAstNode *finally_block = statement->as.try_statement.finally_block;
    int normal_target = continuation;
    int break_cleanup = break_target;
    int continue_cleanup = continue_target;
    int return_cleanup = return_target;
    int exceptional_cleanup = exception_target;

    if (finally_block != NULL)
    {
        VcAstNode *rethrow = vc_ast_new_node(context->tree,
            VC_AST_THROW_STATEMENT, statement->location);
        if (rethrow == NULL)
            return -1;
        rethrow->as.throw_statement.expression = this_member(context->tree,
            statement->location, "_pendingException");
        if (rethrow->as.throw_statement.expression == NULL)
            return -1;
        const int rethrow_state = simple_async_goto_block(context, rethrow,
            context->finish_state, exception_target);
        if (rethrow_state < 0)
            return -1;

        exceptional_cleanup = compile_async_cleanup(context, finally_block,
            rethrow_state, return_target, exception_target);
        normal_target = compile_async_cleanup(context, finally_block,
            continuation, return_target, exception_target);
        if (exceptional_cleanup < 0 || normal_target < 0)
            return -1;
        if (break_target >= 0)
        {
            break_cleanup = compile_async_cleanup(context, finally_block,
                break_target, return_target, exception_target);
            if (break_cleanup < 0) return -1;
        }
        if (continue_target >= 0)
        {
            continue_cleanup = compile_async_cleanup(context, finally_block,
                continue_target, return_target, exception_target);
            if (continue_cleanup < 0) return -1;
        }
        return_cleanup = compile_async_cleanup(context, finally_block,
            return_target, return_target, exception_target);
        if (return_cleanup < 0)
            return -1;
    }

    const size_t catch_count = statement->as.try_statement.catches.count;
    int dispatch_state = -1;
    if (catch_count != 0)
    {
        VcAsyncBlock *dispatch = new_async_block(context, statement->location, exceptional_cleanup);
        if (dispatch == NULL)
            return -1;
        dispatch_state = dispatch->state;
        /* Recursive catch lowering may realloc context->blocks; retain the stable
         * state/index rather than keeping this block pointer across recursion. */
        dispatch->terminator = VC_ASYNC_CATCH_DISPATCH;
        dispatch->try_source = statement;
        dispatch->catch_target_count = catch_count;
        dispatch->catch_targets = calloc(catch_count, sizeof(*dispatch->catch_targets));
        if (dispatch->catch_targets == NULL)
        {
            context_error(context, "out of memory while building async catch dispatch");
            return -1;
        }
        context->has_exception_regions = true;
    }

    for (size_t i = 0; i < catch_count; i++)
    {
        VcAstNode *clause = statement->as.try_statement.catches.items[i];
        VcAsyncCatch *catch_info = add_async_catch(context, clause);
        if (catch_info == NULL ||
            !rewrite_async_rethrows(context, clause->as.catch_clause.body, catch_info->field_name))
            return -1;
        const int entry = compile_async_statement(context, clause->as.catch_clause.body,
            normal_target, break_cleanup, continue_cleanup, return_cleanup,
            exceptional_cleanup);
        if (entry < 0)
            return -1;
        context->blocks[dispatch_state].catch_targets[i] = entry;
    }

    const int try_exception_target = dispatch_state >= 0 ? dispatch_state : exceptional_cleanup;
    if (try_exception_target >= 0 || finally_block != NULL || catch_count != 0)
        context->has_exception_regions = true;
    return compile_async_statement(context, statement->as.try_statement.try_block,
        normal_target, break_cleanup, continue_cleanup, return_cleanup,
        try_exception_target);
}

static int compile_async_using(VcAsyncContext *context, VcAstNode *statement,
    int continuation, int break_target, int continue_target, int return_target,
    int exception_target)
{
    const VcSemanticBinding *binding = vc_semantic_binding(context->semantic, statement);
    if (binding == NULL || !binding->has_using)
    {
        context_error(context, "async using resource binding is unavailable");
        return -1;
    }
    const VcSemanticBinding *dispose_await_binding = NULL;
    if (binding->using_async)
    {
        if (statement->as.using_statement.dispose_await_protocol == NULL)
        {
            context_error(context, "await using DisposeAsync() await protocol is unavailable");
            return -1;
        }
        dispose_await_binding = vc_semantic_binding(context->semantic,
            statement->as.using_statement.dispose_await_protocol);
        if (dispose_await_binding == NULL || !dispose_await_binding->has_await_protocol ||
            dispose_await_binding->type != VC_SEM_TYPE_VOID)
        {
            context_error(context, "await using DisposeAsync() await protocol is invalid");
            return -1;
        }
    }

    const char *field_name = NULL;
    VcAstNode *initializer_statement = NULL;
    if (statement->as.using_statement.declaration != NULL)
    {
        VcAstNode *declaration = statement->as.using_statement.declaration;
        VcAsyncLocal *local = local_for_declaration(context, declaration);
        if (local == NULL)
        {
            context_error(context, "async using declaration local is unavailable");
            return -1;
        }
        local->promoted = true;
        field_name = local->name;
        declaration->as.local_declaration.is_using_resource = false;
        initializer_statement = declaration;
    }
    else
    {
        VcAsyncResource *resource = add_async_resource(context, statement);
        if (resource == NULL)
            return -1;
        field_name = resource->field_name;
        VcAstNode *assign = this_member_assignment(context->tree, statement->location,
            field_name, statement->as.using_statement.expression);
        initializer_statement = assign != NULL
            ? expression_statement(context->tree, statement->location, assign)
            : NULL;
    }
    if (field_name == NULL || initializer_statement == NULL)
        return -1;

    VcAstNode *cleanup_statement = NULL;
    if (!binding->using_async)
    {
        cleanup_statement = make_async_dispose_statement(context,
            statement->location, field_name, binding->using_skip_dispose_if_null);
        if (cleanup_statement == NULL)
            return -1;
    }

    int normal_cleanup = compile_async_using_cleanup(context, binding->using_async,
        cleanup_statement, field_name, dispose_await_binding,
        binding->using_skip_dispose_if_null, continuation, return_target, exception_target,
        statement->location);
    int exceptional_cleanup = -1;
    VcAstNode *rethrow = vc_ast_new_node(context->tree,
        VC_AST_THROW_STATEMENT, statement->location);
    if (rethrow == NULL)
        return -1;
    rethrow->as.throw_statement.expression = this_member(context->tree,
        statement->location, "_pendingException");
    if (rethrow->as.throw_statement.expression == NULL)
        return -1;
    const int rethrow_state = simple_async_goto_block(context, rethrow,
        context->finish_state, exception_target);
    if (rethrow_state < 0)
        return -1;
    exceptional_cleanup = compile_async_using_cleanup(context, binding->using_async,
        cleanup_statement, field_name, dispose_await_binding,
        binding->using_skip_dispose_if_null, rethrow_state, return_target, exception_target,
        statement->location);
    if (normal_cleanup < 0 || exceptional_cleanup < 0)
        return -1;

    int break_cleanup = break_target;
    if (break_target >= 0)
    {
        break_cleanup = compile_async_using_cleanup(context, binding->using_async,
            cleanup_statement, field_name, dispose_await_binding,
            binding->using_skip_dispose_if_null, break_target, return_target,
            exception_target, statement->location);
        if (break_cleanup < 0) return -1;
    }
    int continue_cleanup = continue_target;
    if (continue_target >= 0)
    {
        continue_cleanup = compile_async_using_cleanup(context, binding->using_async,
            cleanup_statement, field_name, dispose_await_binding,
            binding->using_skip_dispose_if_null, continue_target, return_target,
            exception_target, statement->location);
        if (continue_cleanup < 0) return -1;
    }
    const int return_cleanup = compile_async_using_cleanup(context, binding->using_async,
        cleanup_statement, field_name, dispose_await_binding,
        binding->using_skip_dispose_if_null, return_target, return_target, exception_target,
        statement->location);
    if (return_cleanup < 0)
        return -1;

    context->has_exception_regions = true;
    const int body_entry = compile_async_statement(context,
        statement->as.using_statement.body, normal_cleanup, break_cleanup,
        continue_cleanup, return_cleanup, exceptional_cleanup);
    if (body_entry < 0)
        return -1;
    return simple_async_goto_block(context, initializer_statement, body_entry,
        exception_target);
}

static int compile_async_statement(VcAsyncContext *context, VcAstNode *statement,
    int continuation, int break_target, int continue_target, int return_target,
    int exception_target)
{
    if (statement == NULL)
        return continuation;

    switch (statement->kind)
    {
        case VC_AST_BLOCK_STATEMENT:
            return compile_async_block_list(context, &statement->as.block_statement.statements,
                continuation, break_target, continue_target, return_target, exception_target);

        case VC_AST_LOCAL_DECLARATION:
        {
            VcAstNode *operand = NULL;
            if (expression_is_direct_await(statement->as.local_declaration.initializer, &operand))
                return compile_async_await(context, statement->as.local_declaration.initializer,
                    statement, NULL, false,
                    continuation, statement->location, exception_target);
            if (statement_contains_kind(statement, VC_AST_AWAIT_EXPRESSION))
            {
                context_error(context,
                    "await must be a direct local initializer, assignment, expression, or return in this async lowering stage");
                return -1;
            }
            return simple_async_goto_block(context, statement, continuation, exception_target);
        }

        case VC_AST_EXPRESSION_STATEMENT:
        {
            VcAstNode *expression = statement->as.expression_statement.expression;
            VcAstNode *operand = NULL;
            if (expression_is_direct_await(expression, &operand))
                return compile_async_await(context, expression, NULL, NULL, false,
                    continuation, statement->location, exception_target);
            if (expression != NULL && expression->kind == VC_AST_ASSIGNMENT_EXPRESSION &&
                expression_is_direct_await(expression->as.assignment_expression.right, &operand))
                return compile_async_await(context, expression->as.assignment_expression.right, NULL,
                    expression->as.assignment_expression.left, false, continuation,
                    statement->location, exception_target);
            if (statement_contains_kind(statement, VC_AST_AWAIT_EXPRESSION))
            {
                context_error(context,
                    "await must be a direct local initializer, assignment, expression, or return in this async lowering stage");
                return -1;
            }
            return simple_async_goto_block(context, statement, continuation, exception_target);
        }

        case VC_AST_RETURN_STATEMENT:
        {
            VcAstNode *operand = NULL;
            if (expression_is_direct_await(statement->as.return_statement.expression, &operand))
                return compile_async_await(context, statement->as.return_statement.expression,
                    NULL, NULL, true,
                    return_target, statement->location, exception_target);
            if (statement_contains_kind(statement, VC_AST_AWAIT_EXPRESSION))
            {
                context_error(context,
                    "await in a return expression must be the direct returned expression in this async lowering stage");
                return -1;
            }
            VcAsyncBlock *state = new_async_block(context, statement->location, exception_target);
            if (state == NULL)
                return -1;
            state->terminator = VC_ASYNC_RETURN;
            state->expression = statement->as.return_statement.expression;
            state->target = return_target;
            return state->state;
        }

        case VC_AST_IF_STATEMENT:
        {
            if (!async_nested_control_requires_split(statement, break_target, continue_target))
                return simple_async_goto_block(context, statement, continuation, exception_target);
            if (expression_contains_kind(statement->as.if_statement.condition, VC_AST_AWAIT_EXPRESSION))
            {
                context_error(context, "await in an if condition is not supported in this async lowering stage");
                return -1;
            }
            const int then_entry = compile_async_statement(context,
                statement->as.if_statement.then_statement, continuation, break_target,
                continue_target, return_target, exception_target);
            if (then_entry < 0) return -1;
            const int else_entry = statement->as.if_statement.else_statement != NULL
                ? compile_async_statement(context, statement->as.if_statement.else_statement,
                    continuation, break_target, continue_target, return_target, exception_target)
                : continuation;
            if (else_entry < 0) return -1;
            VcAsyncBlock *branch = new_async_block(context, statement->location, exception_target);
            if (branch == NULL) return -1;
            branch->terminator = VC_ASYNC_BRANCH;
            branch->expression = statement->as.if_statement.condition;
            branch->target = then_entry;
            branch->false_target = else_entry;
            return branch->state;
        }

        case VC_AST_WHILE_STATEMENT:
        {
            if (!statement->as.foreach_statement.is_await &&
                !statement_contains_kind(statement, VC_AST_AWAIT_EXPRESSION) &&
                !statement_contains_kind(statement, VC_AST_RETURN_STATEMENT))
                return simple_async_goto_block(context, statement, continuation, exception_target);
            if (expression_contains_kind(statement->as.while_statement.condition, VC_AST_AWAIT_EXPRESSION))
            {
                context_error(context, "await in a loop condition is not supported in this async lowering stage");
                return -1;
            }
            VcAsyncBlock *condition = new_async_block(context, statement->location, exception_target);
            if (condition == NULL) return -1;
            const int condition_state = condition->state;
            const int body_entry = compile_async_statement(context, statement->as.while_statement.body,
                condition_state, continuation, condition_state, return_target, exception_target);
            if (body_entry < 0) return -1;
            condition = &context->blocks[condition_state];
            condition->terminator = VC_ASYNC_BRANCH;
            condition->expression = statement->as.while_statement.condition;
            condition->target = body_entry;
            condition->false_target = continuation;
            return condition_state;
        }

        case VC_AST_DO_WHILE_STATEMENT:
        {
            if (!statement_contains_kind(statement, VC_AST_AWAIT_EXPRESSION) &&
                !statement_contains_kind(statement, VC_AST_RETURN_STATEMENT))
                return simple_async_goto_block(context, statement, continuation, exception_target);
            if (expression_contains_kind(statement->as.while_statement.condition, VC_AST_AWAIT_EXPRESSION))
            {
                context_error(context, "await in a loop condition is not supported in this async lowering stage");
                return -1;
            }
            VcAsyncBlock *condition = new_async_block(context, statement->location, exception_target);
            if (condition == NULL) return -1;
            const int condition_state = condition->state;
            const int body_entry = compile_async_statement(context, statement->as.while_statement.body,
                condition_state, continuation, condition_state, return_target, exception_target);
            if (body_entry < 0) return -1;
            condition = &context->blocks[condition_state];
            condition->terminator = VC_ASYNC_BRANCH;
            condition->expression = statement->as.while_statement.condition;
            condition->target = body_entry;
            condition->false_target = continuation;
            return body_entry;
        }

        case VC_AST_FOR_STATEMENT:
        {
            if (!statement_contains_kind(statement, VC_AST_AWAIT_EXPRESSION) &&
                !statement_contains_kind(statement, VC_AST_RETURN_STATEMENT))
                return simple_async_goto_block(context, statement, continuation, exception_target);
            if (expression_contains_kind(statement->as.for_statement.condition, VC_AST_AWAIT_EXPRESSION) ||
                expression_contains_kind(statement->as.for_statement.increment, VC_AST_AWAIT_EXPRESSION))
            {
                context_error(context, "await in a for condition or increment is not supported in this async lowering stage");
                return -1;
            }
            VcAsyncBlock *condition = new_async_block(context, statement->location, exception_target);
            if (condition == NULL) return -1;
            const int condition_state = condition->state;
            int increment_state = condition_state;
            if (statement->as.for_statement.increment != NULL)
            {
                VcAstNode *increment = expression_statement(context->tree,
                    statement->as.for_statement.increment->location,
                    statement->as.for_statement.increment);
                if (increment == NULL) return -1;
                increment_state = simple_async_goto_block(context, increment,
                    condition_state, exception_target);
                if (increment_state < 0) return -1;
            }
            const int body_entry = compile_async_statement(context, statement->as.for_statement.body,
                increment_state, continuation, increment_state, return_target, exception_target);
            if (body_entry < 0) return -1;
            condition = &context->blocks[condition_state];
            condition->terminator = VC_ASYNC_BRANCH;
            condition->expression = statement->as.for_statement.condition != NULL
                ? statement->as.for_statement.condition
                : bool_literal(context->tree, statement->location, true);
            if (condition->expression == NULL) return -1;
            condition->target = body_entry;
            condition->false_target = continuation;
            return statement->as.for_statement.initializer != NULL
                ? compile_async_statement(context, statement->as.for_statement.initializer,
                    condition_state, break_target, continue_target, return_target, exception_target)
                : condition_state;
        }

        case VC_AST_SWITCH_STATEMENT:
        {
            if (!statement_contains_kind(statement, VC_AST_AWAIT_EXPRESSION) &&
                !statement_contains_kind(statement, VC_AST_RETURN_STATEMENT) &&
                !(continue_target >= 0 && statement_contains_kind(statement, VC_AST_CONTINUE_STATEMENT)))
                return simple_async_goto_block(context, statement, continuation, exception_target);
            if (expression_contains_kind(statement->as.switch_statement.expression, VC_AST_AWAIT_EXPRESSION))
            {
                context_error(context, "await in a switch expression is not supported in this async lowering stage");
                return -1;
            }
            const size_t section_count = statement->as.switch_statement.sections.count;
            int *targets = section_count == 0 ? NULL : malloc(section_count * sizeof(*targets));
            if (section_count != 0 && targets == NULL)
                return -1;
            for (size_t i = 0; i < section_count; i++)
            {
                const VcAstNode *section = statement->as.switch_statement.sections.items[i];
                targets[i] = compile_async_block_list(context, &section->as.switch_section.statements,
                    continuation, continuation, continue_target, return_target, exception_target);
                if (targets[i] < 0)
                {
                    free(targets);
                    return -1;
                }
            }
            VcAsyncBlock *dispatch = new_async_block(context, statement->location, exception_target);
            if (dispatch == NULL)
            {
                free(targets);
                return -1;
            }
            dispatch->terminator = VC_ASYNC_SWITCH;
            dispatch->expression = statement->as.switch_statement.expression;
            dispatch->target = continuation;
            dispatch->switch_source = statement;
            dispatch->switch_targets = targets;
            dispatch->switch_target_count = section_count;
            return dispatch->state;
        }

        case VC_AST_BREAK_STATEMENT:
        {
            if (break_target < 0)
            {
                context_error(context, "'break' is only valid inside async-lowered loop or switch flow");
                return -1;
            }
            VcAsyncBlock *jump = new_async_block(context, statement->location, exception_target);
            if (jump == NULL) return -1;
            jump->terminator = VC_ASYNC_GOTO;
            jump->target = break_target;
            return jump->state;
        }

        case VC_AST_CONTINUE_STATEMENT:
        {
            if (continue_target < 0)
            {
                context_error(context, "'continue' is only valid inside async-lowered loop flow");
                return -1;
            }
            VcAsyncBlock *jump = new_async_block(context, statement->location, exception_target);
            if (jump == NULL) return -1;
            jump->terminator = VC_ASYNC_GOTO;
            jump->target = continue_target;
            return jump->state;
        }

        case VC_AST_TRY_STATEMENT:
            if (!async_nested_control_requires_split(statement, break_target, continue_target))
                return simple_async_goto_block(context, statement, continuation, exception_target);
            return compile_async_try(context, statement, continuation, break_target,
                continue_target, return_target, exception_target);

        case VC_AST_USING_STATEMENT:
            if (!async_nested_control_requires_split(statement, break_target, continue_target))
                return simple_async_goto_block(context, statement, continuation, exception_target);
            return compile_async_using(context, statement, continuation, break_target,
                continue_target, return_target, exception_target);

        case VC_AST_FOREACH_STATEMENT:
        {
            if (!statement_contains_kind(statement, VC_AST_AWAIT_EXPRESSION) &&
                !statement_contains_kind(statement, VC_AST_RETURN_STATEMENT))
                return simple_async_goto_block(context, statement, continuation, exception_target);

            VcAsyncForeach *foreach_info = async_foreach_for_statement(context, statement);
            if (foreach_info == NULL)
            {
                foreach_info = add_async_foreach(context, statement);
                if (foreach_info == NULL)
                    return -1;
            }
            VcAsyncLocal *item_local = add_promoted_async_local(context, statement,
                foreach_info->item_type, "foreach_item");
            if (item_local == NULL)
                return -1;

            if (foreach_info->array_foreach)
            {
                VcAstNode *collection_marker = synthetic_local_marker(context, statement->location);
                VcAstNode *index_marker = synthetic_local_marker(context, statement->location);
                VcAstTypeRef *int_type = named_type(context->tree, statement->location, "int");
                VcAsyncLocal *collection_local = collection_marker != NULL
                    ? add_promoted_async_local(context, collection_marker,
                        foreach_info->collection_type, "foreach_collection") : NULL;
                VcAsyncLocal *index_local = index_marker != NULL && int_type != NULL
                    ? add_promoted_async_local(context, index_marker, int_type, "foreach_index") : NULL;
                if (collection_local == NULL || index_local == NULL)
                    return -1;

                VcAsyncBlock *condition = new_async_block(context, statement->location,
                    exception_target);
                if (condition == NULL)
                    return -1;
                const int condition_state = condition->state;

                VcAstNode *increment_value = binary(context->tree, statement->location,
                    VC_TOKEN_PLUS,
                    this_member(context->tree, statement->location, index_local->name),
                    number_literal(context->tree, statement->location, 1));
                VcAstNode *increment_assign = increment_value != NULL
                    ? this_member_assignment(context->tree, statement->location,
                        index_local->name, increment_value) : NULL;
                VcAstNode *increment_statement = increment_assign != NULL
                    ? expression_statement(context->tree, statement->location, increment_assign) : NULL;
                if (increment_statement == NULL)
                    return -1;
                const int increment_state = simple_async_goto_block(context,
                    increment_statement, condition_state, exception_target);
                if (increment_state < 0)
                    return -1;

                const int body_entry = compile_async_statement(context,
                    statement->as.foreach_statement.body, increment_state, continuation,
                    increment_state, return_target, exception_target);
                if (body_entry < 0)
                    return -1;

                VcAstNode *item_value = foreach_array_item_expression(context,
                    statement->location, collection_local->name, index_local->name,
                    foreach_info->array_rank);
                VcAstNode *item_assign = item_value != NULL
                    ? this_member_assignment(context->tree, statement->location,
                        item_local->name, item_value) : NULL;
                VcAstNode *item_statement = item_assign != NULL
                    ? expression_statement(context->tree, statement->location, item_assign) : NULL;
                if (item_statement == NULL)
                    return -1;
                const int item_state = simple_async_goto_block(context, item_statement,
                    body_entry, exception_target);
                if (item_state < 0)
                    return -1;

                condition = &context->blocks[condition_state];
                condition->terminator = VC_ASYNC_BRANCH;
                condition->expression = binary(context->tree, statement->location,
                    VC_TOKEN_LESS,
                    this_member(context->tree, statement->location, index_local->name),
                    member_access(context->tree, statement->location,
                        this_member(context->tree, statement->location, collection_local->name),
                        "Length"));
                if (condition->expression == NULL)
                    return -1;
                condition->target = item_state;
                condition->false_target = continuation;

                VcAstNode *index_init = this_member_assignment(context->tree,
                    statement->location, index_local->name,
                    number_literal(context->tree, statement->location, 0));
                VcAstNode *index_init_statement = index_init != NULL
                    ? expression_statement(context->tree, statement->location, index_init) : NULL;
                if (index_init_statement == NULL)
                    return -1;
                const int index_init_state = simple_async_goto_block(context,
                    index_init_statement, condition_state, exception_target);
                if (index_init_state < 0)
                    return -1;

                VcAstNode *collection_init = this_member_assignment(context->tree,
                    statement->location, collection_local->name,
                    statement->as.foreach_statement.collection);
                VcAstNode *collection_init_statement = collection_init != NULL
                    ? expression_statement(context->tree, statement->location, collection_init) : NULL;
                if (collection_init_statement == NULL)
                    return -1;
                return simple_async_goto_block(context, collection_init_statement,
                    index_init_state, exception_target);
            }

            VcAstNode *enumerator_marker = synthetic_local_marker(context, statement->location);
            VcAsyncLocal *enumerator_local = enumerator_marker != NULL
                ? add_promoted_async_local(context, enumerator_marker,
                    foreach_info->enumerator_type, "foreach_enumerator") : NULL;
            if (enumerator_local == NULL)
                return -1;

            if (foreach_info->async_foreach)
            {
                VcAstNode *move_marker = synthetic_local_marker(context, statement->location);
                VcAstTypeRef *bool_type = named_type(context->tree, statement->location, "bool");
                VcAsyncLocal *move_local = move_marker != NULL && bool_type != NULL
                    ? add_promoted_async_local(context, move_marker, bool_type,
                        "foreach_move_next") : NULL;
                if (move_local == NULL)
                    return -1;

                int normal_target = compile_async_foreach_dispose_await(context,
                    enumerator_local->name, foreach_info->dispose_await_binding, continuation,
                    exception_target, statement->location);
                if (normal_target < 0)
                    return -1;

                VcAstNode *rethrow = vc_ast_new_node(context->tree,
                    VC_AST_THROW_STATEMENT, statement->location);
                if (rethrow == NULL)
                    return -1;
                rethrow->as.throw_statement.expression = this_member(context->tree,
                    statement->location, "_pendingException");
                if (rethrow->as.throw_statement.expression == NULL)
                    return -1;
                const int rethrow_state = simple_async_goto_block(context, rethrow,
                    context->finish_state, exception_target);
                if (rethrow_state < 0)
                    return -1;

                const int exceptional_target = compile_async_foreach_dispose_await(context,
                    enumerator_local->name, foreach_info->dispose_await_binding, rethrow_state,
                    exception_target, statement->location);
                if (exceptional_target < 0)
                    return -1;
                const int return_cleanup = compile_async_foreach_dispose_await(context,
                    enumerator_local->name, foreach_info->dispose_await_binding, return_target,
                    exception_target, statement->location);
                if (return_cleanup < 0)
                    return -1;
                context->has_exception_regions = true;

                VcAsyncBlock *condition = new_async_block(context, statement->location,
                    exceptional_target);
                if (condition == NULL)
                    return -1;
                const int condition_state = condition->state;
                condition->terminator = VC_ASYNC_BRANCH;
                condition->expression = this_member(context->tree,
                    statement->location, move_local->name);
                if (condition->expression == NULL)
                    return -1;
                condition->false_target = normal_target;

                VcAstNode *move_call = member_call(context->tree, statement->location,
                    this_member(context->tree, statement->location, enumerator_local->name),
                    "MoveNextAsync");
                VcAstNode *move_target = this_member(context->tree,
                    statement->location, move_local->name);
                if (move_call == NULL || move_target == NULL)
                    return -1;
                const int move_entry = compile_async_bound_await(context, move_call,
                    foreach_info->move_next_await_binding, move_target,
                    condition_state, statement->location, exceptional_target);
                if (move_entry < 0)
                    return -1;

                const int body_entry = compile_async_statement(context,
                    statement->as.foreach_statement.body, move_entry, normal_target,
                    move_entry, return_cleanup, exceptional_target);
                if (body_entry < 0)
                    return -1;

                VcAstNode *current = member_access(context->tree, statement->location,
                    this_member(context->tree, statement->location, enumerator_local->name),
                    "Current");
                VcAstNode *item_assign = current != NULL
                    ? this_member_assignment(context->tree, statement->location,
                        item_local->name, current) : NULL;
                VcAstNode *item_statement = item_assign != NULL
                    ? expression_statement(context->tree, statement->location, item_assign) : NULL;
                if (item_statement == NULL)
                    return -1;
                const int item_state = simple_async_goto_block(context, item_statement,
                    body_entry, exceptional_target);
                if (item_state < 0)
                    return -1;
                condition = &context->blocks[condition_state];
                condition->target = item_state;

                VcAstNode *get_enumerator = member_call(context->tree, statement->location,
                    statement->as.foreach_statement.collection, "GetAsyncEnumerator");
                VcAstNode *enumerator_init = get_enumerator != NULL
                    ? this_member_assignment(context->tree, statement->location,
                        enumerator_local->name, get_enumerator) : NULL;
                VcAstNode *enumerator_init_statement = enumerator_init != NULL
                    ? expression_statement(context->tree, statement->location, enumerator_init) : NULL;
                if (enumerator_init_statement == NULL)
                    return -1;
                return simple_async_goto_block(context, enumerator_init_statement,
                    move_entry, exception_target);
            }

            int normal_target = continuation;
            int exceptional_target = exception_target;
            int break_cleanup = continuation;
            int return_cleanup = return_target;
            if (foreach_info->has_dispose)
            {
                VcAstNode *cleanup_call = member_call(context->tree, statement->location,
                    this_member(context->tree, statement->location, enumerator_local->name),
                    "Dispose");
                VcAstNode *cleanup_statement = cleanup_call != NULL
                    ? expression_statement(context->tree, statement->location, cleanup_call) : NULL;
                if (cleanup_statement == NULL)
                    return -1;

                normal_target = compile_async_cleanup(context, cleanup_statement,
                    continuation, return_target, exception_target);
                if (normal_target < 0)
                    return -1;

                VcAstNode *rethrow = vc_ast_new_node(context->tree,
                    VC_AST_THROW_STATEMENT, statement->location);
                if (rethrow == NULL)
                    return -1;
                rethrow->as.throw_statement.expression = this_member(context->tree,
                    statement->location, "_pendingException");
                if (rethrow->as.throw_statement.expression == NULL)
                    return -1;
                const int rethrow_state = simple_async_goto_block(context, rethrow,
                    context->finish_state, exception_target);
                if (rethrow_state < 0)
                    return -1;
                exceptional_target = compile_async_cleanup(context, cleanup_statement,
                    rethrow_state, return_target, exception_target);
                if (exceptional_target < 0)
                    return -1;

                break_cleanup = normal_target;
                return_cleanup = compile_async_cleanup(context, cleanup_statement,
                    return_target, return_target, exception_target);
                if (return_cleanup < 0)
                    return -1;
                context->has_exception_regions = true;
            }

            VcAsyncBlock *condition = new_async_block(context, statement->location,
                exceptional_target);
            if (condition == NULL)
                return -1;
            const int condition_state = condition->state;

            const int body_entry = compile_async_statement(context,
                statement->as.foreach_statement.body, condition_state, break_cleanup,
                condition_state, return_cleanup, exceptional_target);
            if (body_entry < 0)
                return -1;

            VcAstNode *current = member_access(context->tree, statement->location,
                this_member(context->tree, statement->location, enumerator_local->name),
                "Current");
            VcAstNode *item_assign = current != NULL
                ? this_member_assignment(context->tree, statement->location,
                    item_local->name, current) : NULL;
            VcAstNode *item_statement = item_assign != NULL
                ? expression_statement(context->tree, statement->location, item_assign) : NULL;
            if (item_statement == NULL)
                return -1;
            const int item_state = simple_async_goto_block(context, item_statement,
                body_entry, exceptional_target);
            if (item_state < 0)
                return -1;

            condition = &context->blocks[condition_state];
            condition->terminator = VC_ASYNC_BRANCH;
            condition->expression = member_call(context->tree, statement->location,
                this_member(context->tree, statement->location, enumerator_local->name),
                "MoveNext");
            if (condition->expression == NULL)
                return -1;
            condition->target = item_state;
            condition->false_target = normal_target;

            VcAstNode *get_enumerator = member_call(context->tree, statement->location,
                statement->as.foreach_statement.collection, "GetEnumerator");
            VcAstNode *enumerator_init = get_enumerator != NULL
                ? this_member_assignment(context->tree, statement->location,
                    enumerator_local->name, get_enumerator) : NULL;
            VcAstNode *enumerator_init_statement = enumerator_init != NULL
                ? expression_statement(context->tree, statement->location, enumerator_init) : NULL;
            if (enumerator_init_statement == NULL)
                return -1;
            return simple_async_goto_block(context, enumerator_init_statement,
                condition_state, exception_target);
        }

        case VC_AST_LOCK_STATEMENT:
            if (statement_contains_kind(statement, VC_AST_AWAIT_EXPRESSION))
            {
                context_error(context, "await cannot cross a lock statement");
                return -1;
            }
            if (statement_contains_kind(statement, VC_AST_RETURN_STATEMENT))
            {
                context_error(context, "return cannot leave a lock statement in a suspending async method");
                return -1;
            }
            return simple_async_goto_block(context, statement, continuation, exception_target);

        case VC_AST_FIXED_STATEMENT:
            context_error(context, "fixed statements are not supported in async or iterator methods");
            return -1;

        case VC_AST_THROW_STATEMENT:
            if (statement_contains_kind(statement, VC_AST_AWAIT_EXPRESSION))
            {
                context_error(context, "await in a throw expression is not supported in this async lowering stage");
                return -1;
            }
            return simple_async_goto_block(context, statement, continuation, exception_target);

        default:
            if (statement_contains_kind(statement, VC_AST_AWAIT_EXPRESSION))
            {
                context_error(context, "statement kind %d with await is not supported in async lowering",
                    (int)statement->kind);
                return -1;
            }
            return simple_async_goto_block(context, statement, continuation, exception_target);
    }
}

static bool node_uses_local(const VcAsyncContext *context, const VcAstNode *node,
    const VcAstNode *declaration);

static bool expression_uses_local(const VcAsyncContext *context, const VcAstNode *node,
    const VcAstNode *declaration)
{
    if (node == NULL)
        return false;
    if (node->kind == VC_AST_IDENTIFIER_EXPRESSION)
    {
        const VcSemanticBinding *binding = vc_semantic_binding(context->semantic, node);
        return binding != NULL && binding->declaration_node == declaration;
    }
    switch (node->kind)
    {
        case VC_AST_MEMBER_ACCESS_EXPRESSION:
            return expression_uses_local(context, node->as.member_access_expression.target, declaration);
        case VC_AST_CALL_EXPRESSION:
            if (expression_uses_local(context, node->as.call_expression.callee, declaration)) return true;
            for (size_t i = 0; i < node->as.call_expression.arguments.count; i++)
                if (expression_uses_local(context, node->as.call_expression.arguments.items[i], declaration)) return true;
            return false;
        case VC_AST_INDEX_EXPRESSION:
            if (expression_uses_local(context, node->as.index_expression.target, declaration) ||
                expression_uses_local(context, node->as.index_expression.index, declaration)) return true;
            for (size_t i = 0; i < node->as.index_expression.indices.count; i++)
                if (expression_uses_local(context, node->as.index_expression.indices.items[i], declaration)) return true;
            return false;
        case VC_AST_NEW_EXPRESSION:
            if (expression_uses_local(context, node->as.new_expression.array_length, declaration)) return true;
            for (size_t i = 0; i < node->as.new_expression.array_lengths.count; i++)
                if (expression_uses_local(context, node->as.new_expression.array_lengths.items[i], declaration)) return true;
            for (size_t i = 0; i < node->as.new_expression.arguments.count; i++)
                if (expression_uses_local(context, node->as.new_expression.arguments.items[i], declaration)) return true;
            for (size_t i = 0; i < node->as.new_expression.initializers.count; i++)
                if (expression_uses_local(context, node->as.new_expression.initializers.items[i], declaration)) return true;
            return false;
        case VC_AST_OBJECT_INITIALIZER_MEMBER:
            return expression_uses_local(context, node->as.object_initializer_member.value, declaration);
        case VC_AST_COLLECTION_INITIALIZER_ELEMENT:
            for (size_t i = 0; i < node->as.collection_initializer_element.arguments.count; i++)
                if (expression_uses_local(context, node->as.collection_initializer_element.arguments.items[i], declaration)) return true;
            return false;
        case VC_AST_STACKALLOC_EXPRESSION:
            return expression_uses_local(context, node->as.stackalloc_expression.count, declaration);
        case VC_AST_CAST_EXPRESSION:
            return expression_uses_local(context, node->as.cast_expression.expression, declaration);
        case VC_AST_TYPE_RELATION_EXPRESSION:
            if (expression_uses_local(context, node->as.type_relation_expression.expression, declaration) ||
                expression_uses_local(context, node->as.type_relation_expression.pattern_constant, declaration) ||
                expression_uses_local(context, node->as.type_relation_expression.pattern_left, declaration) ||
                expression_uses_local(context, node->as.type_relation_expression.pattern_right, declaration)) return true;
            for (size_t i = 0; i < node->as.type_relation_expression.pattern_properties.count; i++)
                if (expression_uses_local(context, node->as.type_relation_expression.pattern_properties.items[i], declaration)) return true;
            return false;
        case VC_AST_PROPERTY_PATTERN_MEMBER:
            return expression_uses_local(context, node->as.property_pattern_member.pattern, declaration);
        case VC_AST_UNARY_EXPRESSION:
            return expression_uses_local(context, node->as.unary_expression.operand, declaration);
        case VC_AST_AWAIT_EXPRESSION:
            return expression_uses_local(context, node->as.await_expression.operand, declaration);
        case VC_AST_RANGE_EXPRESSION:
            return expression_uses_local(context, node->as.range_expression.start, declaration) ||
                expression_uses_local(context, node->as.range_expression.end, declaration);
        case VC_AST_BINARY_EXPRESSION:
            return expression_uses_local(context, node->as.binary_expression.left, declaration) ||
                expression_uses_local(context, node->as.binary_expression.right, declaration);
        case VC_AST_CONDITIONAL_EXPRESSION:
            return expression_uses_local(context, node->as.conditional_expression.condition, declaration) ||
                expression_uses_local(context, node->as.conditional_expression.when_true, declaration) ||
                expression_uses_local(context, node->as.conditional_expression.when_false, declaration);
        case VC_AST_SWITCH_EXPRESSION:
            if (expression_uses_local(context, node->as.switch_expression.expression, declaration)) return true;
            for (size_t i = 0; i < node->as.switch_expression.arms.count; i++)
                if (expression_uses_local(context, node->as.switch_expression.arms.items[i], declaration)) return true;
            return false;
        case VC_AST_SWITCH_EXPRESSION_ARM:
            return expression_uses_local(context, node->as.switch_expression_arm.pattern, declaration) ||
                expression_uses_local(context, node->as.switch_expression_arm.guard, declaration) ||
                expression_uses_local(context, node->as.switch_expression_arm.result, declaration);
        case VC_AST_ASSIGNMENT_EXPRESSION:
            return expression_uses_local(context, node->as.assignment_expression.left, declaration) ||
                expression_uses_local(context, node->as.assignment_expression.right, declaration);
        case VC_AST_LAMBDA_EXPRESSION:
            return node_uses_local(context, node->as.lambda_expression.body, declaration);
        case VC_AST_PARENTHESIZED_EXPRESSION:
            return expression_uses_local(context, node->as.parenthesized_expression.expression, declaration);
        default:
            return false;
    }
}

static bool node_uses_local(const VcAsyncContext *context, const VcAstNode *node,
    const VcAstNode *declaration)
{
    if (node == NULL)
        return false;
    switch (node->kind)
    {
        case VC_AST_BLOCK_STATEMENT:
            for (size_t i = 0; i < node->as.block_statement.statements.count; i++)
                if (node_uses_local(context, node->as.block_statement.statements.items[i], declaration)) return true;
            return false;
        case VC_AST_EXPRESSION_STATEMENT:
            return expression_uses_local(context, node->as.expression_statement.expression, declaration);
        case VC_AST_RETURN_STATEMENT:
            return expression_uses_local(context, node->as.return_statement.expression, declaration);
        case VC_AST_THROW_STATEMENT:
            return expression_uses_local(context, node->as.throw_statement.expression, declaration);
        case VC_AST_TRY_STATEMENT:
            if (node_uses_local(context, node->as.try_statement.try_block, declaration)) return true;
            for (size_t i = 0; i < node->as.try_statement.catches.count; i++)
                if (node_uses_local(context, node->as.try_statement.catches.items[i], declaration)) return true;
            return node_uses_local(context, node->as.try_statement.finally_block, declaration);
        case VC_AST_CATCH_CLAUSE:
            return node_uses_local(context, node->as.catch_clause.body, declaration);
        case VC_AST_LOCAL_DECLARATION:
            return expression_uses_local(context, node->as.local_declaration.initializer, declaration);
        case VC_AST_IF_STATEMENT:
            return expression_uses_local(context, node->as.if_statement.condition, declaration) ||
                node_uses_local(context, node->as.if_statement.then_statement, declaration) ||
                node_uses_local(context, node->as.if_statement.else_statement, declaration);
        case VC_AST_WHILE_STATEMENT:
        case VC_AST_DO_WHILE_STATEMENT:
            return expression_uses_local(context, node->as.while_statement.condition, declaration) ||
                node_uses_local(context, node->as.while_statement.body, declaration);
        case VC_AST_FOR_STATEMENT:
            return node_uses_local(context, node->as.for_statement.initializer, declaration) ||
                expression_uses_local(context, node->as.for_statement.condition, declaration) ||
                expression_uses_local(context, node->as.for_statement.increment, declaration) ||
                node_uses_local(context, node->as.for_statement.body, declaration);
        case VC_AST_FOREACH_STATEMENT:
            return expression_uses_local(context, node->as.foreach_statement.collection, declaration) ||
                node_uses_local(context, node->as.foreach_statement.body, declaration);
        case VC_AST_USING_STATEMENT:
            return node_uses_local(context, node->as.using_statement.declaration, declaration) ||
                expression_uses_local(context, node->as.using_statement.expression, declaration) ||
                node_uses_local(context, node->as.using_statement.body, declaration);
        case VC_AST_LOCK_STATEMENT:
            return expression_uses_local(context, node->as.lock_statement.expression, declaration) ||
                node_uses_local(context, node->as.lock_statement.body, declaration);
        case VC_AST_FIXED_STATEMENT:
            return expression_uses_local(context, node->as.fixed_statement.initializer, declaration) ||
                node_uses_local(context, node->as.fixed_statement.body, declaration);
        case VC_AST_SWITCH_STATEMENT:
            if (expression_uses_local(context, node->as.switch_statement.expression, declaration)) return true;
            for (size_t i = 0; i < node->as.switch_statement.sections.count; i++)
                if (node_uses_local(context, node->as.switch_statement.sections.items[i], declaration)) return true;
            return false;
        case VC_AST_SWITCH_SECTION:
            for (size_t i = 0; i < node->as.switch_section.labels.count; i++)
                if (node_uses_local(context, node->as.switch_section.labels.items[i], declaration)) return true;
            for (size_t i = 0; i < node->as.switch_section.statements.count; i++)
                if (node_uses_local(context, node->as.switch_section.statements.items[i], declaration)) return true;
            return false;
        case VC_AST_SWITCH_LABEL:
            return expression_uses_local(context, node->as.switch_label.value, declaration) ||
                expression_uses_local(context, node->as.switch_label.pattern, declaration) ||
                expression_uses_local(context, node->as.switch_label.guard, declaration);
        default:
            return expression_uses_local(context, node, declaration);
    }
}

static bool expression_is_local_identifier(const VcAsyncContext *context, const VcAstNode *expression,
    const VcAstNode *declaration)
{
    if (expression == NULL || expression->kind != VC_AST_IDENTIFIER_EXPRESSION)
        return false;
    const VcSemanticBinding *binding = vc_semantic_binding(context->semantic, expression);
    return binding != NULL && binding->declaration_node == declaration;
}

static bool expression_is_direct_definition(const VcAsyncContext *context, const VcAstNode *expression,
    const VcAstNode *declaration)
{
    if (expression == NULL || expression->kind != VC_AST_ASSIGNMENT_EXPRESSION ||
        expression->as.assignment_expression.operator_kind != VC_TOKEN_EQUAL)
        return false;
    return expression_is_local_identifier(context, expression->as.assignment_expression.left, declaration);
}

static bool async_block_uses_local(const VcAsyncContext *context, const VcAsyncBlock *block_node,
    const VcAstNode *declaration)
{
    for (size_t i = 0; i < block_node->statements.count; i++)
        if (node_uses_local(context, block_node->statements.items[i], declaration)) return true;
    if (block_node->terminator == VC_ASYNC_BRANCH || block_node->terminator == VC_ASYNC_SWITCH ||
        block_node->terminator == VC_ASYNC_RETURN)
        return expression_uses_local(context, block_node->expression, declaration);
    if ((block_node->terminator == VC_ASYNC_AWAIT_START ||
            block_node->terminator == VC_ASYNC_AWAIT_RESUME) &&
        block_node->await_index < context->await_count)
    {
        const VcAsyncAwait *await = &context->awaits[block_node->await_index];
        if (block_node->terminator == VC_ASYNC_AWAIT_START)
            return expression_uses_local(context, await->operand, declaration);
        if (await->assignment_target != NULL &&
            !expression_is_local_identifier(context, await->assignment_target, declaration) &&
            expression_uses_local(context, await->assignment_target, declaration))
            return true;
    }
    return false;
}

static bool async_block_defines_local(const VcAsyncContext *context, const VcAsyncBlock *block_node,
    const VcAstNode *declaration)
{
    if (block_node->terminator == VC_ASYNC_AWAIT_RESUME &&
        block_node->await_index < context->await_count)
    {
        const VcAsyncAwait *await = &context->awaits[block_node->await_index];
        if (await->result_declaration == declaration)
            return true;
        if (await->assignment_target != NULL &&
            expression_is_local_identifier(context, await->assignment_target, declaration))
            return true;
    }
    if (block_node->statements.count != 1)
        return false;
    const VcAstNode *statement = block_node->statements.items[0];
    if (statement == declaration && statement->kind == VC_AST_LOCAL_DECLARATION)
        return true;
    return statement != NULL && statement->kind == VC_AST_EXPRESSION_STATEMENT &&
        expression_is_direct_definition(context, statement->as.expression_statement.expression, declaration);
}

static size_t async_block_successors(const VcAsyncBlock *block_node, int *successors, size_t capacity)
{
    size_t count = 0;
#define ADD_SUCCESSOR(value) do { if ((value) >= 0 && count < capacity) successors[count++] = (value); } while (0)
    switch (block_node->terminator)
    {
        case VC_ASYNC_GOTO:
        case VC_ASYNC_AWAIT_START:
        case VC_ASYNC_AWAIT_RESUME:
            ADD_SUCCESSOR(block_node->target);
            break;
        case VC_ASYNC_BRANCH:
            ADD_SUCCESSOR(block_node->target);
            ADD_SUCCESSOR(block_node->false_target);
            break;
        case VC_ASYNC_SWITCH:
            ADD_SUCCESSOR(block_node->target);
            for (size_t i = 0; i < block_node->switch_target_count; i++)
                ADD_SUCCESSOR(block_node->switch_targets[i]);
            break;
        case VC_ASYNC_RETURN:
            ADD_SUCCESSOR(block_node->target);
            break;
        case VC_ASYNC_CATCH_DISPATCH:
            for (size_t i = 0; i < block_node->catch_target_count; i++)
                ADD_SUCCESSOR(block_node->catch_targets[i]);
            break;
        case VC_ASYNC_FINISH:
            break;
    }
    ADD_SUCCESSOR(block_node->exception_target);
#undef ADD_SUCCESSOR
    return count;
}

static bool analyze_async_local_promotion(VcAsyncContext *context)
{
    if (context->local_count == 0 || context->block_count == 0)
        return true;
    bool *live_in = calloc(context->block_count, sizeof(*live_in));
    bool *live_out = calloc(context->block_count, sizeof(*live_out));
    if (live_in == NULL || live_out == NULL)
    {
        free(live_in);
        free(live_out);
        context_error(context, "out of memory while analyzing async local lifetimes");
        return false;
    }

    for (size_t local_index = 0; local_index < context->local_count; local_index++)
    {
        if (context->locals[local_index].promoted)
            continue;
        memset(live_in, 0, context->block_count * sizeof(*live_in));
        memset(live_out, 0, context->block_count * sizeof(*live_out));
        const VcAstNode *declaration = context->locals[local_index].declaration;
        bool changed = true;
        while (changed)
        {
            changed = false;
            for (size_t reverse = context->block_count; reverse > 0; reverse--)
            {
                const size_t i = reverse - 1;
                int successors[128];
                const size_t successor_count = async_block_successors(&context->blocks[i],
                    successors, sizeof(successors) / sizeof(successors[0]));
                bool out = false;
                for (size_t s = 0; s < successor_count; s++)
                {
                    if ((size_t)successors[s] < context->block_count && live_in[successors[s]])
                    {
                        out = true;
                        break;
                    }
                }
                const bool use = async_block_uses_local(context, &context->blocks[i], declaration);
                const bool def = async_block_defines_local(context, &context->blocks[i], declaration);
                const bool in = use || (out && !def);
                if (out != live_out[i] || in != live_in[i])
                {
                    live_out[i] = out;
                    live_in[i] = in;
                    changed = true;
                }
            }
        }

        bool promoted = false;
        for (size_t i = 0; i < context->block_count; i++)
        {
            const VcAsyncBlock *block_node = &context->blocks[i];
            if (block_node->terminator == VC_ASYNC_AWAIT_START && block_node->target >= 0 &&
                (size_t)block_node->target < context->block_count && live_in[block_node->target])
            {
                promoted = true;
                break;
            }
        }
        context->locals[local_index].promoted = promoted;
    }

    free(live_in);
    free(live_out);
    return true;
}

static VcAstNode *make_state_assignment_statement(VcAsyncContext *context,
    VcSourceLocation location, int state)
{
    VcAstNode *assign = this_member_assignment(context->tree, location,
        "_state", number_literal(context->tree, location, state));
    return assign != NULL ? expression_statement(context->tree, location, assign) : NULL;
}

static VcAstNode *local_target(VcAsyncContext *context, const VcAstNode *declaration,
    VcSourceLocation location)
{
    VcAsyncLocal *local = local_for_declaration(context, declaration);
    if (local == NULL)
        return NULL;
    return local->promoted
        ? this_member(context->tree, location, local->name)
        : identifier(context->tree, location, local->name);
}

static bool append_completion(VcAsyncContext *context, VcAstNode *branch,
    VcSourceLocation location, VcAstNode *result)
{
    VcAstNode *state = make_state_assignment_statement(context, location, -2);
    VcAstNode *complete = member_call(context->tree, location,
        this_member(context->tree, location, "_completion"), "SetResult");
    if (state == NULL || complete == NULL)
        return false;
    if (result != NULL &&
        !vc_ast_node_list_push(context->tree, &complete->as.call_expression.arguments, result))
        return false;
    return block_push(context->tree, branch, state) &&
        block_push(context->tree, branch, expression_statement(context->tree, location, complete)) &&
        block_push(context->tree, branch, return_statement(context->tree, location, NULL));
}

static bool append_terminal_exception_completion(VcAsyncContext *context, VcAstNode *branch,
    VcSourceLocation location, const char *error_name)
{
    VcAstNode *classify = vc_ast_new_node(context->tree, VC_AST_TRY_STATEMENT, location);
    VcAstNode *classify_body = block(context->tree, location);
    VcAstNode *rethrow = vc_ast_new_node(context->tree, VC_AST_THROW_STATEMENT, location);
    if (classify == NULL || classify_body == NULL || rethrow == NULL)
        return false;
    rethrow->as.throw_statement.expression = identifier(context->tree, location, error_name);
    if (rethrow->as.throw_statement.expression == NULL || !block_push(context->tree, classify_body, rethrow))
        return false;
    classify->as.try_statement.try_block = classify_body;

    VcAstNode *cancel_clause = vc_ast_new_node(context->tree, VC_AST_CATCH_CLAUSE, location);
    VcAstNode *cancel_body = block(context->tree, location);
    if (cancel_clause == NULL || cancel_body == NULL)
        return false;
    cancel_clause->as.catch_clause.type = named_type(context->tree, location, "OperationCanceledException");
    cancel_clause->as.catch_clause.name = copy_text(context->tree, "__voidc_async_canceled");
    cancel_clause->as.catch_clause.body = cancel_body;
    VcAstNode *cancel_error = identifier(context->tree, location, "__voidc_async_canceled");
    VcAstNode *cancel_token = cancel_error != NULL
        ? member_access(context->tree, location, cancel_error, "CancellationToken")
        : NULL;
    VcAstNode *cancel_call = member_call(context->tree, location,
        this_member(context->tree, location, "_completion"), "SetCanceled");
    if (cancel_clause->as.catch_clause.type == NULL || cancel_clause->as.catch_clause.name == NULL ||
        cancel_token == NULL || cancel_call == NULL ||
        !vc_ast_node_list_push(context->tree, &cancel_call->as.call_expression.arguments, cancel_token) ||
        !block_push(context->tree, cancel_body, make_state_assignment_statement(context, location, -2)) ||
        !block_push(context->tree, cancel_body, expression_statement(context->tree, location, cancel_call)) ||
        !block_push(context->tree, cancel_body, return_statement(context->tree, location, NULL)) ||
        !vc_ast_node_list_push(context->tree, &classify->as.try_statement.catches, cancel_clause))
        return false;

    VcAstNode *fault_clause = vc_ast_new_node(context->tree, VC_AST_CATCH_CLAUSE, location);
    VcAstNode *fault_body = block(context->tree, location);
    if (fault_clause == NULL || fault_body == NULL)
        return false;
    fault_clause->as.catch_clause.type = named_type(context->tree, location, "Exception");
    fault_clause->as.catch_clause.name = copy_text(context->tree, "__voidc_async_fault");
    fault_clause->as.catch_clause.body = fault_body;
    VcAstNode *fault_call = member_call(context->tree, location,
        this_member(context->tree, location, "_completion"), "SetException");
    VcAstNode *fault_error = identifier(context->tree, location, "__voidc_async_fault");
    if (fault_clause->as.catch_clause.type == NULL || fault_clause->as.catch_clause.name == NULL ||
        fault_call == NULL || fault_error == NULL ||
        !vc_ast_node_list_push(context->tree, &fault_call->as.call_expression.arguments, fault_error) ||
        !block_push(context->tree, fault_body, make_state_assignment_statement(context, location, -2)) ||
        !block_push(context->tree, fault_body, expression_statement(context->tree, location, fault_call)) ||
        !block_push(context->tree, fault_body, return_statement(context->tree, location, NULL)) ||
        !vc_ast_node_list_push(context->tree, &classify->as.try_statement.catches, fault_clause))
        return false;

    return block_push(context->tree, branch, classify);
}

static bool make_awaiter_resume_method(VcAsyncContext *context, VcAstNode *state_type)
{
    const VcSourceLocation location = context->method->location;
    VcAstNode *method = vc_ast_new_node(context->tree, VC_AST_METHOD_DECLARATION, location);
    VcAstNode *body = block(context->tree, location);
    VcAstNode *move = member_call(context->tree, location,
        identifier(context->tree, location, "this"), "MoveNext");
    if (method == NULL || body == NULL || move == NULL)
        return false;
    method->as.method_declaration.modifiers = VC_AST_MOD_PRIVATE |
        (context->method->as.method_declaration.modifiers & VC_AST_MOD_UNSAFE);
    method->as.method_declaration.return_type = named_type(context->tree, location, "void");
    method->as.method_declaration.name = copy_text(context->tree, "ResumeAwaiter");
    method->as.method_declaration.body = body;
    return method->as.method_declaration.return_type != NULL &&
        method->as.method_declaration.name != NULL &&
        block_push(context->tree, body, expression_statement(context->tree, location, move)) &&
        vc_ast_node_list_push(context->tree, &state_type->as.type_declaration.members, method);
}

static const char *async_protocol_method_name(
    const VcAsyncContext *context, size_t method_index)
{
    if (context == NULL || context->semantic == NULL ||
        method_index >= context->semantic->method_count)
        return NULL;
    const VcSemanticMethod *method = &context->semantic->methods[method_index];
    if (method->node == NULL || method->node->kind != VC_AST_METHOD_DECLARATION)
        return NULL;
    return method->node->as.method_declaration.name;
}

static const char *async_protocol_property_name(
    const VcAsyncContext *context, size_t struct_index, size_t property_index)
{
    if (context == NULL || context->semantic == NULL ||
        struct_index >= context->semantic->struct_count)
        return NULL;
    const VcSemanticStruct *structure = &context->semantic->structs[struct_index];
    if (property_index >= structure->property_count)
        return NULL;
    const VcSemanticProperty *property = &structure->properties[property_index];
    if (property->node == NULL || property->node->kind != VC_AST_PROPERTY_DECLARATION)
        return NULL;
    return property->node->as.property_declaration.name;
}

static bool append_async_pattern_capture_assignments(
    VcAsyncContext *context, VcAstNode *pattern, VcAstNodeList *statements)
{
    if (pattern == NULL)
        return true;
    if (pattern->kind == VC_AST_PROPERTY_PATTERN_MEMBER)
        return append_async_pattern_capture_assignments(context,
            pattern->as.property_pattern_member.pattern, statements);
    if (pattern->kind != VC_AST_TYPE_RELATION_EXPRESSION)
        return true;

    if (pattern->as.type_relation_expression.pattern_name != NULL)
    {
        VcAsyncLocal *local = local_for_declaration(context, pattern);
        if (local != NULL && local->promoted)
        {
            VcAstNode *target = this_member(context->tree, pattern->location, local->name);
            VcAstNode *value = identifier(context->tree, pattern->location,
                pattern->as.type_relation_expression.pattern_name);
            VcAstNode *statement = target != NULL && value != NULL
                ? async_assignment_statement(context, pattern->location, target, value)
                : NULL;
            if (statement == NULL ||
                !vc_ast_node_list_push(context->tree, statements, statement))
                return false;
        }
    }

    if (!append_async_pattern_capture_assignments(context,
            pattern->as.type_relation_expression.pattern_left, statements) ||
        !append_async_pattern_capture_assignments(context,
            pattern->as.type_relation_expression.pattern_right, statements))
        return false;
    for (size_t i = 0; i < pattern->as.type_relation_expression.pattern_properties.count; i++)
        if (!append_async_pattern_capture_assignments(context,
                pattern->as.type_relation_expression.pattern_properties.items[i], statements))
            return false;
    return true;
}

static bool append_async_switch_pattern_capture_assignments(
    VcAsyncContext *context, const VcAstNode *section, VcAstNodeList *statements)
{
    if (section == NULL || section->kind != VC_AST_SWITCH_SECTION)
        return true;
    for (size_t i = 0; i < section->as.switch_section.labels.count; i++)
    {
        VcAstNode *label = section->as.switch_section.labels.items[i];
        if (label != NULL && label->kind == VC_AST_SWITCH_LABEL &&
            label->as.switch_label.is_pattern &&
            !append_async_pattern_capture_assignments(context,
                label->as.switch_label.pattern, statements))
            return false;
    }
    return true;
}

static bool emit_async_state_block(VcAsyncContext *context, VcAstNode *move_loop,
    const VcAsyncBlock *source)
{
    VcAstNode *condition = binary(context->tree, source->location, VC_TOKEN_EQUAL_EQUAL,
        this_member(context->tree, source->location, "_state"),
        number_literal(context->tree, source->location, source->state));
    VcAstNode *branch = block(context->tree, source->location);
    VcAstNode *outer = vc_ast_new_node(context->tree, VC_AST_IF_STATEMENT, source->location);
    if (condition == NULL || branch == NULL || outer == NULL)
        return false;
    outer->as.if_statement.condition = condition;
    outer->as.if_statement.then_statement = branch;
    for (size_t i = 0; i < source->statements.count; i++)
        if (!block_push(context->tree, branch, source->statements.items[i])) return false;

    switch (source->terminator)
    {
        case VC_ASYNC_GOTO:
            if (!block_push(context->tree, branch,
                    make_state_assignment_statement(context, source->location, source->target)) ||
                !block_push(context->tree, branch, continue_statement(context->tree, source->location)))
                return false;
            break;

        case VC_ASYNC_BRANCH:
        {
            VcAstNode *then_block = block(context->tree, source->location);
            VcAstNode *else_block = block(context->tree, source->location);
            VcAstNode *inner = vc_ast_new_node(context->tree, VC_AST_IF_STATEMENT, source->location);
            if (then_block == NULL || else_block == NULL || inner == NULL ||
                !block_push(context->tree, then_block,
                    make_state_assignment_statement(context, source->location, source->target)) ||
                !block_push(context->tree, else_block,
                    make_state_assignment_statement(context, source->location, source->false_target)))
                return false;
            inner->as.if_statement.condition = source->expression;
            inner->as.if_statement.then_statement = then_block;
            inner->as.if_statement.else_statement = else_block;
            if (!block_push(context->tree, branch, inner) ||
                !block_push(context->tree, branch, continue_statement(context->tree, source->location)))
                return false;
            break;
        }

        case VC_ASYNC_SWITCH:
        {
            if (source->switch_source == NULL ||
                source->switch_target_count != source->switch_source->as.switch_statement.sections.count)
                return false;
            VcAstNode *switch_node = vc_ast_new_node(context->tree,
                VC_AST_SWITCH_STATEMENT, source->location);
            if (switch_node == NULL ||
                !block_push(context->tree, branch,
                    make_state_assignment_statement(context, source->location, source->target)))
                return false;
            switch_node->as.switch_statement.expression = source->expression;
            for (size_t i = 0; i < source->switch_target_count; i++)
            {
                const VcAstNode *source_section =
                    source->switch_source->as.switch_statement.sections.items[i];
                VcAstNode *section = vc_ast_new_node(context->tree,
                    VC_AST_SWITCH_SECTION, source_section->location);
                if (section == NULL) return false;
                for (size_t j = 0; j < source_section->as.switch_section.labels.count; j++)
                    if (!vc_ast_node_list_push(context->tree, &section->as.switch_section.labels,
                            source_section->as.switch_section.labels.items[j])) return false;
                if (!append_async_switch_pattern_capture_assignments(context, source_section,
                        &section->as.switch_section.statements) ||
                    !vc_ast_node_list_push(context->tree, &section->as.switch_section.statements,
                        make_state_assignment_statement(context, source_section->location,
                            source->switch_targets[i])) ||
                    !vc_ast_node_list_push(context->tree, &section->as.switch_section.statements,
                        break_statement(context->tree, source_section->location)) ||
                    !vc_ast_node_list_push(context->tree, &switch_node->as.switch_statement.sections, section))
                    return false;
            }
            if (!block_push(context->tree, branch, switch_node) ||
                !block_push(context->tree, branch, continue_statement(context->tree, source->location)))
                return false;
            break;
        }

        case VC_ASYNC_AWAIT_START:
        {
            if (source->await_index >= context->await_count)
                return false;
            const VcAsyncAwait *await = &context->awaits[source->await_index];
            const char *get_awaiter_name = "GetAwaiter";
            const char *is_completed_name = "IsCompleted";
            const char *on_completed_name = "OnCompleted";
            if (await->protocol_binding != NULL)
            {
                const VcSemanticBinding *binding = await->protocol_binding;
                get_awaiter_name = async_protocol_method_name(
                    context, binding->await_get_awaiter_method_index);
                is_completed_name = async_protocol_property_name(context,
                    binding->await_is_completed_struct_index,
                    binding->await_is_completed_property_index);
                on_completed_name = async_protocol_method_name(
                    context, binding->await_on_completed_method_index);
                if (get_awaiter_name == NULL || is_completed_name == NULL ||
                    on_completed_name == NULL)
                    return false;
            }
            VcAstNode *stored_value = member_call(context->tree, source->location,
                await->operand, get_awaiter_name);

            VcAstNode *store = stored_value != NULL
                ? this_member_assignment(context->tree, source->location,
                    await->field_name, stored_value)
                : NULL;
            VcAstNode *suspend = vc_ast_new_node(context->tree,
                VC_AST_IF_STATEMENT, source->location);
            VcAstNode *suspend_body = block(context->tree, source->location);
            VcAstNode *is_completed = member_access(context->tree, source->location,
                this_member(context->tree, source->location, await->field_name),
                is_completed_name);
            if (store == NULL || suspend == NULL || suspend_body == NULL || is_completed == NULL)
                return false;
            suspend->as.if_statement.condition = unary(context->tree, source->location,
                VC_TOKEN_BANG, is_completed);
            suspend->as.if_statement.then_statement = suspend_body;

            VcAstNode *resume_local = vc_ast_new_node(context->tree,
                VC_AST_LOCAL_DECLARATION, source->location);
            VcAstNode *resume_group = member_access(context->tree, source->location,
                identifier(context->tree, source->location, "this"),
                "ResumeAwaiter");
            VcAstNode *continue_call = member_call(context->tree, source->location,
                this_member(context->tree, source->location, await->field_name),
                on_completed_name);
            VcAstNode *resume_argument = identifier(context->tree, source->location,
                "__voidc_async_resume");
            if (resume_local != NULL)
            {
                resume_local->as.local_declaration.type =
                    named_type(context->tree, source->location, "Action");
                resume_local->as.local_declaration.name = copy_text(context->tree,
                    "__voidc_async_resume");
                resume_local->as.local_declaration.initializer = resume_group;
            }
            if (resume_local == NULL || resume_group == NULL || continue_call == NULL ||
                resume_argument == NULL || resume_local->as.local_declaration.type == NULL ||
                resume_local->as.local_declaration.name == NULL ||
                !vc_ast_node_list_push(context->tree, &continue_call->as.call_expression.arguments,
                    resume_argument) ||
                !block_push(context->tree, branch,
                    expression_statement(context->tree, source->location, store)) ||
                !block_push(context->tree, suspend_body,
                    make_state_assignment_statement(context, source->location, source->target)) ||
                !block_push(context->tree, suspend_body, resume_local) ||
                !block_push(context->tree, suspend_body,
                    expression_statement(context->tree, source->location, continue_call)) ||
                !block_push(context->tree, suspend_body,
                    return_statement(context->tree, source->location, NULL)) ||
                !block_push(context->tree, branch, suspend) ||
                !block_push(context->tree, branch,
                    make_state_assignment_statement(context, source->location, source->target)) ||
                !block_push(context->tree, branch, continue_statement(context->tree, source->location)))
                return false;
            break;
        }

        case VC_ASYNC_AWAIT_RESUME:
        {
            if (source->await_index >= context->await_count)
                return false;
            const VcAsyncAwait *await = &context->awaits[source->await_index];
            const char *get_result_name = "GetResult";
            if (await->protocol_binding != NULL)
            {
                get_result_name = async_protocol_method_name(context,
                    await->protocol_binding->await_get_result_method_index);
                if (get_result_name == NULL)
                    return false;
            }
            VcAstNode *result_call = member_call(context->tree, source->location,
                this_member(context->tree, source->location, await->field_name),
                get_result_name);
            if (result_call == NULL)
                return false;

            if (await->result_declaration != NULL || await->assignment_target != NULL)
            {
                VcAstNode *target = await->result_declaration != NULL
                    ? local_target(context, await->result_declaration, source->location)
                    : await->assignment_target;
                VcAstNode *assign = target != NULL
                    ? assignment_target(context->tree, source->location, target, result_call)
                    : NULL;
                if (assign == NULL || !block_push(context->tree, branch,
                        expression_statement(context->tree, source->location, assign)))
                    return false;
            }
            else if (await->return_result)
            {
                VcAstNode *store = this_member_assignment(context->tree, source->location,
                    "_returnValue", result_call);
                if (store == NULL || !block_push(context->tree, branch,
                        expression_statement(context->tree, source->location, store)))
                    return false;
            }
            else if (!block_push(context->tree, branch,
                    expression_statement(context->tree, source->location, result_call)))
                return false;

            if (!block_push(context->tree, branch,
                    make_state_assignment_statement(context, source->location, source->target)) ||
                !block_push(context->tree, branch, continue_statement(context->tree, source->location)))
                return false;
            break;
        }

        case VC_ASYNC_RETURN:
            if (source->expression != NULL)
            {
                VcAstNode *store = this_member_assignment(context->tree, source->location,
                    "_returnValue", source->expression);
                if (store == NULL || !block_push(context->tree, branch,
                        expression_statement(context->tree, source->location, store)))
                    return false;
            }
            if (!block_push(context->tree, branch,
                    make_state_assignment_statement(context, source->location, source->target)) ||
                !block_push(context->tree, branch, continue_statement(context->tree, source->location)))
                return false;
            break;

        case VC_ASYNC_CATCH_DISPATCH:
        {
            if (source->try_source == NULL ||
                source->catch_target_count != source->try_source->as.try_statement.catches.count)
                return false;
            VcAstNode *dispatch_try = vc_ast_new_node(context->tree,
                VC_AST_TRY_STATEMENT, source->location);
            VcAstNode *dispatch_try_body = block(context->tree, source->location);
            VcAstNode *pending_throw = vc_ast_new_node(context->tree,
                VC_AST_THROW_STATEMENT, source->location);
            if (dispatch_try == NULL || dispatch_try_body == NULL || pending_throw == NULL)
                return false;
            pending_throw->as.throw_statement.expression = this_member(context->tree,
                source->location, "_pendingException");
            if (pending_throw->as.throw_statement.expression == NULL ||
                !block_push(context->tree, dispatch_try_body, pending_throw))
                return false;
            dispatch_try->as.try_statement.try_block = dispatch_try_body;

            bool has_catch_all = false;
            for (size_t i = 0; i < source->catch_target_count; i++)
            {
                const VcAstNode *source_clause =
                    source->try_source->as.try_statement.catches.items[i];
                const VcAsyncCatch *catch_info = async_catch_for_clause(context, source_clause);
                VcAstNode *clause = vc_ast_new_node(context->tree,
                    VC_AST_CATCH_CLAUSE, source_clause->location);
                VcAstNode *catch_body = block(context->tree, source_clause->location);
                char generated[80];
                const int written = snprintf(generated, sizeof(generated),
                    "__voidc_async_dispatch_%d_%zu", source->state, i);
                if (catch_info == NULL || clause == NULL || catch_body == NULL || written < 0 ||
                    (size_t)written >= sizeof(generated))
                    return false;
                clause->as.catch_clause.type = clone_type(context->tree, catch_info->type);
                clause->as.catch_clause.name = copy_text(context->tree, generated);
                clause->as.catch_clause.body = catch_body;
                if (clause->as.catch_clause.type == NULL || clause->as.catch_clause.name == NULL)
                    return false;
                VcAstNode *caught = identifier(context->tree, source_clause->location, generated);
                VcAstNode *store = caught != NULL
                    ? this_member_assignment(context->tree, source_clause->location,
                        catch_info->field_name, caught)
                    : NULL;
                if (store == NULL ||
                    !block_push(context->tree, catch_body,
                        expression_statement(context->tree, source_clause->location, store)) ||
                    !block_push(context->tree, catch_body,
                        make_state_assignment_statement(context, source_clause->location,
                            source->catch_targets[i])) ||
                    !block_push(context->tree, catch_body,
                        continue_statement(context->tree, source_clause->location)) ||
                    !vc_ast_node_list_push(context->tree, &dispatch_try->as.try_statement.catches, clause))
                    return false;
                if (source_clause->as.catch_clause.catch_all ||
                    (catch_info->type->name != NULL &&
                        strcmp(catch_info->type->name, "Exception") == 0))
                    has_catch_all = true;
            }

            if (!has_catch_all)
            {
                VcAstNode *clause = vc_ast_new_node(context->tree,
                    VC_AST_CATCH_CLAUSE, source->location);
                VcAstNode *catch_body = block(context->tree, source->location);
                VcAstNode *rethrow = vc_ast_new_node(context->tree,
                    VC_AST_THROW_STATEMENT, source->location);
                if (clause == NULL || catch_body == NULL || rethrow == NULL)
                    return false;
                clause->as.catch_clause.catch_all = true;
                clause->as.catch_clause.body = catch_body;
                rethrow->as.throw_statement.expression = this_member(context->tree,
                    source->location, "_pendingException");
                if (rethrow->as.throw_statement.expression == NULL ||
                    !block_push(context->tree, catch_body, rethrow) ||
                    !vc_ast_node_list_push(context->tree, &dispatch_try->as.try_statement.catches, clause))
                    return false;
            }
            if (!block_push(context->tree, branch, dispatch_try))
                return false;
            break;
        }

        case VC_ASYNC_FINISH:
        {
            VcAstNode *result = source->expression;
            if (result == NULL && context->semantic_method != NULL &&
                context->semantic_method->async_result_type != VC_SEM_TYPE_VOID)
                result = default_expression(context->tree, source->location,
                    context->method->as.method_declaration.return_type->generic_arguments.items[0]);
            if ((context->semantic_method != NULL &&
                    context->semantic_method->async_result_type != VC_SEM_TYPE_VOID && result == NULL) ||
                !append_completion(context, branch, source->location, result))
                return false;
            break;
        }
    }
    return block_push(context->tree, move_loop, outer);
}

static bool make_async_cfg_move_next_method(VcAsyncContext *context, VcAstNode *state_type)
{
    const VcSourceLocation location = context->method->location;
    VcAstNode *move_next = vc_ast_new_node(context->tree, VC_AST_METHOD_DECLARATION, location);
    VcAstNode *body = block(context->tree, location);
    VcAstNode *move_loop = vc_ast_new_node(context->tree, VC_AST_WHILE_STATEMENT, location);
    VcAstNode *loop_body = block(context->tree, location);
    VcAstNode *try_node = vc_ast_new_node(context->tree, VC_AST_TRY_STATEMENT, location);
    VcAstNode *try_body = block(context->tree, location);
    VcAstNode *catch_clause = vc_ast_new_node(context->tree, VC_AST_CATCH_CLAUSE, location);
    VcAstNode *catch_body = block(context->tree, location);
    if (move_next == NULL || body == NULL || move_loop == NULL || loop_body == NULL ||
        try_node == NULL || try_body == NULL || catch_clause == NULL || catch_body == NULL)
        return false;

    move_next->as.method_declaration.modifiers = VC_AST_MOD_PUBLIC |
        (context->method->as.method_declaration.modifiers & VC_AST_MOD_UNSAFE);
    move_next->as.method_declaration.return_type = named_type(context->tree, location, "void");
    move_next->as.method_declaration.name = copy_text(context->tree, "MoveNext");
    move_next->as.method_declaration.body = body;
    move_loop->as.while_statement.condition = bool_literal(context->tree, location, true);
    move_loop->as.while_statement.body = loop_body;
    if (move_next->as.method_declaration.return_type == NULL ||
        move_next->as.method_declaration.name == NULL || move_loop->as.while_statement.condition == NULL)
        return false;

    VcAstNode *guard = vc_ast_new_node(context->tree, VC_AST_IF_STATEMENT, location);
    if (guard == NULL)
        return false;
    guard->as.if_statement.condition = binary(context->tree, location, VC_TOKEN_EQUAL_EQUAL,
        this_member(context->tree, location, "_state"), number_literal(context->tree, location, -2));
    guard->as.if_statement.then_statement = return_statement(context->tree, location, NULL);
    if (guard->as.if_statement.condition == NULL || guard->as.if_statement.then_statement == NULL ||
        !block_push(context->tree, body, guard))
        return false;

    for (size_t i = 0; i < context->local_count; i++)
    {
        if (context->locals[i].promoted)
            continue;
        VcAstNode *local = vc_ast_new_node(context->tree, VC_AST_LOCAL_DECLARATION, location);
        if (local == NULL)
            return false;
        local->as.local_declaration.type = clone_type(context->tree, context->locals[i].type);
        local->as.local_declaration.name = context->locals[i].name;
        local->as.local_declaration.initializer = default_expression(context->tree, location,
            context->locals[i].type);
        if (local->as.local_declaration.type == NULL || local->as.local_declaration.initializer == NULL ||
            !block_push(context->tree, body, local))
            return false;
    }

    for (size_t i = 0; i < context->block_count; i++)
        if (!emit_async_state_block(context, try_body, &context->blocks[i]))
            return false;
    if (!block_push(context->tree, try_body, return_statement(context->tree, location, NULL)))
        return false;

    catch_clause->as.catch_clause.type = named_type(context->tree, location, "Exception");
    catch_clause->as.catch_clause.name = copy_text(context->tree, "__voidc_async_error");
    catch_clause->as.catch_clause.body = catch_body;
    if (catch_clause->as.catch_clause.type == NULL || catch_clause->as.catch_clause.name == NULL)
        return false;

    for (size_t i = 0; i < context->block_count; i++)
    {
        const VcAsyncBlock *source = &context->blocks[i];
        if (source->exception_target < 0)
            continue;
        VcAstNode *condition = binary(context->tree, source->location, VC_TOKEN_EQUAL_EQUAL,
            this_member(context->tree, source->location, "_state"),
            number_literal(context->tree, source->location, source->state));
        VcAstNode *route_body = block(context->tree, source->location);
        VcAstNode *route = vc_ast_new_node(context->tree, VC_AST_IF_STATEMENT, source->location);
        VcAstNode *error = identifier(context->tree, source->location, "__voidc_async_error");
        VcAstNode *store_error = error != NULL
            ? this_member_assignment(context->tree, source->location, "_pendingException", error)
            : NULL;
        if (condition == NULL || route_body == NULL || route == NULL || store_error == NULL ||
            !block_push(context->tree, route_body,
                expression_statement(context->tree, source->location, store_error)) ||
            !block_push(context->tree, route_body,
                make_state_assignment_statement(context, source->location, source->exception_target)) ||
            !block_push(context->tree, route_body,
                continue_statement(context->tree, source->location)))
            return false;
        route->as.if_statement.condition = condition;
        route->as.if_statement.then_statement = route_body;
        if (!block_push(context->tree, catch_body, route))
            return false;
    }

    if (!append_terminal_exception_completion(context, catch_body, location,
            "__voidc_async_error"))
        return false;

    try_node->as.try_statement.try_block = try_body;
    if (!vc_ast_node_list_push(context->tree, &try_node->as.try_statement.catches, catch_clause) ||
        !block_push(context->tree, loop_body, try_node) ||
        !block_push(context->tree, body, move_loop) ||
        !vc_ast_node_list_push(context->tree, &state_type->as.type_declaration.members, move_next))
        return false;
    return true;
}

static void free_async_context_storage(VcAsyncContext *context)
{
    if (context == NULL)
        return;
    for (size_t i = 0; i < context->block_count; i++)
    {
        free(context->blocks[i].switch_targets);
        free(context->blocks[i].catch_targets);
    }
    free(context->blocks);
    free(context->awaits);
    free(context->catches);
    free(context->resources);
    free(context->spills);
    free(context->foreaches);
    free(context->locals);
    context->blocks = NULL;
    context->awaits = NULL;
    context->catches = NULL;
    context->resources = NULL;
    context->spills = NULL;
    context->foreaches = NULL;
    context->locals = NULL;
}


static bool make_execute_method(VcAsyncContext *context, VcAstNode *state_type, VcAstNode *original_body,
    const VcAstTypeRef *return_type)
{
    VcAstNode *execute = vc_ast_new_node(context->tree, VC_AST_METHOD_DECLARATION, context->method->location);
    if (execute == NULL || original_body == NULL)
        return false;
    execute->as.method_declaration.modifiers = VC_AST_MOD_PRIVATE |
        (context->method->as.method_declaration.modifiers & VC_AST_MOD_UNSAFE);
    execute->as.method_declaration.return_type = return_type == NULL
        ? named_type(context->tree, context->method->location, "void")
        : clone_type(context->tree, return_type);
    execute->as.method_declaration.name = copy_text(context->tree, "Execute");
    execute->as.method_declaration.body = original_body;
    return execute->as.method_declaration.return_type != NULL &&
        execute->as.method_declaration.name != NULL &&
        vc_ast_node_list_push(context->tree, &state_type->as.type_declaration.members, execute);
}

static bool make_move_next_method(VcAsyncContext *context, VcAstNode *state_type,
    const VcAstTypeRef *result_type)
{
    const VcSourceLocation location = context->method->location;
    VcAstNode *move_next = vc_ast_new_node(context->tree, VC_AST_METHOD_DECLARATION, location);
    VcAstNode *body = block(context->tree, location);
    if (move_next == NULL || body == NULL)
        return false;
    move_next->as.method_declaration.modifiers = VC_AST_MOD_PUBLIC |
        (context->method->as.method_declaration.modifiers & VC_AST_MOD_UNSAFE);
    move_next->as.method_declaration.return_type = named_type(context->tree, location, "void");
    move_next->as.method_declaration.name = copy_text(context->tree, "MoveNext");
    move_next->as.method_declaration.body = body;
    if (move_next->as.method_declaration.return_type == NULL ||
        move_next->as.method_declaration.name == NULL)
        return false;

    VcAstNode *guard = vc_ast_new_node(context->tree, VC_AST_IF_STATEMENT, location);
    VcAstNode *guard_condition = binary(context->tree, location, VC_TOKEN_BANG_EQUAL,
        this_member(context->tree, location, "_state"), number_literal(context->tree, location, -1));
    if (guard == NULL || guard_condition == NULL)
        return false;
    guard->as.if_statement.condition = guard_condition;
    guard->as.if_statement.then_statement = return_statement(context->tree, location, NULL);
    if (guard->as.if_statement.then_statement == NULL || !block_push(context->tree, body, guard))
        return false;

    VcAstNode *running_assign = this_member_assignment(context->tree, location,
        "_state", number_literal(context->tree, location, 0));
    if (running_assign == NULL || !block_push(context->tree, body,
            expression_statement(context->tree, location, running_assign)))
        return false;

    VcAstNode *try_node = vc_ast_new_node(context->tree, VC_AST_TRY_STATEMENT, location);
    VcAstNode *try_body = block(context->tree, location);
    VcAstNode *catch_clause = vc_ast_new_node(context->tree, VC_AST_CATCH_CLAUSE, location);
    VcAstNode *catch_body = block(context->tree, location);
    if (try_node == NULL || try_body == NULL || catch_clause == NULL || catch_body == NULL)
        return false;

    VcAstNode *execute_call = member_call(context->tree, location,
        identifier(context->tree, location, "this"), "Execute");
    VcAstNode *completed_assign = this_member_assignment(context->tree, location,
        "_state", number_literal(context->tree, location, -2));
    VcAstNode *completion_call = member_call(context->tree, location,
        this_member(context->tree, location, "_completion"), "SetResult");
    if (execute_call == NULL || completed_assign == NULL || completion_call == NULL)
        return false;
    if (result_type != NULL)
    {
        if (!vc_ast_node_list_push(context->tree, &completion_call->as.call_expression.arguments, execute_call))
            return false;
    }
    else if (!block_push(context->tree, try_body,
            expression_statement(context->tree, location, execute_call)))
        return false;
    if (!block_push(context->tree, try_body,
            expression_statement(context->tree, location, completed_assign)) ||
        !block_push(context->tree, try_body,
            expression_statement(context->tree, location, completion_call)))
        return false;

    catch_clause->as.catch_clause.type = named_type(context->tree, location, "Exception");
    catch_clause->as.catch_clause.name = copy_text(context->tree, "__voidc_async_error");
    catch_clause->as.catch_clause.body = catch_body;
    if (catch_clause->as.catch_clause.type == NULL || catch_clause->as.catch_clause.name == NULL)
        return false;

    if (!append_terminal_exception_completion(context, catch_body, location,
            "__voidc_async_error"))
        return false;

    try_node->as.try_statement.try_block = try_body;
    if (!vc_ast_node_list_push(context->tree, &try_node->as.try_statement.catches, catch_clause) ||
        !block_push(context->tree, body, try_node) ||
        !vc_ast_node_list_push(context->tree, &state_type->as.type_declaration.members, move_next))
        return false;
    return true;
}

static bool make_get_task_method(VcAsyncContext *context, VcAstNode *state_type)
{
    const VcSourceLocation location = context->method->location;
    VcAstNode *method = vc_ast_new_node(context->tree, VC_AST_METHOD_DECLARATION, location);
    VcAstNode *body = block(context->tree, location);
    VcAstNode *completion = this_member(context->tree, location, "_completion");
    VcAstNode *task = completion != NULL ? member_access(context->tree, location, completion, "Task") : NULL;
    if (method == NULL || body == NULL || task == NULL)
        return false;
    VcAstNode *result = task;
    if (context->semantic_method != NULL && context->semantic_method->async_returns_value_task)
    {
        result = new_expression_with_type(context->tree, location,
            context->method->as.method_declaration.return_type);
        if (result == NULL || !vc_ast_node_list_push(context->tree,
                &result->as.new_expression.arguments, task))
            return false;
    }
    method->as.method_declaration.modifiers = VC_AST_MOD_PUBLIC;
    method->as.method_declaration.return_type = clone_type(context->tree,
        context->method->as.method_declaration.return_type);
    method->as.method_declaration.name = copy_text(context->tree, "GetTask");
    method->as.method_declaration.body = body;
    return method->as.method_declaration.return_type != NULL &&
        method->as.method_declaration.name != NULL &&
        block_push(context->tree, body, return_statement(context->tree, location, result)) &&
        vc_ast_node_list_push(context->tree, &state_type->as.type_declaration.members, method);
}

static bool make_wrapper_body(VcAsyncContext *context, const char *state_name, bool captures_this)
{
    const VcSourceLocation location = context->method->location;
    VcAstNode *wrapper = block(context->tree, location);
    VcAstNode *local = vc_ast_new_node(context->tree, VC_AST_LOCAL_DECLARATION, location);
    VcAstNode *create = new_expression(context->tree, location, state_name);
    if (wrapper == NULL || local == NULL || create == NULL)
        return false;

    if (captures_this)
    {
        VcAstNode *self = identifier(context->tree, location, "this");
        if (self == NULL || !vc_ast_node_list_push(context->tree, &create->as.new_expression.arguments, self))
            return false;
    }
    for (size_t i = 0; i < context->method->as.method_declaration.parameters.count; i++)
    {
        VcAstNode *parameter = context->method->as.method_declaration.parameters.items[i];
        VcAstNode *argument = identifier(context->tree, parameter->location, parameter->as.parameter.name);
        if (argument == NULL || !vc_ast_node_list_push(context->tree,
                &create->as.new_expression.arguments, argument))
            return false;
    }

    char local_name[64];
    const int written = snprintf(local_name, sizeof(local_name), "__voidc_async_machine");
    if (written < 0 || (size_t)written >= sizeof(local_name))
        return false;
    local->as.local_declaration.type = named_type(context->tree, location, state_name);
    local->as.local_declaration.name = copy_text(context->tree, local_name);
    local->as.local_declaration.initializer = create;
    if (local->as.local_declaration.type == NULL || local->as.local_declaration.name == NULL ||
        !block_push(context->tree, wrapper, local))
        return false;

    VcAstNode *move = member_call(context->tree, location,
        identifier(context->tree, location, local_name), "MoveNext");
    VcAstNode *get_task = member_call(context->tree, location,
        identifier(context->tree, location, local_name), "GetTask");
    if (move == NULL || get_task == NULL ||
        !block_push(context->tree, wrapper,
            expression_statement(context->tree, location, move)) ||
        !block_push(context->tree, wrapper,
            return_statement(context->tree, location, get_task)))
        return false;

    context->method->as.method_declaration.body = wrapper;
    context->method->as.method_declaration.modifiers &= ~VC_AST_MOD_ASYNC;
    return true;
}

static bool lower_method(VcAstTree *tree, VcAstNodeList *parent_declarations,
    const VcAstNode *owner, VcAstNode *method, size_t async_id,
    const VcSemanticModel *semantic, const VcSemanticModel *generated_semantic,
    char *error, size_t error_size)
{
    if ((method->as.method_declaration.modifiers & VC_AST_MOD_ASYNC) == 0 ||
        method->as.method_declaration.body == NULL)
        return true;

    const VcSemanticModel *method_semantic = semantic;
    const VcSemanticMethod *semantic_method = semantic_method_for_node(method_semantic, method);
    if (semantic_method == NULL && generated_semantic != NULL)
    {
        method_semantic = generated_semantic;
        semantic_method = semantic_method_for_node(method_semantic, method);
    }
    if (semantic_method == NULL || !semantic_method->is_async)
        return true;

    const bool has_outer_result = semantic_method->async_result_type != VC_SEM_TYPE_VOID;
    const VcAstTypeRef *outer_result_type = NULL;
    if (has_outer_result)
    {
        if (method->as.method_declaration.return_type == NULL ||
            method->as.method_declaration.return_type->generic_arguments.count != 1)
            return true;
        outer_result_type = method->as.method_declaration.return_type->generic_arguments.items[0];
    }

    const bool has_await = statement_contains_kind(method->as.method_declaration.body,
        VC_AST_AWAIT_EXPRESSION);
    if (owner->as.type_declaration.generic_parameters.count != 0 ||
        method->as.method_declaration.generic_parameters.count != 0)
        return true;
    if (owner->as.type_declaration.type_kind != VC_AST_TYPE_CLASS &&
        owner->as.type_declaration.type_kind != VC_AST_TYPE_INTERFACE)
        return true;

    VcAsyncContext context = {0};
    context.tree = tree;
    context.owner = owner;
    context.method = method;
    context.semantic = method_semantic;
    context.semantic_method = semantic_method;
    context.error = error;
    context.error_size = error_size;

    if (statement_contains_kind(method->as.method_declaration.body, VC_AST_FIXED_STATEMENT))
    {
        context_error(&context,
            "fixed statements are not supported in async or iterator methods");
        goto fail;
    }

    VcAstNode *original_body = method->as.method_declaration.body;
    int entry_state = -1;
    if (has_await)
    {
        if (statement_contains_kind(original_body, VC_AST_STACKALLOC_EXPRESSION))
        {
            context_error(&context,
                "stackalloc cannot be used in an async method that suspends");
            goto fail;
        }
        if (!normalize_async_foundation_statement(&context, &original_body))
            goto fail;
        method->as.method_declaration.body = original_body;
        if (!collect_async_locals(&context, original_body))
            goto fail;

        VcAsyncBlock *finish = new_async_block(&context, method->location, -1);
        VcAsyncBlock *return_complete = new_async_block(&context, method->location, -1);
        if (finish == NULL || return_complete == NULL)
            goto fail;
        finish->terminator = VC_ASYNC_FINISH;
        return_complete->terminator = VC_ASYNC_FINISH;
        if (has_outer_result)
        {
            return_complete->expression = this_member(tree, method->location, "_returnValue");
            if (return_complete->expression == NULL)
                goto fail;
        }
        context.finish_state = finish->state;
        context.return_state = return_complete->state;
        entry_state = compile_async_statement(&context, original_body,
            finish->state, -1, -1, return_complete->state, -1);
        if (entry_state < 0 || !analyze_async_local_promotion(&context))
            goto fail;
    }

    if (!rewrite_statement(&context, original_body))
    {
        context_error(&context, "out of memory while rewriting async method '%s'",
            method->as.method_declaration.name);
        goto fail;
    }

    char state_name[320];
    const int written = snprintf(state_name, sizeof(state_name), VC_ASYNC_PREFIX "%s$%s$%zu",
        owner->as.type_declaration.name, method->as.method_declaration.name, async_id);
    if (written < 0 || (size_t)written >= sizeof(state_name))
    {
        context_error(&context, "generated async state-machine type name is too long");
        goto fail;
    }

    const VcSourceLocation location = method->location;
    VcAstNode *state_type = vc_ast_new_node(tree, VC_AST_TYPE_DECLARATION, location);
    if (state_type == NULL)
        goto oom;
    state_type->as.type_declaration.type_kind = VC_AST_TYPE_CLASS;
    state_type->as.type_declaration.modifiers = VC_AST_MOD_PUBLIC | VC_AST_MOD_SEALED;
    state_type->as.type_declaration.name = copy_text(tree, state_name);
    if (state_type->as.type_declaration.name == NULL)
        goto oom;

    VcAstTypeRef *completion_type = outer_result_type == NULL
        ? named_type(tree, location, "TaskCompletionSource")
        : generic_type(tree, location, "TaskCompletionSource", outer_result_type);
    if (!add_field(tree, state_type, location, "_state", named_type(tree, location, "int")) ||
        !add_field(tree, state_type, location, "_completion", completion_type))
        goto oom;
    if (has_await && has_outer_result &&
        !add_field(tree, state_type, location, "_returnValue", clone_type(tree, outer_result_type)))
        goto oom;
    if (has_await && context.has_exception_regions &&
        !add_field(tree, state_type, location, "_pendingException", named_type(tree, location, "Exception")))
        goto oom;

    const bool captures_this = (method->as.method_declaration.modifiers & VC_AST_MOD_STATIC) == 0;
    /* The generated owner-typed field also identifies the original declaring type to final semantics.
       Static async methods do not initialize it, but the field keeps private/protected owner access coherent. */
    if (!add_field(tree, state_type, location, "_this",
            named_type(tree, location, owner->as.type_declaration.name)))
        goto oom;

    for (size_t i = 0; i < method->as.method_declaration.parameters.count; i++)
    {
        char field_name[48];
        const int field_written = snprintf(field_name, sizeof(field_name), "__arg%zu", i);
        if (field_written < 0 || (size_t)field_written >= sizeof(field_name) ||
            !add_field(tree, state_type,
                method->as.method_declaration.parameters.items[i]->location,
                field_name, method->as.method_declaration.parameters.items[i]->as.parameter.type))
            goto oom;
    }

    if (has_await)
    {
        for (size_t i = 0; i < context.local_count; i++)
        {
            if (context.locals[i].promoted &&
                !add_field(tree, state_type, context.locals[i].declaration->location,
                    context.locals[i].name, clone_type(tree, context.locals[i].type)))
                goto oom;
        }
        for (size_t i = 0; i < context.await_count; i++)
        {
            if (!add_field(tree, state_type, context.awaits[i].operand->location,
                    context.awaits[i].field_name, clone_type(tree, context.awaits[i].awaiter_type)))
                goto oom;
        }
        for (size_t i = 0; i < context.spill_count; i++)
        {
            if (!add_field(tree, state_type, location, context.spills[i].field_name,
                    clone_type(tree, context.spills[i].type)))
                goto oom;
        }
        for (size_t i = 0; i < context.catch_count; i++)
        {
            if (!add_field(tree, state_type, context.catches[i].clause->location,
                    context.catches[i].field_name, clone_type(tree, context.catches[i].type)))
                goto oom;
        }
        for (size_t i = 0; i < context.resource_count; i++)
        {
            if (!add_field(tree, state_type, location, context.resources[i].field_name,
                    clone_type(tree, context.resources[i].type)))
                goto oom;
        }
    }

    if (!make_state_constructor(&context, state_type, state_name, captures_this,
            outer_result_type, has_await ? entry_state : -1))
        goto oom;
    if (has_await)
    {
        if (!make_awaiter_resume_method(&context, state_type) ||
            !make_async_cfg_move_next_method(&context, state_type))
            goto oom;
    }
    else if (!make_execute_method(&context, state_type, original_body, outer_result_type) ||
        !make_move_next_method(&context, state_type, outer_result_type))
        goto oom;

    if (!make_get_task_method(&context, state_type) ||
        !vc_ast_node_list_push(tree, parent_declarations, state_type) ||
        !make_wrapper_body(&context, state_name, captures_this))
        goto oom;

    free_async_context_storage(&context);
    return true;

oom:
    context_error(&context, "out of memory while lowering async method '%s'",
        method->as.method_declaration.name);
fail:
    free_async_context_storage(&context);
    return false;
}

static bool declarations_contain_node(const VcAstNodeList *declarations, const VcAstNode *wanted)
{
    if (declarations == NULL || wanted == NULL)
        return false;
    for (size_t i = 0; i < declarations->count; i++)
    {
        const VcAstNode *node = declarations->items[i];
        if (node == wanted)
            return true;
        if (node != NULL && node->kind == VC_AST_NAMESPACE_DECLARATION &&
            declarations_contain_node(&node->as.namespace_declaration.declarations, wanted))
            return true;
    }
    return false;
}

static VcAstTree *tree_containing_declaration(
    VcAstTree **trees, size_t tree_count, const VcAstNode *wanted)
{
    for (size_t i = 0; i < tree_count; i++)
    {
        VcAstTree *tree = trees[i];
        if (tree == NULL || tree->root == NULL ||
            tree->root->kind != VC_AST_COMPILATION_UNIT)
            continue;
        if (declarations_contain_node(
                &tree->root->as.compilation_unit.declarations, wanted))
            return tree;
    }
    return NULL;
}

static void async_lambda_error(char *error, size_t error_size, const char *format, ...)
{
    if (error == NULL || error_size == 0 || error[0] != '\0')
        return;
    va_list args;
    va_start(args, format);
    vsnprintf(error, error_size, format, args);
    va_end(args);
}

typedef struct VcAsyncLambdaCaptureRewrite
{
    VcAstTree *tree;
    const VcSemanticModel *semantic;
    const VcSemanticLambda *lambda;
    const VcSemanticMethod *owner_method;
    VcAstNode **sources;
    char *error;
    size_t error_size;
} VcAsyncLambdaCaptureRewrite;

static size_t async_lambda_this_capture_index(const VcSemanticLambda *lambda)
{
    if (lambda == NULL)
        return (size_t)-1;
    for (size_t i = 0; i < lambda->capture_count; i++)
        if (lambda->captures[i].kind == VC_SEM_CAPTURE_THIS)
            return i;
    return (size_t)-1;
}

static size_t async_lambda_local_capture_index(
    const VcSemanticLambda *lambda, const VcSemanticBinding *binding)
{
    if (lambda == NULL || binding == NULL || binding->declaration_node == NULL)
        return (size_t)-1;
    for (size_t i = 0; i < lambda->capture_count; i++)
    {
        const VcSemanticCapture *capture = &lambda->captures[i];
        if (capture->kind == VC_SEM_CAPTURE_LOCAL &&
            capture->declaration_node == binding->declaration_node)
            return i;
    }
    return (size_t)-1;
}

static bool async_lambda_binding_is_instance_owner_member(
    const VcAsyncLambdaCaptureRewrite *rewrite,
    const VcSemanticBinding *binding)
{
    if (rewrite == NULL || rewrite->owner_method == NULL || binding == NULL ||
        !rewrite->owner_method->has_owner_struct)
        return false;

    const size_t owner_index = rewrite->owner_method->owner_struct_index;
    if (binding->has_method && binding->method_index < rewrite->semantic->method_count)
    {
        const VcSemanticMethod *method = &rewrite->semantic->methods[binding->method_index];
        return method->has_owner_struct && method->owner_struct_index == owner_index &&
            !method->is_static;
    }
    if (binding->has_field && binding->struct_index == owner_index &&
        owner_index < rewrite->semantic->struct_count)
    {
        const VcSemanticStruct *owner = &rewrite->semantic->structs[owner_index];
        return binding->field_index < owner->field_count &&
            !owner->fields[binding->field_index].is_static;
    }
    if (binding->has_property && binding->struct_index == owner_index &&
        owner_index < rewrite->semantic->struct_count)
    {
        const VcSemanticStruct *owner = &rewrite->semantic->structs[owner_index];
        return binding->property_index < owner->property_count &&
            !owner->properties[binding->property_index].is_static;
    }
    return false;
}

static VcAstNode *async_lambda_capture_access(
    VcAsyncLambdaCaptureRewrite *rewrite, size_t capture_index, VcSourceLocation location)
{
    if (rewrite == NULL || capture_index >= rewrite->lambda->capture_count)
        return NULL;
    const VcSemanticCapture *capture = &rewrite->lambda->captures[capture_index];
    VcAstNode *node = vc_ast_new_node(rewrite->tree,
        VC_AST_COMPILER_CAPTURE_EXPRESSION, location);
    VcAstNode *target = identifier(rewrite->tree, location, "__voidc_async_closure");
    VcAstTypeRef *type = type_from_semantic_model(
        rewrite->tree, rewrite->semantic, capture->type, location);
    if (node == NULL || target == NULL || type == NULL)
        return NULL;
    node->as.compiler_capture_expression.target = target;
    node->as.compiler_capture_expression.source_expression = rewrite->sources[capture_index];
    node->as.compiler_capture_expression.source_lambda_node = rewrite->lambda->node;
    node->as.compiler_capture_expression.declaration_node = capture->declaration_node;
    node->as.compiler_capture_expression.owner_method_node = rewrite->owner_method->node;
    node->as.compiler_capture_expression.name = copy_text(rewrite->tree, capture->name);
    node->as.compiler_capture_expression.type = type;
    node->as.compiler_capture_expression.captures_this =
        capture->kind == VC_SEM_CAPTURE_THIS;
    if (node->as.compiler_capture_expression.name == NULL)
        return NULL;
    return node;
}

static bool rewrite_async_lambda_capture_expression(
    VcAsyncLambdaCaptureRewrite *rewrite, VcAstNode *node);

static bool rewrite_async_lambda_capture_statement(
    VcAsyncLambdaCaptureRewrite *rewrite, VcAstNode *node)
{
    if (node == NULL)
        return true;
    switch (node->kind)
    {
        case VC_AST_BLOCK_STATEMENT:
            for (size_t i = 0; i < node->as.block_statement.statements.count; i++)
                if (!rewrite_async_lambda_capture_statement(
                        rewrite, node->as.block_statement.statements.items[i])) return false;
            return true;
        case VC_AST_EXPRESSION_STATEMENT:
            return rewrite_async_lambda_capture_expression(
                rewrite, node->as.expression_statement.expression);
        case VC_AST_RETURN_STATEMENT:
            return rewrite_async_lambda_capture_expression(
                rewrite, node->as.return_statement.expression);
        case VC_AST_THROW_STATEMENT:
            return rewrite_async_lambda_capture_expression(
                rewrite, node->as.throw_statement.expression);
        case VC_AST_TRY_STATEMENT:
            if (!rewrite_async_lambda_capture_statement(rewrite,
                    node->as.try_statement.try_block)) return false;
            for (size_t i = 0; i < node->as.try_statement.catches.count; i++)
                if (!rewrite_async_lambda_capture_statement(
                        rewrite, node->as.try_statement.catches.items[i])) return false;
            return rewrite_async_lambda_capture_statement(
                rewrite, node->as.try_statement.finally_block);
        case VC_AST_CATCH_CLAUSE:
            return rewrite_async_lambda_capture_statement(rewrite, node->as.catch_clause.body);
        case VC_AST_LOCAL_DECLARATION:
            return rewrite_async_lambda_capture_expression(
                rewrite, node->as.local_declaration.initializer);
        case VC_AST_IF_STATEMENT:
            return rewrite_async_lambda_capture_expression(rewrite, node->as.if_statement.condition) &&
                rewrite_async_lambda_capture_statement(rewrite, node->as.if_statement.then_statement) &&
                rewrite_async_lambda_capture_statement(rewrite, node->as.if_statement.else_statement);
        case VC_AST_WHILE_STATEMENT:
        case VC_AST_DO_WHILE_STATEMENT:
            return rewrite_async_lambda_capture_expression(rewrite, node->as.while_statement.condition) &&
                rewrite_async_lambda_capture_statement(rewrite, node->as.while_statement.body);
        case VC_AST_FOR_STATEMENT:
            return rewrite_async_lambda_capture_statement(rewrite, node->as.for_statement.initializer) &&
                rewrite_async_lambda_capture_expression(rewrite, node->as.for_statement.condition) &&
                rewrite_async_lambda_capture_expression(rewrite, node->as.for_statement.increment) &&
                rewrite_async_lambda_capture_statement(rewrite, node->as.for_statement.body);
        case VC_AST_FOREACH_STATEMENT:
            return rewrite_async_lambda_capture_expression(rewrite, node->as.foreach_statement.collection) &&
                rewrite_async_lambda_capture_statement(rewrite, node->as.foreach_statement.body);
        case VC_AST_USING_STATEMENT:
            return rewrite_async_lambda_capture_statement(rewrite, node->as.using_statement.declaration) &&
                rewrite_async_lambda_capture_expression(rewrite, node->as.using_statement.expression) &&
                rewrite_async_lambda_capture_statement(rewrite, node->as.using_statement.body);
        case VC_AST_LOCK_STATEMENT:
            return rewrite_async_lambda_capture_expression(rewrite, node->as.lock_statement.expression) &&
                rewrite_async_lambda_capture_statement(rewrite, node->as.lock_statement.body);
        case VC_AST_FIXED_STATEMENT:
            return rewrite_async_lambda_capture_expression(rewrite, node->as.fixed_statement.initializer) &&
                rewrite_async_lambda_capture_statement(rewrite, node->as.fixed_statement.body);
        case VC_AST_SWITCH_STATEMENT:
            if (!rewrite_async_lambda_capture_expression(rewrite,
                    node->as.switch_statement.expression)) return false;
            for (size_t i = 0; i < node->as.switch_statement.sections.count; i++)
                if (!rewrite_async_lambda_capture_statement(
                        rewrite, node->as.switch_statement.sections.items[i])) return false;
            return true;
        case VC_AST_SWITCH_SECTION:
            for (size_t i = 0; i < node->as.switch_section.labels.count; i++)
                if (!rewrite_async_lambda_capture_statement(
                        rewrite, node->as.switch_section.labels.items[i])) return false;
            for (size_t i = 0; i < node->as.switch_section.statements.count; i++)
                if (!rewrite_async_lambda_capture_statement(
                        rewrite, node->as.switch_section.statements.items[i])) return false;
            return true;
        case VC_AST_SWITCH_LABEL:
            return rewrite_async_lambda_capture_expression(rewrite, node->as.switch_label.value) &&
                rewrite_async_lambda_capture_expression(rewrite, node->as.switch_label.pattern) &&
                rewrite_async_lambda_capture_expression(rewrite, node->as.switch_label.guard);
        case VC_AST_YIELD_RETURN_STATEMENT:
            return rewrite_async_lambda_capture_expression(rewrite, node->as.yield_statement.expression);
        default:
            return true;
    }
}

static bool rewrite_async_lambda_capture_expression(
    VcAsyncLambdaCaptureRewrite *rewrite, VcAstNode *node)
{
    if (node == NULL)
        return true;
    switch (node->kind)
    {
        case VC_AST_IDENTIFIER_EXPRESSION:
        {
            const char *name = node->as.identifier_expression.name;
            if (name == NULL)
                return true;
            if (strcmp(name, "base") == 0)
            {
                if (async_lambda_this_capture_index(rewrite->lambda) != (size_t)-1)
                {
                    async_lambda_error(rewrite->error, rewrite->error_size,
                        "base access in a capturing async lambda is not supported yet");
                    return false;
                }
                return true;
            }
            if (strcmp(name, "this") == 0)
            {
                const size_t index = async_lambda_this_capture_index(rewrite->lambda);
                if (index == (size_t)-1)
                    return true;
                VcAstNode *capture = async_lambda_capture_access(rewrite, index, node->location);
                if (capture == NULL)
                    return false;
                *node = *capture;
                return true;
            }

            const VcSemanticBinding *binding = vc_semantic_binding(rewrite->semantic, node);
            const size_t local_capture = async_lambda_local_capture_index(rewrite->lambda, binding);
            if (local_capture != (size_t)-1)
            {
                VcAstNode *capture = async_lambda_capture_access(
                    rewrite, local_capture, node->location);
                if (capture == NULL)
                    return false;
                *node = *capture;
                return true;
            }

            if (async_lambda_binding_is_instance_owner_member(rewrite, binding))
            {
                const size_t this_capture = async_lambda_this_capture_index(rewrite->lambda);
                if (this_capture == (size_t)-1)
                    return true;
                VcAstNode *target = async_lambda_capture_access(
                    rewrite, this_capture, node->location);
                char *member = copy_text(rewrite->tree, name);
                if (target == NULL || member == NULL)
                    return false;
                node->kind = VC_AST_MEMBER_ACCESS_EXPRESSION;
                memset(&node->as, 0, sizeof(node->as));
                node->as.member_access_expression.target = target;
                node->as.member_access_expression.member = member;
                return true;
            }
            return true;
        }
        case VC_AST_MEMBER_ACCESS_EXPRESSION:
            return rewrite_async_lambda_capture_expression(
                rewrite, node->as.member_access_expression.target);
        case VC_AST_CALL_EXPRESSION:
        {
            VcAstNode *callee = node->as.call_expression.callee;
            const VcSemanticBinding *binding = vc_semantic_binding(rewrite->semantic, node);
            if (callee != NULL && callee->kind == VC_AST_IDENTIFIER_EXPRESSION &&
                async_lambda_binding_is_instance_owner_member(rewrite, binding))
            {
                const size_t this_capture = async_lambda_this_capture_index(rewrite->lambda);
                if (this_capture != (size_t)-1)
                {
                    VcAstNode *target = async_lambda_capture_access(
                        rewrite, this_capture, callee->location);
                    char *member = copy_text(rewrite->tree,
                        callee->as.identifier_expression.name);
                    if (target == NULL || member == NULL)
                        return false;
                    callee->kind = VC_AST_MEMBER_ACCESS_EXPRESSION;
                    memset(&callee->as, 0, sizeof(callee->as));
                    callee->as.member_access_expression.target = target;
                    callee->as.member_access_expression.member = member;
                }
            }
            else if (!rewrite_async_lambda_capture_expression(rewrite, callee))
                return false;
            for (size_t i = 0; i < node->as.call_expression.arguments.count; i++)
                if (!rewrite_async_lambda_capture_expression(
                        rewrite, node->as.call_expression.arguments.items[i])) return false;
            return true;
        }
        case VC_AST_INDEX_EXPRESSION:
            if (!rewrite_async_lambda_capture_expression(rewrite, node->as.index_expression.target) ||
                !rewrite_async_lambda_capture_expression(rewrite, node->as.index_expression.index)) return false;
            for (size_t i = 0; i < node->as.index_expression.indices.count; i++)
                if (!rewrite_async_lambda_capture_expression(
                        rewrite, node->as.index_expression.indices.items[i])) return false;
            return true;
        case VC_AST_NEW_EXPRESSION:
            if (!rewrite_async_lambda_capture_expression(rewrite, node->as.new_expression.array_length)) return false;
            for (size_t i = 0; i < node->as.new_expression.array_lengths.count; i++)
                if (!rewrite_async_lambda_capture_expression(
                        rewrite, node->as.new_expression.array_lengths.items[i])) return false;
            for (size_t i = 0; i < node->as.new_expression.arguments.count; i++)
                if (!rewrite_async_lambda_capture_expression(
                        rewrite, node->as.new_expression.arguments.items[i])) return false;
            for (size_t i = 0; i < node->as.new_expression.initializers.count; i++)
                if (!rewrite_async_lambda_capture_expression(
                        rewrite, node->as.new_expression.initializers.items[i])) return false;
            return true;
        case VC_AST_OBJECT_INITIALIZER_MEMBER:
            return rewrite_async_lambda_capture_expression(
                rewrite, node->as.object_initializer_member.value);
        case VC_AST_COLLECTION_INITIALIZER_ELEMENT:
            for (size_t i = 0; i < node->as.collection_initializer_element.arguments.count; i++)
                if (!rewrite_async_lambda_capture_expression(
                        rewrite, node->as.collection_initializer_element.arguments.items[i])) return false;
            return true;
        case VC_AST_STACKALLOC_EXPRESSION:
            return rewrite_async_lambda_capture_expression(rewrite, node->as.stackalloc_expression.count);
        case VC_AST_CAST_EXPRESSION:
            return rewrite_async_lambda_capture_expression(rewrite, node->as.cast_expression.expression);
        case VC_AST_TYPE_RELATION_EXPRESSION:
            if (!rewrite_async_lambda_capture_expression(rewrite, node->as.type_relation_expression.expression) ||
                !rewrite_async_lambda_capture_expression(rewrite, node->as.type_relation_expression.pattern_constant) ||
                !rewrite_async_lambda_capture_expression(rewrite, node->as.type_relation_expression.pattern_left) ||
                !rewrite_async_lambda_capture_expression(rewrite, node->as.type_relation_expression.pattern_right)) return false;
            for (size_t i = 0; i < node->as.type_relation_expression.pattern_properties.count; i++)
                if (!rewrite_async_lambda_capture_expression(
                        rewrite, node->as.type_relation_expression.pattern_properties.items[i])) return false;
            return true;
        case VC_AST_PROPERTY_PATTERN_MEMBER:
            return rewrite_async_lambda_capture_expression(rewrite, node->as.property_pattern_member.pattern);
        case VC_AST_UNARY_EXPRESSION:
            return rewrite_async_lambda_capture_expression(rewrite, node->as.unary_expression.operand);
        case VC_AST_AWAIT_EXPRESSION:
            return rewrite_async_lambda_capture_expression(rewrite, node->as.await_expression.operand);
        case VC_AST_RANGE_EXPRESSION:
            return rewrite_async_lambda_capture_expression(rewrite, node->as.range_expression.start) &&
                rewrite_async_lambda_capture_expression(rewrite, node->as.range_expression.end);
        case VC_AST_BINARY_EXPRESSION:
            return rewrite_async_lambda_capture_expression(rewrite, node->as.binary_expression.left) &&
                rewrite_async_lambda_capture_expression(rewrite, node->as.binary_expression.right);
        case VC_AST_CONDITIONAL_EXPRESSION:
            return rewrite_async_lambda_capture_expression(rewrite, node->as.conditional_expression.condition) &&
                rewrite_async_lambda_capture_expression(rewrite, node->as.conditional_expression.when_true) &&
                rewrite_async_lambda_capture_expression(rewrite, node->as.conditional_expression.when_false);
        case VC_AST_SWITCH_EXPRESSION:
            if (!rewrite_async_lambda_capture_expression(rewrite, node->as.switch_expression.expression)) return false;
            for (size_t i = 0; i < node->as.switch_expression.arms.count; i++)
                if (!rewrite_async_lambda_capture_expression(
                        rewrite, node->as.switch_expression.arms.items[i])) return false;
            return true;
        case VC_AST_SWITCH_EXPRESSION_ARM:
            return rewrite_async_lambda_capture_expression(rewrite, node->as.switch_expression_arm.pattern) &&
                rewrite_async_lambda_capture_expression(rewrite, node->as.switch_expression_arm.guard) &&
                rewrite_async_lambda_capture_expression(rewrite, node->as.switch_expression_arm.result);
        case VC_AST_ASSIGNMENT_EXPRESSION:
            return rewrite_async_lambda_capture_expression(rewrite, node->as.assignment_expression.left) &&
                rewrite_async_lambda_capture_expression(rewrite, node->as.assignment_expression.right);
        case VC_AST_LAMBDA_EXPRESSION:
            return node->as.lambda_expression.expression_body
                ? rewrite_async_lambda_capture_expression(rewrite, node->as.lambda_expression.body)
                : rewrite_async_lambda_capture_statement(rewrite, node->as.lambda_expression.body);
        case VC_AST_COMPILER_CLOSURE_FRAME_EXPRESSION:
            for (size_t i = 0; i < node->as.compiler_closure_frame_expression.captures.count; i++)
                if (!rewrite_async_lambda_capture_expression(rewrite,
                        node->as.compiler_closure_frame_expression.captures.items[i])) return false;
            return true;
        case VC_AST_COMPILER_CAPTURE_EXPRESSION:
            return rewrite_async_lambda_capture_expression(
                rewrite, node->as.compiler_capture_expression.target);
        case VC_AST_PARENTHESIZED_EXPRESSION:
            return rewrite_async_lambda_capture_expression(
                rewrite, node->as.parenthesized_expression.expression);
        default:
            return true;
    }
}


static bool async_lambda_body_contains_node(
    const VcSemanticLambda *lambda, const VcAstNode *wanted)
{
    if (lambda == NULL || lambda->node == NULL || wanted == NULL ||
        lambda->node->kind != VC_AST_LAMBDA_EXPRESSION)
        return false;
    const VcAstNode *body = lambda->node->as.lambda_expression.body;
    return lambda->node->as.lambda_expression.expression_body
        ? expression_contains_node(body, wanted)
        : statement_contains_node(body, wanted);
}

static bool async_lambda_capture_same(
    const VcSemanticCapture *left, const VcSemanticCapture *right)
{
    if (left == NULL || right == NULL || left->kind != right->kind)
        return false;
    if (left->kind == VC_SEM_CAPTURE_THIS)
        return true;
    return left->declaration_node == right->declaration_node &&
        left->scope_kind == right->scope_kind &&
        left->scope_index == right->scope_index &&
        left->scope_node == right->scope_node;
}

static bool async_lambda_capture_is_internal(
    const VcSemanticModel *semantic, size_t owner_lambda_index,
    const VcSemanticLambda *owner, const VcSemanticCapture *capture)
{
    if (semantic == NULL || owner == NULL || capture == NULL ||
        capture->kind == VC_SEM_CAPTURE_THIS)
        return false;
    if (capture->scope_kind == VC_SEM_CAPTURE_SCOPE_LAMBDA)
    {
        if (capture->scope_index == owner_lambda_index)
            return true;
        return capture->scope_index < semantic->lambda_count &&
            async_lambda_body_contains_node(owner,
                semantic->lambdas[capture->scope_index].node);
    }
    if (capture->scope_kind == VC_SEM_CAPTURE_SCOPE_FOREACH)
        return capture->scope_node != NULL &&
            async_lambda_body_contains_node(owner, capture->scope_node);
    return false;
}

static bool async_lambda_push_bridge_capture(
    VcSemanticCapture **captures, size_t *count, size_t *capacity,
    const VcSemanticCapture *capture)
{
    if (captures == NULL || count == NULL || capacity == NULL || capture == NULL)
        return false;
    for (size_t i = 0; i < *count; i++)
        if (async_lambda_capture_same(&(*captures)[i], capture))
            return true;
    if (*count == *capacity)
    {
        const size_t next = *capacity == 0 ? 4 : *capacity * 2;
        VcSemanticCapture *items = realloc(*captures, next * sizeof(*items));
        if (items == NULL)
            return false;
        *captures = items;
        *capacity = next;
    }
    (*captures)[(*count)++] = *capture;
    return true;
}

static bool collect_async_lambda_bridge_captures(
    const VcSemanticModel *semantic, const VcSemanticLambda *lambda,
    VcSemanticCapture **captures_out, size_t *capture_count_out)
{
    if (semantic == NULL || lambda == NULL || captures_out == NULL ||
        capture_count_out == NULL)
        return false;
    *captures_out = NULL;
    *capture_count_out = 0;

    size_t owner_lambda_index = (size_t)-1;
    for (size_t i = 0; i < semantic->lambda_count; i++)
        if (&semantic->lambdas[i] == lambda || semantic->lambdas[i].node == lambda->node)
        {
            owner_lambda_index = i;
            break;
        }
    if (owner_lambda_index == (size_t)-1)
        return false;

    VcSemanticCapture *captures = NULL;
    size_t count = 0;
    size_t capacity = 0;
    for (size_t i = 0; i < lambda->capture_count; i++)
        if (!async_lambda_push_bridge_capture(
                &captures, &count, &capacity, &lambda->captures[i]))
            goto fail;

    for (size_t i = 0; i < semantic->lambda_count; i++)
    {
        const VcSemanticLambda *nested = &semantic->lambdas[i];
        if (i == owner_lambda_index || nested->node == NULL ||
            !async_lambda_body_contains_node(lambda, nested->node))
            continue;
        for (size_t c = 0; c < nested->capture_count; c++)
        {
            const VcSemanticCapture *capture = &nested->captures[c];
            if (async_lambda_capture_is_internal(
                    semantic, owner_lambda_index, lambda, capture))
                continue;
            if (!async_lambda_push_bridge_capture(
                    &captures, &count, &capacity, capture))
                goto fail;
        }
    }

    *captures_out = captures;
    *capture_count_out = count;
    return true;

fail:
    free(captures);
    return false;
}

static bool lower_async_lambda(VcAstTree *tree, VcAstNode *owner,
    const VcSemanticModel *semantic, const VcSemanticLambda *lambda,
    size_t lambda_id, char *error, size_t error_size)
{
    if (tree == NULL || owner == NULL || semantic == NULL || lambda == NULL ||
        lambda->node == NULL || lambda->node->kind != VC_AST_LAMBDA_EXPRESSION)
        return false;

    VcSemanticCapture *bridge_captures = NULL;
    size_t bridge_capture_count = 0;
    if (!collect_async_lambda_bridge_captures(semantic, lambda,
            &bridge_captures, &bridge_capture_count))
    {
        async_lambda_error(error, error_size,
            "out of memory while collecting async lambda captures");
        return false;
    }
    VcSemanticLambda bridged_lambda = *lambda;
    bridged_lambda.captures = bridge_captures;
    bridged_lambda.capture_count = bridge_capture_count;
    bridged_lambda.capture_capacity = bridge_capture_count;
    const VcSemanticLambda *lowering_lambda = &bridged_lambda;

    for (size_t p = 0; p < lambda->parameter_count; p++)
    {
        const VcTokenKind modifier = lambda->parameter_modifiers[p];
        if (modifier == VC_TOKEN_KW_REF || modifier == VC_TOKEN_KW_OUT || modifier == VC_TOKEN_KW_IN)
        {
            async_lambda_error(error, error_size,
                "async lambda cannot target a delegate with %s parameters",
                vc_token_kind_name(modifier));
            free(bridge_captures);
            return false;
        }
    }

    const VcSourceLocation location = lambda->node->location;
    char method_name[96];
    const int written = snprintf(method_name, sizeof(method_name),
        "__voidc$async_lambda$%zu", lambda_id);
    if (written < 0 || (size_t)written >= sizeof(method_name))
    {
        async_lambda_error(error, error_size, "generated async lambda method name is too long");
        free(bridge_captures);
        return false;
    }

    if (lambda->delegate_type_index >= semantic->struct_count)
    {
        free(bridge_captures);
        return false;
    }
    const size_t invoke_index =
        semantic->structs[lambda->delegate_type_index].delegate_invoke_method_index;
    if (invoke_index >= semantic->method_count)
    {
        free(bridge_captures);
        return false;
    }
    const VcAstNode *invoke_node = semantic->methods[invoke_index].node;
    if (invoke_node == NULL || invoke_node->kind != VC_AST_METHOD_DECLARATION ||
        invoke_node->as.method_declaration.return_type == NULL ||
        invoke_node->as.method_declaration.parameters.count != lambda->parameter_count)
    {
        free(bridge_captures);
        return false;
    }

    VcAstNode **capture_sources = NULL;
    VcAstNode *closure_frame = NULL;
    if (lowering_lambda->capture_count != 0)
    {
        capture_sources = calloc(lowering_lambda->capture_count, sizeof(*capture_sources));
        closure_frame = vc_ast_new_node(tree,
            VC_AST_COMPILER_CLOSURE_FRAME_EXPRESSION, location);
        if (capture_sources == NULL || closure_frame == NULL)
            goto oom;
        for (size_t i = 0; i < lowering_lambda->capture_count; i++)
        {
            const VcSemanticCapture *capture = &lowering_lambda->captures[i];
            VcAstNode *source = identifier(tree, location, capture->name);
            if (source == NULL || !vc_ast_node_list_push(tree,
                    &closure_frame->as.compiler_closure_frame_expression.captures, source))
                goto oom;
            capture_sources[i] = source;
        }
    }

    VcAstNode *generated = vc_ast_new_node(tree, VC_AST_METHOD_DECLARATION, location);
    if (generated == NULL)
        goto oom;
    generated->as.method_declaration.modifiers =
        VC_AST_MOD_PRIVATE | VC_AST_MOD_STATIC | VC_AST_MOD_ASYNC;
    generated->as.method_declaration.return_type =
        clone_type(tree, invoke_node->as.method_declaration.return_type);
    generated->as.method_declaration.name = copy_text(tree, method_name);
    if (generated->as.method_declaration.return_type == NULL ||
        generated->as.method_declaration.name == NULL)
        goto oom;

    if (lowering_lambda->capture_count != 0)
    {
        VcAstNode *closure_parameter = make_parameter(tree, location,
            "__voidc_async_closure", named_type(tree, location, "object"));
        if (closure_parameter == NULL || closure_parameter->as.parameter.type == NULL ||
            !vc_ast_node_list_push(tree,
                &generated->as.method_declaration.parameters, closure_parameter))
            goto oom;
    }

    for (size_t p = 0; p < lambda->parameter_count; p++)
    {
        const VcAstNode *invoke_parameter =
            invoke_node->as.method_declaration.parameters.items[p];
        VcAstTypeRef *parameter_type = invoke_parameter != NULL &&
                invoke_parameter->kind == VC_AST_PARAMETER
            ? clone_type(tree, invoke_parameter->as.parameter.type)
            : NULL;
        const char *parameter_name = lambda->node->as.lambda_expression.parameters.items[p];
        VcAstNode *parameter = make_parameter(tree, location, parameter_name, parameter_type);
        if (parameter_type == NULL || parameter == NULL ||
            !vc_ast_node_list_push(tree, &generated->as.method_declaration.parameters, parameter))
            goto oom;
    }

    VcAstNode *original_body = lambda->node->as.lambda_expression.body;
    if (lowering_lambda->capture_count != 0)
    {
        VcAsyncLambdaCaptureRewrite rewrite = {
            .tree = tree,
            .semantic = semantic,
            .lambda = lowering_lambda,
            .owner_method = &semantic->methods[lambda->owner_method_index],
            .sources = capture_sources,
            .error = error,
            .error_size = error_size
        };
        const bool rewritten = lambda->node->as.lambda_expression.expression_body
            ? rewrite_async_lambda_capture_expression(&rewrite, original_body)
            : rewrite_async_lambda_capture_statement(&rewrite, original_body);
        if (!rewritten)
        {
            free(capture_sources);
            free(bridge_captures);
            return false;
        }
    }
    if (lambda->node->as.lambda_expression.expression_body)
    {
        VcAstNode *body = block(tree, location);
        VcAstNode *statement = lambda->async_result_type == VC_SEM_TYPE_VOID
            ? expression_statement(tree, location, original_body)
            : return_statement(tree, location, original_body);
        if (body == NULL || statement == NULL || !block_push(tree, body, statement))
            goto oom;
        generated->as.method_declaration.body = body;
    }
    else
    {
        if (original_body == NULL || original_body->kind != VC_AST_BLOCK_STATEMENT)
        {
            async_lambda_error(error, error_size, "async lambda block body is malformed");
            free(capture_sources);
            free(bridge_captures);
            return false;
        }
        generated->as.method_declaration.body = original_body;
    }

    VcAstNode *forward = call(tree, location, identifier(tree, location, method_name));
    if (forward == NULL || forward->as.call_expression.callee == NULL)
        goto oom;
    if (closure_frame != NULL && !vc_ast_node_list_push(
            tree, &forward->as.call_expression.arguments, closure_frame))
        goto oom;
    for (size_t p = 0; p < lambda->parameter_count; p++)
    {
        const char *parameter_name = lambda->node->as.lambda_expression.parameters.items[p];
        VcAstNode *argument = identifier(tree, location, parameter_name);
        if (argument == NULL || !vc_ast_node_list_push(
                tree, &forward->as.call_expression.arguments, argument))
            goto oom;
    }

    if (!vc_ast_node_list_push(tree, &owner->as.type_declaration.members, generated))
        goto oom;

    VcAstNode *forward_body = block(tree, location);
    VcAstNode *forward_return = return_statement(tree, location, forward);
    if (forward_body == NULL || forward_return == NULL ||
        !block_push(tree, forward_body, forward_return))
        goto oom;

    VcAstNode *lambda_node = (VcAstNode *)lambda->node;
    lambda_node->as.lambda_expression.body = forward_body;
    lambda_node->as.lambda_expression.expression_body = false;
    lambda_node->as.lambda_expression.is_async = false;
    lambda_node->as.lambda_expression.lowered_async_forwarder = true;
    free(capture_sources);
    free(bridge_captures);
    return true;

oom:
    free(capture_sources);
    free(bridge_captures);
    async_lambda_error(error, error_size, "out of memory while lowering async lambda");
    return false;
}

bool vc_lower_async_lambdas(VcAstTree **trees, size_t tree_count,
    const VcSemanticModel *semantic, char *error, size_t error_size)
{
    if (error != NULL && error_size != 0)
        error[0] = '\0';
    if (semantic == NULL)
        return true;

    size_t generated_id = 0;
    for (size_t i = 0; i < semantic->lambda_count; i++)
    {
        const VcSemanticLambda *lambda = &semantic->lambdas[i];
        if (!lambda->is_async)
            continue;
        if (lambda->owner_method_index >= semantic->method_count)
        {
            async_lambda_error(error, error_size, "async lambda owner method is unavailable");
            return false;
        }
        const VcSemanticMethod *owner_method = &semantic->methods[lambda->owner_method_index];
        if (!owner_method->has_owner_struct || owner_method->owner_struct_index >= semantic->struct_count)
        {
            async_lambda_error(error, error_size, "async lambda owner type is unavailable");
            return false;
        }
        VcAstNode *owner = (VcAstNode *)semantic->structs[owner_method->owner_struct_index].node;
        VcAstTree *tree = tree_containing_declaration(trees, tree_count, owner);
        if (owner == NULL || tree == NULL || owner->kind != VC_AST_TYPE_DECLARATION)
        {
            async_lambda_error(error, error_size, "async lambda source tree is unavailable");
            return false;
        }
        if (!lower_async_lambda(tree, owner, semantic, lambda,
                generated_id++, error, error_size))
            return false;
    }
    return true;
}

static bool lower_declarations(VcAstTree *tree, VcAstNodeList *declarations,
    size_t *async_id, const VcSemanticModel *semantic,
    const VcSemanticModel *generated_semantic,
    char *error, size_t error_size)
{
    const size_t original_count = declarations->count;
    for (size_t i = 0; i < original_count; i++)
    {
        VcAstNode *node = declarations->items[i];
        if (node->kind == VC_AST_NAMESPACE_DECLARATION)
        {
            if (!lower_declarations(tree, &node->as.namespace_declaration.declarations,
                    async_id, semantic, generated_semantic, error, error_size))
                return false;
            continue;
        }
        if (node->kind != VC_AST_TYPE_DECLARATION)
            continue;

        const size_t member_count = node->as.type_declaration.members.count;
        for (size_t member_index = 0; member_index < member_count; member_index++)
        {
            VcAstNode *member = node->as.type_declaration.members.items[member_index];
            if (member->kind != VC_AST_METHOD_DECLARATION ||
                member->as.method_declaration.is_constructor)
                continue;
            if (!lower_method(tree, declarations, node, member, (*async_id)++,
                    semantic, generated_semantic, error, error_size))
                return false;
        }
    }
    return true;
}

static bool declarations_have_async(const VcAstNodeList *declarations)
{
    for (size_t i = 0; i < declarations->count; i++)
    {
        const VcAstNode *node = declarations->items[i];
        if (node->kind == VC_AST_NAMESPACE_DECLARATION)
        {
            if (declarations_have_async(&node->as.namespace_declaration.declarations))
                return true;
            continue;
        }
        if (node->kind != VC_AST_TYPE_DECLARATION)
            continue;
        for (size_t member_index = 0; member_index < node->as.type_declaration.members.count; member_index++)
        {
            const VcAstNode *member = node->as.type_declaration.members.items[member_index];
            if (member->kind == VC_AST_METHOD_DECLARATION &&
                !member->as.method_declaration.is_constructor &&
                (member->as.method_declaration.modifiers & VC_AST_MOD_ASYNC) != 0)
                return true;
        }
    }
    return false;
}

bool vc_has_async_methods(VcAstTree **trees, size_t tree_count)
{
    for (size_t i = 0; i < tree_count; i++)
    {
        VcAstTree *tree = trees[i];
        if (tree != NULL && tree->root != NULL &&
            tree->root->kind == VC_AST_COMPILATION_UNIT &&
            declarations_have_async(&tree->root->as.compilation_unit.declarations))
            return true;
    }
    return false;
}

bool vc_lower_async_methods(VcAstTree **trees, size_t tree_count,
    const VcSemanticModel *semantic, const VcSemanticModel *generated_semantic,
    char *error, size_t error_size)
{
    if (error != NULL && error_size != 0)
        error[0] = '\0';
    size_t async_id = 0;
    for (size_t i = 0; i < tree_count; i++)
    {
        VcAstTree *tree = trees[i];
        if (tree == NULL || tree->root == NULL || tree->root->kind != VC_AST_COMPILATION_UNIT)
            continue;
        if (!lower_declarations(tree, &tree->root->as.compilation_unit.declarations,
                &async_id, semantic, generated_semantic, error, error_size))
            return false;
    }
    return true;
}
