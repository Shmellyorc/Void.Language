#include "diagnostic.h"
#include "monomorph.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct VcGenericTemplate
{
    VcAstTree *tree;
    VcAstNodeList *parent;
    VcAstNode *node;
    const char *namespace_name;
} VcGenericTemplate;

typedef struct VcGenericSubstitution
{
    const VcAstStringList *parameters;
    const VcAstTypeList *arguments;
    const VcAstNodeList *constraints;
    const VcAstNode *current_method;
} VcGenericSubstitution;

typedef struct VcGenericMethodRequest
{
    const char *name;
    VcAstTypeList arguments;
    char specialized_name[256];
    bool matched;
    VcAstTree *use_tree;
    VcSourceSpan span;
} VcGenericMethodRequest;

typedef struct VcMonomorphContext
{
    VcAstTree **trees;
    size_t tree_count;
    VcGenericTemplate *templates;
    size_t template_count;
    size_t template_capacity;
    VcGenericMethodRequest *method_requests;
    size_t method_request_count;
    size_t method_request_capacity;
    size_t changes;
    const VcSource *const *sources;
    VcDiagnostic *diagnostic;
    char *error;
    size_t error_size;
} VcMonomorphContext;

static void set_error(VcMonomorphContext *context, const char *format, ...)
{
    if (context->error == NULL || context->error_size == 0 || context->error[0] != '\0')
        return;
    va_list args;
    va_start(args, format);
    vsnprintf(context->error, context->error_size, format, args);
    va_end(args);
}

static const VcSource *source_for_tree(const VcMonomorphContext *context, const VcAstTree *tree)
{
    if (context->sources == NULL) return NULL;
    for (size_t i = 0; i < context->tree_count; i++)
        if (context->trees[i] == tree) return context->sources[i];
    return NULL;
}
static void generic_diagnostic(VcMonomorphContext *context, VcAstTree *tree, VcSourceSpan span)
{
    if (context->diagnostic == NULL || context->diagnostic->message[0] != '\0') return;
    const VcSource *source = source_for_tree(context, tree);
    vc_diagnostic_init(context->diagnostic, source != NULL ? source->path : NULL, span);
    context->diagnostic->source = source;
    vc_diagnostic_set_code(context->diagnostic, VC_DIAG_GENERIC_ARGUMENT);
    snprintf(context->diagnostic->message, sizeof(context->diagnostic->message), "%s",
        context->error != NULL ? context->error : "generic arguments could not be resolved");
    vc_diagnostic_add_context(context->diagnostic, VC_DIAGNOSTIC_NOTE, NULL,
        (VcSourceSpan){0}, "Explicit generic argument count and type names must match an available generic declaration.");
}

static bool append_text(char *output, size_t output_size, size_t *written, const char *text)
{
    const size_t length = strlen(text);
    if (*written + length + 1 > output_size)
        return false;
    memcpy(output + *written, text, length);
    *written += length;
    output[*written] = '\0';
    return true;
}

static bool append_sanitized(char *output, size_t output_size, size_t *written, const char *text)
{
    for (size_t i = 0; text[i] != '\0'; i++)
    {
        const char c = text[i];
        const bool alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        const bool digit = c >= '0' && c <= '9';
        /* Reserve '_' for structural separators; literal underscores must not
           alias namespace dots or generated generic suffixes. */
        if (c == '_' || c == '.')
        {
            if (!append_text(output, output_size, written, c == '_' ? "__u" : "__d"))
                return false;
            continue;
        }
        if (!alpha && !digit)
        {
            char escaped[8];
            snprintf(escaped, sizeof(escaped), "__x%02x", (unsigned)(unsigned char)c);
            if (!append_text(output, output_size, written, escaped))
                return false;
            continue;
        }
        const char value = c;
        if (*written + 2 > output_size)
            return false;
        output[(*written)++] = value;
        output[*written] = '\0';
    }
    return true;
}

bool vc_monomorph_mangle_type_ref(const VcAstTypeRef *type, char *output, size_t output_size)
{
    if (type == NULL || type->name == NULL || output == NULL || output_size == 0)
        return false;
    output[0] = '\0';
    size_t written = 0;
    if (!append_sanitized(output, output_size, &written, type->name))
        return false;
    if (type->generic_arguments.count != 0)
    {
        char count[32];
        snprintf(count, sizeof(count), "__g%zu", type->generic_arguments.count);
        if (!append_text(output, output_size, &written, count))
            return false;
        for (size_t i = 0; i < type->generic_arguments.count; i++)
        {
            char nested[256];
            if (!vc_monomorph_mangle_type_ref(type->generic_arguments.items[i], nested, sizeof(nested)) ||
                !append_text(output, output_size, &written, "_") ||
                !append_text(output, output_size, &written, nested))
                return false;
        }
    }
    if (type->nullable && !append_text(output, output_size, &written, "__nullable"))
        return false;
    for (size_t i = 0; i < type->pointer_depth; i++)
    {
        if (!append_text(output, output_size, &written, "__ptr"))
            return false;
    }
    if (type->rectangular_rank > 1)
    {
        char rank[32];
        snprintf(rank, sizeof(rank), "__md%zu", type->rectangular_rank);
        if (!append_text(output, output_size, &written, rank))
            return false;
    }
    for (size_t i = 0; i < type->array_rank; i++)
    {
        if (!append_text(output, output_size, &written, "__arr"))
            return false;
    }
    return true;
}

static bool same_namespace(const char *left, const char *right)
{
    if (left == NULL || right == NULL)
        return left == right;
    return strcmp(left, right) == 0;
}

static bool push_template(VcMonomorphContext *context, VcGenericTemplate value)
{
    if (context->template_count == context->template_capacity)
    {
        const size_t capacity = context->template_capacity == 0 ? 8 : context->template_capacity * 2;
        VcGenericTemplate *items = realloc(context->templates, capacity * sizeof(*items));
        if (items == NULL)
            return false;
        context->templates = items;
        context->template_capacity = capacity;
    }
    context->templates[context->template_count++] = value;
    return true;
}

static bool collect_templates(
    VcMonomorphContext *context,
    VcAstTree *tree,
    VcAstNodeList *declarations,
    const char *namespace_name)
{
    for (size_t i = 0; i < declarations->count; i++)
    {
        VcAstNode *node = declarations->items[i];
        if (node->kind == VC_AST_NAMESPACE_DECLARATION)
        {
            if (!collect_templates(context, tree, &node->as.namespace_declaration.declarations,
                    node->as.namespace_declaration.name))
                return false;
            continue;
        }
        if (node->kind != VC_AST_TYPE_DECLARATION || node->as.type_declaration.generic_parameters.count == 0)
            continue;
        if (!push_template(context, (VcGenericTemplate){tree, declarations, node, namespace_name}))
        {
            set_error(context, "out of memory while collecting generic templates");
            return false;
        }
    }
    return true;
}

static const VcAstTypeRef *lookup_substitution(
    const VcGenericSubstitution *substitution,
    const VcAstTypeRef *type)
{
    if (substitution == NULL || substitution->parameters == NULL || substitution->arguments == NULL ||
        type == NULL || type->name == NULL || type->generic_arguments.count != 0)
        return NULL;
    for (size_t i = 0; i < substitution->parameters->count && i < substitution->arguments->count; i++)
    {
        if (strcmp(substitution->parameters->items[i], type->name) == 0)
            return substitution->arguments->items[i];
    }
    return NULL;
}

static bool monomorph_type_ref_has_explicit_function_pointer(const VcAstTypeRef *type)
{
    if (type == NULL)
        return false;
    if (type->is_function_pointer && type->generic_parameter_origin == NULL)
        return true;
    for (size_t i = 0; i < type->generic_arguments.count; i++)
        if (monomorph_type_ref_has_explicit_function_pointer(type->generic_arguments.items[i]))
            return true;
    return false;
}

static VcAstTypeRef *clone_type(
    VcMonomorphContext *context,
    VcAstTree *tree,
    const VcAstTypeRef *source,
    const VcGenericSubstitution *substitution);

static VcAstTypeRef *clone_type_plain(
    VcMonomorphContext *context,
    VcAstTree *tree,
    const VcAstTypeRef *source,
    const VcGenericSubstitution *substitution)
{
    VcAstTypeRef *copy = vc_ast_new_type(tree, source->location);
    if (copy == NULL)
    {
        set_error(context, "out of memory while specializing generic type");
        return NULL;
    }
    copy->name = source->name;
    copy->is_function_pointer = source->is_function_pointer;
    copy->pointer_depth = source->pointer_depth;
    copy->array_rank = source->array_rank;
    copy->rectangular_rank = source->rectangular_rank;
    copy->nullable = source->nullable;
    copy->generic_constraint_parameter = source->generic_constraint_parameter;
    copy->is_global_qualified = source->is_global_qualified;
    copy->generic_parameter_origin = source->generic_parameter_origin;
    for (size_t i = 0; i < source->generic_arguments.count; i++)
    {
        VcAstTypeRef *argument = clone_type(context, tree, source->generic_arguments.items[i], substitution);
        if (argument == NULL || !vc_ast_type_list_push(tree, &copy->generic_arguments, argument))
        {
            set_error(context, "out of memory while specializing generic arguments");
            return NULL;
        }
    }
    return copy;
}

static VcAstTypeRef *clone_type(
    VcMonomorphContext *context,
    VcAstTree *tree,
    const VcAstTypeRef *source,
    const VcGenericSubstitution *substitution)
{
    if (source == NULL)
        return NULL;
    const VcAstTypeRef *replacement = lookup_substitution(substitution, source);
    if (replacement == NULL)
        return clone_type_plain(context, tree, source, substitution);

    VcAstTypeRef *copy = clone_type_plain(context, tree, replacement, NULL);
    if (copy == NULL)
        return NULL;
    copy->generic_constraint_parameter = copy->generic_constraint_parameter ||
        source->generic_constraint_parameter;
    if (copy->generic_parameter_origin == NULL && source->name != NULL && substitution != NULL &&
        substitution->parameters != NULL)
    {
        for (size_t i = 0; i < substitution->parameters->count; i++)
            if (strcmp(substitution->parameters->items[i], source->name) == 0)
            {
                copy->generic_parameter_origin = source->name;
                break;
            }
    }
    copy->pointer_depth += source->pointer_depth;
    copy->array_rank += source->array_rank;
    if (source->rectangular_rank != 0)
        copy->rectangular_rank = source->rectangular_rank;
    copy->nullable = copy->nullable || source->nullable;
    return copy;
}

static const VcAstNode *monomorph_find_type_declaration(
    const VcMonomorphContext *context, const char *name)
{
    if (context == NULL || name == NULL)
        return NULL;
    for (size_t t = 0; t < context->tree_count; t++)
    {
        VcAstTree *tree = context->trees[t];
        if (tree == NULL || tree->root == NULL)
            continue;
        const VcAstNodeList *top = &tree->root->as.compilation_unit.declarations;
        for (size_t i = 0; i < top->count; i++)
        {
            const VcAstNode *node = top->items[i];
            if (node->kind == VC_AST_TYPE_DECLARATION && node->as.type_declaration.name != NULL &&
                strcmp(node->as.type_declaration.name, name) == 0)
                return node;
            if (node->kind == VC_AST_NAMESPACE_DECLARATION)
            {
                const VcAstNodeList *decls = &node->as.namespace_declaration.declarations;
                for (size_t j = 0; j < decls->count; j++)
                {
                    const VcAstNode *nested = decls->items[j];
                    if (nested->kind == VC_AST_TYPE_DECLARATION && nested->as.type_declaration.name != NULL &&
                        strcmp(nested->as.type_declaration.name, name) == 0)
                        return nested;
                }
            }
        }
    }
    return NULL;
}

static bool substitution_has_parameter(
    const VcGenericSubstitution *substitution, const char *name)
{
    if (substitution == NULL || substitution->parameters == NULL || name == NULL)
        return false;
    for (size_t i = 0; i < substitution->parameters->count; i++)
        if (strcmp(substitution->parameters->items[i], name) == 0)
            return true;
    return false;
}

static bool substitution_parameter_has_interface_constraint(
    const VcMonomorphContext *context,
    const VcGenericSubstitution *substitution,
    const char *name)
{
    if (substitution == NULL || substitution->constraints == NULL || name == NULL)
        return false;
    for (size_t i = 0; i < substitution->constraints->count; i++)
    {
        const VcAstNode *constraint = substitution->constraints->items[i];
        if (constraint == NULL || constraint->kind != VC_AST_GENERIC_CONSTRAINT ||
            constraint->as.generic_constraint.parameter == NULL ||
            strcmp(constraint->as.generic_constraint.parameter, name) != 0)
            continue;
        for (size_t c = 0; c < constraint->as.generic_constraint.type_constraints.count; c++)
        {
            const VcAstTypeRef *type = constraint->as.generic_constraint.type_constraints.items[c];
            const VcAstNode *interface_node = type != NULL
                ? monomorph_find_type_declaration(context, type->name) : NULL;
            if (interface_node != NULL && interface_node->kind == VC_AST_TYPE_DECLARATION &&
                interface_node->as.type_declaration.type_kind == VC_AST_TYPE_INTERFACE)
                return true;
        }
    }
    return false;
}

static const VcAstTypeRef *lookup_identifier_substitution(
    const VcMonomorphContext *context,
    const VcGenericSubstitution *substitution,
    const char *name)
{
    if (substitution == NULL || substitution->parameters == NULL || substitution->arguments == NULL ||
        name == NULL || !substitution_parameter_has_interface_constraint(context, substitution, name))
        return NULL;
    for (size_t i = 0; i < substitution->parameters->count && i < substitution->arguments->count; i++)
        if (strcmp(substitution->parameters->items[i], name) == 0)
            return substitution->arguments->items[i];
    return NULL;
}


static const char *source_identifier_generic_parameter(
    const VcMonomorphContext *context,
    const VcGenericSubstitution *substitution,
    const VcAstNode *expression)
{
    if (substitution == NULL || substitution->current_method == NULL ||
        expression == NULL || expression->kind != VC_AST_IDENTIFIER_EXPRESSION)
        return NULL;
    const char *name = expression->as.identifier_expression.name;
    const VcAstNode *method = substitution->current_method;
    for (size_t i = 0; i < method->as.method_declaration.parameters.count; i++)
    {
        const VcAstNode *parameter = method->as.method_declaration.parameters.items[i];
        if (parameter == NULL || parameter->kind != VC_AST_PARAMETER ||
            parameter->as.parameter.name == NULL || strcmp(parameter->as.parameter.name, name) != 0)
            continue;
        const VcAstTypeRef *type = parameter->as.parameter.type;
        if (type == NULL || type->name == NULL || type->generic_arguments.count != 0 ||
            type->pointer_depth != 0 || type->array_rank != 0 || type->rectangular_rank != 0 || type->nullable)
            return NULL;
        (void)context;
        if (substitution->parameters == NULL)
            return NULL;
        for (size_t p = 0; p < substitution->parameters->count; p++)
            if (strcmp(substitution->parameters->items[p], type->name) == 0)
                return type->name;
        return NULL;
    }
    return NULL;
}

static VcAstNode *clone_node(
    VcMonomorphContext *context,
    VcAstTree *tree,
    const VcAstNode *source,
    const VcGenericSubstitution *substitution);

static bool clone_node_list(
    VcMonomorphContext *context,
    VcAstTree *tree,
    const VcAstNodeList *source,
    VcAstNodeList *target,
    const VcGenericSubstitution *substitution)
{
    for (size_t i = 0; i < source->count; i++)
    {
        VcAstNode *copy = clone_node(context, tree, source->items[i], substitution);
        if (copy == NULL || !vc_ast_node_list_push(tree, target, copy))
        {
            set_error(context, "out of memory while specializing generic syntax");
            return false;
        }
    }
    return true;
}

static bool clone_type_list(
    VcMonomorphContext *context,
    VcAstTree *tree,
    const VcAstTypeList *source,
    VcAstTypeList *target,
    const VcGenericSubstitution *substitution)
{
    for (size_t i = 0; i < source->count; i++)
    {
        VcAstTypeRef *copy = clone_type(context, tree, source->items[i], substitution);
        if (copy == NULL || !vc_ast_type_list_push(tree, target, copy))
        {
            set_error(context, "out of memory while specializing generic type list");
            return false;
        }
    }
    return true;
}

static VcAstNode *clone_node(
    VcMonomorphContext *context,
    VcAstTree *tree,
    const VcAstNode *source,
    const VcGenericSubstitution *substitution)
{
    if (source == NULL)
        return NULL;
    VcAstNode *copy = vc_ast_new_node(tree, source->kind, source->location);
    if (copy == NULL)
    {
        set_error(context, "out of memory while specializing generic node");
        return NULL;
    }
    copy->span = source->span;
    copy->receiver_type = clone_type(context, tree, source->receiver_type, substitution);
    if (source->kind == VC_AST_TYPE_RECEIVER_EXPRESSION && copy->receiver_type != NULL)
        copy->receiver_type->span = source->receiver_type->span;
    copy->argument_name = source->argument_name;
    if (!clone_node_list(context, tree, &source->attributes, &copy->attributes, substitution))
        return NULL;

    switch (source->kind)
    {
        case VC_AST_COMPILATION_UNIT:
            if (!clone_node_list(context, tree, &source->as.compilation_unit.declarations,
                    &copy->as.compilation_unit.declarations, substitution)) return NULL;
            break;
        case VC_AST_USING_DECLARATION:
            copy->as.using_declaration = source->as.using_declaration;
            break;
        case VC_AST_NAMESPACE_DECLARATION:
            copy->as.namespace_declaration.name = source->as.namespace_declaration.name;
            copy->as.namespace_declaration.file_scoped = source->as.namespace_declaration.file_scoped;
            if (!clone_node_list(context, tree, &source->as.namespace_declaration.declarations,
                    &copy->as.namespace_declaration.declarations, substitution)) return NULL;
            break;
        case VC_AST_TYPE_DECLARATION:
            copy->as.type_declaration.type_kind = source->as.type_declaration.type_kind;
            copy->as.type_declaration.modifiers = source->as.type_declaration.modifiers;
            copy->as.type_declaration.name = source->as.type_declaration.name;
            copy->as.type_declaration.generic_parameters = source->as.type_declaration.generic_parameters;
            copy->as.type_declaration.original_generic_name = source->as.type_declaration.original_generic_name;
            if (!clone_node_list(context, tree, &source->as.type_declaration.generic_constraints,
                    &copy->as.type_declaration.generic_constraints, substitution) ||
                !clone_type_list(context, tree, &source->as.type_declaration.generic_arguments,
                    &copy->as.type_declaration.generic_arguments, substitution) ||
                !clone_type_list(context, tree, &source->as.type_declaration.base_types,
                    &copy->as.type_declaration.base_types, substitution) ||
                !clone_node_list(context, tree, &source->as.type_declaration.members,
                    &copy->as.type_declaration.members, substitution)) return NULL;
            break;
        case VC_AST_ENUM_MEMBER:
            copy->as.enum_member.name = source->as.enum_member.name;
            copy->as.enum_member.value = clone_node(context, tree, source->as.enum_member.value, substitution);
            break;
        case VC_AST_METHOD_DECLARATION:
        {
            VcGenericSubstitution method_substitution = substitution != NULL
                ? *substitution : (VcGenericSubstitution){0};
            method_substitution.current_method = source;
            const VcGenericSubstitution *method_sub = substitution != NULL ? &method_substitution : NULL;
            copy->as.method_declaration.modifiers = source->as.method_declaration.modifiers;
            copy->as.method_declaration.return_type = clone_type(context, tree,
                source->as.method_declaration.return_type, method_sub);
            copy->as.method_declaration.returns_ref = source->as.method_declaration.returns_ref;
            copy->as.method_declaration.returns_ref_readonly = source->as.method_declaration.returns_ref_readonly;
            copy->as.method_declaration.name = source->as.method_declaration.name;
            copy->as.method_declaration.original_generic_name = source->as.method_declaration.original_generic_name;
            copy->as.method_declaration.runtime_source_method_node =
                source->as.method_declaration.runtime_source_method_node;
            copy->as.method_declaration.runtime_hide_frame =
                source->as.method_declaration.runtime_hide_frame;
            copy->as.method_declaration.generic_parameters = source->as.method_declaration.generic_parameters;
            if (!clone_node_list(context, tree, &source->as.method_declaration.generic_constraints,
                    &copy->as.method_declaration.generic_constraints, method_sub)) return NULL;
            copy->as.method_declaration.is_constructor = source->as.method_declaration.is_constructor;
            copy->as.method_declaration.has_base_constructor_initializer =
                source->as.method_declaration.has_base_constructor_initializer;
            copy->as.method_declaration.has_this_constructor_initializer =
                source->as.method_declaration.has_this_constructor_initializer;
            copy->as.method_declaration.is_operator = source->as.method_declaration.is_operator;
            copy->as.method_declaration.operator_kind = source->as.method_declaration.operator_kind;
            if (!clone_node_list(context, tree, &source->as.method_declaration.parameters,
                    &copy->as.method_declaration.parameters, method_sub) ||
                !clone_node_list(context, tree,
                    &source->as.method_declaration.constructor_initializer_arguments,
                    &copy->as.method_declaration.constructor_initializer_arguments,
                    method_sub)) return NULL;
            copy->as.method_declaration.body = clone_node(context, tree, source->as.method_declaration.body, method_sub);
            break;
        }
        case VC_AST_FIELD_DECLARATION:
            copy->as.field_declaration.modifiers = source->as.field_declaration.modifiers;
            copy->as.field_declaration.type = clone_type(context, tree, source->as.field_declaration.type, substitution);
            copy->as.field_declaration.name = source->as.field_declaration.name;
            copy->as.field_declaration.initializer = clone_node(context, tree, source->as.field_declaration.initializer, substitution);
            copy->as.field_declaration.event_add_body = clone_node(context, tree, source->as.field_declaration.event_add_body, substitution);
            copy->as.field_declaration.event_remove_body = clone_node(context, tree, source->as.field_declaration.event_remove_body, substitution);
            copy->as.field_declaration.has_event_add_accessor = source->as.field_declaration.has_event_add_accessor;
            copy->as.field_declaration.has_event_remove_accessor = source->as.field_declaration.has_event_remove_accessor;
            copy->as.field_declaration.is_event = source->as.field_declaration.is_event;
            copy->as.field_declaration.is_ref = source->as.field_declaration.is_ref;
            copy->as.field_declaration.ref_readonly = source->as.field_declaration.ref_readonly;
            break;
        case VC_AST_PROPERTY_DECLARATION:
            copy->as.property_declaration = source->as.property_declaration;
            copy->as.property_declaration.type = clone_type(context, tree, source->as.property_declaration.type, substitution);
            copy->as.property_declaration.getter_body = clone_node(context, tree, source->as.property_declaration.getter_body, substitution);
            copy->as.property_declaration.setter_body = clone_node(context, tree, source->as.property_declaration.setter_body, substitution);
            copy->as.property_declaration.initializer = clone_node(context, tree, source->as.property_declaration.initializer, substitution);
            break;
        case VC_AST_PARAMETER:
            copy->as.parameter.modifier = source->as.parameter.modifier;
            copy->as.parameter.scoped = source->as.parameter.scoped;
            copy->as.parameter.type = clone_type(context, tree, source->as.parameter.type, substitution);
            copy->as.parameter.name = source->as.parameter.name;
            copy->as.parameter.default_value = clone_node(context, tree, source->as.parameter.default_value, substitution);
            copy->as.parameter.extension_receiver = source->as.parameter.extension_receiver;
            break;
        case VC_AST_ATTRIBUTE:
            copy->as.attribute.name = source->as.attribute.name;
            if (!clone_node_list(context, tree, &source->as.attribute.arguments,
                    &copy->as.attribute.arguments, substitution)) return NULL;
            break;
        case VC_AST_GENERIC_CONSTRAINT:
        {
            copy->as.generic_constraint.parameter = source->as.generic_constraint.parameter;
            copy->as.generic_constraint.requires_reference_type =
                source->as.generic_constraint.requires_reference_type;
            copy->as.generic_constraint.requires_value_type =
                source->as.generic_constraint.requires_value_type;
            copy->as.generic_constraint.requires_unmanaged_type =
                source->as.generic_constraint.requires_unmanaged_type;
            copy->as.generic_constraint.requires_constructor =
                source->as.generic_constraint.requires_constructor;
            if (!clone_type_list(context, tree, &source->as.generic_constraint.type_constraints,
                    &copy->as.generic_constraint.type_constraints, substitution)) return NULL;
            if (source->as.generic_constraint.argument_type != NULL)
            {
                copy->as.generic_constraint.argument_type = clone_type(context, tree,
                    source->as.generic_constraint.argument_type, substitution);
                if (copy->as.generic_constraint.argument_type == NULL) return NULL;
            }
            else if (substitution != NULL && substitution->parameters != NULL &&
                substitution->arguments != NULL)
            {
                for (size_t i = 0; i < substitution->parameters->count &&
                    i < substitution->arguments->count; i++)
                {
                    if (strcmp(substitution->parameters->items[i],
                            source->as.generic_constraint.parameter) == 0)
                    {
                        copy->as.generic_constraint.argument_type = clone_type(context, tree,
                            substitution->arguments->items[i], NULL);
                        if (copy->as.generic_constraint.argument_type == NULL) return NULL;
                        break;
                    }
                }
            }
            break;
        }
        case VC_AST_BLOCK_STATEMENT:
            if (!clone_node_list(context, tree, &source->as.block_statement.statements,
                    &copy->as.block_statement.statements, substitution)) return NULL;
            break;
        case VC_AST_EXPRESSION_STATEMENT:
            copy->as.expression_statement.expression = clone_node(context, tree, source->as.expression_statement.expression, substitution);
            break;
        case VC_AST_RETURN_STATEMENT:
            copy->as.return_statement.expression = clone_node(context, tree, source->as.return_statement.expression, substitution);
            copy->as.return_statement.is_ref = source->as.return_statement.is_ref;
            break;
        case VC_AST_THROW_STATEMENT:
            copy->as.throw_statement.expression = clone_node(context, tree, source->as.throw_statement.expression, substitution);
            break;
        case VC_AST_TRY_STATEMENT:
            copy->as.try_statement.try_block = clone_node(context, tree, source->as.try_statement.try_block, substitution);
            if (!clone_node_list(context, tree, &source->as.try_statement.catches,
                    &copy->as.try_statement.catches, substitution)) return NULL;
            copy->as.try_statement.finally_block = clone_node(context, tree,
                source->as.try_statement.finally_block, substitution);
            break;
        case VC_AST_CATCH_CLAUSE:
            copy->as.catch_clause.type = clone_type(context, tree, source->as.catch_clause.type, substitution);
            copy->as.catch_clause.name = source->as.catch_clause.name;
            copy->as.catch_clause.body = clone_node(context, tree, source->as.catch_clause.body, substitution);
            copy->as.catch_clause.catch_all = source->as.catch_clause.catch_all;
            break;
        case VC_AST_LOCAL_DECLARATION:
            copy->as.local_declaration.type = clone_type(context, tree, source->as.local_declaration.type, substitution);
            copy->as.local_declaration.name = source->as.local_declaration.name;
            copy->as.local_declaration.initializer = clone_node(context, tree, source->as.local_declaration.initializer, substitution);
            copy->as.local_declaration.is_var = source->as.local_declaration.is_var;
            copy->as.local_declaration.is_ref = source->as.local_declaration.is_ref;
            copy->as.local_declaration.ref_readonly = source->as.local_declaration.ref_readonly;
            copy->as.local_declaration.scoped = source->as.local_declaration.scoped;
            copy->as.local_declaration.is_using_resource = source->as.local_declaration.is_using_resource;
            copy->as.local_declaration.is_await_using_resource = source->as.local_declaration.is_await_using_resource;
            break;
        case VC_AST_IF_STATEMENT:
            copy->as.if_statement.condition = clone_node(context, tree, source->as.if_statement.condition, substitution);
            copy->as.if_statement.then_statement = clone_node(context, tree, source->as.if_statement.then_statement, substitution);
            copy->as.if_statement.else_statement = clone_node(context, tree, source->as.if_statement.else_statement, substitution);
            break;
        case VC_AST_WHILE_STATEMENT:
            copy->as.while_statement.condition = clone_node(context, tree, source->as.while_statement.condition, substitution);
            copy->as.while_statement.body = clone_node(context, tree, source->as.while_statement.body, substitution);
            break;
        case VC_AST_DO_WHILE_STATEMENT:
            copy->as.while_statement.condition = clone_node(context, tree, source->as.while_statement.condition, substitution);
            copy->as.while_statement.body = clone_node(context, tree, source->as.while_statement.body, substitution);
            break;
        case VC_AST_FOR_STATEMENT:
            copy->as.for_statement.initializer = clone_node(context, tree, source->as.for_statement.initializer, substitution);
            copy->as.for_statement.condition = clone_node(context, tree, source->as.for_statement.condition, substitution);
            copy->as.for_statement.increment = clone_node(context, tree, source->as.for_statement.increment, substitution);
            copy->as.for_statement.body = clone_node(context, tree, source->as.for_statement.body, substitution);
            break;
        case VC_AST_FOREACH_STATEMENT:
            copy->as.foreach_statement.type = clone_type(context, tree, source->as.foreach_statement.type, substitution);
            copy->as.foreach_statement.name = source->as.foreach_statement.name;
            copy->as.foreach_statement.is_var = source->as.foreach_statement.is_var;
            copy->as.foreach_statement.is_await = source->as.foreach_statement.is_await;
            copy->as.foreach_statement.is_ref = source->as.foreach_statement.is_ref;
            copy->as.foreach_statement.ref_readonly = source->as.foreach_statement.ref_readonly;
            copy->as.foreach_statement.collection = clone_node(context, tree, source->as.foreach_statement.collection, substitution);
            copy->as.foreach_statement.body = clone_node(context, tree, source->as.foreach_statement.body, substitution);
            copy->as.foreach_statement.move_next_await_protocol = clone_node(context, tree,
                source->as.foreach_statement.move_next_await_protocol, substitution);
            copy->as.foreach_statement.dispose_await_protocol = clone_node(context, tree,
                source->as.foreach_statement.dispose_await_protocol, substitution);
            break;
        case VC_AST_USING_STATEMENT:
            copy->as.using_statement.declaration = clone_node(context, tree,
                source->as.using_statement.declaration, substitution);
            copy->as.using_statement.expression = clone_node(context, tree,
                source->as.using_statement.expression, substitution);
            copy->as.using_statement.body = clone_node(context, tree,
                source->as.using_statement.body, substitution);
            copy->as.using_statement.dispose_await_protocol = clone_node(context, tree,
                source->as.using_statement.dispose_await_protocol, substitution);
            copy->as.using_statement.is_declaration = source->as.using_statement.is_declaration;
            copy->as.using_statement.is_await = source->as.using_statement.is_await;
            break;
        case VC_AST_LOCK_STATEMENT:
            copy->as.lock_statement.expression = clone_node(context, tree,
                source->as.lock_statement.expression, substitution);
            copy->as.lock_statement.body = clone_node(context, tree,
                source->as.lock_statement.body, substitution);
            break;
        case VC_AST_FIXED_STATEMENT:
            copy->as.fixed_statement.type = clone_type(context, tree,
                source->as.fixed_statement.type, substitution);
            copy->as.fixed_statement.name = source->as.fixed_statement.name;
            copy->as.fixed_statement.initializer = clone_node(context, tree,
                source->as.fixed_statement.initializer, substitution);
            copy->as.fixed_statement.body = clone_node(context, tree,
                source->as.fixed_statement.body, substitution);
            break;
        case VC_AST_SWITCH_STATEMENT:
            copy->as.switch_statement.expression = clone_node(context, tree,
                source->as.switch_statement.expression, substitution);
            if (!clone_node_list(context, tree, &source->as.switch_statement.sections,
                    &copy->as.switch_statement.sections, substitution)) return NULL;
            break;
        case VC_AST_SWITCH_SECTION:
            if (!clone_node_list(context, tree, &source->as.switch_section.labels,
                    &copy->as.switch_section.labels, substitution) ||
                !clone_node_list(context, tree, &source->as.switch_section.statements,
                    &copy->as.switch_section.statements, substitution)) return NULL;
            break;
        case VC_AST_SWITCH_LABEL:
            copy->as.switch_label.is_default = source->as.switch_label.is_default;
            copy->as.switch_label.is_pattern = source->as.switch_label.is_pattern;
            copy->as.switch_label.value = clone_node(context, tree,
                source->as.switch_label.value, substitution);
            copy->as.switch_label.pattern = clone_node(context, tree,
                source->as.switch_label.pattern, substitution);
            copy->as.switch_label.guard = clone_node(context, tree,
                source->as.switch_label.guard, substitution);
            break;
        case VC_AST_YIELD_RETURN_STATEMENT:
            copy->as.yield_statement.expression = clone_node(context, tree,
                source->as.yield_statement.expression, substitution);
            break;
        case VC_AST_YIELD_BREAK_STATEMENT:
        case VC_AST_BREAK_STATEMENT:
        case VC_AST_CONTINUE_STATEMENT:
            break;
        case VC_AST_TYPE_RECEIVER_EXPRESSION:
            break;
        case VC_AST_IDENTIFIER_EXPRESSION:
            copy->as.identifier_expression.name = source->as.identifier_expression.name;
            break;
        case VC_AST_LITERAL_EXPRESSION:
            copy->as.literal_expression = source->as.literal_expression;
            break;
        case VC_AST_MEMBER_ACCESS_EXPRESSION:
            copy->as.member_access_expression.target = clone_node(context, tree,
                source->as.member_access_expression.target, substitution);
            copy->as.member_access_expression.member = source->as.member_access_expression.member;
            copy->as.member_access_expression.constrained_static_parameter =
                source->as.member_access_expression.constrained_static_parameter;
            copy->as.member_access_expression.original_generic_name =
                source->as.member_access_expression.original_generic_name;
            if (!clone_type_list(context, tree, &source->as.member_access_expression.generic_arguments,
                    &copy->as.member_access_expression.generic_arguments, substitution) ||
                !clone_type_list(context, tree, &source->as.member_access_expression.specialized_generic_arguments,
                    &copy->as.member_access_expression.specialized_generic_arguments, substitution))
                return NULL;
            copy->as.member_access_expression.null_conditional = source->as.member_access_expression.null_conditional;
            copy->as.member_access_expression.null_conditional_direct = source->as.member_access_expression.null_conditional_direct;
            break;
        case VC_AST_CALL_EXPRESSION:
            copy->as.call_expression.callee = clone_node(context, tree, source->as.call_expression.callee, substitution);
            if (source->as.call_expression.callee != NULL &&
                source->as.call_expression.callee->kind == VC_AST_MEMBER_ACCESS_EXPRESSION &&
                source->as.call_expression.callee->as.member_access_expression.target != NULL &&
                source->as.call_expression.callee->as.member_access_expression.target->kind == VC_AST_IDENTIFIER_EXPRESSION)
            {
                const char *generic_target_name =
                    source->as.call_expression.callee->as.member_access_expression.target->as.identifier_expression.name;
                const VcAstTypeRef *replacement = lookup_identifier_substitution(context, substitution,
                    generic_target_name);
                if (replacement != NULL)
                {
                    VcAstNode *callee_copy = copy->as.call_expression.callee;
                    char type_name[256];
                    if (callee_copy == NULL ||
                        callee_copy->kind != VC_AST_MEMBER_ACCESS_EXPRESSION ||
                        callee_copy->as.member_access_expression.target == NULL ||
                        callee_copy->as.member_access_expression.target->kind != VC_AST_IDENTIFIER_EXPRESSION ||
                        !vc_monomorph_mangle_type_ref(replacement, type_name, sizeof(type_name)))
                        return NULL;
                    callee_copy->as.member_access_expression.target->as.identifier_expression.name =
                        vc_ast_copy_text(tree, type_name, strlen(type_name));
                    if (callee_copy->as.member_access_expression.target->as.identifier_expression.name == NULL)
                        return NULL;
                    callee_copy->as.member_access_expression.constrained_static_parameter =
                        vc_ast_copy_text(tree, generic_target_name, strlen(generic_target_name));
                    if (callee_copy->as.member_access_expression.constrained_static_parameter == NULL)
                        return NULL;
                }
                else if (substitution_has_parameter(substitution, generic_target_name))
                {
                    set_error(context,
                        "static member '%s.%s' requires a matching static interface method constraint",
                        generic_target_name,
                        source->as.call_expression.callee->as.member_access_expression.member);
                }
            }
            copy->as.call_expression.original_generic_name = source->as.call_expression.original_generic_name;
            copy->as.call_expression.had_explicit_function_pointer_type_argument =
                source->as.call_expression.had_explicit_function_pointer_type_argument;
            if (!clone_type_list(context, tree, &source->as.call_expression.generic_arguments,
                    &copy->as.call_expression.generic_arguments, substitution) ||
                !clone_type_list(context, tree, &source->as.call_expression.specialized_generic_arguments,
                    &copy->as.call_expression.specialized_generic_arguments, substitution) ||
                !clone_node_list(context, tree, &source->as.call_expression.arguments,
                    &copy->as.call_expression.arguments, substitution)) return NULL;
            break;
        case VC_AST_INDEX_EXPRESSION:
            copy->as.index_expression.target = clone_node(context, tree, source->as.index_expression.target, substitution);
            for (size_t i = 0; i < source->as.index_expression.indices.count; i++)
            {
                VcAstNode *index = clone_node(context, tree,
                    source->as.index_expression.indices.items[i], substitution);
                if (index == NULL || !vc_ast_node_list_push(tree,
                        &copy->as.index_expression.indices, index))
                    return NULL;
            }
            copy->as.index_expression.index = copy->as.index_expression.indices.count != 0
                ? copy->as.index_expression.indices.items[0]
                : clone_node(context, tree, source->as.index_expression.index, substitution);
            copy->as.index_expression.null_conditional = source->as.index_expression.null_conditional;
            copy->as.index_expression.null_conditional_direct = source->as.index_expression.null_conditional_direct;
            break;
        case VC_AST_NEW_EXPRESSION:
            copy->as.new_expression.type = clone_type(context, tree, source->as.new_expression.type, substitution);
            copy->as.new_expression.target_typed = source->as.new_expression.target_typed;
            copy->as.new_expression.is_array = source->as.new_expression.is_array;
            for (size_t i = 0; i < source->as.new_expression.array_lengths.count; i++)
            {
                VcAstNode *length = clone_node(context, tree,
                    source->as.new_expression.array_lengths.items[i], substitution);
                if (length == NULL || !vc_ast_node_list_push(tree,
                        &copy->as.new_expression.array_lengths, length))
                    return NULL;
            }
            copy->as.new_expression.array_length = copy->as.new_expression.array_lengths.count != 0
                ? copy->as.new_expression.array_lengths.items[0]
                : clone_node(context, tree, source->as.new_expression.array_length, substitution);
            if (!clone_node_list(context, tree, &source->as.new_expression.arguments,
                    &copy->as.new_expression.arguments, substitution) ||
                !clone_node_list(context, tree, &source->as.new_expression.initializers,
                    &copy->as.new_expression.initializers, substitution)) return NULL;
            break;
        case VC_AST_OBJECT_INITIALIZER_MEMBER:
            copy->as.object_initializer_member.member = source->as.object_initializer_member.member;
            copy->as.object_initializer_member.value = clone_node(context, tree,
                source->as.object_initializer_member.value, substitution);
            break;
        case VC_AST_COLLECTION_INITIALIZER_ELEMENT:
            if (!clone_node_list(context, tree,
                    &source->as.collection_initializer_element.arguments,
                    &copy->as.collection_initializer_element.arguments, substitution)) return NULL;
            break;
        case VC_AST_DEFAULT_EXPRESSION:
            copy->as.default_expression.type = clone_type(context, tree,
                source->as.default_expression.type, substitution);
            break;
        case VC_AST_TYPEOF_EXPRESSION:
            copy->as.typeof_expression.type = clone_type(context, tree,
                source->as.typeof_expression.type, substitution);
            break;
        case VC_AST_SIZEOF_EXPRESSION:
            copy->as.sizeof_expression.type = clone_type(context, tree, source->as.sizeof_expression.type, substitution);
            break;
        case VC_AST_STACKALLOC_EXPRESSION:
            copy->as.stackalloc_expression.type = clone_type(context, tree,
                source->as.stackalloc_expression.type, substitution);
            copy->as.stackalloc_expression.count = clone_node(context, tree,
                source->as.stackalloc_expression.count, substitution);
            break;
        case VC_AST_CAST_EXPRESSION:
            copy->as.cast_expression.type = clone_type(context, tree,
                source->as.cast_expression.type, substitution);
            copy->as.cast_expression.expression = clone_node(context, tree,
                source->as.cast_expression.expression, substitution);
            break;
        case VC_AST_TYPE_RELATION_EXPRESSION:
            copy->as.type_relation_expression.operator_kind = source->as.type_relation_expression.operator_kind;
            copy->as.type_relation_expression.expression = clone_node(context, tree,
                source->as.type_relation_expression.expression, substitution);
            copy->as.type_relation_expression.type = clone_type(context, tree,
                source->as.type_relation_expression.type, substitution);
            copy->as.type_relation_expression.pattern_name =
                source->as.type_relation_expression.pattern_name;
            copy->as.type_relation_expression.pattern_constant = clone_node(context, tree,
                source->as.type_relation_expression.pattern_constant, substitution);
            copy->as.type_relation_expression.pattern_operator_kind =
                source->as.type_relation_expression.pattern_operator_kind;
            copy->as.type_relation_expression.pattern_logical_kind =
                source->as.type_relation_expression.pattern_logical_kind;
            copy->as.type_relation_expression.pattern_property =
                source->as.type_relation_expression.pattern_property;
            copy->as.type_relation_expression.pattern_left = clone_node(context, tree,
                source->as.type_relation_expression.pattern_left, substitution);
            copy->as.type_relation_expression.pattern_right = clone_node(context, tree,
                source->as.type_relation_expression.pattern_right, substitution);
            if (!clone_node_list(context, tree, &source->as.type_relation_expression.pattern_properties,
                    &copy->as.type_relation_expression.pattern_properties, substitution))
                return NULL;
            break;
        case VC_AST_PROPERTY_PATTERN_MEMBER:
            copy->as.property_pattern_member.name = source->as.property_pattern_member.name;
            copy->as.property_pattern_member.pattern = clone_node(context, tree,
                source->as.property_pattern_member.pattern, substitution);
            break;
        case VC_AST_UNARY_EXPRESSION:
            copy->as.unary_expression.operator_kind = source->as.unary_expression.operator_kind;
            copy->as.unary_expression.operand = clone_node(context, tree, source->as.unary_expression.operand, substitution);
            copy->as.unary_expression.inline_out_type = clone_type(context, tree,
                source->as.unary_expression.inline_out_type, substitution);
            copy->as.unary_expression.inline_out_inference_span =
                source->as.unary_expression.inline_out_inference_span;
            copy->as.unary_expression.inline_out_inferred =
                source->as.unary_expression.inline_out_inferred;
            copy->as.unary_expression.inline_out_discard =
                source->as.unary_expression.inline_out_discard;
            copy->as.unary_expression.constrained_static_parameter =
                source->as.unary_expression.constrained_static_parameter != NULL
                    ? source->as.unary_expression.constrained_static_parameter
                    : source_identifier_generic_parameter(context, substitution, source->as.unary_expression.operand);
            copy->as.unary_expression.postfix = source->as.unary_expression.postfix;
            break;
        case VC_AST_AWAIT_EXPRESSION:
            copy->as.await_expression.operand = clone_node(context, tree,
                source->as.await_expression.operand, substitution);
            copy->as.await_expression.awaiter_inference_call = clone_node(context, tree,
                source->as.await_expression.awaiter_inference_call, substitution);
            break;
        case VC_AST_RANGE_EXPRESSION:
            copy->as.range_expression.start = clone_node(context, tree,
                source->as.range_expression.start, substitution);
            copy->as.range_expression.end = clone_node(context, tree,
                source->as.range_expression.end, substitution);
            break;
        case VC_AST_BINARY_EXPRESSION:
            copy->as.binary_expression.operator_kind = source->as.binary_expression.operator_kind;
            copy->as.binary_expression.left = clone_node(context, tree, source->as.binary_expression.left, substitution);
            copy->as.binary_expression.right = clone_node(context, tree, source->as.binary_expression.right, substitution);
            copy->as.binary_expression.left_constrained_static_parameter =
                source->as.binary_expression.left_constrained_static_parameter != NULL
                    ? source->as.binary_expression.left_constrained_static_parameter
                    : source_identifier_generic_parameter(context, substitution, source->as.binary_expression.left);
            copy->as.binary_expression.right_constrained_static_parameter =
                source->as.binary_expression.right_constrained_static_parameter != NULL
                    ? source->as.binary_expression.right_constrained_static_parameter
                    : source_identifier_generic_parameter(context, substitution, source->as.binary_expression.right);
            break;
        case VC_AST_CONDITIONAL_EXPRESSION:
            copy->as.conditional_expression.condition = clone_node(context, tree,
                source->as.conditional_expression.condition, substitution);
            copy->as.conditional_expression.when_true = clone_node(context, tree,
                source->as.conditional_expression.when_true, substitution);
            copy->as.conditional_expression.when_false = clone_node(context, tree,
                source->as.conditional_expression.when_false, substitution);
            break;
        case VC_AST_SWITCH_EXPRESSION:
            copy->as.switch_expression.expression = clone_node(context, tree,
                source->as.switch_expression.expression, substitution);
            if (!clone_node_list(context, tree, &source->as.switch_expression.arms,
                    &copy->as.switch_expression.arms, substitution))
                return NULL;
            break;
        case VC_AST_SWITCH_EXPRESSION_ARM:
            copy->as.switch_expression_arm.is_discard = source->as.switch_expression_arm.is_discard;
            copy->as.switch_expression_arm.pattern = clone_node(context, tree,
                source->as.switch_expression_arm.pattern, substitution);
            copy->as.switch_expression_arm.guard = clone_node(context, tree,
                source->as.switch_expression_arm.guard, substitution);
            copy->as.switch_expression_arm.result = clone_node(context, tree,
                source->as.switch_expression_arm.result, substitution);
            break;
        case VC_AST_ASSIGNMENT_EXPRESSION:
            copy->as.assignment_expression.operator_kind = source->as.assignment_expression.operator_kind;
            copy->as.assignment_expression.is_ref = source->as.assignment_expression.is_ref;
            copy->as.assignment_expression.left = clone_node(context, tree, source->as.assignment_expression.left, substitution);
            copy->as.assignment_expression.right = clone_node(context, tree, source->as.assignment_expression.right, substitution);
            copy->as.assignment_expression.left_constrained_static_parameter =
                source->as.assignment_expression.left_constrained_static_parameter != NULL
                    ? source->as.assignment_expression.left_constrained_static_parameter
                    : source_identifier_generic_parameter(context, substitution, source->as.assignment_expression.left);
            copy->as.assignment_expression.right_constrained_static_parameter =
                source->as.assignment_expression.right_constrained_static_parameter != NULL
                    ? source->as.assignment_expression.right_constrained_static_parameter
                    : source_identifier_generic_parameter(context, substitution, source->as.assignment_expression.right);
            break;
        case VC_AST_LAMBDA_EXPRESSION:
            copy->as.lambda_expression.parameters = source->as.lambda_expression.parameters;
            copy->as.lambda_expression.body = clone_node(context, tree, source->as.lambda_expression.body, substitution);
            copy->as.lambda_expression.expression_body = source->as.lambda_expression.expression_body;
            copy->as.lambda_expression.is_async = source->as.lambda_expression.is_async;
            copy->as.lambda_expression.lowered_async_forwarder =
                source->as.lambda_expression.lowered_async_forwarder;
            break;
        case VC_AST_COMPILER_CLOSURE_FRAME_EXPRESSION:
            if (!clone_node_list(context, tree,
                    &source->as.compiler_closure_frame_expression.captures,
                    &copy->as.compiler_closure_frame_expression.captures, substitution))
                return NULL;
            break;
        case VC_AST_COMPILER_CAPTURE_EXPRESSION:
            copy->as.compiler_capture_expression.target = clone_node(context, tree,
                source->as.compiler_capture_expression.target, substitution);
            copy->as.compiler_capture_expression.source_expression =
                source->as.compiler_capture_expression.source_expression;
            copy->as.compiler_capture_expression.source_lambda_node =
                source->as.compiler_capture_expression.source_lambda_node;
            copy->as.compiler_capture_expression.declaration_node =
                source->as.compiler_capture_expression.declaration_node;
            copy->as.compiler_capture_expression.owner_method_node =
                source->as.compiler_capture_expression.owner_method_node;
            copy->as.compiler_capture_expression.name =
                source->as.compiler_capture_expression.name;
            copy->as.compiler_capture_expression.type = clone_type(context, tree,
                source->as.compiler_capture_expression.type, substitution);
            copy->as.compiler_capture_expression.captures_this =
                source->as.compiler_capture_expression.captures_this;
            break;
        case VC_AST_PARENTHESIZED_EXPRESSION:
            copy->as.parenthesized_expression.expression = clone_node(context, tree, source->as.parenthesized_expression.expression, substitution);
            break;
    }
    return copy;
}

static VcGenericTemplate *find_type_template(
    VcMonomorphContext *context,
    const char *namespace_name,
    const VcAstTypeRef *type)
{
    VcGenericTemplate *fallback = NULL;
    bool ambiguous = false;
    if (type->is_global_qualified)
        namespace_name = NULL;
    const char *simple_name = strrchr(type->name, '.');
    const size_t qualifier_length = simple_name != NULL ? (size_t)(simple_name - type->name) : 0;
    simple_name = simple_name != NULL ? simple_name + 1 : type->name;
    for (size_t i = 0; i < context->template_count; i++)
    {
        VcGenericTemplate *candidate = &context->templates[i];
        if (strcmp(candidate->node->as.type_declaration.name, simple_name) != 0 ||
            candidate->node->as.type_declaration.generic_parameters.count != type->generic_arguments.count)
            continue;
        if (qualifier_length != 0)
        {
            if (candidate->namespace_name != NULL && strlen(candidate->namespace_name) == qualifier_length &&
                strncmp(candidate->namespace_name, type->name, qualifier_length) == 0) return candidate;
            continue;
        }
        if (same_namespace(candidate->namespace_name, namespace_name))
            return candidate;
        if (fallback != NULL)
            ambiguous = true;
        else
            fallback = candidate;
    }
    return ambiguous ? NULL : fallback;
}

static bool find_declared_type_identity_in_list(
    const VcAstNodeList *declarations,
    const char *namespace_name,
    const char *target_namespace,
    const char *name,
    size_t generic_count,
    const char **resolved_namespace,
    const char **resolved_name)
{
    if (declarations == NULL || name == NULL)
        return false;
    for (size_t i = 0; i < declarations->count; i++)
    {
        const VcAstNode *node = declarations->items[i];
        if (node == NULL)
            continue;
        if (node->kind == VC_AST_NAMESPACE_DECLARATION)
        {
            if (find_declared_type_identity_in_list(
                    &node->as.namespace_declaration.declarations,
                    node->as.namespace_declaration.name,
                    target_namespace, name, generic_count,
                    resolved_namespace, resolved_name))
                return true;
            continue;
        }
        if (node->kind != VC_AST_TYPE_DECLARATION ||
            node->as.type_declaration.original_generic_name != NULL ||
            strcmp(node->as.type_declaration.name, name) != 0 ||
            node->as.type_declaration.generic_parameters.count != generic_count ||
            !same_namespace(namespace_name, target_namespace))
            continue;
        *resolved_namespace = namespace_name;
        *resolved_name = node->as.type_declaration.name;
        return true;
    }
    return false;
}

static bool find_declared_type_identity(
    const VcMonomorphContext *context,
    const char *namespace_name,
    const VcAstTypeRef *type,
    const char **resolved_namespace,
    const char **resolved_name)
{
    if (context == NULL || type == NULL || type->name == NULL)
        return false;

    const char *last_dot = strrchr(type->name, '.');
    char qualifier[512];
    const char *target_namespace = type->is_global_qualified ? NULL : namespace_name;
    const char *simple_name = type->name;
    const bool qualified = last_dot != NULL;
    if (qualified)
    {
        const size_t length = (size_t)(last_dot - type->name);
        if (length == 0 || length >= sizeof(qualifier) || last_dot[1] == '\0')
            return false;
        memcpy(qualifier, type->name, length);
        qualifier[length] = '\0';
        target_namespace = qualifier;
        simple_name = last_dot + 1;
    }

    /* Match the semantic resolver's namespace rule: an exact current/qualified
       namespace wins; otherwise an unqualified name may bind only when there is
       exactly one declaration with that name/arity across namespaces. */
    {
        for (size_t t = 0; t < context->tree_count; t++)
        {
            const VcAstTree *tree = context->trees[t];
            if (tree == NULL || tree->root == NULL)
                continue;
            if (find_declared_type_identity_in_list(
                    &tree->root->as.compilation_unit.declarations, NULL,
                    target_namespace, simple_name, type->generic_arguments.count,
                    resolved_namespace, resolved_name))
                return true;
        }
        if (qualified)
            return false;
    }

    const char *match_namespace = NULL;
    const char *match_name = NULL;
    bool found = false;
    for (size_t t = 0; t < context->tree_count; t++)
    {
        const VcAstTree *tree = context->trees[t];
        if (tree == NULL || tree->root == NULL)
            continue;
        const VcAstNodeList *top = &tree->root->as.compilation_unit.declarations;
        for (size_t i = 0; i < top->count; i++)
        {
            const VcAstNode *node = top->items[i];
            if (node == NULL)
                continue;
            if (node->kind == VC_AST_TYPE_DECLARATION)
            {
                if (node->as.type_declaration.original_generic_name == NULL &&
                    strcmp(node->as.type_declaration.name, simple_name) == 0 &&
                    node->as.type_declaration.generic_parameters.count == type->generic_arguments.count)
                {
                    if (found)
                        return false;
                    match_namespace = NULL;
                    match_name = node->as.type_declaration.name;
                    found = true;
                }
                continue;
            }
            if (node->kind != VC_AST_NAMESPACE_DECLARATION)
                continue;
            const VcAstNodeList *decls = &node->as.namespace_declaration.declarations;
            for (size_t j = 0; j < decls->count; j++)
            {
                const VcAstNode *nested = decls->items[j];
                if (nested == NULL || nested->kind != VC_AST_TYPE_DECLARATION ||
                    nested->as.type_declaration.original_generic_name != NULL ||
                    strcmp(nested->as.type_declaration.name, simple_name) != 0 ||
                    nested->as.type_declaration.generic_parameters.count != type->generic_arguments.count)
                    continue;
                if (found)
                    return false;
                match_namespace = node->as.namespace_declaration.name;
                match_name = nested->as.type_declaration.name;
                found = true;
            }
        }
    }
    if (!found)
        return false;
    *resolved_namespace = match_namespace;
    *resolved_name = match_name;
    return true;
}

/* Arguments move from the use site into a template's declaration scope.
   Preserve their resolved names before substitution, including nested arguments. */
static bool canonicalize_argument(
    VcMonomorphContext *context, VcAstTree *tree,
    VcAstTypeRef *type, const char *namespace_name)
{
    const char *resolved_namespace = NULL;
    const char *resolved_name = NULL;
    if (find_declared_type_identity(context, namespace_name, type,
            &resolved_namespace, &resolved_name))
    {
        type->is_global_qualified = resolved_namespace == NULL;
        if (resolved_namespace != NULL)
        {
            char name[768];
            const int written = snprintf(name, sizeof(name), "%s.%s",
                resolved_namespace, resolved_name);
            if (written < 0 || (size_t)written >= sizeof(name))
                return false;
            type->name = vc_ast_copy_text(tree, name, (size_t)written);
            if (type->name == NULL)
                return false;
        }
    }
    for (size_t i = 0; i < type->generic_arguments.count; i++)
        if (!canonicalize_argument(context, tree,
                type->generic_arguments.items[i], namespace_name))
            return false;
    return true;
}

static bool clone_canonical_arguments(
    VcMonomorphContext *context, VcAstTree *tree,
    const VcAstTypeList *source, VcAstTypeList *target,
    const char *namespace_name)
{
    if (!clone_type_list(context, tree, source, target, NULL))
        return false;
    for (size_t i = 0; i < target->count; i++)
        if (!canonicalize_argument(context, tree, target->items[i], namespace_name))
        {
            set_error(context, "could not preserve resolved generic argument identity");
            return false;
        }
    return true;
}

static bool mangle_canonical_type_ref(
    const VcMonomorphContext *context,
    const VcAstTypeRef *type,
    const char *namespace_name,
    bool canonicalize_base,
    char *output,
    size_t output_size)
{
    if (type == NULL || type->name == NULL || output == NULL || output_size == 0)
        return false;

    char canonical_name[768];
    const char *resolved_namespace = NULL;
    const char *resolved_name = NULL;
    const char *base_name = type->name;
    if (canonicalize_base && find_declared_type_identity(context, namespace_name, type,
            &resolved_namespace, &resolved_name))
    {
        if (resolved_namespace != NULL)
        {
            const int written = snprintf(canonical_name, sizeof(canonical_name),
                "%s.%s", resolved_namespace, resolved_name);
            if (written < 0 || (size_t)written >= sizeof(canonical_name))
                return false;
            base_name = canonical_name;
        }
        else
            base_name = resolved_name;
    }

    output[0] = '\0';
    size_t written = 0;
    if (!append_sanitized(output, output_size, &written, base_name))
        return false;
    if (type->generic_arguments.count != 0)
    {
        char count[32];
        snprintf(count, sizeof(count), "__g%zu", type->generic_arguments.count);
        if (!append_text(output, output_size, &written, count))
            return false;
        for (size_t i = 0; i < type->generic_arguments.count; i++)
        {
            char nested[512];
            if (!mangle_canonical_type_ref(context, type->generic_arguments.items[i],
                    namespace_name, true, nested, sizeof(nested)) ||
                !append_text(output, output_size, &written, "_") ||
                !append_text(output, output_size, &written, nested))
                return false;
        }
    }
    if (type->nullable && !append_text(output, output_size, &written, "__nullable"))
        return false;
    for (size_t i = 0; i < type->pointer_depth; i++)
        if (!append_text(output, output_size, &written, "__ptr"))
            return false;
    if (type->rectangular_rank > 1)
    {
        char rank[32];
        snprintf(rank, sizeof(rank), "__md%zu", type->rectangular_rank);
        if (!append_text(output, output_size, &written, rank))
            return false;
    }
    for (size_t i = 0; i < type->array_rank; i++)
        if (!append_text(output, output_size, &written, "__arr"))
            return false;
    return true;
}

static bool list_has_named_type(const VcAstNodeList *list, const char *name)
{
    for (size_t i = 0; i < list->count; i++)
    {
        const VcAstNode *node = list->items[i];
        if (node->kind == VC_AST_TYPE_DECLARATION && strcmp(node->as.type_declaration.name, name) == 0 &&
            node->as.type_declaration.generic_parameters.count == 0)
            return true;
    }
    return false;
}

static bool ensure_type_specialization(
    VcMonomorphContext *context,
    VcAstTree *use_tree,
    VcAstTypeRef *type,
    const char *namespace_name);

static bool process_type_ref(
    VcMonomorphContext *context,
    VcAstTree *tree,
    VcAstTypeRef *type,
    const char *namespace_name)
{
    if (type == NULL)
        return true;
    for (size_t i = 0; i < type->generic_arguments.count; i++)
    {
        if (!process_type_ref(context, tree, type->generic_arguments.items[i], namespace_name))
            return false;
    }
    if (type->is_function_pointer)
        return true;
    if (type->generic_arguments.count != 0)
    {
        VcAstTypeRef specialization = *type;
        specialization.pointer_depth = 0;
        specialization.array_rank = 0;
        specialization.rectangular_rank = 0;
        specialization.nullable = false;
        return ensure_type_specialization(context, tree, &specialization, namespace_name);
    }
    return true;
}

static bool ensure_type_specialization(
    VcMonomorphContext *context,
    VcAstTree *use_tree,
    VcAstTypeRef *type,
    const char *namespace_name)
{
    VcGenericTemplate *template = find_type_template(context, namespace_name, type);
    if (template == NULL)
    {
        set_error(context, "generic type '%s' with %zu argument(s) was not found",
            type->name, type->generic_arguments.count);
        generic_diagnostic(context, use_tree, type->span);
        if (context->diagnostic != NULL)
        {
            for (size_t i = 0; i < context->template_count; i++)
            {
                const VcGenericTemplate *candidate = &context->templates[i];
                if (strcmp(candidate->node->as.type_declaration.name, type->name) != 0 ||
                    !same_namespace(candidate->namespace_name, namespace_name)) continue;
                const VcSource *candidate_source = source_for_tree(context, candidate->tree);
                if (candidate_source == NULL) continue;
                char note[160];
                snprintf(note, sizeof(note), "Generic type declaration requires %zu type argument(s) here.",
                    candidate->node->as.type_declaration.generic_parameters.count);
                vc_diagnostic_add_source_context(context->diagnostic, VC_DIAGNOSTIC_NOTE,
                    candidate_source, vc_source_span_at_location(candidate_source, candidate->node->location), note);
            }
        }
        return false;
    }

    char specialized_name[256];
    VcAstTypeRef canonical = *type;
    canonical.name = template->node->as.type_declaration.name;
    if (!mangle_canonical_type_ref(context, &canonical, namespace_name, false,
            specialized_name, sizeof(specialized_name)))
    {
        set_error(context, "generic specialization name for '%s' is too long", type->name);
        return false;
    }
    if (list_has_named_type(template->parent, specialized_name))
        return true;

    VcAstTypeList arguments = {0};
    if (!clone_canonical_arguments(context, template->tree,
            &type->generic_arguments, &arguments, namespace_name))
        return false;
    const VcAstNode *source = template->node;
    VcGenericSubstitution substitution = {
        &source->as.type_declaration.generic_parameters,
        &arguments,
        &source->as.type_declaration.generic_constraints,
        NULL
    };
    VcAstNode *copy = clone_node(context, template->tree, source, &substitution);
    if (copy == NULL)
        return false;
    copy->as.type_declaration.original_generic_name = source->as.type_declaration.name;
    if (!clone_type_list(context, template->tree, &arguments,
            &copy->as.type_declaration.generic_arguments, NULL))
        return false;
    copy->as.type_declaration.name = vc_ast_copy_text(template->tree,
        specialized_name, strlen(specialized_name));
    if (copy->as.type_declaration.name == NULL)
    {
        set_error(context, "out of memory while naming generic specialization");
        return false;
    }
    copy->as.type_declaration.generic_parameters.count = 0;
    copy->as.type_declaration.generic_parameters.capacity = 0;
    copy->as.type_declaration.generic_parameters.items = NULL;

    if (!vc_ast_node_list_push(template->tree, template->parent, copy))
    {
        set_error(context, "out of memory while registering generic specialization");
        return false;
    }
    context->changes++;
    return true;
}

static bool method_has_name(const VcAstNode *type_node, const char *name)
{
    for (size_t i = 0; i < type_node->as.type_declaration.members.count; i++)
    {
        const VcAstNode *member = type_node->as.type_declaration.members.items[i];
        if (member->kind == VC_AST_METHOD_DECLARATION &&
            strcmp(member->as.method_declaration.name, name) == 0 &&
            member->as.method_declaration.generic_parameters.count == 0)
            return true;
    }
    return false;
}

static bool mangle_method_name(
    const VcMonomorphContext *context,
    const char *name,
    const VcAstTypeList *arguments,
    const char *namespace_name,
    char *output,
    size_t output_size)
{
    size_t written = 0;
    output[0] = '\0';
    if (!append_sanitized(output, output_size, &written, name))
        return false;
    char count[32];
    snprintf(count, sizeof(count), "__gm%zu", arguments->count);
    if (!append_text(output, output_size, &written, count))
        return false;
    for (size_t i = 0; i < arguments->count; i++)
    {
        char type_name[256];
        if (!mangle_canonical_type_ref(context, arguments->items[i], namespace_name, true,
                type_name, sizeof(type_name)) ||
            !append_text(output, output_size, &written, "_") ||
            !append_text(output, output_size, &written, type_name))
            return false;
    }
    return true;
}

static VcGenericMethodRequest *find_method_request(
    VcMonomorphContext *context,
    const char *specialized_name)
{
    for (size_t i = 0; i < context->method_request_count; i++)
    {
        if (strcmp(context->method_requests[i].specialized_name, specialized_name) == 0)
            return &context->method_requests[i];
    }
    return NULL;
}

static VcGenericMethodRequest *register_method_request(
    VcMonomorphContext *context,
    const char *name,
    const VcAstTypeList *arguments,
    const char *specialized_name)
{
    VcGenericMethodRequest *existing = find_method_request(context, specialized_name);
    if (existing != NULL)
        return existing;

    if (context->method_request_count == context->method_request_capacity)
    {
        const size_t capacity = context->method_request_capacity == 0
            ? 8 : context->method_request_capacity * 2;
        VcGenericMethodRequest *items = realloc(context->method_requests,
            capacity * sizeof(*items));
        if (items == NULL)
            return NULL;
        context->method_requests = items;
        context->method_request_capacity = capacity;
    }

    VcGenericMethodRequest *request =
        &context->method_requests[context->method_request_count++];
    memset(request, 0, sizeof(*request));
    request->name = name;
    request->arguments = *arguments;
    snprintf(request->specialized_name, sizeof(request->specialized_name),
        "%s", specialized_name);
    return request;
}

static bool apply_method_request_to_type(
    VcMonomorphContext *context,
    VcAstTree *tree,
    VcAstNode *type_node,
    VcGenericMethodRequest *request)
{
    bool has_template = false;
    const size_t original_count = type_node->as.type_declaration.members.count;
    for (size_t i = 0; i < original_count; i++)
    {
        const VcAstNode *member = type_node->as.type_declaration.members.items[i];
        if (member->kind == VC_AST_METHOD_DECLARATION &&
            !member->as.method_declaration.is_constructor &&
            strcmp(member->as.method_declaration.name, request->name) == 0 &&
            member->as.method_declaration.generic_parameters.count == request->arguments.count)
        {
            has_template = true;
            break;
        }
    }
    if (!has_template)
        return true;

    request->matched = true;
    if (method_has_name(type_node, request->specialized_name))
        return true;

    for (size_t i = 0; i < original_count; i++)
    {
        VcAstNode *template = type_node->as.type_declaration.members.items[i];
        if (template->kind != VC_AST_METHOD_DECLARATION ||
            template->as.method_declaration.is_constructor ||
            strcmp(template->as.method_declaration.name, request->name) != 0 ||
            template->as.method_declaration.generic_parameters.count != request->arguments.count)
            continue;

        VcGenericSubstitution substitution = {
            &template->as.method_declaration.generic_parameters,
            &request->arguments,
            &template->as.method_declaration.generic_constraints,
            template
        };
        VcAstNode *copy = clone_node(context, tree, template, &substitution);
        if (copy == NULL)
            return false;
        copy->as.method_declaration.name = vc_ast_copy_text(tree,
            request->specialized_name, strlen(request->specialized_name));
        if (copy->as.method_declaration.name == NULL)
        {
            set_error(context, "out of memory while naming generic method specialization");
            return false;
        }
        copy->as.method_declaration.original_generic_name = template->as.method_declaration.name;
        copy->as.method_declaration.generic_parameters.count = 0;
        copy->as.method_declaration.generic_parameters.capacity = 0;
        copy->as.method_declaration.generic_parameters.items = NULL;
        if (!vc_ast_node_list_push(tree, &type_node->as.type_declaration.members, copy))
        {
            set_error(context, "out of memory while registering generic method specialization");
            return false;
        }
        context->changes++;
    }
    return true;
}

static bool apply_method_requests_to_type(
    VcMonomorphContext *context,
    VcAstTree *tree,
    VcAstNode *type_node)
{
    for (size_t i = 0; i < context->method_request_count; i++)
    {
        if (!apply_method_request_to_type(context, tree, type_node,
                &context->method_requests[i]))
            return false;
    }
    return true;
}

static bool process_node(
    VcMonomorphContext *context,
    VcAstTree *tree,
    VcAstNode *node,
    const char *namespace_name,
    VcAstNode *current_type);

static bool process_node_list(
    VcMonomorphContext *context,
    VcAstTree *tree,
    VcAstNodeList *list,
    const char *namespace_name,
    VcAstNode *current_type)
{
    for (size_t i = 0; i < list->count; i++)
    {
        if (!process_node(context, tree, list->items[i], namespace_name, current_type))
            return false;
    }
    return true;
}

static bool register_generic_method_specialization(
    VcMonomorphContext *context,
    VcAstTree *tree,
    const char *name,
    VcAstTypeList *arguments,
    VcSourceSpan span,
    const char *namespace_name,
    char *specialized_name,
    size_t specialized_name_size)
{
    if (!mangle_method_name(context, name, arguments, namespace_name,
            specialized_name, specialized_name_size))
    {
        set_error(context, "generic method specialization name for '%s' is too long", name);
        return false;
    }

    VcAstTypeList canonical_arguments = {0};
    if (!clone_canonical_arguments(context, tree, arguments,
            &canonical_arguments, namespace_name))
        return false;
    VcGenericMethodRequest *request = register_method_request(context, name,
        &canonical_arguments, specialized_name);
    if (request == NULL)
    {
        set_error(context, "out of memory while registering generic method request");
        return false;
    }
    if (request->use_tree == NULL)
    {
        request->use_tree = tree;
        request->span = span;
    }
    return true;
}

static bool specialize_generic_call(
    VcMonomorphContext *context,
    VcAstTree *tree,
    VcAstNode *call,
    const char *namespace_name,
    VcAstNode *current_type)
{
    if (call->as.call_expression.generic_arguments.count == 0)
        return true;
    if (current_type == NULL)
    {
        set_error(context, "generic method call is not inside a type declaration");
        return false;
    }

    VcAstNode *callee = call->as.call_expression.callee;
    char *name = NULL;
    if (callee->kind == VC_AST_IDENTIFIER_EXPRESSION)
        name = callee->as.identifier_expression.name;
    else if (callee->kind == VC_AST_MEMBER_ACCESS_EXPRESSION)
        name = callee->as.member_access_expression.member;
    else
    {
        set_error(context, "explicit generic arguments require a method call target");
        return false;
    }

    char specialized_name[256];
    if (!register_generic_method_specialization(context, tree, name,
            &call->as.call_expression.generic_arguments, call->span, namespace_name,
            specialized_name, sizeof(specialized_name)))
        return false;

    call->as.call_expression.original_generic_name = name;
    for (size_t i = 0; i < call->as.call_expression.generic_arguments.count; i++)
    {
        if (monomorph_type_ref_has_explicit_function_pointer(
                call->as.call_expression.generic_arguments.items[i]))
        {
            call->as.call_expression.had_explicit_function_pointer_type_argument = true;
            break;
        }
    }

    char *rewritten = vc_ast_copy_text(tree, specialized_name, strlen(specialized_name));
    if (rewritten == NULL)
    {
        set_error(context, "out of memory while rewriting generic method call");
        return false;
    }
    if (callee->kind == VC_AST_IDENTIFIER_EXPRESSION)
        callee->as.identifier_expression.name = rewritten;
    else
        callee->as.member_access_expression.member = rewritten;
    call->as.call_expression.specialized_generic_arguments =
        call->as.call_expression.generic_arguments;
    call->as.call_expression.generic_arguments.count = 0;
    call->as.call_expression.generic_arguments.capacity = 0;
    call->as.call_expression.generic_arguments.items = NULL;
    context->changes++;
    return true;
}

static bool specialize_generic_method_group(
    VcMonomorphContext *context,
    VcAstTree *tree,
    VcAstNode *member,
    const char *namespace_name,
    VcAstNode *current_type)
{
    if (member->as.member_access_expression.generic_arguments.count == 0)
        return true;
    if (current_type == NULL)
    {
        set_error(context, "generic method group is not inside a type declaration");
        return false;
    }

    char *name = member->as.member_access_expression.member;
    char specialized_name[256];
    if (!register_generic_method_specialization(context, tree, name,
            &member->as.member_access_expression.generic_arguments, member->span, namespace_name,
            specialized_name, sizeof(specialized_name)))
        return false;

    char *rewritten = vc_ast_copy_text(tree, specialized_name, strlen(specialized_name));
    if (rewritten == NULL)
    {
        set_error(context, "out of memory while rewriting generic method group");
        return false;
    }
    member->as.member_access_expression.original_generic_name = name;
    member->as.member_access_expression.member = rewritten;
    member->as.member_access_expression.specialized_generic_arguments =
        member->as.member_access_expression.generic_arguments;
    member->as.member_access_expression.generic_arguments.count = 0;
    member->as.member_access_expression.generic_arguments.capacity = 0;
    member->as.member_access_expression.generic_arguments.items = NULL;
    context->changes++;
    return true;
}

static bool process_node(
    VcMonomorphContext *context,
    VcAstTree *tree,
    VcAstNode *node,
    const char *namespace_name,
    VcAstNode *current_type)
{
    if (node == NULL)
        return true;
    switch (node->kind)
    {
        case VC_AST_COMPILATION_UNIT:
            return process_node_list(context, tree, &node->as.compilation_unit.declarations, namespace_name, NULL);
        case VC_AST_NAMESPACE_DECLARATION:
            return process_node_list(context, tree, &node->as.namespace_declaration.declarations,
                node->as.namespace_declaration.name, NULL);
        case VC_AST_TYPE_DECLARATION:
            if (node->as.type_declaration.generic_parameters.count != 0)
                return true;
            if (!apply_method_requests_to_type(context, tree, node))
                return false;
            if (!process_node_list(context, tree, &node->as.type_declaration.generic_constraints, namespace_name, node))
                return false;
            for (size_t i = 0; i < node->as.type_declaration.base_types.count; i++)
                if (!process_type_ref(context, tree, node->as.type_declaration.base_types.items[i], namespace_name)) return false;
            return process_node_list(context, tree, &node->as.type_declaration.members, namespace_name, node);
        case VC_AST_METHOD_DECLARATION:
            if (node->as.method_declaration.generic_parameters.count != 0)
                return true;
            if (!process_node_list(context, tree, &node->as.method_declaration.generic_constraints, namespace_name, current_type))
                return false;
            if (!process_type_ref(context, tree, node->as.method_declaration.return_type, namespace_name)) return false;
            if (!process_node_list(context, tree, &node->as.method_declaration.parameters, namespace_name, current_type)) return false;
            return process_node(context, tree, node->as.method_declaration.body, namespace_name, current_type);
        case VC_AST_GENERIC_CONSTRAINT:
            if (!process_type_ref(context, tree, node->as.generic_constraint.argument_type, namespace_name))
                return false;
            for (size_t i = 0; i < node->as.generic_constraint.type_constraints.count; i++)
                if (!process_type_ref(context, tree, node->as.generic_constraint.type_constraints.items[i], namespace_name)) return false;
            return true;
        case VC_AST_FIELD_DECLARATION:
            return process_type_ref(context, tree, node->as.field_declaration.type, namespace_name) &&
                process_node(context, tree, node->as.field_declaration.initializer, namespace_name, current_type) &&
                process_node(context, tree, node->as.field_declaration.event_add_body, namespace_name, current_type) &&
                process_node(context, tree, node->as.field_declaration.event_remove_body, namespace_name, current_type);
        case VC_AST_PROPERTY_DECLARATION:
            return process_type_ref(context, tree, node->as.property_declaration.type, namespace_name) &&
                process_node(context, tree, node->as.property_declaration.getter_body, namespace_name, current_type) &&
                process_node(context, tree, node->as.property_declaration.setter_body, namespace_name, current_type) &&
                process_node(context, tree, node->as.property_declaration.initializer, namespace_name, current_type);
        case VC_AST_PARAMETER:
            return process_type_ref(context, tree, node->as.parameter.type, namespace_name) &&
                process_node(context, tree, node->as.parameter.default_value, namespace_name, current_type);
        case VC_AST_ATTRIBUTE:
            return process_node_list(context, tree, &node->as.attribute.arguments, namespace_name, current_type);
        case VC_AST_BLOCK_STATEMENT:
            return process_node_list(context, tree, &node->as.block_statement.statements, namespace_name, current_type);
        case VC_AST_EXPRESSION_STATEMENT:
            return process_node(context, tree, node->as.expression_statement.expression, namespace_name, current_type);
        case VC_AST_RETURN_STATEMENT:
            return process_node(context, tree, node->as.return_statement.expression, namespace_name, current_type);
        case VC_AST_THROW_STATEMENT:
            return process_node(context, tree, node->as.throw_statement.expression, namespace_name, current_type);
        case VC_AST_TRY_STATEMENT:
            return process_node(context, tree, node->as.try_statement.try_block, namespace_name, current_type) &&
                process_node_list(context, tree, &node->as.try_statement.catches, namespace_name, current_type) &&
                process_node(context, tree, node->as.try_statement.finally_block, namespace_name, current_type);
        case VC_AST_CATCH_CLAUSE:
            return process_type_ref(context, tree, node->as.catch_clause.type, namespace_name) &&
                process_node(context, tree, node->as.catch_clause.body, namespace_name, current_type);
        case VC_AST_LOCAL_DECLARATION:
            return process_type_ref(context, tree, node->as.local_declaration.type, namespace_name) &&
                process_node(context, tree, node->as.local_declaration.initializer, namespace_name, current_type);
        case VC_AST_IF_STATEMENT:
            return process_node(context, tree, node->as.if_statement.condition, namespace_name, current_type) &&
                process_node(context, tree, node->as.if_statement.then_statement, namespace_name, current_type) &&
                process_node(context, tree, node->as.if_statement.else_statement, namespace_name, current_type);
        case VC_AST_WHILE_STATEMENT:
            return process_node(context, tree, node->as.while_statement.condition, namespace_name, current_type) &&
                process_node(context, tree, node->as.while_statement.body, namespace_name, current_type);
        case VC_AST_DO_WHILE_STATEMENT:
            return process_node(context, tree, node->as.while_statement.body, namespace_name, current_type) &&
                process_node(context, tree, node->as.while_statement.condition, namespace_name, current_type);
        case VC_AST_FOR_STATEMENT:
            return process_node(context, tree, node->as.for_statement.initializer, namespace_name, current_type) &&
                process_node(context, tree, node->as.for_statement.condition, namespace_name, current_type) &&
                process_node(context, tree, node->as.for_statement.increment, namespace_name, current_type) &&
                process_node(context, tree, node->as.for_statement.body, namespace_name, current_type);
        case VC_AST_FOREACH_STATEMENT:
            return process_type_ref(context, tree, node->as.foreach_statement.type, namespace_name) &&
                process_node(context, tree, node->as.foreach_statement.collection, namespace_name, current_type) &&
                process_node(context, tree, node->as.foreach_statement.body, namespace_name, current_type) &&
                process_node(context, tree, node->as.foreach_statement.move_next_await_protocol,
                    namespace_name, current_type) &&
                process_node(context, tree, node->as.foreach_statement.dispose_await_protocol,
                    namespace_name, current_type);
        case VC_AST_USING_STATEMENT:
            return process_node(context, tree, node->as.using_statement.declaration, namespace_name, current_type) &&
                process_node(context, tree, node->as.using_statement.expression, namespace_name, current_type) &&
                process_node(context, tree, node->as.using_statement.body, namespace_name, current_type) &&
                process_node(context, tree, node->as.using_statement.dispose_await_protocol,
                    namespace_name, current_type);
        case VC_AST_LOCK_STATEMENT:
            return process_node(context, tree, node->as.lock_statement.expression, namespace_name, current_type) &&
                process_node(context, tree, node->as.lock_statement.body, namespace_name, current_type);
        case VC_AST_FIXED_STATEMENT:
            return process_type_ref(context, tree, node->as.fixed_statement.type, namespace_name) &&
                process_node(context, tree, node->as.fixed_statement.initializer, namespace_name, current_type) &&
                process_node(context, tree, node->as.fixed_statement.body, namespace_name, current_type);
        case VC_AST_SWITCH_STATEMENT:
            return process_node(context, tree, node->as.switch_statement.expression, namespace_name, current_type) &&
                process_node_list(context, tree, &node->as.switch_statement.sections, namespace_name, current_type);
        case VC_AST_SWITCH_SECTION:
            return process_node_list(context, tree, &node->as.switch_section.labels, namespace_name, current_type) &&
                process_node_list(context, tree, &node->as.switch_section.statements, namespace_name, current_type);
        case VC_AST_SWITCH_LABEL:
            return process_node(context, tree, node->as.switch_label.value, namespace_name, current_type) &&
                process_node(context, tree, node->as.switch_label.pattern, namespace_name, current_type) &&
                process_node(context, tree, node->as.switch_label.guard, namespace_name, current_type);
        case VC_AST_YIELD_RETURN_STATEMENT:
            return process_node(context, tree, node->as.yield_statement.expression, namespace_name, current_type);
        case VC_AST_YIELD_BREAK_STATEMENT:
            return true;
        case VC_AST_TYPE_RECEIVER_EXPRESSION:
            return process_type_ref(context, tree, node->receiver_type, namespace_name);
        case VC_AST_MEMBER_ACCESS_EXPRESSION:
            for (size_t i = 0; i < node->as.member_access_expression.generic_arguments.count; i++)
                if (!process_type_ref(context, tree,
                        node->as.member_access_expression.generic_arguments.items[i], namespace_name)) return false;
            if (!specialize_generic_method_group(context, tree, node, namespace_name, current_type)) return false;
            return process_node(context, tree, node->as.member_access_expression.target, namespace_name, current_type);
        case VC_AST_CALL_EXPRESSION:
            for (size_t i = 0; i < node->as.call_expression.generic_arguments.count; i++)
                if (!process_type_ref(context, tree, node->as.call_expression.generic_arguments.items[i], namespace_name)) return false;
            if (!specialize_generic_call(context, tree, node, namespace_name, current_type)) return false;
            if (!process_node(context, tree, node->as.call_expression.callee, namespace_name, current_type)) return false;
            return process_node_list(context, tree, &node->as.call_expression.arguments, namespace_name, current_type);
        case VC_AST_INDEX_EXPRESSION:
            return process_node(context, tree, node->as.index_expression.target, namespace_name, current_type) &&
                (node->as.index_expression.indices.count != 0
                    ? process_node_list(context, tree, &node->as.index_expression.indices, namespace_name, current_type)
                    : process_node(context, tree, node->as.index_expression.index, namespace_name, current_type));
        case VC_AST_NEW_EXPRESSION:
            return process_type_ref(context, tree, node->as.new_expression.type, namespace_name) &&
                (node->as.new_expression.array_lengths.count != 0
                    ? process_node_list(context, tree, &node->as.new_expression.array_lengths, namespace_name, current_type)
                    : process_node(context, tree, node->as.new_expression.array_length, namespace_name, current_type)) &&
                process_node_list(context, tree, &node->as.new_expression.arguments, namespace_name, current_type) &&
                process_node_list(context, tree, &node->as.new_expression.initializers, namespace_name, current_type);
        case VC_AST_OBJECT_INITIALIZER_MEMBER:
            return process_node(context, tree, node->as.object_initializer_member.value,
                namespace_name, current_type);
        case VC_AST_COLLECTION_INITIALIZER_ELEMENT:
            return process_node_list(context, tree,
                &node->as.collection_initializer_element.arguments,
                namespace_name, current_type);
        case VC_AST_DEFAULT_EXPRESSION:
            return process_type_ref(context, tree, node->as.default_expression.type, namespace_name);
        case VC_AST_TYPEOF_EXPRESSION:
            return process_type_ref(context, tree, node->as.typeof_expression.type, namespace_name);
        case VC_AST_SIZEOF_EXPRESSION:
            return process_type_ref(context, tree, node->as.sizeof_expression.type, namespace_name);
        case VC_AST_STACKALLOC_EXPRESSION:
            return process_type_ref(context, tree, node->as.stackalloc_expression.type, namespace_name) &&
                process_node(context, tree, node->as.stackalloc_expression.count, namespace_name, current_type);
        case VC_AST_CAST_EXPRESSION:
            return process_type_ref(context, tree, node->as.cast_expression.type, namespace_name) &&
                process_node(context, tree, node->as.cast_expression.expression, namespace_name, current_type);
        case VC_AST_TYPE_RELATION_EXPRESSION:
            return process_type_ref(context, tree, node->as.type_relation_expression.type, namespace_name) &&
                process_node(context, tree, node->as.type_relation_expression.expression, namespace_name, current_type) &&
                process_node(context, tree, node->as.type_relation_expression.pattern_constant, namespace_name, current_type) &&
                process_node(context, tree, node->as.type_relation_expression.pattern_left, namespace_name, current_type) &&
                process_node(context, tree, node->as.type_relation_expression.pattern_right, namespace_name, current_type) &&
                process_node_list(context, tree, &node->as.type_relation_expression.pattern_properties, namespace_name, current_type);
        case VC_AST_PROPERTY_PATTERN_MEMBER:
            return process_node(context, tree, node->as.property_pattern_member.pattern, namespace_name, current_type);
        case VC_AST_UNARY_EXPRESSION:
            return process_node(context, tree, node->as.unary_expression.operand, namespace_name, current_type);
        case VC_AST_AWAIT_EXPRESSION:
            return process_node(context, tree, node->as.await_expression.operand, namespace_name, current_type) &&
                process_node(context, tree, node->as.await_expression.awaiter_inference_call,
                    namespace_name, current_type);
        case VC_AST_RANGE_EXPRESSION:
            return process_node(context, tree, node->as.range_expression.start, namespace_name, current_type) &&
                process_node(context, tree, node->as.range_expression.end, namespace_name, current_type);
        case VC_AST_BINARY_EXPRESSION:
            return process_node(context, tree, node->as.binary_expression.left, namespace_name, current_type) &&
                process_node(context, tree, node->as.binary_expression.right, namespace_name, current_type);
        case VC_AST_CONDITIONAL_EXPRESSION:
            return process_node(context, tree, node->as.conditional_expression.condition,
                    namespace_name, current_type) &&
                process_node(context, tree, node->as.conditional_expression.when_true,
                    namespace_name, current_type) &&
                process_node(context, tree, node->as.conditional_expression.when_false,
                    namespace_name, current_type);
        case VC_AST_SWITCH_EXPRESSION:
            return process_node(context, tree, node->as.switch_expression.expression,
                    namespace_name, current_type) &&
                process_node_list(context, tree, &node->as.switch_expression.arms,
                    namespace_name, current_type);
        case VC_AST_SWITCH_EXPRESSION_ARM:
            return process_node(context, tree, node->as.switch_expression_arm.pattern,
                    namespace_name, current_type) &&
                process_node(context, tree, node->as.switch_expression_arm.guard,
                    namespace_name, current_type) &&
                process_node(context, tree, node->as.switch_expression_arm.result,
                    namespace_name, current_type);
        case VC_AST_ASSIGNMENT_EXPRESSION:
            return process_node(context, tree, node->as.assignment_expression.left, namespace_name, current_type) &&
                process_node(context, tree, node->as.assignment_expression.right, namespace_name, current_type);
        case VC_AST_LAMBDA_EXPRESSION:
            return process_node(context, tree, node->as.lambda_expression.body, namespace_name, current_type);
        case VC_AST_COMPILER_CLOSURE_FRAME_EXPRESSION:
            return process_node_list(context, tree,
                &node->as.compiler_closure_frame_expression.captures,
                namespace_name, current_type);
        case VC_AST_COMPILER_CAPTURE_EXPRESSION:
            return process_type_ref(context, tree,
                    node->as.compiler_capture_expression.type, namespace_name) &&
                process_node(context, tree,
                    node->as.compiler_capture_expression.target, namespace_name, current_type);
        case VC_AST_PARENTHESIZED_EXPRESSION:
            return process_node(context, tree, node->as.parenthesized_expression.expression, namespace_name, current_type);
        case VC_AST_ENUM_MEMBER:
            return process_node(context, tree, node->as.enum_member.value, namespace_name, current_type);
        case VC_AST_USING_DECLARATION:
        case VC_AST_BREAK_STATEMENT:
        case VC_AST_CONTINUE_STATEMENT:
        case VC_AST_IDENTIFIER_EXPRESSION:
        case VC_AST_LITERAL_EXPRESSION:
            return true;
    }
    return true;
}

bool vc_monomorphize_diagnostics(VcAstTree **trees, const VcSource *const *sources,
    size_t tree_count, VcDiagnostic *diagnostic, char *error, size_t error_size)
{
    VcMonomorphContext context = {0};
    context.trees = trees;
    context.sources = sources;
    context.diagnostic = diagnostic;
    context.tree_count = tree_count;
    context.error = error;
    context.error_size = error_size;
    if (error != NULL && error_size != 0)
        error[0] = '\0';

    for (size_t i = 0; i < tree_count; i++)
    {
        VcAstTree *tree = trees[i];
        if (tree == NULL || tree->root == NULL || tree->root->kind != VC_AST_COMPILATION_UNIT)
            continue;
        if (!collect_templates(&context, tree, &tree->root->as.compilation_unit.declarations, NULL))
        {
            free(context.templates);
            free(context.method_requests);
            return false;
        }
    }

    for (size_t pass = 0; pass < 64; pass++)
    {
        const size_t before = context.changes;
        for (size_t i = 0; i < tree_count; i++)
        {
            if (trees[i] != NULL && trees[i]->root != NULL &&
                !process_node(&context, trees[i], trees[i]->root, NULL, NULL))
            {
                free(context.templates);
                free(context.method_requests);
                return false;
            }
        }
        if (context.changes == before)
        {
            if (context.error != NULL && context.error_size != 0 && context.error[0] != '\0')
            {
                free(context.templates);
                free(context.method_requests);
                return false;
            }
            for (size_t i = 0; i < context.method_request_count; i++)
            {
                const VcGenericMethodRequest *request = &context.method_requests[i];
                if (!request->matched)
                {
                    set_error(&context, "generic method '%s' with %zu type argument(s) was not found",
                        request->name, request->arguments.count);
                    generic_diagnostic(&context, request->use_tree, request->span);
                    free(context.templates);
                    free(context.method_requests);
                    return false;
                }
            }
            free(context.templates);
            free(context.method_requests);
            return true;
        }
    }

    set_error(&context, "generic specialization did not converge");
    free(context.templates);
    free(context.method_requests);
    return false;
}

bool vc_monomorphize(VcAstTree **trees, size_t tree_count, char *error, size_t error_size)
{
    return vc_monomorphize_diagnostics(trees, NULL, tree_count, NULL, error, error_size);
}
