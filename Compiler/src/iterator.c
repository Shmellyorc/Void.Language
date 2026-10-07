#include "iterator.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VC_ITERATOR_PREFIX "__voidc$iterator$"
#define VC_ITERATOR_ASYNC_FOREACH_CLEANUP "__voidc_iterator_async_foreach_cleanup"

typedef struct VcIteratorCapture
{
    const char *name;
    const VcAstNode *declaration;
    VcAstTypeRef *type;
} VcIteratorCapture;

typedef enum VcIteratorTerminator
{
    VC_ITER_GOTO,
    VC_ITER_BRANCH,
    VC_ITER_SWITCH,
    VC_ITER_YIELD,
    VC_ITER_FINISH
} VcIteratorTerminator;

typedef struct VcIteratorBlock
{
    int state;
    VcSourceLocation location;
    VcAstNodeList statements;
    VcIteratorTerminator terminator;
    VcAstNode *expression;
    int target;
    int false_target;
    const VcAstNode *switch_source;
    int *switch_targets;
    size_t switch_target_count;
    const VcAstNode **active_finally_blocks;
    size_t active_finally_count;
} VcIteratorBlock;

typedef struct VcIteratorContext
{
    VcAstTree *tree;
    const VcAstNode *owner;
    VcAstNode *method;
    const VcSemanticModel *semantic;
    VcIteratorCapture *captures;
    size_t capture_count;
    size_t capture_capacity;
    size_t local_id;
    VcIteratorBlock *blocks;
    size_t block_count;
    size_t block_capacity;
    const VcAstNode **active_finally_blocks;
    size_t active_finally_count;
    size_t active_finally_capacity;
    int finish_state;
    size_t foreach_id;
    bool preserve_pattern_locals;
    bool async_iterator;
    char *error;
    size_t error_size;
} VcIteratorContext;

static void set_error(char *error, size_t error_size, const char *format, ...)
{
    if (error == NULL || error_size == 0 || error[0] != '\0')
        return;
    va_list args;
    va_start(args, format);
    vsnprintf(error, error_size, format, args);
    va_end(args);
}

static void context_error(VcIteratorContext *context, const char *format, ...)
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

static bool type_name_matches(const VcAstTypeRef *type, const char *name)
{
    if (type == NULL || type->name == NULL || name == NULL)
        return false;
    if (strcmp(type->name, name) == 0)
        return true;
    const char *last_dot = strrchr(type->name, '.');
    return last_dot != NULL && strcmp(last_dot + 1, name) == 0;
}

static VcAstTypeRef *type_from_semantic(
    VcIteratorContext *context,
    VcSemanticType type,
    VcSourceLocation location)
{
    if (context->semantic == NULL)
        return NULL;

    if (vc_semantic_type_is_array(type))
    {
        VcAstTypeRef *element = type_from_semantic(context,
            vc_semantic_array_element_type(context->semantic, type), location);
        if (element == NULL)
            return NULL;
        const size_t rank = vc_semantic_array_rank(context->semantic, type);
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
        VcAstTypeRef *underlying = type_from_semantic(context,
            vc_semantic_nullable_underlying_type(context->semantic, type), location);
        if (underlying == NULL || underlying->nullable)
            return NULL;
        underlying->nullable = true;
        return underlying;
    }

    if (vc_semantic_type_is_pointer(type))
    {
        VcAstTypeRef *element = type_from_semantic(context,
            vc_semantic_pointer_element_type(context->semantic, type), location);
        if (element == NULL)
            return NULL;
        element->pointer_depth++;
        return element;
    }

    const char *name = vc_semantic_type_name(context->semantic, type);
    if (name == NULL || name[0] == '<')
        return NULL;
    return named_type(context->tree, location, name);
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
    snprintf(text, sizeof(text), "%d", value);
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

static VcAstNode *null_literal(VcAstTree *tree, VcSourceLocation location)
{
    VcAstNode *node = vc_ast_new_node(tree, VC_AST_LITERAL_EXPRESSION, location);
    if (node == NULL)
        return NULL;
    node->as.literal_expression.literal_kind = VC_AST_LITERAL_NULL;
    node->as.literal_expression.text = copy_text(tree, "null");
    return node->as.literal_expression.text != NULL ? node : NULL;
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

static VcAstNode *conditional_expression(VcAstTree *tree, VcSourceLocation location,
    VcAstNode *condition, VcAstNode *when_true, VcAstNode *when_false)
{
    VcAstNode *node = vc_ast_new_node(tree, VC_AST_CONDITIONAL_EXPRESSION, location);
    if (node == NULL)
        return NULL;
    node->as.conditional_expression.condition = condition;
    node->as.conditional_expression.when_true = when_true;
    node->as.conditional_expression.when_false = when_false;
    return node;
}

static bool node_has_attribute(const VcAstNode *node, const char *name)
{
    if (node == NULL || name == NULL)
        return false;
    for (size_t i = 0; i < node->attributes.count; i++)
    {
        const VcAstNode *attribute = node->attributes.items[i];
        if (attribute != NULL && attribute->kind == VC_AST_ATTRIBUTE &&
            attribute->as.attribute.name != NULL &&
            strcmp(attribute->as.attribute.name, name) == 0)
            return true;
    }
    return false;
}

static void remove_node_attribute(VcAstNode *node, const char *name)
{
    if (node == NULL || name == NULL)
        return;
    size_t write = 0;
    for (size_t i = 0; i < node->attributes.count; i++)
    {
        VcAstNode *attribute = node->attributes.items[i];
        if (attribute != NULL && attribute->kind == VC_AST_ATTRIBUTE &&
            attribute->as.attribute.name != NULL &&
            strcmp(attribute->as.attribute.name, name) == 0)
            continue;
        node->attributes.items[write++] = attribute;
    }
    node->attributes.count = write;
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

static VcAstNode *call(VcAstTree *tree, VcSourceLocation location, VcAstNode *callee)
{
    VcAstNode *node = vc_ast_new_node(tree, VC_AST_CALL_EXPRESSION, location);
    if (node == NULL)
        return NULL;
    node->as.call_expression.callee = callee;
    return node;
}

static VcAstNode *index_expression(VcAstTree *tree, VcSourceLocation location,
    VcAstNode *target, VcAstNode *index)
{
    VcAstNode *node = vc_ast_new_node(tree, VC_AST_INDEX_EXPRESSION, location);
    if (node == NULL)
        return NULL;
    node->as.index_expression.target = target;
    node->as.index_expression.index = index;
    return node;
}

static VcAstNode *assignment_target(VcAstTree *tree, VcSourceLocation location,
    VcAstNode *left, VcAstNode *value)
{
    VcAstNode *node = vc_ast_new_node(tree, VC_AST_ASSIGNMENT_EXPRESSION, location);
    if (node == NULL)
        return NULL;
    node->as.assignment_expression.operator_kind = VC_TOKEN_EQUAL;
    node->as.assignment_expression.left = left;
    node->as.assignment_expression.right = value;
    return node;
}

static VcAstNode *assignment(VcAstTree *tree, VcSourceLocation location,
    const char *name, VcAstNode *value)
{
    return assignment_target(tree, location, identifier(tree, location, name), value);
}

static VcAstNode *this_member_assignment(VcAstTree *tree, VcSourceLocation location,
    const char *name, VcAstNode *value)
{
    VcAstNode *self = identifier(tree, location, "this");
    if (self == NULL)
        return NULL;
    VcAstNode *left = member_access(tree, location, self, name);
    return left != NULL ? assignment_target(tree, location, left, value) : NULL;
}

static VcAstNode *expression_statement(VcAstTree *tree, VcSourceLocation location, VcAstNode *expression)
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

static VcAstNode *await_expression(VcAstTree *tree, VcSourceLocation location, VcAstNode *operand)
{
    VcAstNode *node = vc_ast_new_node(tree, VC_AST_AWAIT_EXPRESSION, location);
    if (node == NULL)
        return NULL;
    node->as.await_expression.operand = operand;
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

static VcAstNode *make_async_foreach_cleanup(
    VcIteratorContext *context, VcSourceLocation location, const char *enumerator_name)
{
    VcAstNode *dispose_call = call(context->tree, location,
        member_access(context->tree, location,
            identifier(context->tree, location, enumerator_name), "DisposeAsync"));
    VcAstNode *dispose_await = await_expression(context->tree, location, dispose_call);
    VcAstNode *dispose_statement = expression_statement(
        context->tree, location, dispose_await);
    VcAstNode *cleanup = block(context->tree, location);
    if (dispose_call == NULL || dispose_await == NULL || dispose_statement == NULL ||
        cleanup == NULL || !block_push(context->tree, cleanup, dispose_statement))
        return NULL;
    cleanup->argument_name = copy_text(context->tree, VC_ITERATOR_ASYNC_FOREACH_CLEANUP);
    return cleanup->argument_name != NULL ? cleanup : NULL;
}

static const char *async_foreach_cleanup_enumerator(const VcAstNode *cleanup)
{
    if (cleanup == NULL || cleanup->kind != VC_AST_BLOCK_STATEMENT ||
        cleanup->argument_name == NULL ||
        strcmp(cleanup->argument_name, VC_ITERATOR_ASYNC_FOREACH_CLEANUP) != 0 ||
        cleanup->as.block_statement.statements.count != 1)
        return NULL;
    const VcAstNode *statement = cleanup->as.block_statement.statements.items[0];
    if (statement == NULL || statement->kind != VC_AST_EXPRESSION_STATEMENT)
        return NULL;
    const VcAstNode *await_node = statement->as.expression_statement.expression;
    if (await_node == NULL || await_node->kind != VC_AST_AWAIT_EXPRESSION)
        return NULL;
    const VcAstNode *call_node = await_node->as.await_expression.operand;
    if (call_node == NULL || call_node->kind != VC_AST_CALL_EXPRESSION ||
        call_node->as.call_expression.callee == NULL ||
        call_node->as.call_expression.callee->kind != VC_AST_MEMBER_ACCESS_EXPRESSION)
        return NULL;
    const VcAstNode *member = call_node->as.call_expression.callee;
    if (member->as.member_access_expression.member == NULL ||
        strcmp(member->as.member_access_expression.member, "DisposeAsync") != 0 ||
        member->as.member_access_expression.target == NULL ||
        member->as.member_access_expression.target->kind != VC_AST_IDENTIFIER_EXPRESSION)
        return NULL;
    return member->as.member_access_expression.target->as.identifier_expression.name;
}

static VcAstNode *active_finally_instance(
    VcIteratorContext *context, const VcAstNode *cleanup)
{
    const char *enumerator_name = async_foreach_cleanup_enumerator(cleanup);
    if (enumerator_name != NULL)
        return make_async_foreach_cleanup(context, cleanup->location, enumerator_name);
    return (VcAstNode *)cleanup;
}

static bool node_contains_yield(const VcAstNode *node)
{
    if (node == NULL)
        return false;
    switch (node->kind)
    {
        case VC_AST_YIELD_RETURN_STATEMENT:
        case VC_AST_YIELD_BREAK_STATEMENT:
            return true;
        case VC_AST_BLOCK_STATEMENT:
            for (size_t i = 0; i < node->as.block_statement.statements.count; i++)
                if (node_contains_yield(node->as.block_statement.statements.items[i])) return true;
            return false;
        case VC_AST_IF_STATEMENT:
            return node_contains_yield(node->as.if_statement.then_statement) ||
                node_contains_yield(node->as.if_statement.else_statement);
        case VC_AST_WHILE_STATEMENT:
            return node_contains_yield(node->as.while_statement.body);
        case VC_AST_DO_WHILE_STATEMENT:
            return node_contains_yield(node->as.while_statement.body);
        case VC_AST_TRY_STATEMENT:
            if (node_contains_yield(node->as.try_statement.try_block)) return true;
            for (size_t i = 0; i < node->as.try_statement.catches.count; i++)
                if (node_contains_yield(node->as.try_statement.catches.items[i])) return true;
            return node_contains_yield(node->as.try_statement.finally_block);
        case VC_AST_CATCH_CLAUSE:
            return node_contains_yield(node->as.catch_clause.body);
        case VC_AST_FOR_STATEMENT:
            return node_contains_yield(node->as.for_statement.initializer) ||
                node_contains_yield(node->as.for_statement.body);
        case VC_AST_FOREACH_STATEMENT:
            return node_contains_yield(node->as.foreach_statement.body);
        case VC_AST_USING_STATEMENT:
            return node_contains_yield(node->as.using_statement.body);
        case VC_AST_LOCK_STATEMENT:
            return node_contains_yield(node->as.lock_statement.body);
        case VC_AST_SWITCH_STATEMENT:
            for (size_t i = 0; i < node->as.switch_statement.sections.count; i++)
                if (node_contains_yield(node->as.switch_statement.sections.items[i])) return true;
            return false;
        case VC_AST_SWITCH_SECTION:
            for (size_t i = 0; i < node->as.switch_section.statements.count; i++)
                if (node_contains_yield(node->as.switch_section.statements.items[i])) return true;
            return false;
        default:
            return false;
    }
}

static bool capture_exists(const VcIteratorContext *context, const char *name)
{
    for (size_t i = 0; i < context->capture_count; i++)
        if (strcmp(context->captures[i].name, name) == 0) return true;
    return false;
}

static VcIteratorCapture *capture_for_declaration(
    VcIteratorContext *context,
    const VcAstNode *declaration)
{
    if (declaration == NULL)
        return NULL;
    for (size_t i = 0; i < context->capture_count; i++)
        if (context->captures[i].declaration == declaration) return &context->captures[i];
    return NULL;
}

static const VcIteratorCapture *capture_for_identifier(
    const VcIteratorContext *context,
    const VcAstNode *identifier_node)
{
    if (context->semantic == NULL || identifier_node == NULL)
        return NULL;
    const VcSemanticBinding *binding = vc_semantic_binding(context->semantic, identifier_node);
    if (binding == NULL || binding->declaration_node == NULL)
        return NULL;
    for (size_t i = 0; i < context->capture_count; i++)
        if (context->captures[i].declaration == binding->declaration_node)
            return &context->captures[i];
    return NULL;
}

static VcAstTypeRef *capture_type(const VcIteratorContext *context, const char *name)
{
    for (size_t i = 0; i < context->capture_count; i++)
        if (strcmp(context->captures[i].name, name) == 0) return context->captures[i].type;
    return NULL;
}

static VcAstTypeRef *owner_member_type(const VcIteratorContext *context, const char *name)
{
    if (context->owner == NULL || context->owner->kind != VC_AST_TYPE_DECLARATION || name == NULL)
        return NULL;
    for (size_t i = 0; i < context->owner->as.type_declaration.members.count; i++)
    {
        VcAstNode *member = context->owner->as.type_declaration.members.items[i];
        if (member->kind == VC_AST_FIELD_DECLARATION &&
            member->as.field_declaration.name != NULL &&
            strcmp(member->as.field_declaration.name, name) == 0)
            return member->as.field_declaration.type;
        if (member->kind == VC_AST_PROPERTY_DECLARATION &&
            member->as.property_declaration.name != NULL &&
            strcmp(member->as.property_declaration.name, name) == 0)
            return member->as.property_declaration.type;
    }
    return NULL;
}

static VcAstTypeRef *owner_method_return_type(const VcIteratorContext *context, const char *name)
{
    if (context->owner == NULL || context->owner->kind != VC_AST_TYPE_DECLARATION || name == NULL)
        return NULL;
    VcAstTypeRef *result = NULL;
    for (size_t i = 0; i < context->owner->as.type_declaration.members.count; i++)
    {
        VcAstNode *member = context->owner->as.type_declaration.members.items[i];
        if (member->kind != VC_AST_METHOD_DECLARATION || member->as.method_declaration.is_constructor ||
            member->as.method_declaration.name == NULL ||
            strcmp(member->as.method_declaration.name, name) != 0)
            continue;
        if (result != NULL)
            return NULL;
        result = member->as.method_declaration.return_type;
    }
    return result;
}

static VcAstTypeRef *expression_type(VcIteratorContext *context, const VcAstNode *node)
{
    if (node == NULL)
        return NULL;
    if (context->semantic != NULL)
    {
        const VcSemanticBinding *binding = vc_semantic_binding(context->semantic, node);
        if (binding != NULL && binding->type != VC_SEM_TYPE_ERROR &&
            binding->type != VC_SEM_TYPE_UNKNOWN && binding->type != VC_SEM_TYPE_VOID)
        {
            VcAstTypeRef *type = type_from_semantic(context, binding->type, node->location);
            if (type != NULL)
                return type;
        }
    }
    switch (node->kind)
    {
        case VC_AST_IDENTIFIER_EXPRESSION:
        {
            const char *name = node->as.identifier_expression.name;
            VcAstTypeRef *type = capture_type(context, name);
            if (type != NULL)
                return type;
            return owner_member_type(context, name);
        }
        case VC_AST_MEMBER_ACCESS_EXPRESSION:
            if (node->as.member_access_expression.target != NULL &&
                node->as.member_access_expression.target->kind == VC_AST_IDENTIFIER_EXPRESSION)
            {
                const char *target =
                    node->as.member_access_expression.target->as.identifier_expression.name;
                if (strcmp(target, "this") == 0 || strcmp(target, "_this") == 0)
                    return owner_member_type(context, node->as.member_access_expression.member);
            }
            return NULL;
        case VC_AST_NEW_EXPRESSION:
        {
            if (node->as.new_expression.type == NULL)
                return NULL;
            VcAstTypeRef *type = clone_type(context->tree, node->as.new_expression.type);
            if (type == NULL)
                return NULL;
            if (node->as.new_expression.is_array &&
                node->as.new_expression.array_lengths.count == 1 &&
                type->rectangular_rank == 0)
                type->array_rank++;
            return type;
        }
        case VC_AST_CALL_EXPRESSION:
        {
            const VcAstNode *callee = node->as.call_expression.callee;
            if (callee == NULL)
                return NULL;
            if (callee->kind == VC_AST_IDENTIFIER_EXPRESSION)
                return owner_method_return_type(context, callee->as.identifier_expression.name);
            if (callee->kind == VC_AST_MEMBER_ACCESS_EXPRESSION &&
                callee->as.member_access_expression.target != NULL &&
                callee->as.member_access_expression.target->kind == VC_AST_IDENTIFIER_EXPRESSION)
            {
                const char *target =
                    callee->as.member_access_expression.target->as.identifier_expression.name;
                if (strcmp(target, "this") == 0 || strcmp(target, "_this") == 0)
                    return owner_method_return_type(context, callee->as.member_access_expression.member);
            }
            return NULL;
        }
        case VC_AST_PARENTHESIZED_EXPRESSION:
            return expression_type(context, node->as.parenthesized_expression.expression);
        default:
            return NULL;
    }
}

static VcAstTypeRef *enumerator_type(VcIteratorContext *context, VcAstTypeRef *item_type)
{
    VcAstTypeRef *type = named_type(context->tree, item_type->location, "IEnumerator");
    VcAstTypeRef *item = clone_type(context->tree, item_type);
    if (type == NULL || item == NULL ||
        !vc_ast_type_list_push(context->tree, &type->generic_arguments, item))
        return NULL;
    return type;
}

static bool generated_capture_name(VcIteratorContext *context, const char *kind,
    size_t id, char *buffer, size_t buffer_size)
{
    const int written = snprintf(buffer, buffer_size, "__foreach_%s_%zu", kind, id);
    if (written < 0 || (size_t)written >= buffer_size)
        return false;
    return !capture_exists(context, buffer);
}

static bool add_capture(VcIteratorContext *context, const char *name,
    const VcAstNode *declaration, VcAstTypeRef *type)
{
    if (name == NULL || type == NULL)
        return false;
    if (strcmp(name, "_state") == 0 || strcmp(name, "_current") == 0 || strcmp(name, "_this") == 0)
    {
        context_error(context, "iterator local or parameter name '%s' is reserved", name);
        return false;
    }
    if (capture_exists(context, name))
    {
        context_error(context,
            "iterator capture does not allow shadowed captured name '%s'", name);
        return false;
    }
    if (context->capture_count == context->capture_capacity)
    {
        const size_t capacity = context->capture_capacity == 0 ? 8 : context->capture_capacity * 2;
        VcIteratorCapture *captures = realloc(context->captures, capacity * sizeof(*captures));
        if (captures == NULL)
        {
            context_error(context, "out of memory while collecting iterator captures");
            return false;
        }
        context->captures = captures;
        context->capture_capacity = capacity;
    }
    context->captures[context->capture_count++] = (VcIteratorCapture){name, declaration, type};
    return true;
}

static bool add_local_capture(VcIteratorContext *context, VcAstNode *declaration)
{
    if (context->semantic == NULL || declaration == NULL)
        return false;
    const VcSemanticBinding *binding = vc_semantic_binding(context->semantic, declaration);
    if (binding == NULL || binding->type == VC_SEM_TYPE_ERROR ||
        binding->type == VC_SEM_TYPE_UNKNOWN || binding->type == VC_SEM_TYPE_VOID)
    {
        context_error(context, "iterator local type could not be resolved");
        return false;
    }

    VcAstTypeRef *type = type_from_semantic(context, binding->type, declaration->location);
    if (type == NULL)
    {
        context_error(context, "iterator local type could not be represented");
        return false;
    }

    const VcAstTypeRef *declared_type = NULL;
    if (declaration->kind == VC_AST_LOCAL_DECLARATION)
        declared_type = declaration->as.local_declaration.type;
    else if (declaration->kind == VC_AST_FOREACH_STATEMENT)
        declared_type = declaration->as.foreach_statement.type;
    if (declared_type != NULL)
    {
        type->generic_constraint_parameter = declared_type->generic_constraint_parameter;
        type->generic_parameter_origin = declared_type->generic_parameter_origin;
    }
    if (binding->generic_parameter_origin != NULL)
        type->generic_parameter_origin = binding->generic_parameter_origin;

    char name[64];
    do
    {
        const int written = snprintf(name, sizeof(name), "__local_%zu", context->local_id++);
        if (written < 0 || (size_t)written >= sizeof(name))
            return false;
    } while (capture_exists(context, name));

    char *copy = copy_text(context->tree, name);
    return copy != NULL && add_capture(context, copy, declaration, type);
}

static bool add_pattern_capture(VcIteratorContext *context, VcAstNode *pattern)
{
    if (context->semantic == NULL || pattern == NULL ||
        pattern->kind != VC_AST_TYPE_RELATION_EXPRESSION ||
        pattern->as.type_relation_expression.pattern_name == NULL)
        return false;
    const VcSemanticBinding *binding = vc_semantic_binding(context->semantic, pattern);
    if (binding == NULL || !binding->has_type_relation ||
        binding->relation_target_type == VC_SEM_TYPE_ERROR ||
        binding->relation_target_type == VC_SEM_TYPE_UNKNOWN ||
        binding->relation_target_type == VC_SEM_TYPE_VOID)
    {
        context_error(context, "iterator pattern local type could not be resolved");
        return false;
    }

    VcAstTypeRef *type = type_from_semantic(
        context, binding->relation_target_type, pattern->location);
    if (type == NULL)
    {
        context_error(context, "iterator pattern local type could not be represented");
        return false;
    }

    char name[64];
    do
    {
        const int written = snprintf(name, sizeof(name), "__local_%zu", context->local_id++);
        if (written < 0 || (size_t)written >= sizeof(name))
            return false;
    } while (capture_exists(context, name));

    char *copy = copy_text(context->tree, name);
    return copy != NULL && add_capture(context, copy, pattern, type);
}

static bool collect_pattern_captures(VcIteratorContext *context, VcAstNode *pattern)
{
    if (pattern == NULL || pattern->kind != VC_AST_TYPE_RELATION_EXPRESSION)
        return true;
    if (pattern->as.type_relation_expression.pattern_name != NULL &&
        capture_for_declaration(context, pattern) == NULL &&
        !add_pattern_capture(context, pattern))
        return false;
    if (!collect_pattern_captures(context,
            pattern->as.type_relation_expression.pattern_left) ||
        !collect_pattern_captures(context,
            pattern->as.type_relation_expression.pattern_right))
        return false;
    for (size_t i = 0; i < pattern->as.type_relation_expression.pattern_properties.count; i++)
    {
        VcAstNode *member = pattern->as.type_relation_expression.pattern_properties.items[i];
        if (member == NULL || member->kind != VC_AST_PROPERTY_PATTERN_MEMBER ||
            !collect_pattern_captures(context, member->as.property_pattern_member.pattern))
            return false;
    }
    return true;
}

static VcAstNode *declaration_pattern_condition(VcAstNode *expression)
{
    while (expression != NULL && expression->kind == VC_AST_PARENTHESIZED_EXPRESSION)
        expression = expression->as.parenthesized_expression.expression;
    if (expression == NULL || expression->kind != VC_AST_TYPE_RELATION_EXPRESSION ||
        expression->as.type_relation_expression.operator_kind != VC_TOKEN_KW_IS)
        return NULL;
    return expression;
}

static bool collect_condition_pattern_captures(
    VcIteratorContext *context,
    VcAstNode *expression)
{
    VcAstNode *pattern = declaration_pattern_condition(expression);
    return pattern == NULL || collect_pattern_captures(context, pattern);
}

static bool collect_locals(VcIteratorContext *context, VcAstNode *node)
{
    if (node == NULL)
        return true;
    switch (node->kind)
    {
        case VC_AST_BLOCK_STATEMENT:
            for (size_t i = 0; i < node->as.block_statement.statements.count; i++)
                if (!collect_locals(context, node->as.block_statement.statements.items[i])) return false;
            return true;
        case VC_AST_LOCAL_DECLARATION:
            if (node->as.local_declaration.initializer != NULL &&
                node->as.local_declaration.initializer->kind == VC_AST_STACKALLOC_EXPRESSION)
            {
                context_error(context,
                    "stackalloc local '%s' cannot be used in an iterator method",
                    node->as.local_declaration.name);
                return false;
            }
            return add_local_capture(context, node);
        case VC_AST_IF_STATEMENT:
            return collect_condition_pattern_captures(context, node->as.if_statement.condition) &&
                collect_locals(context, node->as.if_statement.then_statement) &&
                collect_locals(context, node->as.if_statement.else_statement);
        case VC_AST_WHILE_STATEMENT:
            return collect_condition_pattern_captures(context, node->as.while_statement.condition) &&
                collect_locals(context, node->as.while_statement.body);
        case VC_AST_DO_WHILE_STATEMENT:
            return collect_locals(context, node->as.while_statement.body);
        case VC_AST_TRY_STATEMENT:
            if (!collect_locals(context, node->as.try_statement.try_block)) return false;
            for (size_t i = 0; i < node->as.try_statement.catches.count; i++)
                if (!collect_locals(context, node->as.try_statement.catches.items[i])) return false;
            return collect_locals(context, node->as.try_statement.finally_block);
        case VC_AST_CATCH_CLAUSE:
            return collect_locals(context, node->as.catch_clause.body);
        case VC_AST_FOR_STATEMENT:
            return collect_locals(context, node->as.for_statement.initializer) &&
                collect_locals(context, node->as.for_statement.body);
        case VC_AST_FOREACH_STATEMENT:
        {
            if (!add_local_capture(context, node))
                return false;
            return collect_locals(context, node->as.foreach_statement.body);
        }
        case VC_AST_USING_STATEMENT:
            return collect_locals(context, node->as.using_statement.declaration) &&
                collect_locals(context, node->as.using_statement.body);
        case VC_AST_LOCK_STATEMENT:
            return collect_locals(context, node->as.lock_statement.body);
        case VC_AST_SWITCH_STATEMENT:
            for (size_t i = 0; i < node->as.switch_statement.sections.count; i++)
                if (!collect_locals(context, node->as.switch_statement.sections.items[i])) return false;
            return true;
        case VC_AST_SWITCH_SECTION:
            for (size_t i = 0; i < node->as.switch_section.labels.count; i++)
            {
                VcAstNode *label = node->as.switch_section.labels.items[i];
                if (label != NULL && label->kind == VC_AST_SWITCH_LABEL &&
                    label->as.switch_label.is_pattern &&
                    !collect_pattern_captures(context, label->as.switch_label.pattern))
                    return false;
            }
            for (size_t i = 0; i < node->as.switch_section.statements.count; i++)
                if (!collect_locals(context, node->as.switch_section.statements.items[i])) return false;
            return true;
        default:
            return true;
    }
}

static bool owner_has_instance_member(const VcAstNode *owner, const char *name)
{
    if (owner == NULL || owner->kind != VC_AST_TYPE_DECLARATION || name == NULL)
        return false;
    for (size_t i = 0; i < owner->as.type_declaration.members.count; i++)
    {
        const VcAstNode *member = owner->as.type_declaration.members.items[i];
        const char *member_name = NULL;
        uint32_t modifiers = 0;
        switch (member->kind)
        {
            case VC_AST_FIELD_DECLARATION:
                member_name = member->as.field_declaration.name;
                modifiers = member->as.field_declaration.modifiers;
                break;
            case VC_AST_PROPERTY_DECLARATION:
                member_name = member->as.property_declaration.name;
                modifiers = member->as.property_declaration.modifiers;
                break;
            case VC_AST_METHOD_DECLARATION:
                if (member->as.method_declaration.is_constructor) continue;
                member_name = member->as.method_declaration.name;
                modifiers = member->as.method_declaration.modifiers;
                break;
            default:
                continue;
        }
        if (member_name != NULL && strcmp(member_name, name) == 0 &&
            (modifiers & VC_AST_MOD_STATIC) == 0)
            return true;
    }
    return false;
}

static bool semantic_binding_is_instance_member(
    const VcIteratorContext *context,
    const VcSemanticBinding *binding)
{
    if (context->semantic == NULL || binding == NULL)
        return false;

    if (binding->has_method && binding->method_index < context->semantic->method_count)
        return !context->semantic->methods[binding->method_index].is_static;

    if (binding->has_field && binding->struct_index < context->semantic->struct_count)
    {
        const VcSemanticStruct *owner = &context->semantic->structs[binding->struct_index];
        return binding->field_index < owner->field_count &&
            !owner->fields[binding->field_index].is_static;
    }

    if (binding->has_property && binding->struct_index < context->semantic->struct_count)
    {
        const VcSemanticStruct *owner = &context->semantic->structs[binding->struct_index];
        return binding->property_index < owner->property_count &&
            !owner->properties[binding->property_index].is_static;
    }

    return false;
}

static bool rewrite_expression(VcIteratorContext *context, VcAstNode *node)
{
    if (node == NULL)
        return true;
    switch (node->kind)
    {
        case VC_AST_IDENTIFIER_EXPRESSION:
        {
            char *name = node->as.identifier_expression.name;
            if (strcmp(name, "base") == 0)
                return true;
            if (strcmp(name, "this") == 0)
            {
                if ((context->method->as.method_declaration.modifiers & VC_AST_MOD_STATIC) != 0)
                    return true;
                node->as.identifier_expression.name = copy_text(context->tree, "_this");
                return node->as.identifier_expression.name != NULL;
            }
            const VcIteratorCapture *capture = capture_for_identifier(context, node);
            if (capture != NULL)
            {
                if (context->preserve_pattern_locals && capture->declaration != NULL &&
                    capture->declaration->kind == VC_AST_TYPE_RELATION_EXPRESSION &&
                    capture->declaration->as.type_relation_expression.pattern_name != NULL)
                    return true;
                node->as.identifier_expression.name = (char *)capture->name;
                return true;
            }
            if (capture_exists(context, name))
                return true;
            const VcSemanticBinding *binding = context->semantic != NULL
                ? vc_semantic_binding(context->semantic, node)
                : NULL;
            if ((context->method->as.method_declaration.modifiers & VC_AST_MOD_STATIC) == 0 &&
                (owner_has_instance_member(context->owner, name) ||
                 semantic_binding_is_instance_member(context, binding)))
            {
                VcAstNode *target = identifier(context->tree, node->location, "_this");
                char *member = copy_text(context->tree, name);
                if (target == NULL || member == NULL)
                {
                    context_error(context, "out of memory while capturing iterator owner member");
                    return false;
                }
                node->kind = VC_AST_MEMBER_ACCESS_EXPRESSION;
                node->as.member_access_expression.target = target;
                node->as.member_access_expression.member = member;
            }
            return true;
        }
        case VC_AST_MEMBER_ACCESS_EXPRESSION:
            return rewrite_expression(context, node->as.member_access_expression.target);
        case VC_AST_CALL_EXPRESSION:
        {
            VcAstNode *callee = node->as.call_expression.callee;
            bool rewritten_callee = false;
            if (callee != NULL && callee->kind == VC_AST_IDENTIFIER_EXPRESSION &&
                (context->method->as.method_declaration.modifiers & VC_AST_MOD_STATIC) == 0)
            {
                const VcSemanticBinding *binding = context->semantic != NULL
                    ? vc_semantic_binding(context->semantic, node)
                    : NULL;
                if (semantic_binding_is_instance_member(context, binding))
                {
                    VcAstNode *target = identifier(context->tree, callee->location, "_this");
                    char *member = copy_text(context->tree,
                        callee->as.identifier_expression.name);
                    if (target == NULL || member == NULL)
                    {
                        context_error(context,
                            "out of memory while capturing iterator owner method");
                        return false;
                    }
                    callee->kind = VC_AST_MEMBER_ACCESS_EXPRESSION;
                    callee->as.member_access_expression.target = target;
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
            if (!rewrite_expression(context, node->as.index_expression.target)) return false;
            if (node->as.index_expression.indices.count != 0)
            {
                for (size_t i = 0; i < node->as.index_expression.indices.count; i++)
                    if (!rewrite_expression(context, node->as.index_expression.indices.items[i])) return false;
                return true;
            }
            return rewrite_expression(context, node->as.index_expression.index);
        case VC_AST_NEW_EXPRESSION:
            if (node->as.new_expression.array_lengths.count != 0)
            {
                for (size_t i = 0; i < node->as.new_expression.array_lengths.count; i++)
                    if (!rewrite_expression(context, node->as.new_expression.array_lengths.items[i])) return false;
            }
            else if (!rewrite_expression(context, node->as.new_expression.array_length)) return false;
            for (size_t i = 0; i < node->as.new_expression.arguments.count; i++)
                if (!rewrite_expression(context, node->as.new_expression.arguments.items[i])) return false;
            for (size_t i = 0; i < node->as.new_expression.initializers.count; i++)
                if (!rewrite_expression(context, node->as.new_expression.initializers.items[i])) return false;
            return true;
        case VC_AST_OBJECT_INITIALIZER_MEMBER:
            return rewrite_expression(context, node->as.object_initializer_member.value);
        case VC_AST_COLLECTION_INITIALIZER_ELEMENT:
            for (size_t i = 0; i < node->as.collection_initializer_element.arguments.count; i++)
                if (!rewrite_expression(context,
                        node->as.collection_initializer_element.arguments.items[i])) return false;
            return true;
        case VC_AST_DEFAULT_EXPRESSION:
            return true;
        case VC_AST_TYPEOF_EXPRESSION:
            return true;
        case VC_AST_SIZEOF_EXPRESSION:
            return true;
        case VC_AST_STACKALLOC_EXPRESSION:
            return rewrite_expression(context, node->as.stackalloc_expression.count);
        case VC_AST_CAST_EXPRESSION:
            return rewrite_expression(context, node->as.cast_expression.expression);
        case VC_AST_AWAIT_EXPRESSION:
            return rewrite_expression(context, node->as.await_expression.operand);
        case VC_AST_TYPE_RELATION_EXPRESSION:
            if (!(rewrite_expression(context, node->as.type_relation_expression.expression) &&
                rewrite_expression(context, node->as.type_relation_expression.pattern_constant) &&
                rewrite_expression(context, node->as.type_relation_expression.pattern_left) &&
                rewrite_expression(context, node->as.type_relation_expression.pattern_right)))
                return false;
            for (size_t i = 0; i < node->as.type_relation_expression.pattern_properties.count; i++)
                if (!rewrite_expression(context, node->as.type_relation_expression.pattern_properties.items[i])) return false;
            return true;
        case VC_AST_PROPERTY_PATTERN_MEMBER:
            return rewrite_expression(context, node->as.property_pattern_member.pattern);
        case VC_AST_UNARY_EXPRESSION:
            return rewrite_expression(context, node->as.unary_expression.operand);
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
        case VC_AST_PARENTHESIZED_EXPRESSION:
            return rewrite_expression(context, node->as.parenthesized_expression.expression);
        case VC_AST_LAMBDA_EXPRESSION:
            return node->as.lambda_expression.body == NULL ||
                rewrite_expression(context, node->as.lambda_expression.body);
        case VC_AST_LITERAL_EXPRESSION:
            return true;
        default:
            return true;
    }
}

static bool rewrite_pattern_scope_expression(VcIteratorContext *context, VcAstNode *expression)
{
    const bool saved = context->preserve_pattern_locals;
    context->preserve_pattern_locals = true;
    const bool result = rewrite_expression(context, expression);
    context->preserve_pattern_locals = saved;
    return result;
}

static bool rewrite_statement(VcIteratorContext *context, VcAstNode *node)
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
        case VC_AST_LOCAL_DECLARATION:
        {
            VcIteratorCapture *capture = capture_for_declaration(context, node);
            if (capture == NULL)
            {
                context_error(context, "internal error: iterator local was not captured");
                return false;
            }
            VcAstNode *initializer = node->as.local_declaration.initializer;
            if (!rewrite_expression(context, initializer)) return false;
            if (initializer == NULL)
            {
                memset(&node->as, 0, sizeof(node->as));
                node->kind = VC_AST_BLOCK_STATEMENT;
                return true;
            }
            VcAstNode *assign = assignment(context->tree, node->location,
                capture->name, initializer);
            if (assign == NULL)
            {
                context_error(context, "out of memory while lowering iterator local '%s'",
                    capture->name);
                return false;
            }
            node->kind = VC_AST_EXPRESSION_STATEMENT;
            node->as.expression_statement.expression = assign;
            return true;
        }
        case VC_AST_IF_STATEMENT:
            return rewrite_pattern_scope_expression(context, node->as.if_statement.condition) &&
                rewrite_statement(context, node->as.if_statement.then_statement) &&
                rewrite_statement(context, node->as.if_statement.else_statement);
        case VC_AST_WHILE_STATEMENT:
            return rewrite_pattern_scope_expression(context, node->as.while_statement.condition) &&
                rewrite_statement(context, node->as.while_statement.body);
        case VC_AST_DO_WHILE_STATEMENT:
            return rewrite_statement(context, node->as.while_statement.body) &&
                rewrite_expression(context, node->as.while_statement.condition);
        case VC_AST_TRY_STATEMENT:
            if (!rewrite_statement(context, node->as.try_statement.try_block)) return false;
            for (size_t i = 0; i < node->as.try_statement.catches.count; i++)
                if (!rewrite_statement(context, node->as.try_statement.catches.items[i])) return false;
            return rewrite_statement(context, node->as.try_statement.finally_block);
        case VC_AST_CATCH_CLAUSE:
            return rewrite_statement(context, node->as.catch_clause.body);
        case VC_AST_FOR_STATEMENT:
            return rewrite_statement(context, node->as.for_statement.initializer) &&
                rewrite_expression(context, node->as.for_statement.condition) &&
                rewrite_expression(context, node->as.for_statement.increment) &&
                rewrite_statement(context, node->as.for_statement.body);
        case VC_AST_FOREACH_STATEMENT:
        {
            VcIteratorCapture *capture = capture_for_declaration(context, node);
            if (capture == NULL)
            {
                context_error(context, "internal error: iterator foreach item was not captured");
                return false;
            }
            if (!rewrite_expression(context, node->as.foreach_statement.collection) ||
                !rewrite_statement(context, node->as.foreach_statement.body))
                return false;
            node->as.foreach_statement.name = (char *)capture->name;
            return true;
        }
        case VC_AST_USING_STATEMENT:
            return rewrite_statement(context, node->as.using_statement.declaration) &&
                rewrite_expression(context, node->as.using_statement.expression) &&
                rewrite_statement(context, node->as.using_statement.body);
        case VC_AST_LOCK_STATEMENT:
            return rewrite_expression(context, node->as.lock_statement.expression) &&
                rewrite_statement(context, node->as.lock_statement.body);
        case VC_AST_SWITCH_STATEMENT:
            if (!rewrite_expression(context, node->as.switch_statement.expression)) return false;
            for (size_t i = 0; i < node->as.switch_statement.sections.count; i++)
                if (!rewrite_statement(context, node->as.switch_statement.sections.items[i])) return false;
            return true;
        case VC_AST_SWITCH_SECTION:
            for (size_t i = 0; i < node->as.switch_section.labels.count; i++)
            {
                VcAstNode *label = node->as.switch_section.labels.items[i];
                if (!label->as.switch_label.is_default)
                {
                    if (label->as.switch_label.is_pattern)
                    {
                        if (!rewrite_pattern_scope_expression(
                                context, label->as.switch_label.pattern)) return false;
                    }
                    else if (!rewrite_expression(context, label->as.switch_label.value)) return false;
                    if (!rewrite_pattern_scope_expression(
                            context, label->as.switch_label.guard)) return false;
                }
            }
            for (size_t i = 0; i < node->as.switch_section.statements.count; i++)
                if (!rewrite_statement(context, node->as.switch_section.statements.items[i])) return false;
            return true;
        case VC_AST_YIELD_RETURN_STATEMENT:
            return rewrite_expression(context, node->as.yield_statement.expression);
        case VC_AST_YIELD_BREAK_STATEMENT:
        case VC_AST_BREAK_STATEMENT:
        case VC_AST_CONTINUE_STATEMENT:
            return true;
        case VC_AST_THROW_STATEMENT:
            return rewrite_expression(context, node->as.throw_statement.expression);
        case VC_AST_RETURN_STATEMENT:
            context_error(context, "iterator methods cannot use 'return'; use 'yield break'");
            return false;
        default:
            context_error(context, "statement kind %d is not supported in iterator lowering", (int)node->kind);
            return false;
    }
}

static VcIteratorBlock *new_iterator_block(VcIteratorContext *context, VcSourceLocation location)
{
    if (context->block_count == context->block_capacity)
    {
        const size_t capacity = context->block_capacity == 0 ? 16 : context->block_capacity * 2;
        VcIteratorBlock *blocks = realloc(context->blocks, capacity * sizeof(*blocks));
        if (blocks == NULL)
        {
            context_error(context, "out of memory while building iterator state machine");
            return NULL;
        }
        context->blocks = blocks;
        context->block_capacity = capacity;
    }
    VcIteratorBlock *block = &context->blocks[context->block_count];
    memset(block, 0, sizeof(*block));
    block->state = (int)context->block_count;
    block->location = location;
    block->target = -1;
    block->false_target = -1;
    if (context->active_finally_count > 0)
    {
        block->active_finally_blocks = malloc(
            context->active_finally_count * sizeof(*block->active_finally_blocks));
        if (block->active_finally_blocks == NULL)
        {
            context_error(context, "out of memory while tracking iterator finally state");
            return NULL;
        }
        memcpy(block->active_finally_blocks, context->active_finally_blocks,
            context->active_finally_count * sizeof(*block->active_finally_blocks));
        block->active_finally_count = context->active_finally_count;
    }
    context->block_count++;
    return block;
}

static bool push_active_finally(VcIteratorContext *context, const VcAstNode *finally_block)
{
    if (context->active_finally_count == context->active_finally_capacity)
    {
        const size_t capacity = context->active_finally_capacity == 0
            ? 4 : context->active_finally_capacity * 2;
        const VcAstNode **blocks = realloc(context->active_finally_blocks,
            capacity * sizeof(*blocks));
        if (blocks == NULL)
        {
            context_error(context, "out of memory while tracking iterator finally scopes");
            return false;
        }
        context->active_finally_blocks = blocks;
        context->active_finally_capacity = capacity;
    }
    context->active_finally_blocks[context->active_finally_count++] = finally_block;
    return true;
}

static void pop_active_finally(VcIteratorContext *context)
{
    if (context->active_finally_count > 0)
        context->active_finally_count--;
}

static int compile_statement(VcIteratorContext *context, VcAstNode *statement,
    int continuation, int break_target, int continue_target);

static int compile_block_list(VcIteratorContext *context, const VcAstNodeList *statements,
    int continuation, int break_target, int continue_target)
{
    int entry = continuation;
    for (size_t i = statements->count; i > 0; i--)
    {
        entry = compile_statement(context, statements->items[i - 1], entry, break_target, continue_target);
        if (entry < 0)
            return -1;
    }
    return entry;
}

static int simple_goto_block(VcIteratorContext *context, VcAstNode *statement, int continuation)
{
    VcIteratorBlock *block_node = new_iterator_block(context, statement->location);
    if (block_node == NULL)
        return -1;
    block_node->terminator = VC_ITER_GOTO;
    block_node->target = continuation;
    if (!vc_ast_node_list_push(context->tree, &block_node->statements, statement))
    {
        context_error(context, "out of memory while storing iterator statement");
        return -1;
    }
    return block_node->state;
}

static int cleanup_chain_to(VcIteratorContext *context, int target)
{
    const size_t saved_count = context->active_finally_count;
    int entry = target;
    for (size_t i = 0; i < saved_count; i++)
    {
        context->active_finally_count = i;
        VcAstNode *cleanup = active_finally_instance(
            context, context->active_finally_blocks[i]);
        if (cleanup == NULL)
        {
            context->active_finally_count = saved_count;
            return -1;
        }
        entry = simple_goto_block(context, cleanup, entry);
        if (entry < 0)
        {
            context->active_finally_count = saved_count;
            return -1;
        }
    }
    context->active_finally_count = saved_count;
    return entry;
}

static int compile_statement(VcIteratorContext *context, VcAstNode *statement,
    int continuation, int break_target, int continue_target)
{
    if (statement == NULL)
        return continuation;

    switch (statement->kind)
    {
        case VC_AST_BLOCK_STATEMENT:
            return compile_block_list(context, &statement->as.block_statement.statements,
                continuation, break_target, continue_target);

        case VC_AST_EXPRESSION_STATEMENT:
            return simple_goto_block(context, statement, continuation);

        case VC_AST_YIELD_RETURN_STATEMENT:
        {
            VcIteratorBlock *yield = new_iterator_block(context, statement->location);
            if (yield == NULL) return -1;
            yield->terminator = VC_ITER_YIELD;
            yield->expression = statement->as.yield_statement.expression;
            yield->target = continuation;
            return yield->state;
        }

        case VC_AST_YIELD_BREAK_STATEMENT:
            return cleanup_chain_to(context, context->finish_state);

        case VC_AST_IF_STATEMENT:
        {
            const int then_entry = compile_statement(context, statement->as.if_statement.then_statement,
                continuation, break_target, continue_target);
            if (then_entry < 0) return -1;
            const int else_entry = statement->as.if_statement.else_statement != NULL
                ? compile_statement(context, statement->as.if_statement.else_statement,
                    continuation, break_target, continue_target)
                : continuation;
            if (else_entry < 0) return -1;
            VcIteratorBlock *branch = new_iterator_block(context, statement->location);
            if (branch == NULL) return -1;
            branch->terminator = VC_ITER_BRANCH;
            branch->expression = statement->as.if_statement.condition;
            branch->target = then_entry;
            branch->false_target = else_entry;
            return branch->state;
        }

        case VC_AST_WHILE_STATEMENT:
        {
            VcIteratorBlock *condition = new_iterator_block(context, statement->location);
            if (condition == NULL) return -1;
            const int condition_state = condition->state;
            const int body_entry = compile_statement(context, statement->as.while_statement.body,
                condition_state, continuation, condition_state);
            if (body_entry < 0) return -1;
            condition = &context->blocks[condition_state];
            condition->terminator = VC_ITER_BRANCH;
            condition->expression = statement->as.while_statement.condition;
            condition->target = body_entry;
            condition->false_target = continuation;
            return condition_state;
        }

        case VC_AST_DO_WHILE_STATEMENT:
        {
            VcIteratorBlock *condition = new_iterator_block(context, statement->location);
            if (condition == NULL) return -1;
            const int condition_state = condition->state;
            const int body_entry = compile_statement(context, statement->as.while_statement.body,
                condition_state, continuation, condition_state);
            if (body_entry < 0) return -1;
            condition = &context->blocks[condition_state];
            condition->terminator = VC_ITER_BRANCH;
            condition->expression = statement->as.while_statement.condition;
            condition->target = body_entry;
            condition->false_target = continuation;
            return body_entry;
        }

        case VC_AST_FOR_STATEMENT:
        {
            VcIteratorBlock *condition = new_iterator_block(context, statement->location);
            if (condition == NULL) return -1;
            const int condition_state = condition->state;

            int increment_state = condition_state;
            if (statement->as.for_statement.increment != NULL)
            {
                VcAstNode *increment_statement = expression_statement(context->tree,
                    statement->as.for_statement.increment->location,
                    statement->as.for_statement.increment);
                if (increment_statement == NULL)
                {
                    context_error(context, "out of memory while lowering iterator for increment");
                    return -1;
                }
                increment_state = simple_goto_block(context, increment_statement, condition_state);
                if (increment_state < 0) return -1;
            }

            const int body_entry = compile_statement(context, statement->as.for_statement.body,
                increment_state, continuation, increment_state);
            if (body_entry < 0) return -1;
            condition = &context->blocks[condition_state];
            condition->terminator = VC_ITER_BRANCH;
            condition->expression = statement->as.for_statement.condition != NULL
                ? statement->as.for_statement.condition
                : bool_literal(context->tree, statement->location, true);
            if (condition->expression == NULL)
            {
                context_error(context, "out of memory while lowering iterator for condition");
                return -1;
            }
            condition->target = body_entry;
            condition->false_target = continuation;

            return statement->as.for_statement.initializer != NULL
                ? compile_statement(context, statement->as.for_statement.initializer,
                    condition_state, break_target, continue_target)
                : condition_state;
        }

        case VC_AST_THROW_STATEMENT:
            return simple_goto_block(context, statement, continuation);

        case VC_AST_LOCK_STATEMENT:
            if (node_contains_yield(statement->as.lock_statement.body))
            {
                context_error(context, "yield cannot be used inside a lock statement");
                return -1;
            }
            return simple_goto_block(context, statement, continuation);

        case VC_AST_USING_STATEMENT:
        {
            const VcSemanticBinding *binding = vc_semantic_binding(context->semantic, statement);
            if (binding == NULL || !binding->has_using)
            {
                context_error(context, "internal error: iterator using binding is unavailable");
                return -1;
            }

            const char *resource_name = NULL;
            VcAstNode *initializer_statement = NULL;
            if (statement->as.using_statement.declaration != NULL)
            {
                const VcIteratorCapture *capture = capture_for_declaration(
                    context, statement->as.using_statement.declaration);
                if (capture == NULL)
                {
                    context_error(context, "internal error: iterator using resource was not captured");
                    return -1;
                }
                resource_name = capture->name;
                initializer_statement = statement->as.using_statement.declaration;
            }
            else
            {
                char generated_name[64];
                size_t id = context->local_id++;
                const int written = snprintf(generated_name, sizeof(generated_name),
                    "__using_resource_%zu", id);
                if (written < 0 || (size_t)written >= sizeof(generated_name))
                    return -1;
                while (capture_exists(context, generated_name))
                {
                    id = context->local_id++;
                    const int retry = snprintf(generated_name, sizeof(generated_name),
                        "__using_resource_%zu", id);
                    if (retry < 0 || (size_t)retry >= sizeof(generated_name))
                        return -1;
                }

                VcAstTypeRef *resource_type = type_from_semantic(
                    context, binding->using_resource_type, statement->location);
                char *capture_name = copy_text(context->tree, generated_name);
                if (resource_type == NULL || capture_name == NULL ||
                    !add_capture(context, capture_name, NULL, resource_type))
                {
                    context_error(context, "out of memory while capturing iterator using resource");
                    return -1;
                }
                resource_name = capture_name;
                VcAstNode *assign = assignment(context->tree, statement->location, resource_name,
                    statement->as.using_statement.expression);
                initializer_statement = expression_statement(context->tree, statement->location, assign);
                if (assign == NULL || initializer_statement == NULL)
                    return -1;
            }

            VcAstNode *dispose_call = call(context->tree, statement->location,
                member_access(context->tree, statement->location,
                    identifier(context->tree, statement->location, resource_name), "Dispose"));
            VcAstNode *dispose_statement = expression_statement(
                context->tree, statement->location, dispose_call);
            if (dispose_call == NULL || dispose_statement == NULL)
                return -1;

            VcAstNode *cleanup_statement = dispose_statement;
            if (binding->using_skip_dispose_if_null)
            {
                VcAstNode *condition = binary(context->tree, statement->location, VC_TOKEN_BANG_EQUAL,
                    identifier(context->tree, statement->location, resource_name),
                    null_literal(context->tree, statement->location));
                VcAstNode *if_node = vc_ast_new_node(
                    context->tree, VC_AST_IF_STATEMENT, statement->location);
                if (condition == NULL || if_node == NULL)
                    return -1;
                if_node->as.if_statement.condition = condition;
                if_node->as.if_statement.then_statement = dispose_statement;
                cleanup_statement = if_node;
            }

            VcAstNode *cleanup = block(context->tree, statement->location);
            if (cleanup == NULL || !block_push(context->tree, cleanup, cleanup_statement))
                return -1;

            const int normal_cleanup = simple_goto_block(context, cleanup, continuation);
            if (normal_cleanup < 0)
                return -1;

            int break_cleanup = break_target;
            if (break_target >= 0)
            {
                break_cleanup = simple_goto_block(context, cleanup, break_target);
                if (break_cleanup < 0)
                    return -1;
            }

            int continue_cleanup = continue_target;
            if (continue_target >= 0)
            {
                continue_cleanup = simple_goto_block(context, cleanup, continue_target);
                if (continue_cleanup < 0)
                    return -1;
            }

            if (!push_active_finally(context, cleanup))
                return -1;
            const int body_entry = compile_statement(context, statement->as.using_statement.body,
                normal_cleanup, break_cleanup, continue_cleanup);
            pop_active_finally(context);
            if (body_entry < 0)
                return -1;

            return simple_goto_block(context, initializer_statement, body_entry);
        }

        case VC_AST_TRY_STATEMENT:
        {
            if (!node_contains_yield(statement))
                return simple_goto_block(context, statement, continuation);

            if (statement->as.try_statement.catches.count != 0 ||
                statement->as.try_statement.finally_block == NULL ||
                node_contains_yield(statement->as.try_statement.finally_block))
            {
                context_error(context,
                    "internal error: unsupported yielding iterator exception region reached lowering");
                return -1;
            }

            VcAstNode *finally_block = statement->as.try_statement.finally_block;
            const int normal_cleanup = simple_goto_block(context, finally_block, continuation);
            if (normal_cleanup < 0) return -1;

            int break_cleanup = break_target;
            if (break_target >= 0)
            {
                break_cleanup = simple_goto_block(context, finally_block, break_target);
                if (break_cleanup < 0) return -1;
            }

            int continue_cleanup = continue_target;
            if (continue_target >= 0)
            {
                continue_cleanup = simple_goto_block(context, finally_block, continue_target);
                if (continue_cleanup < 0) return -1;
            }

            if (!push_active_finally(context, finally_block))
                return -1;
            const int entry = compile_statement(context, statement->as.try_statement.try_block,
                normal_cleanup, break_cleanup, continue_cleanup);
            pop_active_finally(context);
            return entry;
        }

        case VC_AST_BREAK_STATEMENT:
        {
            if (break_target < 0)
            {
                context_error(context, "'break' is only valid inside an iterator loop");
                return -1;
            }
            VcIteratorBlock *jump = new_iterator_block(context, statement->location);
            if (jump == NULL) return -1;
            jump->terminator = VC_ITER_GOTO;
            jump->target = break_target;
            return jump->state;
        }

        case VC_AST_CONTINUE_STATEMENT:
        {
            if (continue_target < 0)
            {
                context_error(context, "'continue' is only valid inside an iterator loop");
                return -1;
            }
            VcIteratorBlock *jump = new_iterator_block(context, statement->location);
            if (jump == NULL) return -1;
            jump->terminator = VC_ITER_GOTO;
            jump->target = continue_target;
            return jump->state;
        }

        case VC_AST_FOREACH_STATEMENT:
        {
            VcAstTypeRef *item_type = capture_type(context, statement->as.foreach_statement.name);
            VcAstTypeRef *collection_type = expression_type(
                context, statement->as.foreach_statement.collection);
            if (context->async_iterator && statement->as.foreach_statement.is_await)
            {
                const VcSemanticBinding *binding = vc_semantic_binding(context->semantic, statement);
                if (binding == NULL || !binding->has_foreach || !binding->foreach_async)
                {
                    context_error(context,
                        "internal error: async iterator await foreach binding is unavailable");
                    return -1;
                }
                if (item_type == NULL || collection_type == NULL)
                {
                    context_error(context,
                        "internal error: async iterator await foreach types are unavailable");
                    return -1;
                }

                VcAstTypeRef *enumerator_type = type_from_semantic(context,
                    binding->foreach_enumerator_type, statement->location);
                if (enumerator_type == NULL)
                {
                    context_error(context,
                        "internal error: async iterator await foreach enumerator type is unavailable");
                    return -1;
                }

                size_t id = context->foreach_id++;
                char enumerator_buffer[64];
                char collection_buffer[64];
                while (!generated_capture_name(context, "async_enumerator", id,
                        enumerator_buffer, sizeof(enumerator_buffer)) ||
                    !generated_capture_name(context, "async_collection", id,
                        collection_buffer, sizeof(collection_buffer)))
                    id = context->foreach_id++;

                char *enumerator_name = copy_text(context->tree, enumerator_buffer);
                char *collection_name = copy_text(context->tree, collection_buffer);
                VcAstTypeRef *stored_collection_type = clone_type(context->tree, collection_type);
                if (enumerator_name == NULL || collection_name == NULL ||
                    stored_collection_type == NULL ||
                    !add_capture(context, collection_name, NULL, stored_collection_type) ||
                    !add_capture(context, enumerator_name, NULL, enumerator_type))
                    return -1;

                VcAstNode *cleanup_template = make_async_foreach_cleanup(
                    context, statement->location, enumerator_name);
                VcAstNode *normal_cleanup_statement = make_async_foreach_cleanup(
                    context, statement->location, enumerator_name);
                if (cleanup_template == NULL || normal_cleanup_statement == NULL)
                    return -1;

                const int normal_cleanup = simple_goto_block(
                    context, normal_cleanup_statement, continuation);
                if (normal_cleanup < 0)
                    return -1;

                if (!push_active_finally(context, cleanup_template))
                    return -1;

                VcIteratorBlock *condition = new_iterator_block(context, statement->location);
                if (condition == NULL)
                {
                    pop_active_finally(context);
                    return -1;
                }
                const int condition_state = condition->state;

                const int body_entry = compile_statement(context,
                    statement->as.foreach_statement.body, condition_state, normal_cleanup,
                    condition_state);
                if (body_entry < 0)
                {
                    pop_active_finally(context);
                    return -1;
                }

                VcAstNode *current = member_access(context->tree, statement->location,
                    identifier(context->tree, statement->location, enumerator_name), "Current");
                VcAstNode *item_assignment = expression_statement(context->tree,
                    statement->location, assignment(context->tree, statement->location,
                        statement->as.foreach_statement.name, current));
                if (current == NULL || item_assignment == NULL)
                {
                    pop_active_finally(context);
                    return -1;
                }
                const int item_state = simple_goto_block(context, item_assignment, body_entry);
                if (item_state < 0)
                {
                    pop_active_finally(context);
                    return -1;
                }

                VcAstNode *move_call = call(context->tree, statement->location,
                    member_access(context->tree, statement->location,
                        identifier(context->tree, statement->location, enumerator_name),
                        "MoveNextAsync"));
                VcAstNode *move_await = await_expression(context->tree, statement->location,
                    move_call);
                if (move_call == NULL || move_await == NULL)
                {
                    pop_active_finally(context);
                    return -1;
                }
                condition = &context->blocks[condition_state];
                condition->terminator = VC_ITER_BRANCH;
                condition->expression = move_await;
                condition->target = item_state;
                condition->false_target = normal_cleanup;

                pop_active_finally(context);

                VcAstNode *get_enumerator = call(context->tree, statement->location,
                    member_access(context->tree, statement->location,
                        identifier(context->tree, statement->location, collection_name),
                        "GetAsyncEnumerator"));
                VcAstNode *enumerator_init = expression_statement(context->tree,
                    statement->location, assignment(context->tree, statement->location,
                        enumerator_name, get_enumerator));
                if (get_enumerator == NULL || enumerator_init == NULL)
                    return -1;
                const int enumerator_init_state = simple_goto_block(context,
                    enumerator_init, condition_state);
                if (enumerator_init_state < 0)
                    return -1;

                VcAstNode *collection_init = expression_statement(context->tree,
                    statement->location, assignment(context->tree, statement->location,
                        collection_name, statement->as.foreach_statement.collection));
                if (collection_init == NULL)
                    return -1;
                return simple_goto_block(context, collection_init, enumerator_init_state);
            }
            if (item_type == NULL)
            {
                context_error(context, "internal error: iterator foreach item was not captured");
                return -1;
            }

            size_t id = context->foreach_id++;
            char first_name[64];
            char second_name[64];
            char collection_name_buffer[64];
            while (!generated_capture_name(context, "value", id, first_name, sizeof(first_name)) ||
                !generated_capture_name(context, "index", id, second_name, sizeof(second_name)) ||
                !generated_capture_name(context, "collection", id,
                    collection_name_buffer, sizeof(collection_name_buffer)))
                id = context->foreach_id++;

            const bool array_foreach = collection_type != NULL &&
                collection_type->array_rank > 0 && collection_type->rectangular_rank == 0;
            if (array_foreach)
            {
                char *array_name = copy_text(context->tree, first_name);
                char *index_name = copy_text(context->tree, second_name);
                VcAstTypeRef *array_type = clone_type(context->tree, collection_type);
                VcAstTypeRef *int_type = named_type(context->tree, statement->location, "int");
                if (array_name == NULL || index_name == NULL || array_type == NULL || int_type == NULL ||
                    !add_capture(context, array_name, NULL, array_type) ||
                    !add_capture(context, index_name, NULL, int_type))
                    return -1;

                VcIteratorBlock *condition = new_iterator_block(context, statement->location);
                if (condition == NULL) return -1;
                const int condition_state = condition->state;

                VcAstNode *increment = assignment(context->tree, statement->location, index_name,
                    binary(context->tree, statement->location, VC_TOKEN_PLUS,
                        identifier(context->tree, statement->location, index_name),
                        number_literal(context->tree, statement->location, 1)));
                VcAstNode *increment_statement = expression_statement(
                    context->tree, statement->location, increment);
                if (increment == NULL || increment_statement == NULL)
                    return -1;
                const int increment_state = simple_goto_block(
                    context, increment_statement, condition_state);
                if (increment_state < 0) return -1;

                const int body_entry = compile_statement(context, statement->as.foreach_statement.body,
                    increment_state, continuation, increment_state);
                if (body_entry < 0) return -1;

                VcAstNode *item_value = index_expression(context->tree, statement->location,
                    identifier(context->tree, statement->location, array_name),
                    identifier(context->tree, statement->location, index_name));
                VcAstNode *item_assignment = expression_statement(context->tree, statement->location,
                    assignment(context->tree, statement->location,
                        statement->as.foreach_statement.name, item_value));
                if (item_value == NULL || item_assignment == NULL)
                    return -1;
                const int item_state = simple_goto_block(context, item_assignment, body_entry);
                if (item_state < 0) return -1;

                condition = &context->blocks[condition_state];
                condition->terminator = VC_ITER_BRANCH;
                condition->expression = binary(context->tree, statement->location, VC_TOKEN_LESS,
                    identifier(context->tree, statement->location, index_name),
                    member_access(context->tree, statement->location,
                        identifier(context->tree, statement->location, array_name), "Length"));
                if (condition->expression == NULL)
                    return -1;
                condition->target = item_state;
                condition->false_target = continuation;

                VcAstNode *index_init = expression_statement(context->tree, statement->location,
                    assignment(context->tree, statement->location, index_name,
                        number_literal(context->tree, statement->location, 0)));
                if (index_init == NULL)
                    return -1;
                const int index_init_state = simple_goto_block(context, index_init, condition_state);
                if (index_init_state < 0) return -1;

                VcAstNode *array_init = expression_statement(context->tree, statement->location,
                    assignment(context->tree, statement->location, array_name,
                        statement->as.foreach_statement.collection));
                if (array_init == NULL)
                    return -1;
                return simple_goto_block(context, array_init, index_init_state);
            }

            char *enumerator_name = copy_text(context->tree, first_name);
            char *collection_name = copy_text(context->tree, collection_name_buffer);
            VcAstTypeRef *enumerator = enumerator_type(context, item_type);
            VcAstTypeRef *stored_collection_type = clone_type(context->tree, collection_type);
            if (enumerator_name == NULL || collection_name == NULL || enumerator == NULL ||
                stored_collection_type == NULL ||
                !add_capture(context, collection_name, NULL, stored_collection_type) ||
                !add_capture(context, enumerator_name, NULL, enumerator))
                return -1;

            VcIteratorBlock *condition = new_iterator_block(context, statement->location);
            if (condition == NULL) return -1;
            const int condition_state = condition->state;

            const int body_entry = compile_statement(context, statement->as.foreach_statement.body,
                condition_state, continuation, condition_state);
            if (body_entry < 0) return -1;

            VcAstNode *current = member_access(context->tree, statement->location,
                identifier(context->tree, statement->location, enumerator_name), "Current");
            VcAstNode *item_assignment = expression_statement(context->tree, statement->location,
                assignment(context->tree, statement->location,
                    statement->as.foreach_statement.name, current));
            if (current == NULL || item_assignment == NULL)
                return -1;
            const int item_state = simple_goto_block(context, item_assignment, body_entry);
            if (item_state < 0) return -1;

            condition = &context->blocks[condition_state];
            condition->terminator = VC_ITER_BRANCH;
            condition->expression = call(context->tree, statement->location,
                member_access(context->tree, statement->location,
                    identifier(context->tree, statement->location, enumerator_name), "MoveNext"));
            if (condition->expression == NULL)
                return -1;
            condition->target = item_state;
            condition->false_target = continuation;

            VcAstNode *get_enumerator = call(context->tree, statement->location,
                member_access(context->tree, statement->location,
                    identifier(context->tree, statement->location, collection_name), "GetEnumerator"));
            VcAstNode *enumerator_init = expression_statement(context->tree, statement->location,
                assignment(context->tree, statement->location, enumerator_name, get_enumerator));
            if (get_enumerator == NULL || enumerator_init == NULL)
                return -1;
            const int enumerator_init_state = simple_goto_block(
                context, enumerator_init, condition_state);
            if (enumerator_init_state < 0)
                return -1;

            VcAstNode *collection_init = expression_statement(context->tree, statement->location,
                assignment(context->tree, statement->location, collection_name,
                    statement->as.foreach_statement.collection));
            if (collection_init == NULL)
                return -1;
            return simple_goto_block(context, collection_init, enumerator_init_state);
        }

        case VC_AST_SWITCH_STATEMENT:
        {
            const size_t section_count = statement->as.switch_statement.sections.count;
            int *targets = section_count == 0 ? NULL : calloc(section_count, sizeof(*targets));
            if (section_count != 0 && targets == NULL)
            {
                context_error(context, "out of memory while lowering iterator switch");
                return -1;
            }

            for (size_t i = 0; i < section_count; i++)
            {
                const VcAstNode *section = statement->as.switch_statement.sections.items[i];
                targets[i] = compile_block_list(context, &section->as.switch_section.statements,
                    continuation, continuation, continue_target);
                if (targets[i] < 0)
                {
                    free(targets);
                    return -1;
                }
            }

            VcIteratorBlock *dispatch = new_iterator_block(context, statement->location);
            if (dispatch == NULL)
            {
                free(targets);
                return -1;
            }
            dispatch->terminator = VC_ITER_SWITCH;
            dispatch->expression = statement->as.switch_statement.expression;
            dispatch->target = continuation;
            dispatch->switch_source = statement;
            dispatch->switch_targets = targets;
            dispatch->switch_target_count = section_count;
            return dispatch->state;
        }

        case VC_AST_RETURN_STATEMENT:
            context_error(context, "iterator methods cannot use 'return'; use 'yield break'");
            return -1;

        default:
            context_error(context, "statement kind %d is not supported in iterator lowering", (int)statement->kind);
            return -1;
    }
}

static bool append_pattern_capture_assignments(
    VcIteratorContext *context,
    const VcAstNode *pattern,
    VcAstNodeList *statements)
{
    if (pattern == NULL || pattern->kind != VC_AST_TYPE_RELATION_EXPRESSION)
        return true;
    if (pattern->as.type_relation_expression.pattern_name != NULL)
    {
        VcIteratorCapture *capture = capture_for_declaration(context, pattern);
        if (capture == NULL)
        {
            context_error(context, "internal error: iterator pattern local was not captured");
            return false;
        }
        VcAstNode *value = identifier(context->tree, pattern->location,
            pattern->as.type_relation_expression.pattern_name);
        VcAstNode *assign = assignment(context->tree, pattern->location,
            capture->name, value);
        VcAstNode *statement = expression_statement(context->tree, pattern->location, assign);
        if (value == NULL || assign == NULL || statement == NULL ||
            !vc_ast_node_list_push(context->tree, statements, statement))
        {
            context_error(context, "out of memory while lowering iterator pattern local");
            return false;
        }
    }
    if (!append_pattern_capture_assignments(context,
            pattern->as.type_relation_expression.pattern_left, statements) ||
        !append_pattern_capture_assignments(context,
            pattern->as.type_relation_expression.pattern_right, statements))
        return false;
    for (size_t i = 0; i < pattern->as.type_relation_expression.pattern_properties.count; i++)
    {
        const VcAstNode *member = pattern->as.type_relation_expression.pattern_properties.items[i];
        if (member == NULL || member->kind != VC_AST_PROPERTY_PATTERN_MEMBER ||
            !append_pattern_capture_assignments(
                context, member->as.property_pattern_member.pattern, statements))
            return false;
    }
    return true;
}

static bool append_condition_pattern_capture_assignments(
    VcIteratorContext *context,
    VcAstNode *expression,
    VcAstNodeList *statements)
{
    VcAstNode *pattern = declaration_pattern_condition(expression);
    return pattern == NULL || append_pattern_capture_assignments(context, pattern, statements);
}

static VcAstNode *make_state_assignment_statement(VcIteratorContext *context,
    VcSourceLocation location, int state)
{
    return expression_statement(context->tree, location,
        assignment(context->tree, location, "_state", number_literal(context->tree, location, state)));
}

static bool emit_state_block(VcIteratorContext *context, VcAstNode *move_loop,
    const VcIteratorBlock *source)
{
    VcAstNode *condition = binary(context->tree, source->location, VC_TOKEN_EQUAL_EQUAL,
        identifier(context->tree, source->location, "_state"),
        number_literal(context->tree, source->location, source->state));
    VcAstNode *branch = block(context->tree, source->location);
    VcAstNode *if_statement = vc_ast_new_node(context->tree, VC_AST_IF_STATEMENT, source->location);
    if (condition == NULL || branch == NULL || if_statement == NULL)
        return false;
    if_statement->as.if_statement.condition = condition;
    if_statement->as.if_statement.then_statement = branch;

    for (size_t i = 0; i < source->statements.count; i++)
        if (!block_push(context->tree, branch, source->statements.items[i])) return false;

    switch (source->terminator)
    {
        case VC_ITER_GOTO:
            if (!block_push(context->tree, branch,
                    make_state_assignment_statement(context, source->location, source->target)) ||
                !block_push(context->tree, branch, continue_statement(context->tree, source->location)))
                return false;
            break;

        case VC_ITER_BRANCH:
        {
            VcAstNode *then_block = block(context->tree, source->location);
            VcAstNode *else_block = block(context->tree, source->location);
            VcAstNode *inner = vc_ast_new_node(context->tree, VC_AST_IF_STATEMENT, source->location);
            if (then_block == NULL || else_block == NULL || inner == NULL ||
                !append_condition_pattern_capture_assignments(context,
                    source->expression, &then_block->as.block_statement.statements) ||
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

        case VC_ITER_SWITCH:
        {
            if (source->switch_source == NULL ||
                source->switch_target_count != source->switch_source->as.switch_statement.sections.count)
                return false;

            VcAstNode *switch_node = vc_ast_new_node(
                context->tree, VC_AST_SWITCH_STATEMENT, source->location);
            if (switch_node == NULL ||
                !block_push(context->tree, branch,
                    make_state_assignment_statement(context, source->location, source->target)))
                return false;
            switch_node->as.switch_statement.expression = source->expression;

            for (size_t i = 0; i < source->switch_target_count; i++)
            {
                const VcAstNode *source_section =
                    source->switch_source->as.switch_statement.sections.items[i];
                VcAstNode *section = vc_ast_new_node(
                    context->tree, VC_AST_SWITCH_SECTION, source_section->location);
                if (section == NULL)
                    return false;
                for (size_t label_index = 0;
                    label_index < source_section->as.switch_section.labels.count; label_index++)
                {
                    if (!vc_ast_node_list_push(context->tree, &section->as.switch_section.labels,
                            source_section->as.switch_section.labels.items[label_index]))
                        return false;
                }
                for (size_t label_index = 0;
                    label_index < source_section->as.switch_section.labels.count; label_index++)
                {
                    const VcAstNode *label = source_section->as.switch_section.labels.items[label_index];
                    if (label != NULL && label->kind == VC_AST_SWITCH_LABEL &&
                        label->as.switch_label.is_pattern &&
                        !append_pattern_capture_assignments(context,
                            label->as.switch_label.pattern,
                            &section->as.switch_section.statements))
                        return false;
                }
                if (!vc_ast_node_list_push(context->tree, &section->as.switch_section.statements,
                        make_state_assignment_statement(context, source_section->location,
                            source->switch_targets[i])) ||
                    !vc_ast_node_list_push(context->tree, &section->as.switch_section.statements,
                        break_statement(context->tree, source_section->location)) ||
                    !vc_ast_node_list_push(context->tree, &switch_node->as.switch_statement.sections,
                        section))
                    return false;
            }

            if (!block_push(context->tree, branch, switch_node) ||
                !block_push(context->tree, branch, continue_statement(context->tree, source->location)))
                return false;
            break;
        }

        case VC_ITER_YIELD:
            if (!block_push(context->tree, branch,
                    expression_statement(context->tree, source->location,
                        assignment(context->tree, source->location, "_current", source->expression))) ||
                !block_push(context->tree, branch,
                    expression_statement(context->tree, source->location,
                        assignment(context->tree, source->location, "_dispose_state",
                            number_literal(context->tree, source->location, source->state)))) ||
                !block_push(context->tree, branch,
                    make_state_assignment_statement(context, source->location, source->target)) ||
                !block_push(context->tree, branch,
                    return_statement(context->tree, source->location,
                        bool_literal(context->tree, source->location, true))))
                return false;
            break;

        case VC_ITER_FINISH:
            if (!block_push(context->tree, branch,
                    expression_statement(context->tree, source->location,
                        assignment(context->tree, source->location, "_dispose_state",
                            number_literal(context->tree, source->location, -1)))) ||
                !block_push(context->tree, branch,
                    make_state_assignment_statement(context, source->location, -1)) ||
                !block_push(context->tree, branch,
                    return_statement(context->tree, source->location,
                        bool_literal(context->tree, source->location, false))))
                return false;
            break;
    }

    return block_push(context->tree, move_loop, if_statement);
}

static VcAstNode *fault_cleanup_block(VcIteratorContext *context,
    const VcIteratorBlock *source)
{
    if (source->active_finally_count == 0)
        return NULL;

    VcAstNode *current = block(context->tree, source->location);
    VcAstNode *innermost = active_finally_instance(context,
        source->active_finally_blocks[source->active_finally_count - 1]);
    if (current == NULL || innermost == NULL ||
        !block_push(context->tree, current, innermost))
        return NULL;

    for (size_t i = source->active_finally_count - 1; i > 0; i--)
    {
        VcAstNode *try_node = vc_ast_new_node(context->tree,
            VC_AST_TRY_STATEMENT, source->location);
        VcAstNode *wrapper = block(context->tree, source->location);
        if (try_node == NULL || wrapper == NULL)
            return NULL;
        VcAstNode *outer_cleanup = active_finally_instance(
            context, source->active_finally_blocks[i - 1]);
        if (outer_cleanup == NULL)
            return NULL;
        try_node->as.try_statement.try_block = current;
        try_node->as.try_statement.finally_block = outer_cleanup;
        if (!block_push(context->tree, wrapper, try_node))
            return NULL;
        current = wrapper;
    }
    return current;
}

static VcAstNode *wrap_move_loop_for_exceptions(VcIteratorContext *context,
    VcAstNode *move_loop, VcSourceLocation location)
{
    bool has_finally_states = false;
    for (size_t i = 0; i < context->block_count; i++)
    {
        if (context->blocks[i].active_finally_count != 0)
        {
            has_finally_states = true;
            break;
        }
    }

    VcAstNode *try_node = vc_ast_new_node(context->tree, VC_AST_TRY_STATEMENT, location);
    VcAstNode *try_block = block(context->tree, location);
    VcAstNode *catch_clause = vc_ast_new_node(context->tree, VC_AST_CATCH_CLAUSE, location);
    VcAstNode *catch_body = block(context->tree, location);
    if (try_node == NULL || try_block == NULL || catch_clause == NULL || catch_body == NULL ||
        !block_push(context->tree, try_block, move_loop))
        return NULL;

    if (has_finally_states)
    {
        VcAstNode *fault_local = vc_ast_new_node(
            context->tree, VC_AST_LOCAL_DECLARATION, location);
        if (fault_local == NULL)
            return NULL;
        fault_local->as.local_declaration.type = named_type(context->tree, location, "int");
        fault_local->as.local_declaration.name = copy_text(
            context->tree, "__iterator_fault_state");
        fault_local->as.local_declaration.initializer = identifier(
            context->tree, location, "_state");
        if (fault_local->as.local_declaration.type == NULL ||
            fault_local->as.local_declaration.name == NULL ||
            fault_local->as.local_declaration.initializer == NULL ||
            !block_push(context->tree, catch_body, fault_local))
            return NULL;
    }

    if (!block_push(context->tree, catch_body,
            expression_statement(context->tree, location,
                assignment(context->tree, location, "_dispose_state",
                    number_literal(context->tree, location, -1)))) ||
        !block_push(context->tree, catch_body,
            expression_statement(context->tree, location,
                assignment(context->tree, location, "_state",
                    number_literal(context->tree, location, -1)))))
        return NULL;

    if (has_finally_states)
    {
        for (size_t i = 0; i < context->block_count; i++)
        {
            const VcIteratorBlock *source = &context->blocks[i];
            if (source->active_finally_count == 0)
                continue;
            VcAstNode *cleanup = fault_cleanup_block(context, source);
            VcAstNode *condition = binary(context->tree, source->location, VC_TOKEN_EQUAL_EQUAL,
                identifier(context->tree, source->location, "__iterator_fault_state"),
                number_literal(context->tree, source->location, source->state));
            VcAstNode *if_node = vc_ast_new_node(
                context->tree, VC_AST_IF_STATEMENT, source->location);
            if (cleanup == NULL || condition == NULL || if_node == NULL)
                return NULL;
            if_node->as.if_statement.condition = condition;
            if_node->as.if_statement.then_statement = cleanup;
            if (!block_push(context->tree, catch_body, if_node))
                return NULL;
        }
    }

    VcAstNode *rethrow = vc_ast_new_node(context->tree, VC_AST_THROW_STATEMENT, location);
    if (rethrow == NULL || !block_push(context->tree, catch_body, rethrow))
        return NULL;

    catch_clause->as.catch_clause.catch_all = true;
    catch_clause->as.catch_clause.body = catch_body;
    try_node->as.try_statement.try_block = try_block;
    if (!vc_ast_node_list_push(context->tree, &try_node->as.try_statement.catches, catch_clause))
        return NULL;
    return try_node;
}

static bool add_field(VcIteratorContext *context, VcAstNode *iterator,
    VcSourceLocation location, const char *name, VcAstTypeRef *type)
{
    VcAstNode *field = vc_ast_new_node(context->tree, VC_AST_FIELD_DECLARATION, location);
    if (field == NULL)
        return false;
    field->as.field_declaration.modifiers = VC_AST_MOD_PRIVATE;
    field->as.field_declaration.type = type;
    field->as.field_declaration.name = copy_text(context->tree, name);
    return field->as.field_declaration.name != NULL &&
        vc_ast_node_list_push(context->tree, &iterator->as.type_declaration.members, field);
}

static VcAstNode *make_parameter(VcIteratorContext *context, VcSourceLocation location,
    const char *name, VcAstTypeRef *type)
{
    VcAstNode *parameter = vc_ast_new_node(context->tree, VC_AST_PARAMETER, location);
    if (parameter == NULL)
        return NULL;
    parameter->as.parameter.modifier = VC_TOKEN_EOF;
    parameter->as.parameter.type = type;
    parameter->as.parameter.name = copy_text(context->tree, name);
    return parameter->as.parameter.name != NULL ? parameter : NULL;
}

static bool lower_method(VcAstTree *tree, VcAstNodeList *parent_declarations,
    const VcAstNode *owner, VcAstNode *method, size_t iterator_id,
    const VcSemanticModel *semantic,
    char *error, size_t error_size)
{
    VcAstNode *body = method->as.method_declaration.body;
    if (body == NULL || !node_contains_yield(body))
        return true;

    VcAstTypeRef *return_type = method->as.method_declaration.return_type;
    const bool async_iterator =
        (method->as.method_declaration.modifiers & VC_AST_MOD_ASYNC) != 0;
    const char *required_return = async_iterator ? "IAsyncEnumerable" : "IEnumerator";
    if (return_type == NULL || return_type->name == NULL ||
        !type_name_matches(return_type, required_return) ||
        return_type->generic_arguments.count != 1 || return_type->array_rank != 0)
    {
        set_error(error, error_size,
            async_iterator
                ? "async iterator method '%s' must return IAsyncEnumerable<T>"
                : "yield method '%s' must return IEnumerator<T>",
            method->as.method_declaration.name);
        return false;
    }

    if ((method->as.method_declaration.modifiers & VC_AST_MOD_ABSTRACT) != 0 || body == NULL)
    {
        set_error(error, error_size, "iterator method '%s' must have a body",
            method->as.method_declaration.name);
        return false;
    }

    VcIteratorContext context = {0};
    context.tree = tree;
    context.owner = owner;
    context.method = method;
    context.semantic = semantic;
    context.async_iterator = async_iterator;
    context.error = error;
    context.error_size = error_size;

    for (size_t i = 0; i < method->as.method_declaration.parameters.count; i++)
    {
        VcAstNode *parameter = method->as.method_declaration.parameters.items[i];
        if (parameter->as.parameter.modifier == VC_TOKEN_KW_REF ||
            parameter->as.parameter.modifier == VC_TOKEN_KW_OUT ||
            parameter->as.parameter.modifier == VC_TOKEN_KW_IN)
        {
            context_error(&context,
                "iterator method '%s' cannot use ref, out, or in parameters",
                method->as.method_declaration.name);
            goto fail;
        }
        if (!add_capture(&context, parameter->as.parameter.name, parameter,
                parameter->as.parameter.type))
            goto fail;
    }

    if (!collect_locals(&context, body) || !rewrite_statement(&context, body))
        goto fail;

    VcIteratorBlock *finish = new_iterator_block(&context, method->location);
    if (finish == NULL)
        goto fail;
    finish->terminator = VC_ITER_FINISH;
    context.finish_state = finish->state;

    const int entry_state = compile_statement(&context, body, finish->state, -1, -1);
    if (entry_state < 0)
        goto fail;

    char iterator_name[256];
    const int written = snprintf(iterator_name, sizeof(iterator_name), VC_ITERATOR_PREFIX "%s$%s$%zu",
        owner->as.type_declaration.name, method->as.method_declaration.name, iterator_id);
    if (written < 0 || (size_t)written >= sizeof(iterator_name))
    {
        context_error(&context, "generated iterator type name is too long");
        goto fail;
    }

    const VcSourceLocation location = method->location;
    VcAstTypeRef *item_type = return_type->generic_arguments.items[0];
    size_t enumerator_cancellation_parameter = (size_t)-1;
    if (async_iterator)
    {
        for (size_t i = 0; i < method->as.method_declaration.parameters.count; i++)
        {
            if (node_has_attribute(method->as.method_declaration.parameters.items[i],
                    "EnumeratorCancellation"))
            {
                enumerator_cancellation_parameter = i;
                break;
            }
        }
    }
    VcAstNode *iterator = vc_ast_new_node(tree, VC_AST_TYPE_DECLARATION, location);
    if (iterator == NULL)
        goto oom;
    iterator->as.type_declaration.type_kind = VC_AST_TYPE_CLASS;
    iterator->as.type_declaration.modifiers = VC_AST_MOD_PUBLIC | VC_AST_MOD_SEALED;
    iterator->as.type_declaration.name = copy_text(tree, iterator_name);
    if (iterator->as.type_declaration.name == NULL ||
        !vc_ast_type_list_push(tree, &iterator->as.type_declaration.base_types, return_type))
        goto oom;
    if (async_iterator)
    {
        VcAstTypeRef *async_enumerator_type = generic_type(tree, location,
            "IAsyncEnumerator", item_type);
        if (async_enumerator_type == NULL ||
            !vc_ast_type_list_push(tree, &iterator->as.type_declaration.base_types,
                async_enumerator_type))
            goto oom;
    }

    for (size_t i = 0; i < owner->as.type_declaration.generic_constraints.count; i++)
        if (!vc_ast_node_list_push(tree, &iterator->as.type_declaration.generic_constraints,
                owner->as.type_declaration.generic_constraints.items[i]))
            goto oom;
    for (size_t i = 0; i < method->as.method_declaration.generic_constraints.count; i++)
        if (!vc_ast_node_list_push(tree, &iterator->as.type_declaration.generic_constraints,
                method->as.method_declaration.generic_constraints.items[i]))
            goto oom;

    if (!add_field(&context, iterator, location, "_state", named_type(tree, location, "int")) ||
        !add_field(&context, iterator, location, "_dispose_state", named_type(tree, location, "int")) ||
        !add_field(&context, iterator, location, "_current", item_type))
        goto oom;

    const bool captures_this = (method->as.method_declaration.modifiers & VC_AST_MOD_STATIC) == 0;
    VcAstTypeRef *owner_type = NULL;
    if (captures_this || async_iterator)
    {
        owner_type = named_type(tree, location, owner->as.type_declaration.name);
        if (owner_type == NULL || !add_field(&context, iterator, location, "_this", owner_type))
            goto oom;
    }

    for (size_t i = 0; i < context.capture_count; i++)
        if (!add_field(&context, iterator, location,
                context.captures[i].name, context.captures[i].type))
            goto oom;

    VcAstNode *constructor = vc_ast_new_node(tree, VC_AST_METHOD_DECLARATION, location);
    VcAstNode *constructor_body = block(tree, location);
    if (constructor == NULL || constructor_body == NULL)
        goto oom;
    constructor->as.method_declaration.modifiers = VC_AST_MOD_PUBLIC;
    constructor->as.method_declaration.name = copy_text(tree, iterator_name);
    constructor->as.method_declaration.is_constructor = true;
    constructor->as.method_declaration.body = constructor_body;
    if (constructor->as.method_declaration.name == NULL)
        goto oom;

    if (captures_this)
    {
        VcAstNode *owner_parameter = make_parameter(&context, location, "__owner", owner_type);
        if (owner_parameter == NULL ||
            !vc_ast_node_list_push(tree, &constructor->as.method_declaration.parameters, owner_parameter) ||
            !block_push(tree, constructor_body,
                expression_statement(tree, location,
                    this_member_assignment(tree, location, "_this",
                        identifier(tree, location, "__owner")))))
            goto oom;
    }

    for (size_t i = 0; i < method->as.method_declaration.parameters.count; i++)
    {
        VcAstNode *source_parameter = method->as.method_declaration.parameters.items[i];
        VcAstNode *parameter = make_parameter(&context, source_parameter->location,
            source_parameter->as.parameter.name, source_parameter->as.parameter.type);
        if (parameter == NULL ||
            !vc_ast_node_list_push(tree, &constructor->as.method_declaration.parameters, parameter) ||
            !block_push(tree, constructor_body,
                expression_statement(tree, source_parameter->location,
                    this_member_assignment(tree, source_parameter->location,
                        source_parameter->as.parameter.name,
                        identifier(tree, source_parameter->location,
                            source_parameter->as.parameter.name)))))
            goto oom;
    }

    if (!block_push(tree, constructor_body,
            expression_statement(tree, location,
                this_member_assignment(tree, location, "_state",
                    number_literal(tree, location, entry_state)))) ||
        !block_push(tree, constructor_body,
            expression_statement(tree, location,
                this_member_assignment(tree, location, "_dispose_state",
                    number_literal(tree, location, -1)))) ||
        !vc_ast_node_list_push(tree, &iterator->as.type_declaration.members, constructor))
        goto oom;

    VcAstNode *current_property = vc_ast_new_node(tree, VC_AST_PROPERTY_DECLARATION, location);
    if (current_property == NULL)
        goto oom;
    current_property->as.property_declaration.modifiers = VC_AST_MOD_PUBLIC;
    current_property->as.property_declaration.type = item_type;
    current_property->as.property_declaration.name = copy_text(tree, "Current");
    current_property->as.property_declaration.has_getter = true;
    current_property->as.property_declaration.getter_expression = true;
    current_property->as.property_declaration.getter_body = identifier(tree, location, "_current");
    if (current_property->as.property_declaration.name == NULL ||
        current_property->as.property_declaration.getter_body == NULL ||
        !vc_ast_node_list_push(tree, &iterator->as.type_declaration.members, current_property))
        goto oom;

    VcAstNode *move_next = vc_ast_new_node(tree, VC_AST_METHOD_DECLARATION, location);
    VcAstNode *move_body = block(tree, location);
    VcAstNode *move_loop = vc_ast_new_node(tree, VC_AST_WHILE_STATEMENT, location);
    VcAstNode *move_loop_body = block(tree, location);
    if (move_next == NULL || move_body == NULL || move_loop == NULL || move_loop_body == NULL)
        goto oom;
    move_next->as.method_declaration.modifiers = VC_AST_MOD_PUBLIC |
        (async_iterator ? VC_AST_MOD_ASYNC : 0);
    move_next->as.method_declaration.return_type = async_iterator
        ? generic_type(tree, location, "Task",
            named_type(tree, location, "bool"))
        : named_type(tree, location, "bool");
    move_next->as.method_declaration.name = copy_text(tree,
        async_iterator ? "MoveNextAsync" : "MoveNext");
    move_next->as.method_declaration.runtime_source_method_node = method;
    move_next->as.method_declaration.body = move_body;
    move_loop->as.while_statement.condition = bool_literal(tree, location, true);
    move_loop->as.while_statement.body = move_loop_body;
    if (move_next->as.method_declaration.return_type == NULL || move_next->as.method_declaration.name == NULL ||
        move_loop->as.while_statement.condition == NULL)
        goto oom;

    for (size_t i = 0; i < context.block_count; i++)
        if (!emit_state_block(&context, move_loop_body, &context.blocks[i])) goto oom;

    VcAstNode *guarded_move_loop = wrap_move_loop_for_exceptions(&context, move_loop, location);
    if (!block_push(tree, move_loop_body,
            return_statement(tree, location, bool_literal(tree, location, false))) ||
        guarded_move_loop == NULL ||
        !block_push(tree, move_body, guarded_move_loop) ||
        !vc_ast_node_list_push(tree, &iterator->as.type_declaration.members, move_next))
        goto oom;

    VcAstNode *dispose = vc_ast_new_node(tree, VC_AST_METHOD_DECLARATION, location);
    VcAstNode *dispose_body = block(tree, location);
    VcAstNode *dispose_state = vc_ast_new_node(tree, VC_AST_LOCAL_DECLARATION, location);
    VcAstNode *not_started = vc_ast_new_node(tree, VC_AST_IF_STATEMENT, location);
    if (dispose == NULL || dispose_body == NULL || dispose_state == NULL || not_started == NULL)
        goto oom;
    dispose->as.method_declaration.modifiers = VC_AST_MOD_PUBLIC |
        (async_iterator ? VC_AST_MOD_ASYNC : 0);
    dispose->as.method_declaration.return_type = named_type(tree, location,
        async_iterator ? "Task" : "void");
    dispose->as.method_declaration.name = copy_text(tree,
        async_iterator ? "DisposeAsync" : "Dispose");
    dispose->as.method_declaration.runtime_source_method_node = method;
    dispose->as.method_declaration.body = dispose_body;
    dispose_state->as.local_declaration.type = named_type(tree, location, "int");
    dispose_state->as.local_declaration.name = copy_text(tree, "__iterator_dispose_state");
    dispose_state->as.local_declaration.initializer = identifier(tree, location, "_dispose_state");
    not_started->as.if_statement.condition = binary(tree, location, VC_TOKEN_LESS,
        identifier(tree, location, "__iterator_dispose_state"), number_literal(tree, location, 0));
    not_started->as.if_statement.then_statement = return_statement(tree, location, NULL);
    if (dispose->as.method_declaration.return_type == NULL ||
        dispose->as.method_declaration.name == NULL ||
        dispose_state->as.local_declaration.type == NULL ||
        dispose_state->as.local_declaration.name == NULL ||
        dispose_state->as.local_declaration.initializer == NULL ||
        not_started->as.if_statement.condition == NULL ||
        not_started->as.if_statement.then_statement == NULL ||
        !block_push(tree, dispose_body, dispose_state) ||
        !block_push(tree, dispose_body,
            expression_statement(tree, location,
                this_member_assignment(tree, location, "_dispose_state",
                    number_literal(tree, location, -1)))) ||
        !block_push(tree, dispose_body,
            expression_statement(tree, location,
                this_member_assignment(tree, location, "_state",
                    number_literal(tree, location, -1)))) ||
        !block_push(tree, dispose_body, not_started))
        goto oom;

    for (size_t i = 0; i < context.block_count; i++)
    {
        const VcIteratorBlock *source = &context.blocks[i];
        if (source->active_finally_count == 0)
            continue;
        VcAstNode *cleanup = fault_cleanup_block(&context, source);
        VcAstNode *condition = binary(tree, source->location, VC_TOKEN_EQUAL_EQUAL,
            identifier(tree, source->location, "__iterator_dispose_state"),
            number_literal(tree, source->location, source->state));
        VcAstNode *if_node = vc_ast_new_node(tree, VC_AST_IF_STATEMENT, source->location);
        if (cleanup == NULL || condition == NULL || if_node == NULL)
            goto oom;
        if_node->as.if_statement.condition = condition;
        if_node->as.if_statement.then_statement = cleanup;
        if (!block_push(tree, dispose_body, if_node))
            goto oom;
    }

    if (!vc_ast_node_list_push(tree, &iterator->as.type_declaration.members, dispose))
        goto oom;

    if (async_iterator)
    {
        VcAstNode *get_async = vc_ast_new_node(tree, VC_AST_METHOD_DECLARATION, location);
        VcAstNode *get_body = block(tree, location);
        VcAstNode *token_parameter = make_parameter(&context, location,
            "cancellationToken", named_type(tree, location, "CancellationToken"));
        VcAstNode *default_token = vc_ast_new_node(tree, VC_AST_DEFAULT_EXPRESSION, location);
        VcAstNode *create_enumerator = vc_ast_new_node(tree, VC_AST_NEW_EXPRESSION, location);
        if (get_async == NULL || get_body == NULL || token_parameter == NULL ||
            default_token == NULL || create_enumerator == NULL)
            goto oom;

        get_async->as.method_declaration.modifiers = VC_AST_MOD_PUBLIC;
        get_async->as.method_declaration.return_type = generic_type(tree, location,
            "IAsyncEnumerator", item_type);
        get_async->as.method_declaration.name = copy_text(tree, "GetAsyncEnumerator");
        get_async->as.method_declaration.body = get_body;
        token_parameter->as.parameter.default_value = default_token;
        create_enumerator->as.new_expression.type = named_type(tree, location, iterator_name);
        if (get_async->as.method_declaration.return_type == NULL ||
            get_async->as.method_declaration.name == NULL ||
            create_enumerator->as.new_expression.type == NULL ||
            !vc_ast_node_list_push(tree, &get_async->as.method_declaration.parameters,
                token_parameter))
            goto oom;

        if (captures_this)
        {
            VcAstNode *owner_value = member_access(tree, location,
                identifier(tree, location, "this"), "_this");
            if (owner_value == NULL ||
                !vc_ast_node_list_push(tree, &create_enumerator->as.new_expression.arguments,
                    owner_value))
                goto oom;
        }
        for (size_t i = 0; i < method->as.method_declaration.parameters.count; i++)
        {
            const VcAstNode *source_parameter =
                method->as.method_declaration.parameters.items[i];
            VcAstNode *value = NULL;
            if (i == enumerator_cancellation_parameter)
            {
                VcAstNode *consumer_token = identifier(tree, source_parameter->location,
                    "cancellationToken");
                VcAstNode *can_cancel = member_access(tree, source_parameter->location,
                    identifier(tree, source_parameter->location, "cancellationToken"),
                    "CanBeCanceled");
                VcAstNode *captured_token = member_access(tree, source_parameter->location,
                    identifier(tree, source_parameter->location, "this"),
                    source_parameter->as.parameter.name);
                value = consumer_token != NULL && can_cancel != NULL && captured_token != NULL
                    ? conditional_expression(tree, source_parameter->location,
                        can_cancel, consumer_token, captured_token)
                    : NULL;
            }
            else
            {
                value = member_access(tree, source_parameter->location,
                    identifier(tree, source_parameter->location, "this"),
                    source_parameter->as.parameter.name);
            }
            if (value == NULL ||
                !vc_ast_node_list_push(tree, &create_enumerator->as.new_expression.arguments,
                    value))
                goto oom;
        }

        if (!block_push(tree, get_body,
                return_statement(tree, location, create_enumerator)) ||
            !vc_ast_node_list_push(tree, &iterator->as.type_declaration.members, get_async))
            goto oom;
    }

    if (!vc_ast_node_list_push(tree, parent_declarations, iterator))
        goto oom;

    VcAstNode *new_iterator = vc_ast_new_node(tree, VC_AST_NEW_EXPRESSION, location);
    VcAstNode *return_iterator = return_statement(tree, location, new_iterator);
    VcAstNode *new_body = block(tree, location);
    if (new_iterator == NULL || return_iterator == NULL || new_body == NULL)
        goto oom;
    new_iterator->as.new_expression.type = named_type(tree, location, iterator_name);
    if (new_iterator->as.new_expression.type == NULL)
        goto oom;

    if (captures_this)
    {
        VcAstNode *self = identifier(tree, location, "this");
        if (self == NULL || !vc_ast_node_list_push(tree, &new_iterator->as.new_expression.arguments, self))
            goto oom;
    }
    for (size_t i = 0; i < method->as.method_declaration.parameters.count; i++)
    {
        VcAstNode *parameter = method->as.method_declaration.parameters.items[i];
        VcAstNode *argument = identifier(tree, parameter->location, parameter->as.parameter.name);
        if (argument == NULL || !vc_ast_node_list_push(tree, &new_iterator->as.new_expression.arguments, argument))
            goto oom;
    }

    if (!block_push(tree, new_body, return_iterator))
        goto oom;
    method->as.method_declaration.body = new_body;
    if (async_iterator)
    {
        if (enumerator_cancellation_parameter != (size_t)-1)
            remove_node_attribute(
                method->as.method_declaration.parameters.items[enumerator_cancellation_parameter],
                "EnumeratorCancellation");
        method->as.method_declaration.modifiers &= ~VC_AST_MOD_ASYNC;
    }

    free(context.captures);
    free(context.active_finally_blocks);
    for (size_t i = 0; i < context.block_count; i++)
    {
        free(context.blocks[i].switch_targets);
        free(context.blocks[i].active_finally_blocks);
    }
    free(context.blocks);
    return true;

oom:
    context_error(&context, "out of memory while lowering iterator method '%s'",
        method->as.method_declaration.name);
fail:
    free(context.captures);
    free(context.active_finally_blocks);
    for (size_t i = 0; i < context.block_count; i++)
    {
        free(context.blocks[i].switch_targets);
        free(context.blocks[i].active_finally_blocks);
    }
    free(context.blocks);
    return false;
}

static bool lower_declarations(VcAstTree *tree, VcAstNodeList *declarations,
    size_t *iterator_id, const VcSemanticModel *semantic,
    char *error, size_t error_size)
{
    const size_t original_count = declarations->count;
    for (size_t i = 0; i < original_count; i++)
    {
        VcAstNode *node = declarations->items[i];
        if (node->kind == VC_AST_NAMESPACE_DECLARATION)
        {
            if (!lower_declarations(tree, &node->as.namespace_declaration.declarations,
                    iterator_id, semantic, error, error_size))
                return false;
            continue;
        }
        if (node->kind != VC_AST_TYPE_DECLARATION || node->as.type_declaration.type_kind == VC_AST_TYPE_INTERFACE ||
            node->as.type_declaration.generic_parameters.count != 0)
            continue;
        for (size_t member_index = 0; member_index < node->as.type_declaration.members.count; member_index++)
        {
            VcAstNode *member = node->as.type_declaration.members.items[member_index];
            if (member->kind != VC_AST_METHOD_DECLARATION || member->as.method_declaration.is_constructor)
                continue;
            if (member->as.method_declaration.generic_parameters.count != 0)
                continue;
            if (!lower_method(tree, declarations, node, member, (*iterator_id)++,
                    semantic, error, error_size))
                return false;
        }
    }
    return true;
}

static bool declarations_have_iterator(const VcAstNodeList *declarations)
{
    for (size_t i = 0; i < declarations->count; i++)
    {
        const VcAstNode *node = declarations->items[i];
        if (node->kind == VC_AST_NAMESPACE_DECLARATION)
        {
            if (declarations_have_iterator(&node->as.namespace_declaration.declarations))
                return true;
            continue;
        }
        if (node->kind != VC_AST_TYPE_DECLARATION)
            continue;
        for (size_t member_index = 0;
             member_index < node->as.type_declaration.members.count;
             member_index++)
        {
            const VcAstNode *member = node->as.type_declaration.members.items[member_index];
            if (member->kind == VC_AST_METHOD_DECLARATION &&
                !member->as.method_declaration.is_constructor &&
                node_contains_yield(member->as.method_declaration.body))
                return true;
        }
    }
    return false;
}

static bool declarations_have_async_iterator(const VcAstNodeList *declarations)
{
    for (size_t i = 0; i < declarations->count; i++)
    {
        const VcAstNode *node = declarations->items[i];
        if (node->kind == VC_AST_NAMESPACE_DECLARATION)
        {
            if (declarations_have_async_iterator(&node->as.namespace_declaration.declarations))
                return true;
            continue;
        }
        if (node->kind != VC_AST_TYPE_DECLARATION)
            continue;
        for (size_t member_index = 0;
             member_index < node->as.type_declaration.members.count;
             member_index++)
        {
            const VcAstNode *member = node->as.type_declaration.members.items[member_index];
            if (member->kind == VC_AST_METHOD_DECLARATION &&
                !member->as.method_declaration.is_constructor &&
                (member->as.method_declaration.modifiers & VC_AST_MOD_ASYNC) != 0 &&
                node_contains_yield(member->as.method_declaration.body))
                return true;
        }
    }
    return false;
}

bool vc_has_iterators(VcAstTree **trees, size_t tree_count)
{
    for (size_t i = 0; i < tree_count; i++)
    {
        VcAstTree *tree = trees[i];
        if (tree != NULL && tree->root != NULL &&
            tree->root->kind == VC_AST_COMPILATION_UNIT &&
            declarations_have_iterator(&tree->root->as.compilation_unit.declarations))
            return true;
    }
    return false;
}

bool vc_has_async_iterators(VcAstTree **trees, size_t tree_count)
{
    for (size_t i = 0; i < tree_count; i++)
    {
        VcAstTree *tree = trees[i];
        if (tree != NULL && tree->root != NULL &&
            tree->root->kind == VC_AST_COMPILATION_UNIT &&
            declarations_have_async_iterator(&tree->root->as.compilation_unit.declarations))
            return true;
    }
    return false;
}

bool vc_lower_iterators(VcAstTree **trees, size_t tree_count,
    const VcSemanticModel *semantic, char *error, size_t error_size)
{
    if (error != NULL && error_size != 0)
        error[0] = '\0';
    size_t iterator_id = 0;
    for (size_t i = 0; i < tree_count; i++)
    {
        VcAstTree *tree = trees[i];
        if (tree == NULL || tree->root == NULL || tree->root->kind != VC_AST_COMPILATION_UNIT)
            continue;
        if (!lower_declarations(tree, &tree->root->as.compilation_unit.declarations,
                &iterator_id, semantic, error, error_size))
            return false;
    }
    return true;
}
