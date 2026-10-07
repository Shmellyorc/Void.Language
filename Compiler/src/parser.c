#include "diagnostic.h"
#include "parser.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct VcParser
{
    const VcSource *source;
    const VcTokenList *tokens;
    size_t current;
    size_t pending_greater;
    size_t async_method_depth;
    VcAstTree *tree;
    VcParseResult *result;
} VcParser;

static VcAstNode *parse_expression(VcParser *parser);
static VcAstNode *parse_block(VcParser *parser);
static VcAstNode *parse_declaration(VcParser *parser, bool allow_namespace);
static VcAstTypeRef *parse_type(VcParser *parser, bool allow_void);

void vc_parse_result_init(VcParseResult *result)
{
    memset(result, 0, sizeof(*result));
}

static const VcToken *token_at(const VcParser *parser, size_t index)
{
    if (index >= parser->tokens->count)
        return &parser->tokens->items[parser->tokens->count - 1];
    return &parser->tokens->items[index];
}

static const VcToken *current_token(const VcParser *parser)
{
    return token_at(parser, parser->current);
}

static const VcToken *previous_token(const VcParser *parser)
{
    return parser->current == 0 ? token_at(parser, 0) : token_at(parser, parser->current - 1);
}

static VcSourceSpan span_for_location(const VcParser *parser, VcSourceLocation location)
{
    for (size_t i = 0; i < parser->tokens->count; i++)
    {
        if (parser->tokens->items[i].location.offset == location.offset)
            return parser->tokens->items[i].span;
    }

    VcSourceSpan span = {
        .start = location,
        .end = location
    };
    return span;
}

static VcAstNode *new_node(VcParser *parser, VcAstKind kind, VcSourceLocation location)
{
    VcAstNode *node = vc_ast_new_node(parser->tree, kind, location);
    if (node != NULL)
        node->span = span_for_location(parser, location);
    return node;
}

static VcAstTypeRef *new_type(VcParser *parser, VcSourceLocation location)
{
    VcAstTypeRef *type = vc_ast_new_type(parser->tree, location);
    if (type != NULL)
        type->span = span_for_location(parser, location);
    return type;
}

static VcTokenKind current_kind(const VcParser *parser)
{
    if (parser->pending_greater > 0)
        return VC_TOKEN_GREATER;
    return current_token(parser)->kind;
}

static bool at_end(const VcParser *parser)
{
    return current_kind(parser) == VC_TOKEN_EOF;
}

static bool check(const VcParser *parser, VcTokenKind kind)
{
    return current_kind(parser) == kind;
}

static bool check_actual(const VcParser *parser, VcTokenKind kind)
{
    return parser->pending_greater == 0 && current_token(parser)->kind == kind;
}

static const VcToken *advance_token(VcParser *parser)
{
    if (parser->pending_greater > 0)
    {
        parser->pending_greater--;
        return previous_token(parser);
    }

    if (!at_end(parser))
        parser->current++;
    return previous_token(parser);
}

static bool match(VcParser *parser, VcTokenKind kind)
{
    if (!check(parser, kind))
        return false;
    advance_token(parser);
    return true;
}

static void parser_error_at(VcParser *parser, VcSourceLocation location, const char *format, ...)
{
    if (parser->result->has_error)
        return;

    vc_diagnostic_set_code(&parser->result->diagnostic, VC_DIAG_SYNTAX);
    parser->result->has_error = true;
    parser->result->diagnostic.source = parser->source;
    parser->result->diagnostic.path = parser->source->path;
    parser->result->diagnostic.span = span_for_location(parser, location);

    va_list args;
    va_start(args, format);
    vsnprintf(parser->result->diagnostic.message,
        sizeof(parser->result->diagnostic.message), format, args);
    va_end(args);
}

static void parser_error(VcParser *parser, const char *format, ...)
{
    if (parser->result->has_error)
        return;

    vc_diagnostic_set_code(&parser->result->diagnostic, VC_DIAG_SYNTAX);
    parser->result->has_error = true;
    parser->result->diagnostic.source = parser->source;
    parser->result->diagnostic.path = parser->source->path;
    parser->result->diagnostic.span = current_token(parser)->span;

    va_list args;
    va_start(args, format);
    vsnprintf(parser->result->diagnostic.message,
        sizeof(parser->result->diagnostic.message), format, args);
    va_end(args);
}

static void out_of_memory(VcParser *parser)
{
    parser_error(parser, "out of memory while building syntax tree");
}

static VcAstNode *make_expression_body_block(
    VcParser *parser,
    VcAstNode *expression,
    bool returns_value,
    bool returns_ref)
{
    VcAstNode *statement = new_node(parser,
        returns_value ? VC_AST_RETURN_STATEMENT : VC_AST_EXPRESSION_STATEMENT,
        expression->location);
    VcAstNode *block = new_node(parser, VC_AST_BLOCK_STATEMENT, expression->location);
    if (statement == NULL || block == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }

    if (returns_value)
    {
        statement->as.return_statement.expression = expression;
        statement->as.return_statement.is_ref = returns_ref;
    }
    else
        statement->as.expression_statement.expression = expression;

    if (!vc_ast_node_list_push(parser->tree, &block->as.block_statement.statements, statement))
    {
        out_of_memory(parser);
        return NULL;
    }
    return block;
}

static const VcToken *consume(VcParser *parser, VcTokenKind kind, const char *description)
{
    if (check(parser, kind))
        return advance_token(parser);

    if (parser->result->has_error) return NULL;
    parser_error(parser, "expected %s, found '%s'", description, vc_token_kind_name(current_kind(parser)));
    vc_diagnostic_set_code(&parser->result->diagnostic, VC_DIAG_EXPECTED_TOKEN);
    if (kind == VC_TOKEN_SEMICOLON || kind == VC_TOKEN_RIGHT_PAREN ||
        kind == VC_TOKEN_RIGHT_BRACKET || kind == VC_TOKEN_RIGHT_BRACE)
        parser->result->diagnostic.span.end = parser->result->diagnostic.span.start;
    if (kind == VC_TOKEN_SEMICOLON || kind == VC_TOKEN_RIGHT_PAREN ||
        kind == VC_TOKEN_RIGHT_BRACKET || kind == VC_TOKEN_RIGHT_BRACE || kind == VC_TOKEN_COMMA)
    {
        char help[128];
        snprintf(help, sizeof(help), "Insert %s before the highlighted token.", description);
        vc_diagnostic_add_context(&parser->result->diagnostic, VC_DIAGNOSTIC_HELP,
            NULL, (VcSourceSpan){0}, help);
    }
    return NULL;
}

static bool finish_list(VcParser *parser, VcTokenKind closing, const char *description)
{
    if (check(parser, closing)) { advance_token(parser); return true; }
    if (parser->result->has_error) return false;
    parser_error(parser, "expected ',' or %s, found '%s'", description,
        vc_token_kind_name(current_kind(parser)));
    vc_diagnostic_set_code(&parser->result->diagnostic, VC_DIAG_EXPECTED_TOKEN);
    vc_diagnostic_add_context(&parser->result->diagnostic, VC_DIAGNOSTIC_HELP,
        NULL, (VcSourceSpan){0}, "Separate list elements with a comma or close the list with its matching delimiter.");
    return false;
}

static bool consume_type_greater(VcParser *parser)
{
    if (parser->pending_greater > 0)
    {
        parser->pending_greater--;
        return true;
    }

    if (check_actual(parser, VC_TOKEN_GREATER))
    {
        parser->current++;
        return true;
    }

    if (check_actual(parser, VC_TOKEN_GREATER_GREATER))
    {
        parser->current++;
        parser->pending_greater = 1;
        return true;
    }

    parser_error(parser, "expected '>', found '%s'", vc_token_kind_name(current_kind(parser)));
    vc_diagnostic_set_code(&parser->result->diagnostic, VC_DIAG_EXPECTED_TOKEN);
    return false;
}

static char *copy_token_text(VcParser *parser, const VcToken *token)
{
    char *text = vc_ast_copy_text(parser->tree,
        parser->source->text + token->location.offset,
        token->length);
    if (text == NULL)
        out_of_memory(parser);
    return text;
}

static bool token_text_equals(const VcParser *parser, const VcToken *token, const char *value)
{
    const size_t length = strlen(value);
    return token->length == length &&
        memcmp(parser->source->text + token->location.offset, value, length) == 0;
}

static bool is_identifier_token(VcTokenKind kind)
{
    return kind == VC_TOKEN_IDENTIFIER;
}

static char *parse_qualified_name(VcParser *parser)
{
    if (!is_identifier_token(current_kind(parser)))
    {
        parser_error(parser, "expected identifier, found '%s'", vc_token_kind_name(current_kind(parser)));
    vc_diagnostic_set_code(&parser->result->diagnostic, VC_DIAG_EXPECTED_TOKEN);
        return NULL;
    }

    size_t scan = parser->current;
    size_t total = 0;
    size_t segment_count = 0;

    for (;;)
    {
        const VcToken *segment = token_at(parser, scan);
        if (segment->kind != VC_TOKEN_IDENTIFIER)
            break;

        total += segment->length;
        segment_count++;
        scan++;

        if (token_at(parser, scan)->kind != VC_TOKEN_DOT ||
            token_at(parser, scan + 1)->kind != VC_TOKEN_IDENTIFIER)
            break;

        total++;
        scan++;
    }

    char *name = vc_ast_alloc(parser->tree, total + 1);
    if (name == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }

    size_t written = 0;
    for (size_t i = 0; i < segment_count; i++)
    {
        const VcToken *segment = current_token(parser);
        memcpy(name + written, parser->source->text + segment->location.offset, segment->length);
        written += segment->length;
        parser->current++;

        if (i + 1 < segment_count)
        {
            parser->current++;
            name[written++] = '.';
        }
    }

    name[written] = '\0';
    return name;
}

static bool is_type_keyword(VcTokenKind kind, bool allow_void)
{
    switch (kind)
    {
        case VC_TOKEN_KW_BOOL:
        case VC_TOKEN_KW_BYTE:
        case VC_TOKEN_KW_SBYTE:
        case VC_TOKEN_KW_SHORT:
        case VC_TOKEN_KW_USHORT:
        case VC_TOKEN_KW_INT:
        case VC_TOKEN_KW_UINT:
        case VC_TOKEN_KW_LONG:
        case VC_TOKEN_KW_ULONG:
        case VC_TOKEN_KW_FLOAT:
        case VC_TOKEN_KW_DOUBLE:
        case VC_TOKEN_KW_DECIMAL:
        case VC_TOKEN_KW_CHAR:
        case VC_TOKEN_KW_STRING:
        case VC_TOKEN_KW_OBJECT:
            return true;
        case VC_TOKEN_KW_VOID:
            return allow_void;
        default:
            return false;
    }
}

typedef struct VcTypeScan
{
    size_t position;
    size_t pending_greater;
} VcTypeScan;

static VcTokenKind type_scan_kind(const VcParser *parser, const VcTypeScan *scan)
{
    return scan->pending_greater != 0
        ? VC_TOKEN_GREATER
        : token_at(parser, scan->position)->kind;
}

static void type_scan_advance(VcTypeScan *scan)
{
    if (scan->pending_greater != 0)
        scan->pending_greater--;
    else
        scan->position++;
}

static bool type_scan_greater(const VcParser *parser, VcTypeScan *scan)
{
    if (scan->pending_greater != 0)
    {
        scan->pending_greater--;
        return true;
    }

    const VcTokenKind kind = token_at(parser, scan->position)->kind;
    if (kind == VC_TOKEN_GREATER)
    {
        scan->position++;
        return true;
    }
    if (kind == VC_TOKEN_GREATER_GREATER)
    {
        scan->position++;
        scan->pending_greater = 1;
        return true;
    }
    return false;
}

static bool scan_type_tokens_state(const VcParser *parser, VcTypeScan *scan, bool allow_void)
{
    const VcTokenKind kind = type_scan_kind(parser, scan);
    const bool pointer_void = scan->pending_greater == 0 && kind == VC_TOKEN_KW_VOID &&
        token_at(parser, scan->position + 1)->kind == VC_TOKEN_STAR;
    const bool function_pointer = scan->pending_greater == 0 && kind == VC_TOKEN_KW_DELEGATE &&
        token_at(parser, scan->position + 1)->kind == VC_TOKEN_STAR &&
        token_at(parser, scan->position + 2)->kind == VC_TOKEN_LESS;

    if (function_pointer)
    {
        scan->position += 3;
        if (type_scan_kind(parser, scan) == VC_TOKEN_GREATER)
            return false;
        if (!scan_type_tokens_state(parser, scan, true))
            return false;
        while (type_scan_kind(parser, scan) == VC_TOKEN_COMMA)
        {
            type_scan_advance(scan);
            if (!scan_type_tokens_state(parser, scan, true))
                return false;
        }
        if (!type_scan_greater(parser, scan))
            return false;
    }
    else if (kind == VC_TOKEN_IDENTIFIER)
    {
        type_scan_advance(scan);
        while (scan->pending_greater == 0 &&
               token_at(parser, scan->position)->kind == VC_TOKEN_DOT &&
               token_at(parser, scan->position + 1)->kind == VC_TOKEN_IDENTIFIER)
            scan->position += 2;
    }
    else if (is_type_keyword(kind, allow_void) || pointer_void)
    {
        type_scan_advance(scan);
    }
    else
    {
        return false;
    }

    if (!function_pointer && type_scan_kind(parser, scan) == VC_TOKEN_LESS)
    {
        type_scan_advance(scan);
        if (!scan_type_tokens_state(parser, scan, false))
            return false;
        while (type_scan_kind(parser, scan) == VC_TOKEN_COMMA)
        {
            type_scan_advance(scan);
            if (!scan_type_tokens_state(parser, scan, false))
                return false;
        }
        if (!type_scan_greater(parser, scan))
            return false;
    }

    if (type_scan_kind(parser, scan) == VC_TOKEN_QUESTION)
        type_scan_advance(scan);

    while (type_scan_kind(parser, scan) == VC_TOKEN_STAR)
        type_scan_advance(scan);

    while (scan->pending_greater == 0 &&
           token_at(parser, scan->position)->kind == VC_TOKEN_LEFT_BRACKET)
    {
        size_t position = scan->position + 1;
        if (token_at(parser, position)->kind == VC_TOKEN_RIGHT_BRACKET)
        {
            scan->position = position + 1;
            continue;
        }
        if (token_at(parser, position)->kind == VC_TOKEN_COMMA)
        {
            while (token_at(parser, position)->kind == VC_TOKEN_COMMA)
                position++;
            if (token_at(parser, position)->kind != VC_TOKEN_RIGHT_BRACKET)
                return false;
            scan->position = position + 1;
            continue;
        }
        break;
    }

    return true;
}

static bool scan_type_tokens(const VcParser *parser, size_t *position, bool allow_void)
{
    VcTypeScan scan = {*position, 0};
    if (!scan_type_tokens_state(parser, &scan, allow_void) || scan.pending_greater != 0)
        return false;
    *position = scan.position;
    return true;
}

static bool token_can_start_cast_operand(VcTokenKind kind, bool allow_prefix_operator)
{
    switch (kind)
    {
        case VC_TOKEN_IDENTIFIER:
        case VC_TOKEN_NUMBER:
        case VC_TOKEN_STRING:
        case VC_TOKEN_CHARACTER:
        case VC_TOKEN_INTERPOLATED_START:
        case VC_TOKEN_KW_THIS:
        case VC_TOKEN_KW_BASE:
        case VC_TOKEN_KW_TRUE:
        case VC_TOKEN_KW_FALSE:
        case VC_TOKEN_KW_NULL:
        case VC_TOKEN_KW_NEW:
        case VC_TOKEN_KW_DEFAULT:
        case VC_TOKEN_KW_SIZEOF:
        case VC_TOKEN_KW_STACKALLOC:
        case VC_TOKEN_LEFT_PAREN:
            return true;
        case VC_TOKEN_PLUS:
        case VC_TOKEN_MINUS:
        case VC_TOKEN_BANG:
        case VC_TOKEN_TILDE:
        case VC_TOKEN_PLUS_PLUS:
        case VC_TOKEN_MINUS_MINUS:
        case VC_TOKEN_STAR:
        case VC_TOKEN_AMPERSAND:
            return allow_prefix_operator;
        default:
            return false;
    }
}

static bool looks_like_cast(const VcParser *parser)
{
    if (current_kind(parser) != VC_TOKEN_LEFT_PAREN)
        return false;

    size_t position = parser->current + 1;
    const VcTokenKind first = token_at(parser, position)->kind;
    const bool builtin = is_type_keyword(first, false) ||
        (first == VC_TOKEN_KW_VOID && token_at(parser, position + 1)->kind == VC_TOKEN_STAR);

    if (!scan_type_tokens(parser, &position, false) ||
        token_at(parser, position)->kind != VC_TOKEN_RIGHT_PAREN)
        return false;

    return token_can_start_cast_operand(token_at(parser, position + 1)->kind, builtin);
}

static VcAstTypeRef *parse_type(VcParser *parser, bool allow_void)
{
    const VcToken *start = current_token(parser);
    const bool pointer_void = current_kind(parser) == VC_TOKEN_KW_VOID &&
        token_at(parser, parser->current + 1)->kind == VC_TOKEN_STAR;
    const bool function_pointer = current_kind(parser) == VC_TOKEN_KW_DELEGATE &&
        token_at(parser, parser->current + 1)->kind == VC_TOKEN_STAR &&
        token_at(parser, parser->current + 2)->kind == VC_TOKEN_LESS;
    if (!function_pointer && !is_identifier_token(current_kind(parser)) &&
        !is_type_keyword(current_kind(parser), allow_void) && !pointer_void)
    {
        parser_error(parser, "expected type, found '%s'", vc_token_kind_name(current_kind(parser)));
    vc_diagnostic_set_code(&parser->result->diagnostic, VC_DIAG_EXPECTED_TOKEN);
        return NULL;
    }

    VcAstTypeRef *type = new_type(parser, start->location);
    if (type == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }

    if (function_pointer)
    {
        advance_token(parser);
        advance_token(parser);
        advance_token(parser);
        type->is_function_pointer = true;
        type->name = vc_ast_copy_text(parser->tree, "delegate*", strlen("delegate*"));
        if (type->name == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        if (check_actual(parser, VC_TOKEN_GREATER))
        {
            parser_error(parser, "native function pointer type requires a return type");
            return NULL;
        }
        do
        {
            VcAstTypeRef *signature_type = parse_type(parser, true);
            if (signature_type == NULL)
                return NULL;
            if (!vc_ast_type_list_push(parser->tree, &type->generic_arguments, signature_type))
            {
                out_of_memory(parser);
                return NULL;
            }
        } while (match(parser, VC_TOKEN_COMMA));
        if (!consume_type_greater(parser))
            return NULL;
        for (size_t i = 0; i + 1 < type->generic_arguments.count; i++)
        {
            const VcAstTypeRef *parameter_type = type->generic_arguments.items[i];
            if (parameter_type != NULL && !parameter_type->is_function_pointer &&
                parameter_type->name != NULL && strcmp(parameter_type->name, "void") == 0 &&
                parameter_type->pointer_depth == 0 && parameter_type->array_rank == 0 &&
                parameter_type->rectangular_rank == 0 && !parameter_type->nullable)
            {
                parser_error(parser, "native function pointer parameters cannot be void");
                return NULL;
            }
        }
    }
    else
    {
        if (is_type_keyword(current_kind(parser), allow_void) || pointer_void)
        {
            const VcToken *name = advance_token(parser);
            type->name = copy_token_text(parser, name);
        }
        else
        {
            type->name = parse_qualified_name(parser);
        }

        if (parser->result->has_error)
            return NULL;

        if (match(parser, VC_TOKEN_LESS))
        {
            do
            {
                VcAstTypeRef *argument = parse_type(parser, false);
                if (argument == NULL)
                    return NULL;
                if (!vc_ast_type_list_push(parser->tree, &type->generic_arguments, argument))
                {
                    out_of_memory(parser);
                    return NULL;
                }
            } while (match(parser, VC_TOKEN_COMMA));

            if (!consume_type_greater(parser))
                return NULL;
        }
    }

    if (match(parser, VC_TOKEN_QUESTION))
    {
        if (type->is_function_pointer)
        {
            parser_error(parser, "native function pointers are already nullable and cannot use '?'");
            return NULL;
        }
        type->nullable = true;
    }

    if (type->is_function_pointer && check_actual(parser, VC_TOKEN_STAR))
    {
        parser_error(parser, "native function pointers are distinct from data pointers and cannot use a data-pointer '*' suffix");
        return NULL;
    }
    while (match(parser, VC_TOKEN_STAR))
        type->pointer_depth++;

    while (check_actual(parser, VC_TOKEN_LEFT_BRACKET) &&
           (token_at(parser, parser->current + 1)->kind == VC_TOKEN_RIGHT_BRACKET ||
            token_at(parser, parser->current + 1)->kind == VC_TOKEN_COMMA))
    {
        advance_token(parser);
        if (match(parser, VC_TOKEN_RIGHT_BRACKET))
        {
            type->array_rank++;
            continue;
        }

        size_t rank = 1;
        while (match(parser, VC_TOKEN_COMMA))
            rank++;
        if (rank == 1 || consume(parser, VC_TOKEN_RIGHT_BRACKET, "']'") == NULL)
        {
            parser_error(parser, "array type dimensions must be empty or comma-separated");
            return NULL;
        }
        if (type->rectangular_rank != 0 || type->array_rank != 0)
        {
            parser_error(parser, "only one rectangular array suffix is supported before jagged suffixes");
            return NULL;
        }
        type->rectangular_rank = rank;
    }

    type->span.end = previous_token(parser)->span.end;

    return type;
}

static bool parse_generic_parameters(VcParser *parser, VcAstStringList *parameters)
{
    if (!match(parser, VC_TOKEN_LESS))
        return true;

    do
    {
        const VcToken *name = consume(parser, VC_TOKEN_IDENTIFIER, "generic parameter name");
        if (name == NULL)
            return false;

        char *copy = copy_token_text(parser, name);
        if (copy == NULL)
            return false;
        for (size_t i = 0; i < parameters->count; i++)
        {
            if (strcmp(parameters->items[i], copy) == 0)
            {
                parser_error(parser, "duplicate generic parameter '%s'", copy);
                return false;
            }
        }

        if (!vc_ast_string_list_push(parser->tree, parameters, copy))
        {
            out_of_memory(parser);
            return false;
        }
    } while (match(parser, VC_TOKEN_COMMA));

    return consume_type_greater(parser);
}

static bool generic_parameter_exists(const VcAstStringList *parameters, const char *name)
{
    for (size_t i = 0; i < parameters->count; i++)
    {
        if (strcmp(parameters->items[i], name) == 0)
            return true;
    }
    return false;
}

static bool parse_generic_constraints(
    VcParser *parser,
    const VcAstStringList *parameters,
    VcAstNodeList *constraints)
{
    while (match(parser, VC_TOKEN_KW_WHERE))
    {
        const VcToken *parameter_token = consume(parser, VC_TOKEN_IDENTIFIER, "generic parameter name");
        if (parameter_token == NULL)
            return false;
        char *parameter = copy_token_text(parser, parameter_token);
        if (parameter == NULL)
            return false;
        if (!generic_parameter_exists(parameters, parameter))
        {
            parser_error(parser, "generic constraint refers to unknown parameter '%s'", parameter);
            return false;
        }
        for (size_t i = 0; i < constraints->count; i++)
        {
            if (strcmp(constraints->items[i]->as.generic_constraint.parameter, parameter) == 0)
            {
                parser_error(parser, "generic parameter '%s' has more than one where clause", parameter);
                return false;
            }
        }
        if (consume(parser, VC_TOKEN_COLON, "':'") == NULL)
            return false;

        VcAstNode *constraint = new_node(parser, VC_AST_GENERIC_CONSTRAINT, parameter_token->location);
        if (constraint == NULL)
        {
            out_of_memory(parser);
            return false;
        }
        constraint->as.generic_constraint.parameter = parameter;

        bool saw_any = false;
        bool saw_constructor = false;
        for (;;)
        {
            if (match(parser, VC_TOKEN_KW_CLASS))
            {
                if (saw_any)
                {
                    parser_error(parser, "'class' constraint for '%s' must appear first", parameter);
                    return false;
                }
                if (constraint->as.generic_constraint.requires_value_type)
                {
                    parser_error(parser, "generic parameter '%s' cannot have both class and struct constraints", parameter);
                    return false;
                }
                if (constraint->as.generic_constraint.requires_unmanaged_type)
                {
                    parser_error(parser, "generic parameter '%s' cannot combine unmanaged with class or struct constraints", parameter);
                    return false;
                }
                if (constraint->as.generic_constraint.requires_reference_type)
                {
                    parser_error(parser, "duplicate 'class' constraint for '%s'", parameter);
                    return false;
                }
                constraint->as.generic_constraint.requires_reference_type = true;
                saw_any = true;
            }
            else if (match(parser, VC_TOKEN_KW_STRUCT))
            {
                if (constraint->as.generic_constraint.requires_unmanaged_type)
                {
                    parser_error(parser, "generic parameter '%s' cannot have both struct and unmanaged constraints", parameter);
                    return false;
                }
                if (saw_any)
                {
                    parser_error(parser, "'struct' constraint for '%s' must appear first", parameter);
                    return false;
                }
                if (constraint->as.generic_constraint.requires_reference_type)
                {
                    parser_error(parser, "generic parameter '%s' cannot have both class and struct constraints", parameter);
                    return false;
                }
                if (constraint->as.generic_constraint.requires_value_type)
                {
                    parser_error(parser, "duplicate 'struct' constraint for '%s'", parameter);
                    return false;
                }
                constraint->as.generic_constraint.requires_value_type = true;
                saw_any = true;
            }
            else if (check(parser, VC_TOKEN_IDENTIFIER) &&
                token_text_equals(parser, current_token(parser), "unmanaged"))
            {
                if (constraint->as.generic_constraint.requires_unmanaged_type)
                {
                    parser_error(parser, "duplicate 'unmanaged' constraint for '%s'", parameter);
                    return false;
                }
                if (constraint->as.generic_constraint.requires_reference_type ||
                    constraint->as.generic_constraint.requires_value_type)
                {
                    parser_error(parser, "generic parameter '%s' cannot combine unmanaged with class or struct constraints", parameter);
                    return false;
                }
                if (saw_any)
                {
                    parser_error(parser, "'unmanaged' constraint for '%s' must appear first", parameter);
                    return false;
                }
                advance_token(parser);
                constraint->as.generic_constraint.requires_unmanaged_type = true;
                saw_any = true;
            }
            else if (match(parser, VC_TOKEN_KW_NEW))
            {
                if (saw_constructor)
                {
                    parser_error(parser, "duplicate 'new()' constraint for '%s'", parameter);
                    return false;
                }
                if (constraint->as.generic_constraint.requires_value_type)
                {
                    parser_error(parser, "'new()' cannot be combined with the 'struct' constraint for '%s'", parameter);
                    return false;
                }
                if (constraint->as.generic_constraint.requires_unmanaged_type)
                {
                    parser_error(parser, "'new()' cannot be combined with the 'unmanaged' constraint for '%s'", parameter);
                    return false;
                }
                if (consume(parser, VC_TOKEN_LEFT_PAREN, "'('") == NULL ||
                    consume(parser, VC_TOKEN_RIGHT_PAREN, "')'") == NULL)
                    return false;
                constraint->as.generic_constraint.requires_constructor = true;
                saw_constructor = true;
                saw_any = true;
                if (check(parser, VC_TOKEN_COMMA))
                {
                    parser_error(parser, "'new()' constraint for '%s' must appear last", parameter);
                    return false;
                }
            }
            else
            {
                if (saw_constructor)
                {
                    parser_error(parser, "'new()' constraint for '%s' must appear last", parameter);
                    return false;
                }
                VcAstTypeRef *type = parse_type(parser, false);
                if (type == NULL)
                    return false;
                if (type->generic_arguments.count == 0 && type->pointer_depth == 0 &&
                    type->array_rank == 0 && type->rectangular_rank == 0 && !type->nullable &&
                    generic_parameter_exists(parameters, type->name))
                    type->generic_constraint_parameter = true;
                if (!vc_ast_type_list_push(parser->tree,
                        &constraint->as.generic_constraint.type_constraints, type))
                {
                    out_of_memory(parser);
                    return false;
                }
                saw_any = true;
            }

            if (!match(parser, VC_TOKEN_COMMA))
                break;
        }

        if (!saw_any)
        {
            parser_error(parser, "generic parameter '%s' requires at least one constraint", parameter);
            return false;
        }
        if (!vc_ast_node_list_push(parser->tree, constraints, constraint))
        {
            out_of_memory(parser);
            return false;
        }
    }
    return true;
}

static uint32_t modifier_flag(VcTokenKind kind)
{
    switch (kind)
    {
        case VC_TOKEN_KW_PUBLIC: return VC_AST_MOD_PUBLIC;
        case VC_TOKEN_KW_PRIVATE: return VC_AST_MOD_PRIVATE;
        case VC_TOKEN_KW_PROTECTED: return VC_AST_MOD_PROTECTED;
        case VC_TOKEN_KW_INTERNAL: return VC_AST_MOD_INTERNAL;
        case VC_TOKEN_KW_STATIC: return VC_AST_MOD_STATIC;
        case VC_TOKEN_KW_SEALED: return VC_AST_MOD_SEALED;
        case VC_TOKEN_KW_ABSTRACT: return VC_AST_MOD_ABSTRACT;
        case VC_TOKEN_KW_READONLY: return VC_AST_MOD_READONLY;
        case VC_TOKEN_KW_UNSAFE: return VC_AST_MOD_UNSAFE;
        case VC_TOKEN_KW_EXTERN: return VC_AST_MOD_EXTERN;
        case VC_TOKEN_KW_VIRTUAL: return VC_AST_MOD_VIRTUAL;
        case VC_TOKEN_KW_OVERRIDE: return VC_AST_MOD_OVERRIDE;
        case VC_TOKEN_KW_CONST: return VC_AST_MOD_CONST;
        default: return 0;
    }
}

static bool scan_generic_method_parameter_suffix(const VcParser *parser, size_t *position)
{
    if (token_at(parser, *position)->kind != VC_TOKEN_LESS)
        return true;

    (*position)++;
    for (;;)
    {
        if (token_at(parser, *position)->kind != VC_TOKEN_IDENTIFIER)
            return false;
        (*position)++;
        if (token_at(parser, *position)->kind == VC_TOKEN_GREATER)
        {
            (*position)++;
            return true;
        }
        if (token_at(parser, *position)->kind != VC_TOKEN_COMMA)
            return false;
        (*position)++;
    }
}

static bool contextual_async_is_type_name(
    const VcParser *parser,
    const char *containing_type_name)
{
    if (current_kind(parser) != VC_TOKEN_IDENTIFIER ||
        !token_text_equals(parser, current_token(parser), "async"))
        return false;

    if (containing_type_name != NULL && strcmp(containing_type_name, "async") == 0 &&
        token_at(parser, parser->current + 1)->kind == VC_TOKEN_LEFT_PAREN)
        return true;

    size_t position = parser->current;
    if (!scan_type_tokens(parser, &position, true))
        return false;

    const VcToken *member_name = token_at(parser, position);
    if (member_name->kind != VC_TOKEN_IDENTIFIER)
        return false;

    if (containing_type_name != NULL &&
        token_text_equals(parser, member_name, containing_type_name) &&
        token_at(parser, position + 1)->kind == VC_TOKEN_LEFT_PAREN)
        return false;

    position++;
    if (token_at(parser, position)->kind == VC_TOKEN_LESS)
    {
        if (!scan_generic_method_parameter_suffix(parser, &position))
            return false;
        return token_at(parser, position)->kind == VC_TOKEN_LEFT_PAREN;
    }

    switch (token_at(parser, position)->kind)
    {
        case VC_TOKEN_LEFT_PAREN:
        case VC_TOKEN_LEFT_BRACE:
        case VC_TOKEN_FAT_ARROW:
        case VC_TOKEN_EQUAL:
        case VC_TOKEN_SEMICOLON:
            return true;
        default:
            return false;
    }
}

static uint32_t parse_modifiers(
    VcParser *parser,
    bool allow_async,
    const char *containing_type_name)
{
    uint32_t modifiers = 0;
    for (;;)
    {
        const uint32_t flag = modifier_flag(current_kind(parser));
        if (flag != 0)
        {
            modifiers |= flag;
            advance_token(parser);
            continue;
        }

        if (allow_async && current_kind(parser) == VC_TOKEN_IDENTIFIER &&
            token_text_equals(parser, current_token(parser), "async") &&
            !contextual_async_is_type_name(parser, containing_type_name))
        {
            modifiers |= VC_AST_MOD_ASYNC;
            advance_token(parser);
            continue;
        }
        break;
    }
    return modifiers;
}

static int binary_precedence(VcTokenKind kind)
{
    switch (kind)
    {
        case VC_TOKEN_QUESTION_QUESTION: return 1;
        case VC_TOKEN_PIPE_PIPE: return 2;
        case VC_TOKEN_AMPERSAND_AMPERSAND: return 3;
        case VC_TOKEN_PIPE: return 4;
        case VC_TOKEN_CARET: return 5;
        case VC_TOKEN_AMPERSAND: return 6;
        case VC_TOKEN_EQUAL_EQUAL:
        case VC_TOKEN_BANG_EQUAL:
            return 7;
        case VC_TOKEN_LESS:
        case VC_TOKEN_LESS_EQUAL:
        case VC_TOKEN_GREATER:
        case VC_TOKEN_GREATER_EQUAL:
            return 8;
        case VC_TOKEN_LESS_LESS:
        case VC_TOKEN_GREATER_GREATER:
            return 9;
        case VC_TOKEN_PLUS:
        case VC_TOKEN_MINUS:
            return 10;
        case VC_TOKEN_STAR:
        case VC_TOKEN_SLASH:
        case VC_TOKEN_PERCENT:
            return 11;
        default:
            return 0;
    }
}

static bool is_assignment_operator(VcTokenKind kind)
{
    switch (kind)
    {
        case VC_TOKEN_EQUAL:
        case VC_TOKEN_PLUS_EQUAL:
        case VC_TOKEN_MINUS_EQUAL:
        case VC_TOKEN_STAR_EQUAL:
        case VC_TOKEN_SLASH_EQUAL:
        case VC_TOKEN_PERCENT_EQUAL:
        case VC_TOKEN_AMPERSAND_EQUAL:
        case VC_TOKEN_PIPE_EQUAL:
        case VC_TOKEN_CARET_EQUAL:
        case VC_TOKEN_LESS_LESS_EQUAL:
        case VC_TOKEN_GREATER_GREATER_EQUAL:
        case VC_TOKEN_QUESTION_QUESTION_EQUAL:
            return true;
        default:
            return false;
    }
}

static bool is_prefix_operator(VcTokenKind kind)
{
    switch (kind)
    {
        case VC_TOKEN_PLUS:
        case VC_TOKEN_MINUS:
        case VC_TOKEN_BANG:
        case VC_TOKEN_TILDE:
        case VC_TOKEN_PLUS_PLUS:
        case VC_TOKEN_MINUS_MINUS:
        case VC_TOKEN_STAR:
        case VC_TOKEN_AMPERSAND:
        case VC_TOKEN_CARET:
            return true;
        default:
            return false;
    }
}


static VcAstNode *parse_argument(VcParser *parser)
{
    char *argument_name = NULL;
    if (current_kind(parser) == VC_TOKEN_IDENTIFIER &&
        token_at(parser, parser->current + 1)->kind == VC_TOKEN_COLON)
    {
        const VcToken *name = advance_token(parser);
        advance_token(parser);
        argument_name = copy_token_text(parser, name);
        if (argument_name == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
    }

    if (check(parser, VC_TOKEN_KW_REF) || check(parser, VC_TOKEN_KW_OUT) || check(parser, VC_TOKEN_KW_IN))
    {
        const VcToken *modifier = advance_token(parser);
        VcAstNode *operand = parse_expression(parser);
        if (operand == NULL)
            return NULL;

        VcAstNode *node = new_node(parser, VC_AST_UNARY_EXPRESSION, modifier->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.unary_expression.operator_kind = modifier->kind;
        node->as.unary_expression.operand = operand;
        node->as.unary_expression.postfix = false;
        node->argument_name = argument_name;
        return node;
    }

    VcAstNode *argument = parse_expression(parser);
    if (argument != NULL)
        argument->argument_name = argument_name;
    return argument;
}

static bool parse_collection_initializer_element(VcParser *parser, VcAstNode *new_expression)
{
    const VcSourceLocation location = current_token(parser)->location;
    VcAstNode *initializer = new_node(parser,
        VC_AST_COLLECTION_INITIALIZER_ELEMENT, location);
    if (initializer == NULL)
    {
        out_of_memory(parser);
        return false;
    }

    if (match(parser, VC_TOKEN_LEFT_BRACE))
    {
        if (check(parser, VC_TOKEN_RIGHT_BRACE))
        {
            parser_error(parser, "collection initializer element cannot be empty");
            return false;
        }
        do
        {
            VcAstNode *argument = parse_expression(parser);
            if (argument == NULL)
                return false;
            if (!vc_ast_node_list_push(parser->tree,
                    &initializer->as.collection_initializer_element.arguments, argument))
            {
                out_of_memory(parser);
                return false;
            }
        } while (match(parser, VC_TOKEN_COMMA));

        if (consume(parser, VC_TOKEN_RIGHT_BRACE, "'}'") == NULL)
            return false;
    }
    else
    {
        VcAstNode *argument = parse_expression(parser);
        if (argument == NULL)
            return false;
        if (!vc_ast_node_list_push(parser->tree,
                &initializer->as.collection_initializer_element.arguments, argument))
        {
            out_of_memory(parser);
            return false;
        }
    }

    if (!vc_ast_node_list_push(parser->tree,
            &new_expression->as.new_expression.initializers, initializer))
    {
        out_of_memory(parser);
        return false;
    }
    return true;
}

static VcAstNode *parse_rectangular_initializer_group(VcParser *parser)
{
    const VcToken *brace = consume(parser, VC_TOKEN_LEFT_BRACE, "'{'");
    if (brace == NULL)
        return NULL;

    VcAstNode *group = new_node(parser, VC_AST_COLLECTION_INITIALIZER_ELEMENT,
        brace->location);
    if (group == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }

    if (match(parser, VC_TOKEN_RIGHT_BRACE))
        return group;

    do
    {
        VcAstNode *item = check(parser, VC_TOKEN_LEFT_BRACE)
            ? parse_rectangular_initializer_group(parser)
            : parse_expression(parser);
        if (item == NULL)
            return NULL;
        if (!vc_ast_node_list_push(parser->tree,
                &group->as.collection_initializer_element.arguments, item))
        {
            out_of_memory(parser);
            return NULL;
        }
    } while (match(parser, VC_TOKEN_COMMA) && !check(parser, VC_TOKEN_RIGHT_BRACE));

    if (consume(parser, VC_TOKEN_RIGHT_BRACE, "'}'") == NULL)
        return NULL;
    return group;
}

static bool parse_rectangular_initializer(VcParser *parser, VcAstNode *new_expression)
{
    if (consume(parser, VC_TOKEN_LEFT_BRACE, "'{'") == NULL)
        return false;
    new_expression->as.new_expression.has_initializer = true;

    if (match(parser, VC_TOKEN_RIGHT_BRACE))
        return true;

    do
    {
        VcAstNode *item = check(parser, VC_TOKEN_LEFT_BRACE)
            ? parse_rectangular_initializer_group(parser)
            : parse_expression(parser);
        if (item == NULL)
            return false;
        if (!vc_ast_node_list_push(parser->tree,
                &new_expression->as.new_expression.initializers, item))
        {
            out_of_memory(parser);
            return false;
        }
    } while (match(parser, VC_TOKEN_COMMA) && !check(parser, VC_TOKEN_RIGHT_BRACE));

    return consume(parser, VC_TOKEN_RIGHT_BRACE, "'}'") != NULL;
}

static bool parse_initializer(VcParser *parser, VcAstNode *new_expression)
{
    if (!match(parser, VC_TOKEN_LEFT_BRACE))
        return true;
    new_expression->as.new_expression.has_initializer = true;

    if (check(parser, VC_TOKEN_RIGHT_BRACE))
        return consume(parser, VC_TOKEN_RIGHT_BRACE, "'}'") != NULL;

    const bool object_initializer = is_identifier_token(current_kind(parser)) &&
        token_at(parser, parser->current + 1)->kind == VC_TOKEN_EQUAL;

    do
    {
        if (object_initializer)
        {
            if (!is_identifier_token(current_kind(parser)))
            {
                parser_error(parser, "expected object initializer member name");
                return false;
            }
            const VcToken *name = advance_token(parser);
            if (consume(parser, VC_TOKEN_EQUAL, "'='") == NULL)
                return false;
            VcAstNode *value = parse_expression(parser);
            if (value == NULL)
                return false;

            VcAstNode *initializer = new_node(parser,
                VC_AST_OBJECT_INITIALIZER_MEMBER, name->location);
            if (initializer == NULL)
            {
                out_of_memory(parser);
                return false;
            }
            initializer->as.object_initializer_member.member = copy_token_text(parser, name);
            initializer->as.object_initializer_member.value = value;
            if (initializer->as.object_initializer_member.member == NULL ||
                !vc_ast_node_list_push(parser->tree,
                    &new_expression->as.new_expression.initializers, initializer))
            {
                out_of_memory(parser);
                return false;
            }
        }
        else if (!parse_collection_initializer_element(parser, new_expression))
            return false;
    } while (match(parser, VC_TOKEN_COMMA) && !check(parser, VC_TOKEN_RIGHT_BRACE));

    return finish_list(parser, VC_TOKEN_RIGHT_BRACE, "'}'");
}

static VcAstNode *synthetic_interpolation_string_literal(
    VcParser *parser,
    const VcToken *token)
{
    if (parser == NULL || token == NULL)
        return NULL;

    VcAstNode *literal = new_node(parser, VC_AST_LITERAL_EXPRESSION, token->location);
    if (literal == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }
    literal->as.literal_expression.literal_kind = VC_AST_LITERAL_STRING;

    const char *source = parser->source->text + token->location.offset;
    char *text = vc_ast_alloc(parser->tree, token->length + 3u);
    if (text == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }

    size_t output = 0;
    text[output++] = '"';
    for (size_t i = 0; i < token->length; i++)
    {
        if (i + 1u < token->length &&
            ((source[i] == '{' && source[i + 1u] == '{') ||
             (source[i] == '}' && source[i + 1u] == '}')))
        {
            text[output++] = source[i];
            i++;
            continue;
        }
        text[output++] = source[i];
    }
    text[output++] = '"';
    text[output] = '\0';
    literal->as.literal_expression.text = text;
    return literal;
}

static VcAstNode *synthetic_interpolation_call(
    VcParser *parser,
    VcAstNode *receiver,
    const char *member_name,
    VcAstNode *argument,
    VcSourceLocation location)
{
    VcAstNode *member = new_node(parser, VC_AST_MEMBER_ACCESS_EXPRESSION, location);
    if (member == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }
    member->as.member_access_expression.target = receiver;
    member->as.member_access_expression.member = vc_ast_copy_text(
        parser->tree, member_name, strlen(member_name));
    if (member->as.member_access_expression.member == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }

    VcAstNode *call = new_node(parser, VC_AST_CALL_EXPRESSION, location);
    if (call == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }
    call->as.call_expression.callee = member;
    if (argument != NULL &&
        !vc_ast_node_list_push(parser->tree, &call->as.call_expression.arguments, argument))
    {
        out_of_memory(parser);
        return NULL;
    }
    return call;
}

static VcAstNode *parse_interpolated_string(VcParser *parser)
{
    const VcToken *start = current_token(parser);
    if (!match(parser, VC_TOKEN_INTERPOLATED_START))
        return NULL;

    VcAstNode *builder = new_node(parser, VC_AST_NEW_EXPRESSION, start->location);
    if (builder == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }
    builder->as.new_expression.type = new_type(parser, start->location);
    if (builder->as.new_expression.type == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }
    static const char builder_type[] = "Void.Text.StringBuilder";
    builder->as.new_expression.type->name = vc_ast_copy_text(
        parser->tree, builder_type, sizeof(builder_type) - 1u);
    if (builder->as.new_expression.type->name == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }

    VcAstNode *chain = builder;
    while (!check(parser, VC_TOKEN_INTERPOLATED_END))
    {
        if (at_end(parser))
        {
            parser_error_at(parser, start->location, "unterminated interpolated string literal");
            return NULL;
        }

        if (check(parser, VC_TOKEN_INTERPOLATED_TEXT))
        {
            const VcToken *text_token = advance_token(parser);
            VcAstNode *literal = synthetic_interpolation_string_literal(parser, text_token);
            if (literal == NULL)
                return NULL;
            chain = synthetic_interpolation_call(
                parser, chain, "Append", literal, text_token->location);
            if (chain == NULL)
                return NULL;
            continue;
        }

        if (check(parser, VC_TOKEN_INTERPOLATION_START))
        {
            const VcToken *expression_start = advance_token(parser);
            if (check(parser, VC_TOKEN_INTERPOLATION_END))
            {
                parser_error_at(parser, expression_start->location,
                    "interpolation expression cannot be empty");
                return NULL;
            }
            VcAstNode *value = parse_expression(parser);
            if (value == NULL)
                return NULL;
            if (consume(parser, VC_TOKEN_INTERPOLATION_END,
                    "'}' after interpolation expression") == NULL)
                return NULL;
            chain = synthetic_interpolation_call(
                parser, chain, "Append", value, expression_start->location);
            if (chain == NULL)
                return NULL;
            continue;
        }

        parser_error(parser, "expected interpolated string text or expression, found '%s'",
            vc_token_kind_name(current_kind(parser)));
        return NULL;
    }

    const VcToken *end = advance_token(parser);
    return synthetic_interpolation_call(parser, chain, "ToString", NULL, end->location);
}

static VcAstNode *parse_primary(VcParser *parser)
{
    const VcToken *token = current_token(parser);

    if (check(parser, VC_TOKEN_INTERPOLATED_START))
        return parse_interpolated_string(parser);

    if (match(parser, VC_TOKEN_KW_DEFAULT))
    {
        VcAstTypeRef *type = NULL;
        if (match(parser, VC_TOKEN_LEFT_PAREN))
        {
            type = parse_type(parser, false);
            if (type == NULL)
                return NULL;
            if (consume(parser, VC_TOKEN_RIGHT_PAREN, "')'") == NULL)
                return NULL;
        }

        VcAstNode *node = new_node(parser, VC_AST_DEFAULT_EXPRESSION, token->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.default_expression.type = type;
        return node;
    }

    if (match(parser, VC_TOKEN_KW_TYPEOF))
    {
        if (consume(parser, VC_TOKEN_LEFT_PAREN, "'('") == NULL)
            return NULL;
        VcAstTypeRef *type = parse_type(parser, true);
        if (type == NULL)
            return NULL;
        if (consume(parser, VC_TOKEN_RIGHT_PAREN, "')'") == NULL)
            return NULL;

        VcAstNode *node = new_node(parser, VC_AST_TYPEOF_EXPRESSION, token->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.typeof_expression.type = type;
        return node;
    }

    if (match(parser, VC_TOKEN_KW_SIZEOF))
    {
        if (consume(parser, VC_TOKEN_LEFT_PAREN, "'('") == NULL)
            return NULL;
        VcAstTypeRef *type = parse_type(parser, false);
        if (type == NULL)
            return NULL;
        if (consume(parser, VC_TOKEN_RIGHT_PAREN, "')'") == NULL)
            return NULL;

        VcAstNode *node = new_node(parser, VC_AST_SIZEOF_EXPRESSION, token->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.sizeof_expression.type = type;
        return node;
    }

    if (match(parser, VC_TOKEN_KW_STACKALLOC))
    {
        VcAstTypeRef *type = parse_type(parser, false);
        if (type == NULL)
            return NULL;
        if (consume(parser, VC_TOKEN_LEFT_BRACKET, "'['") == NULL)
            return NULL;
        VcAstNode *count = parse_expression(parser);
        if (count == NULL)
            return NULL;
        if (consume(parser, VC_TOKEN_RIGHT_BRACKET, "']'") == NULL)
            return NULL;

        VcAstNode *node = new_node(parser, VC_AST_STACKALLOC_EXPRESSION, token->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.stackalloc_expression.type = type;
        node->as.stackalloc_expression.count = count;
        return node;
    }

    if (is_type_keyword(current_kind(parser), false))
    {
        advance_token(parser);
        VcAstNode *node = new_node(parser, VC_AST_TYPE_RECEIVER_EXPRESSION, token->location);
        VcAstTypeRef *type = new_type(parser, token->location);
        if (node == NULL || type == NULL) { out_of_memory(parser); return NULL; }
        type->name = copy_token_text(parser, token);
        node->receiver_type = type;
        return node;
    }

    if (match(parser, VC_TOKEN_IDENTIFIER) ||
        match(parser, VC_TOKEN_KW_THIS) ||
        match(parser, VC_TOKEN_KW_BASE))
    {
        VcAstNode *node = new_node(parser, VC_AST_IDENTIFIER_EXPRESSION, token->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.identifier_expression.name = copy_token_text(parser, token);
        return node;
    }

    VcAstLiteralKind literal_kind;
    bool literal = true;
    switch (current_kind(parser))
    {
        case VC_TOKEN_NUMBER: literal_kind = VC_AST_LITERAL_NUMBER; break;
        case VC_TOKEN_STRING: literal_kind = VC_AST_LITERAL_STRING; break;
        case VC_TOKEN_CHARACTER: literal_kind = VC_AST_LITERAL_CHARACTER; break;
        case VC_TOKEN_KW_TRUE: literal_kind = VC_AST_LITERAL_TRUE; break;
        case VC_TOKEN_KW_FALSE: literal_kind = VC_AST_LITERAL_FALSE; break;
        case VC_TOKEN_KW_NULL: literal_kind = VC_AST_LITERAL_NULL; break;
        default: literal = false; literal_kind = VC_AST_LITERAL_NULL; break;
    }

    if (literal)
    {
        advance_token(parser);
        VcAstNode *node = new_node(parser, VC_AST_LITERAL_EXPRESSION, token->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.literal_expression.literal_kind = literal_kind;
        node->as.literal_expression.text = copy_token_text(parser, token);
        return node;
    }

    if (match(parser, VC_TOKEN_LEFT_PAREN))
    {
        VcAstNode *expression = parse_expression(parser);
        if (expression == NULL)
            return NULL;
        if (consume(parser, VC_TOKEN_RIGHT_PAREN, "')'") == NULL)
            return NULL;

        VcAstNode *node = new_node(parser, VC_AST_PARENTHESIZED_EXPRESSION, token->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.parenthesized_expression.expression = expression;
        return node;
    }

    if (match(parser, VC_TOKEN_KW_NEW))
    {
        VcAstNode *node = new_node(parser, VC_AST_NEW_EXPRESSION, token->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }

        if (check(parser, VC_TOKEN_LEFT_PAREN))
            node->as.new_expression.target_typed = true;
        else if (check(parser, VC_TOKEN_LEFT_BRACKET) &&
                 (token_at(parser, parser->current + 1)->kind == VC_TOKEN_RIGHT_BRACKET ||
                  token_at(parser, parser->current + 1)->kind == VC_TOKEN_COMMA))
        {
            advance_token(parser);
            size_t rank = 1;
            while (match(parser, VC_TOKEN_COMMA))
                rank++;
            if (consume(parser, VC_TOKEN_RIGHT_BRACKET, "']'") == NULL)
                return NULL;

            node->as.new_expression.target_typed = true;
            node->as.new_expression.is_array = true;
            node->as.new_expression.implicit_array_rank = rank;
            if (rank > 1)
            {
                if (!parse_rectangular_initializer(parser, node))
                    return NULL;
            }
            else if (!parse_initializer(parser, node))
                return NULL;
            return node;
        }
        else
        {
            node->as.new_expression.type = parse_type(parser, false);
            if (node->as.new_expression.type == NULL)
                return NULL;
        }

        if (!node->as.new_expression.target_typed && match(parser, VC_TOKEN_LEFT_BRACKET))
        {
            if (node->as.new_expression.type != NULL &&
                (node->as.new_expression.type->array_rank != 0 ||
                 node->as.new_expression.type->rectangular_rank != 0))
            {
                parser_error(parser,
                    "sized array dimension must appear before jagged array suffixes");
                return NULL;
            }
            node->as.new_expression.is_array = true;
            do
            {
                VcAstNode *length = parse_expression(parser);
                if (length == NULL)
                    return NULL;
                if (node->as.new_expression.array_length == NULL)
                    node->as.new_expression.array_length = length;
                if (!vc_ast_node_list_push(parser->tree,
                        &node->as.new_expression.array_lengths, length))
                {
                    out_of_memory(parser);
                    return NULL;
                }
            } while (match(parser, VC_TOKEN_COMMA));
            if (consume(parser, VC_TOKEN_RIGHT_BRACKET, "']'") == NULL)
                return NULL;
            if (node->as.new_expression.array_lengths.count > 1)
                node->as.new_expression.type->rectangular_rank =
                    node->as.new_expression.array_lengths.count;
            if (node->as.new_expression.array_lengths.count > 1 &&
                check_actual(parser, VC_TOKEN_LEFT_BRACKET) &&
                token_at(parser, parser->current + 1)->kind == VC_TOKEN_RIGHT_BRACKET)
            {
                parser_error(parser,
                    "jagged suffixes after sized rectangular dimensions are not supported");
                return NULL;
            }
            while (check_actual(parser, VC_TOKEN_LEFT_BRACKET) &&
                   token_at(parser, parser->current + 1)->kind == VC_TOKEN_RIGHT_BRACKET)
            {
                advance_token(parser);
                advance_token(parser);
                node->as.new_expression.type->array_rank++;
            }
            if (node->as.new_expression.type->rectangular_rank > 1 &&
                node->as.new_expression.type->array_rank == 0 &&
                check(parser, VC_TOKEN_LEFT_BRACE))
            {
                if (!parse_rectangular_initializer(parser, node))
                    return NULL;
            }
            return node;
        }

        if (!node->as.new_expression.target_typed &&
            node->as.new_expression.type != NULL &&
            (node->as.new_expression.type->array_rank > 0 ||
             node->as.new_expression.type->rectangular_rank > 0) &&
            check(parser, VC_TOKEN_LEFT_BRACE))
        {
            node->as.new_expression.is_array = true;
            if (node->as.new_expression.type->rectangular_rank > 1 &&
                node->as.new_expression.type->array_rank == 0)
            {
                if (!parse_rectangular_initializer(parser, node))
                    return NULL;
            }
            else if (!parse_initializer(parser, node))
                return NULL;
            return node;
        }

        if (check(parser, VC_TOKEN_LEFT_BRACE) && !node->as.new_expression.target_typed)
        {
            if (!parse_initializer(parser, node))
                return NULL;
            return node;
        }

        if (consume(parser, VC_TOKEN_LEFT_PAREN, "'('") == NULL)
            return NULL;

        if (!check(parser, VC_TOKEN_RIGHT_PAREN))
        {
            do
            {
                VcAstNode *argument = parse_argument(parser);
                if (argument == NULL)
                    return NULL;
                if (!vc_ast_node_list_push(parser->tree, &node->as.new_expression.arguments, argument))
                {
                    out_of_memory(parser);
                    return NULL;
                }
            } while (match(parser, VC_TOKEN_COMMA));
        }

        if (!finish_list(parser, VC_TOKEN_RIGHT_PAREN, "')'"))
            return NULL;
        if (!parse_initializer(parser, node))
            return NULL;
        return node;
    }

    parser_error(parser, "expected expression, found '%s'", vc_token_kind_name(current_kind(parser)));
    vc_diagnostic_set_code(&parser->result->diagnostic, VC_DIAG_EXPECTED_TOKEN);
    return NULL;
}

typedef struct VcGenericCallScan
{
    size_t position;
    size_t pending_greater;
} VcGenericCallScan;

static VcTokenKind generic_call_scan_kind(
    const VcParser *parser,
    const VcGenericCallScan *scan)
{
    return scan->pending_greater != 0
        ? VC_TOKEN_GREATER
        : token_at(parser, scan->position)->kind;
}

static void generic_call_scan_advance(VcGenericCallScan *scan)
{
    if (scan->pending_greater != 0)
        scan->pending_greater--;
    else
        scan->position++;
}

static bool generic_call_scan_greater(
    const VcParser *parser,
    VcGenericCallScan *scan)
{
    if (scan->pending_greater != 0)
    {
        scan->pending_greater--;
        return true;
    }
    const VcTokenKind kind = token_at(parser, scan->position)->kind;
    if (kind == VC_TOKEN_GREATER)
    {
        scan->position++;
        return true;
    }
    if (kind == VC_TOKEN_GREATER_GREATER)
    {
        scan->position++;
        scan->pending_greater = 1;
        return true;
    }
    return false;
}

static bool scan_generic_call_type(
    const VcParser *parser,
    VcGenericCallScan *scan,
    bool allow_void)
{
    VcTokenKind kind = generic_call_scan_kind(parser, scan);
    const bool pointer_void = scan->pending_greater == 0 && kind == VC_TOKEN_KW_VOID &&
        token_at(parser, scan->position + 1)->kind == VC_TOKEN_STAR;
    const bool function_pointer = scan->pending_greater == 0 && kind == VC_TOKEN_KW_DELEGATE &&
        token_at(parser, scan->position + 1)->kind == VC_TOKEN_STAR &&
        token_at(parser, scan->position + 2)->kind == VC_TOKEN_LESS;

    if (function_pointer)
    {
        scan->position += 3;
        if (generic_call_scan_kind(parser, scan) == VC_TOKEN_GREATER)
            return false;
        if (!scan_generic_call_type(parser, scan, true))
            return false;
        while (generic_call_scan_kind(parser, scan) == VC_TOKEN_COMMA)
        {
            generic_call_scan_advance(scan);
            if (!scan_generic_call_type(parser, scan, true))
                return false;
        }
        if (!generic_call_scan_greater(parser, scan))
            return false;
    }
    else
    {
        if (!is_identifier_token(kind) && !is_type_keyword(kind, allow_void) && !pointer_void)
            return false;
        generic_call_scan_advance(scan);

        while (scan->pending_greater == 0 &&
               token_at(parser, scan->position)->kind == VC_TOKEN_DOT &&
               token_at(parser, scan->position + 1)->kind == VC_TOKEN_IDENTIFIER)
            scan->position += 2;

        if (generic_call_scan_kind(parser, scan) == VC_TOKEN_LESS)
        {
            generic_call_scan_advance(scan);
            if (!scan_generic_call_type(parser, scan, false))
                return false;
            while (generic_call_scan_kind(parser, scan) == VC_TOKEN_COMMA)
            {
                generic_call_scan_advance(scan);
                if (!scan_generic_call_type(parser, scan, false))
                    return false;
            }
            if (!generic_call_scan_greater(parser, scan))
                return false;
        }
    }

    if (generic_call_scan_kind(parser, scan) == VC_TOKEN_QUESTION)
        generic_call_scan_advance(scan);

    while (generic_call_scan_kind(parser, scan) == VC_TOKEN_STAR)
        generic_call_scan_advance(scan);

    while (scan->pending_greater == 0 &&
           token_at(parser, scan->position)->kind == VC_TOKEN_LEFT_BRACKET)
    {
        size_t position = scan->position + 1;
        while (token_at(parser, position)->kind == VC_TOKEN_COMMA)
            position++;
        if (token_at(parser, position)->kind != VC_TOKEN_RIGHT_BRACKET)
            break;
        scan->position = position + 1;
    }

    return true;
}

static bool looks_like_generic_suffix(const VcParser *parser, VcTokenKind following)
{
    if (current_kind(parser) != VC_TOKEN_LESS)
        return false;

    VcGenericCallScan scan = {parser->current + 1, 0};
    if (!scan_generic_call_type(parser, &scan, false))
        return false;
    while (generic_call_scan_kind(parser, &scan) == VC_TOKEN_COMMA)
    {
        generic_call_scan_advance(&scan);
        if (!scan_generic_call_type(parser, &scan, false))
            return false;
    }
    if (!generic_call_scan_greater(parser, &scan) || scan.pending_greater != 0)
        return false;
    return token_at(parser, scan.position)->kind == following;
}

static bool expression_has_null_conditional(const VcAstNode *expression)
{
    if (expression == NULL)
        return false;
    if (expression->kind == VC_AST_MEMBER_ACCESS_EXPRESSION)
        return expression->as.member_access_expression.null_conditional;
    if (expression->kind == VC_AST_INDEX_EXPRESSION)
        return expression->as.index_expression.null_conditional;
    if (expression->kind == VC_AST_CALL_EXPRESSION)
        return expression_has_null_conditional(expression->as.call_expression.callee);
    return false;
}

static VcAstNode *parse_postfix(VcParser *parser)
{
    const VcSourceLocation receiver_start = current_token(parser)->span.start;
    VcAstNode *expression = parse_primary(parser);
    if (expression == NULL)
        return NULL;

    for (;;)
    {
        if (check(parser, VC_TOKEN_DOT) || check(parser, VC_TOKEN_QUESTION_DOT))
        {
            const bool null_conditional = match(parser, VC_TOKEN_QUESTION_DOT);
            if (!null_conditional)
                advance_token(parser);
            const VcToken *member = consume(parser, VC_TOKEN_IDENTIFIER, "member name");
            if (member == NULL)
                return NULL;

            VcAstNode *node = new_node(parser, VC_AST_MEMBER_ACCESS_EXPRESSION, member->location);
            if (node == NULL)
            {
                out_of_memory(parser);
                return NULL;
            }
            node->as.member_access_expression.target = expression;
            node->as.member_access_expression.member = copy_token_text(parser, member);
            node->as.member_access_expression.null_conditional_direct = null_conditional;
            node->as.member_access_expression.null_conditional = null_conditional ||
                expression_has_null_conditional(expression);
            expression = node;
            continue;
        }

        char receiver_name[512];
        if (looks_like_generic_suffix(parser, VC_TOKEN_DOT) &&
            vc_ast_receiver_name(expression, receiver_name, sizeof(receiver_name)))
        {
            VcAstNode *receiver = new_node(parser, VC_AST_TYPE_RECEIVER_EXPRESSION, receiver_start);
            VcAstTypeRef *type = new_type(parser, receiver_start);
            if (receiver == NULL || type == NULL) { out_of_memory(parser); return NULL; }
            type->name = vc_ast_copy_text(parser->tree, receiver_name, strlen(receiver_name));
            if (type->name == NULL) { out_of_memory(parser); return NULL; }
            advance_token(parser);
            do
            {
                VcAstTypeRef *argument = parse_type(parser, false);
                if (argument == NULL) return NULL;
                if (!vc_ast_type_list_push(parser->tree, &type->generic_arguments, argument))
                { out_of_memory(parser); return NULL; }
            } while (match(parser, VC_TOKEN_COMMA));
            if (!consume_type_greater(parser)) return NULL;
            type->span.start = receiver_start;
            type->span.end = token_at(parser, parser->current - 1)->span.end;
            receiver->span = type->span;
            receiver->receiver_type = type;
            expression = receiver;
            continue;
        }

        VcAstTypeList call_generic_arguments = {0};
        if (looks_like_generic_suffix(parser, VC_TOKEN_LEFT_PAREN))
        {
            advance_token(parser);
            do
            {
                VcAstTypeRef *argument = parse_type(parser, false);
                if (argument == NULL)
                    return NULL;
                if (!vc_ast_type_list_push(parser->tree, &call_generic_arguments, argument))
                {
                    out_of_memory(parser);
                    return NULL;
                }
            } while (match(parser, VC_TOKEN_COMMA));
            if (!consume_type_greater(parser))
                return NULL;
        }

        if (match(parser, VC_TOKEN_LEFT_PAREN))
        {
            VcAstNode *node = new_node(parser, VC_AST_CALL_EXPRESSION, expression->location);
            if (node == NULL)
            {
                out_of_memory(parser);
                return NULL;
            }
            node->as.call_expression.callee = expression;
            node->as.call_expression.generic_arguments = call_generic_arguments;

            if (!check(parser, VC_TOKEN_RIGHT_PAREN))
            {
                do
                {
                    VcAstNode *argument = parse_argument(parser);
                    if (argument == NULL)
                        return NULL;
                    if (!vc_ast_node_list_push(parser->tree, &node->as.call_expression.arguments, argument))
                    {
                        out_of_memory(parser);
                        return NULL;
                    }
                } while (match(parser, VC_TOKEN_COMMA));
            }

            if (!finish_list(parser, VC_TOKEN_RIGHT_PAREN, "')'"))
                return NULL;
            expression = node;
            continue;
        }

        if (call_generic_arguments.count != 0)
        {
            parser_error(parser, "generic arguments must be followed by a call");
            return NULL;
        }

        bool null_conditional_index = false;
        bool has_index = false;
        if (check(parser, VC_TOKEN_QUESTION) &&
            token_at(parser, parser->current + 1)->kind == VC_TOKEN_LEFT_BRACKET)
        {
            advance_token(parser);
            advance_token(parser);
            null_conditional_index = true;
            has_index = true;
        }
        else if (match(parser, VC_TOKEN_LEFT_BRACKET))
            has_index = true;

        if (has_index)
        {
            VcAstNode *node = new_node(parser, VC_AST_INDEX_EXPRESSION, expression->location);
            if (node == NULL)
            {
                out_of_memory(parser);
                return NULL;
            }
            VcAstNode *index = NULL;
            do
            {
                VcAstNode *item = parse_expression(parser);
                if (item == NULL)
                    return NULL;
                if (index == NULL)
                    index = item;
                if (!vc_ast_node_list_push(parser->tree, &node->as.index_expression.indices, item))
                {
                    out_of_memory(parser);
                    return NULL;
                }
            } while (match(parser, VC_TOKEN_COMMA));
            if (consume(parser, VC_TOKEN_RIGHT_BRACKET, "']'") == NULL)
                return NULL;

            node->as.index_expression.target = expression;
            node->as.index_expression.index = index;
            node->as.index_expression.null_conditional_direct = null_conditional_index;
            node->as.index_expression.null_conditional = null_conditional_index ||
                expression_has_null_conditional(expression);
            expression = node;
            continue;
        }

        if (check(parser, VC_TOKEN_PLUS_PLUS) || check(parser, VC_TOKEN_MINUS_MINUS))
        {
            const VcToken *operator_token = advance_token(parser);
            VcAstNode *node = new_node(parser, VC_AST_UNARY_EXPRESSION, operator_token->location);
            if (node == NULL)
            {
                out_of_memory(parser);
                return NULL;
            }
            node->as.unary_expression.operator_kind = operator_token->kind;
            node->as.unary_expression.operand = expression;
            node->as.unary_expression.postfix = true;
            expression = node;
            continue;
        }

        break;
    }

    return expression;
}

static bool contextual_await_expression(const VcParser *parser)
{
    if (current_kind(parser) != VC_TOKEN_IDENTIFIER ||
        !token_text_equals(parser, current_token(parser), "await"))
        return false;
    if (parser->async_method_depth != 0)
        return true;

    switch (token_at(parser, parser->current + 1)->kind)
    {
        case VC_TOKEN_IDENTIFIER:
        case VC_TOKEN_NUMBER:
        case VC_TOKEN_STRING:
        case VC_TOKEN_CHARACTER:
        case VC_TOKEN_INTERPOLATED_START:
        case VC_TOKEN_KW_THIS:
        case VC_TOKEN_KW_BASE:
        case VC_TOKEN_KW_TRUE:
        case VC_TOKEN_KW_FALSE:
        case VC_TOKEN_KW_NULL:
        case VC_TOKEN_KW_NEW:
        case VC_TOKEN_KW_DEFAULT:
        case VC_TOKEN_KW_TYPEOF:
        case VC_TOKEN_KW_SIZEOF:
        case VC_TOKEN_KW_STACKALLOC:
            return true;
        default:
            return false;
    }
}

static VcAstNode *parse_unary(VcParser *parser)
{
    if (contextual_await_expression(parser))
    {
        const VcToken *await_token = advance_token(parser);
        VcAstNode *operand = parse_unary(parser);
        if (operand == NULL)
            return NULL;

        VcAstNode *node = new_node(parser, VC_AST_AWAIT_EXPRESSION, await_token->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.await_expression.operand = operand;
        return node;
    }
    if (looks_like_cast(parser))
    {
        const VcToken *start = advance_token(parser);
        VcAstTypeRef *type = parse_type(parser, false);
        if (type == NULL || consume(parser, VC_TOKEN_RIGHT_PAREN, "')'") == NULL)
            return NULL;

        VcAstNode *operand = parse_unary(parser);
        if (operand == NULL)
            return NULL;

        VcAstNode *node = new_node(parser, VC_AST_CAST_EXPRESSION, start->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.cast_expression.type = type;
        node->as.cast_expression.expression = operand;
        return node;
    }

    if (is_prefix_operator(current_kind(parser)))
    {
        const VcToken *operator_token = advance_token(parser);
        VcAstNode *operand = parse_unary(parser);
        if (operand == NULL)
            return NULL;

        VcAstNode *node = new_node(parser, VC_AST_UNARY_EXPRESSION, operator_token->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.unary_expression.operator_kind = operator_token->kind;
        node->as.unary_expression.operand = operand;
        return node;
    }

    return parse_postfix(parser);
}


static bool current_contextual_pattern_word(const VcParser *parser, const char *word)
{
    return current_kind(parser) == VC_TOKEN_IDENTIFIER &&
        token_text_equals(parser, current_token(parser), word);
}

static VcAstNode *new_pattern_leaf(VcParser *parser, VcSourceLocation location)
{
    VcAstNode *node = new_node(parser, VC_AST_TYPE_RELATION_EXPRESSION, location);
    if (node == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }
    node->as.type_relation_expression.operator_kind = VC_TOKEN_KW_IS;
    return node;
}

static VcAstNode *parse_pattern_or(VcParser *parser);

static VcAstNode *parse_pattern_primary(VcParser *parser)
{
    if (match(parser, VC_TOKEN_LEFT_PAREN))
    {
        VcAstNode *pattern = parse_pattern_or(parser);
        if (pattern == NULL || consume(parser, VC_TOKEN_RIGHT_PAREN, "')' after pattern") == NULL)
            return NULL;
        return pattern;
    }

    const VcSourceLocation location = current_token(parser)->location;
    VcAstNode *node = new_pattern_leaf(parser, location);
    if (node == NULL)
        return NULL;

    if (match(parser, VC_TOKEN_LEFT_BRACE))
    {
        node->as.type_relation_expression.pattern_property = true;
        if (!check(parser, VC_TOKEN_RIGHT_BRACE))
        {
            for (;;)
            {
                const VcToken *name = consume(parser, VC_TOKEN_IDENTIFIER,
                    "property name in property pattern");
                if (name == NULL || consume(parser, VC_TOKEN_COLON,
                        "':' after property pattern member name") == NULL)
                    return NULL;
                VcAstNode *member = new_node(parser, VC_AST_PROPERTY_PATTERN_MEMBER,
                    name->location);
                if (member == NULL)
                {
                    out_of_memory(parser);
                    return NULL;
                }
                member->as.property_pattern_member.name = copy_token_text(parser, name);
                member->as.property_pattern_member.pattern = parse_pattern_or(parser);
                if (member->as.property_pattern_member.name == NULL ||
                    member->as.property_pattern_member.pattern == NULL ||
                    !vc_ast_node_list_push(parser->tree,
                        &node->as.type_relation_expression.pattern_properties, member))
                {
                    if (member->as.property_pattern_member.name == NULL)
                        out_of_memory(parser);
                    return NULL;
                }
                if (!match(parser, VC_TOKEN_COMMA))
                    break;
                if (check(parser, VC_TOKEN_RIGHT_BRACE))
                    break;
            }
        }
        if (consume(parser, VC_TOKEN_RIGHT_BRACE, "'}' after property pattern") == NULL)
            return NULL;
        return node;
    }

    VcTokenKind next = current_kind(parser);
    if (next == VC_TOKEN_LESS || next == VC_TOKEN_LESS_EQUAL ||
        next == VC_TOKEN_GREATER || next == VC_TOKEN_GREATER_EQUAL)
    {
        node->as.type_relation_expression.pattern_operator_kind = advance_token(parser)->kind;
        next = current_kind(parser);
        const bool literal = next == VC_TOKEN_NUMBER || next == VC_TOKEN_STRING ||
            next == VC_TOKEN_CHARACTER || next == VC_TOKEN_KW_TRUE ||
            next == VC_TOKEN_KW_FALSE || next == VC_TOKEN_KW_NULL ||
            ((next == VC_TOKEN_PLUS || next == VC_TOKEN_MINUS) &&
             token_at(parser, parser->current + 1)->kind == VC_TOKEN_NUMBER);
        if (!literal)
        {
            parser_error(parser, "expected constant after relational pattern operator");
            return NULL;
        }
        node->as.type_relation_expression.pattern_constant = parse_unary(parser);
        return node->as.type_relation_expression.pattern_constant != NULL ? node : NULL;
    }

    const bool literal = next == VC_TOKEN_NUMBER || next == VC_TOKEN_STRING ||
        next == VC_TOKEN_CHARACTER || next == VC_TOKEN_KW_TRUE ||
        next == VC_TOKEN_KW_FALSE || next == VC_TOKEN_KW_NULL ||
        ((next == VC_TOKEN_PLUS || next == VC_TOKEN_MINUS) &&
         token_at(parser, parser->current + 1)->kind == VC_TOKEN_NUMBER);
    if (literal)
    {
        node->as.type_relation_expression.pattern_constant = parse_unary(parser);
        return node->as.type_relation_expression.pattern_constant != NULL ? node : NULL;
    }

    node->as.type_relation_expression.type = parse_type(parser, false);
    if (node->as.type_relation_expression.type == NULL)
        return NULL;
    if (check(parser, VC_TOKEN_IDENTIFIER) &&
        !current_contextual_pattern_word(parser, "and") &&
        !current_contextual_pattern_word(parser, "or") &&
        !current_contextual_pattern_word(parser, "when"))
        node->as.type_relation_expression.pattern_name = copy_token_text(parser, advance_token(parser));
    return node;
}

static VcAstNode *parse_pattern_not(VcParser *parser)
{
    if (!current_contextual_pattern_word(parser, "not"))
        return parse_pattern_primary(parser);
    const VcSourceLocation location = advance_token(parser)->location;
    VcAstNode *child = parse_pattern_not(parser);
    if (child == NULL)
        return NULL;
    VcAstNode *node = new_pattern_leaf(parser, location);
    if (node == NULL)
        return NULL;
    node->as.type_relation_expression.pattern_logical_kind = VC_TOKEN_BANG;
    node->as.type_relation_expression.pattern_left = child;
    return node;
}

static VcAstNode *parse_pattern_and(VcParser *parser)
{
    VcAstNode *left = parse_pattern_not(parser);
    if (left == NULL)
        return NULL;
    while (current_contextual_pattern_word(parser, "and"))
    {
        const VcSourceLocation location = advance_token(parser)->location;
        VcAstNode *right = parse_pattern_not(parser);
        if (right == NULL)
            return NULL;
        VcAstNode *node = new_pattern_leaf(parser, location);
        if (node == NULL)
            return NULL;
        node->as.type_relation_expression.pattern_logical_kind = VC_TOKEN_AMPERSAND_AMPERSAND;
        node->as.type_relation_expression.pattern_left = left;
        node->as.type_relation_expression.pattern_right = right;
        left = node;
    }
    return left;
}

static VcAstNode *parse_pattern_or(VcParser *parser)
{
    VcAstNode *left = parse_pattern_and(parser);
    if (left == NULL)
        return NULL;
    while (current_contextual_pattern_word(parser, "or"))
    {
        const VcSourceLocation location = advance_token(parser)->location;
        VcAstNode *right = parse_pattern_and(parser);
        if (right == NULL)
            return NULL;
        VcAstNode *node = new_pattern_leaf(parser, location);
        if (node == NULL)
            return NULL;
        node->as.type_relation_expression.pattern_logical_kind = VC_TOKEN_PIPE_PIPE;
        node->as.type_relation_expression.pattern_left = left;
        node->as.type_relation_expression.pattern_right = right;
        left = node;
    }
    return left;
}

static VcAstNode *parse_binary(VcParser *parser, int minimum_precedence)
{
    VcAstNode *left = parse_unary(parser);
    if (left == NULL)
        return NULL;

    for (;;)
    {
        const VcTokenKind current = current_kind(parser);
        const bool type_relation = current == VC_TOKEN_KW_IS || current == VC_TOKEN_KW_AS;
        const int precedence = type_relation ? 8 : binary_precedence(current);
        if (precedence < minimum_precedence || precedence == 0)
            break;

        const VcToken *operator_token = advance_token(parser);
        if (type_relation)
        {
            if (operator_token->kind == VC_TOKEN_KW_IS)
            {
                VcAstNode *pattern = parse_pattern_or(parser);
                if (pattern == NULL)
                    return NULL;
                pattern->as.type_relation_expression.expression = left;
                left = pattern;
                continue;
            }

            VcAstTypeRef *type = parse_type(parser, false);
            if (type == NULL)
                return NULL;
            VcAstNode *node = new_node(parser,
                VC_AST_TYPE_RELATION_EXPRESSION, operator_token->location);
            if (node == NULL)
            {
                out_of_memory(parser);
                return NULL;
            }
            node->as.type_relation_expression.operator_kind = operator_token->kind;
            node->as.type_relation_expression.expression = left;
            node->as.type_relation_expression.type = type;
            left = node;
            continue;
        }

        VcAstNode *right = parse_binary(parser,
            current == VC_TOKEN_QUESTION_QUESTION ? precedence : precedence + 1);
        if (right == NULL)
            return NULL;

        VcAstNode *node = new_node(parser, VC_AST_BINARY_EXPRESSION, operator_token->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.binary_expression.operator_kind = operator_token->kind;
        node->as.binary_expression.left = left;
        node->as.binary_expression.right = right;
        left = node;
    }

    return left;
}

static VcAstNode *synthetic_int_zero(VcParser *parser, VcSourceLocation location)
{
    VcAstNode *node = new_node(parser, VC_AST_LITERAL_EXPRESSION, location);
    if (node == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }
    node->as.literal_expression.literal_kind = VC_AST_LITERAL_NUMBER;
    node->as.literal_expression.text = vc_ast_copy_text(parser->tree, "0", 1);
    if (node->as.literal_expression.text == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }
    return node;
}

static VcAstNode *synthetic_from_end_zero(VcParser *parser, VcSourceLocation location)
{
    VcAstNode *operand = synthetic_int_zero(parser, location);
    if (operand == NULL)
        return NULL;

    VcAstNode *node = new_node(parser, VC_AST_UNARY_EXPRESSION, location);
    if (node == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }
    node->as.unary_expression.operator_kind = VC_TOKEN_CARET;
    node->as.unary_expression.operand = operand;
    node->as.unary_expression.postfix = false;
    return node;
}

static bool range_endpoint_is_omitted(const VcParser *parser)
{
    switch (current_kind(parser))
    {
        case VC_TOKEN_EOF:
        case VC_TOKEN_COMMA:
        case VC_TOKEN_SEMICOLON:
        case VC_TOKEN_RIGHT_PAREN:
        case VC_TOKEN_RIGHT_BRACKET:
        case VC_TOKEN_RIGHT_BRACE:
        case VC_TOKEN_COLON:
        case VC_TOKEN_QUESTION:
        case VC_TOKEN_FAT_ARROW:
            return true;
        default:
            return false;
    }
}

static VcAstNode *parse_range(VcParser *parser)
{
    VcAstNode *start = NULL;
    VcAstNode *end = NULL;
    const VcToken *range_token = NULL;

    if (match(parser, VC_TOKEN_DOT_DOT))
    {
        range_token = previous_token(parser);
        start = synthetic_int_zero(parser, range_token->location);
        if (start == NULL)
            return NULL;
    }
    else
    {
        start = parse_binary(parser, 1);
        if (start == NULL)
            return NULL;
        if (!match(parser, VC_TOKEN_DOT_DOT))
            return start;
        range_token = previous_token(parser);
    }

    if (range_endpoint_is_omitted(parser))
        end = synthetic_from_end_zero(parser, range_token->location);
    else
        end = parse_binary(parser, 1);
    if (end == NULL)
        return NULL;

    if (check(parser, VC_TOKEN_DOT_DOT))
    {
        parser_error(parser, "range operator '..' cannot be chained");
        return NULL;
    }

    VcAstNode *node = new_node(parser, VC_AST_RANGE_EXPRESSION, range_token->location);
    if (node == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }
    node->as.range_expression.start = start;
    node->as.range_expression.end = end;
    return node;
}

static bool looks_like_lambda_at(const VcParser *parser, size_t start)
{
    if (token_at(parser, start)->kind == VC_TOKEN_IDENTIFIER &&
        token_at(parser, start + 1)->kind == VC_TOKEN_FAT_ARROW)
        return true;

    if (token_at(parser, start)->kind != VC_TOKEN_LEFT_PAREN)
        return false;

    size_t position = start + 1;
    if (token_at(parser, position)->kind == VC_TOKEN_RIGHT_PAREN)
        return token_at(parser, position + 1)->kind == VC_TOKEN_FAT_ARROW;

    for (;;)
    {
        if (token_at(parser, position)->kind != VC_TOKEN_IDENTIFIER)
            return false;
        position++;
        if (token_at(parser, position)->kind == VC_TOKEN_COMMA)
        {
            position++;
            continue;
        }
        if (token_at(parser, position)->kind != VC_TOKEN_RIGHT_PAREN)
            return false;
        return token_at(parser, position + 1)->kind == VC_TOKEN_FAT_ARROW;
    }
}

static bool looks_like_lambda(const VcParser *parser)
{
    if (looks_like_lambda_at(parser, parser->current))
        return true;

    return current_kind(parser) == VC_TOKEN_IDENTIFIER &&
        token_text_equals(parser, current_token(parser), "async") &&
        token_at(parser, parser->current + 1)->kind != VC_TOKEN_FAT_ARROW &&
        looks_like_lambda_at(parser, parser->current + 1);
}

static VcAstNode *parse_lambda(VcParser *parser)
{
    const VcToken *start = current_token(parser);
    const bool is_async = current_kind(parser) == VC_TOKEN_IDENTIFIER &&
        token_text_equals(parser, current_token(parser), "async") &&
        token_at(parser, parser->current + 1)->kind != VC_TOKEN_FAT_ARROW &&
        looks_like_lambda_at(parser, parser->current + 1);
    if (is_async)
        advance_token(parser);

    VcAstNode *node = new_node(parser, VC_AST_LAMBDA_EXPRESSION, start->location);
    if (node == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }

    if (match(parser, VC_TOKEN_IDENTIFIER))
    {
        char *name = copy_token_text(parser, previous_token(parser));
        if (name == NULL || !vc_ast_string_list_push(parser->tree, &node->as.lambda_expression.parameters, name))
        {
            out_of_memory(parser);
            return NULL;
        }
    }
    else
    {
        if (consume(parser, VC_TOKEN_LEFT_PAREN, "'('") == NULL)
            return NULL;
        if (!check(parser, VC_TOKEN_RIGHT_PAREN))
        {
            do
            {
                const VcToken *parameter = consume(parser, VC_TOKEN_IDENTIFIER, "lambda parameter name");
                if (parameter == NULL)
                    return NULL;
                char *name = copy_token_text(parser, parameter);
                if (name == NULL || !vc_ast_string_list_push(parser->tree, &node->as.lambda_expression.parameters, name))
                {
                    out_of_memory(parser);
                    return NULL;
                }
            } while (match(parser, VC_TOKEN_COMMA));
        }
        if (consume(parser, VC_TOKEN_RIGHT_PAREN, "')'") == NULL)
            return NULL;
    }

    if (consume(parser, VC_TOKEN_FAT_ARROW, "'=>'") == NULL)
        return NULL;

    node->as.lambda_expression.is_async = is_async;
    if (is_async)
        parser->async_method_depth++;
    if (check(parser, VC_TOKEN_LEFT_BRACE))
    {
        node->as.lambda_expression.body = parse_block(parser);
        node->as.lambda_expression.expression_body = false;
    }
    else
    {
        node->as.lambda_expression.body = parse_expression(parser);
        node->as.lambda_expression.expression_body = true;
    }
    if (is_async)
        parser->async_method_depth--;
    return node->as.lambda_expression.body != NULL ? node : NULL;
}

static VcAstNode *parse_switch_expression(VcParser *parser, VcAstNode *governing, const VcToken *keyword)
{
    VcAstNode *node = new_node(parser, VC_AST_SWITCH_EXPRESSION, keyword->location);
    if (node == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }
    node->as.switch_expression.expression = governing;
    if (consume(parser, VC_TOKEN_LEFT_BRACE, "'{' after switch expression") == NULL)
        return NULL;
    if (check(parser, VC_TOKEN_RIGHT_BRACE))
    {
        parser_error(parser, "switch expression requires at least one arm");
        return NULL;
    }
    for (;;)
    {
        const VcToken *start = current_token(parser);
        VcAstNode *arm = new_node(parser, VC_AST_SWITCH_EXPRESSION_ARM, start->location);
        if (arm == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        if (current_kind(parser) == VC_TOKEN_IDENTIFIER && token_text_equals(parser, current_token(parser), "_") )
        {
            advance_token(parser);
            arm->as.switch_expression_arm.is_discard = true;
        }
        else
        {
            arm->as.switch_expression_arm.pattern = parse_pattern_or(parser);
            if (arm->as.switch_expression_arm.pattern == NULL)
                return NULL;
        }
        if (current_contextual_pattern_word(parser, "when"))
        {
            advance_token(parser);
            arm->as.switch_expression_arm.guard = parse_expression(parser);
            if (arm->as.switch_expression_arm.guard == NULL)
                return NULL;
        }
        if (consume(parser, VC_TOKEN_FAT_ARROW, "'=>' in switch expression arm") == NULL)
            return NULL;
        arm->as.switch_expression_arm.result = parse_expression(parser);
        if (arm->as.switch_expression_arm.result == NULL ||
            !vc_ast_node_list_push(parser->tree, &node->as.switch_expression.arms, arm))
        {
            if (arm->as.switch_expression_arm.result != NULL)
                out_of_memory(parser);
            return NULL;
        }
        if (!match(parser, VC_TOKEN_COMMA))
            break;
        if (check(parser, VC_TOKEN_RIGHT_BRACE))
            break;
    }
    if (consume(parser, VC_TOKEN_RIGHT_BRACE, "'}' after switch expression") == NULL)
        return NULL;
    return node;
}

static VcAstNode *parse_expression_core(VcParser *parser)
{
    if (looks_like_lambda(parser))
        return parse_lambda(parser);

    VcAstNode *left = parse_range(parser);
    if (left == NULL)
        return NULL;

    if (match(parser, VC_TOKEN_KW_SWITCH))
    {
        left = parse_switch_expression(parser, left, previous_token(parser));
        if (left == NULL)
            return NULL;
    }

    if (match(parser, VC_TOKEN_QUESTION))
    {
        const VcToken *question = previous_token(parser);
        VcAstNode *when_true = parse_expression(parser);
        if (when_true == NULL)
            return NULL;
        if (consume(parser, VC_TOKEN_COLON, "':'") == NULL)
            return NULL;
        VcAstNode *when_false = parse_expression(parser);
        if (when_false == NULL)
            return NULL;

        VcAstNode *node = new_node(parser, VC_AST_CONDITIONAL_EXPRESSION, question->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.conditional_expression.condition = left;
        node->as.conditional_expression.when_true = when_true;
        node->as.conditional_expression.when_false = when_false;
        left = node;
    }

    if (is_assignment_operator(current_kind(parser)))
    {
        const VcToken *operator_token = advance_token(parser);
        const bool is_ref_assignment = operator_token->kind == VC_TOKEN_EQUAL &&
            match(parser, VC_TOKEN_KW_REF);
        VcAstNode *right = parse_expression(parser);
        if (right == NULL)
            return NULL;

        VcAstNode *node = new_node(parser, VC_AST_ASSIGNMENT_EXPRESSION, operator_token->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.assignment_expression.operator_kind = operator_token->kind;
        node->as.assignment_expression.is_ref = is_ref_assignment;
        node->as.assignment_expression.left = left;
        node->as.assignment_expression.right = right;
        return node;
    }

    return left;
}

static VcAstNode *parse_expression(VcParser *parser)
{
    VcSourceLocation start = current_token(parser)->span.start;
    VcAstNode *node = parse_expression_core(parser);
    if (node != NULL)
    {
        node->span.start = start;
        node->span.end = previous_token(parser)->span.end;
    }
    return node;
}

static bool parse_attribute_lists(VcParser *parser, VcAstNodeList *attributes)
{
    while (match(parser, VC_TOKEN_LEFT_BRACKET))
    {
        do
        {
            const VcToken *start = current_token(parser);
            char *name = parse_qualified_name(parser);
            if (name == NULL)
                return false;

            VcAstNode *attribute = new_node(parser, VC_AST_ATTRIBUTE, start->location);
            if (attribute == NULL)
            {
                out_of_memory(parser);
                return false;
            }
            attribute->as.attribute.name = name;

            if (match(parser, VC_TOKEN_LEFT_PAREN))
            {
                if (!check(parser, VC_TOKEN_RIGHT_PAREN))
                {
                    do
                    {
                        VcAstNode *argument = parse_expression(parser);
                        if (argument == NULL)
                            return false;
                        if (!vc_ast_node_list_push(parser->tree, &attribute->as.attribute.arguments, argument))
                        {
                            out_of_memory(parser);
                            return false;
                        }
                    } while (match(parser, VC_TOKEN_COMMA));
                }

                if (consume(parser, VC_TOKEN_RIGHT_PAREN, "')'") == NULL)
                    return false;
            }

            if (!vc_ast_node_list_push(parser->tree, attributes, attribute))
            {
                out_of_memory(parser);
                return false;
            }
        } while (match(parser, VC_TOKEN_COMMA));

        if (consume(parser, VC_TOKEN_RIGHT_BRACKET, "']'") == NULL)
            return false;
    }

    return true;
}

static VcAstNode *parse_statement(VcParser *parser);
static VcAstNode *parse_using_declaration(VcParser *parser, const VcToken *keyword, bool is_await);
static bool wrap_using_declarations(VcParser *parser, VcAstNodeList *statements);

static bool check_contextual_await_using(const VcParser *parser)
{
    return current_kind(parser) == VC_TOKEN_IDENTIFIER &&
        token_text_equals(parser, current_token(parser), "await") &&
        token_at(parser, parser->current + 1)->kind == VC_TOKEN_KW_USING;
}

static VcAstNode *parse_block(VcParser *parser)
{
    const VcToken *start = consume(parser, VC_TOKEN_LEFT_BRACE, "'{'");
    if (start == NULL)
        return NULL;

    VcAstNode *block = new_node(parser, VC_AST_BLOCK_STATEMENT, start->location);
    if (block == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }

    while (!check(parser, VC_TOKEN_RIGHT_BRACE) && !at_end(parser))
    {
        VcAstNode *statement = NULL;
        if ((check(parser, VC_TOKEN_KW_USING) &&
                token_at(parser, parser->current + 1)->kind != VC_TOKEN_LEFT_PAREN) ||
            (check_contextual_await_using(parser) &&
                token_at(parser, parser->current + 2)->kind != VC_TOKEN_LEFT_PAREN))
        {
            const bool is_await = check_contextual_await_using(parser);
            const VcToken *keyword = current_token(parser);
            if (is_await)
            {
                advance_token(parser);
                if (consume(parser, VC_TOKEN_KW_USING, "'using'") == NULL)
                    return NULL;
            }
            else
            {
                advance_token(parser);
            }
            statement = parse_using_declaration(parser, keyword, is_await);
        }
        else
        {
            statement = parse_statement(parser);
        }
        if (statement == NULL)
            return NULL;
        if (!vc_ast_node_list_push(parser->tree, &block->as.block_statement.statements, statement))
        {
            out_of_memory(parser);
            return NULL;
        }
    }

    if (consume(parser, VC_TOKEN_RIGHT_BRACE, "'}'") == NULL)
        return NULL;
    if (!wrap_using_declarations(parser, &block->as.block_statement.statements))
        return NULL;
    return block;
}

static bool looks_like_typed_local(const VcParser *parser)
{
    if (parser->async_method_depth != 0 && current_kind(parser) == VC_TOKEN_IDENTIFIER &&
        token_text_equals(parser, current_token(parser), "await"))
        return false;

    size_t index = parser->current;
    if (!scan_type_tokens(parser, &index, false))
        return false;
    if (token_at(parser, index)->kind != VC_TOKEN_IDENTIFIER)
        return false;
    index++;
    const VcTokenKind kind = token_at(parser, index)->kind;
    return kind == VC_TOKEN_EQUAL || kind == VC_TOKEN_SEMICOLON;
}

static VcAstNode *parse_typed_local_ex(VcParser *parser, bool scoped, const VcToken *start)
{
    VcAstTypeRef *type = parse_type(parser, false);
    if (type == NULL)
        return NULL;

    const VcToken *name = consume(parser, VC_TOKEN_IDENTIFIER, "local variable name");
    if (name == NULL)
        return NULL;

    VcAstNode *node = new_node(parser, VC_AST_LOCAL_DECLARATION, start->location);
    if (node == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }

    node->as.local_declaration.type = type;
    node->as.local_declaration.name = copy_token_text(parser, name);
    node->as.local_declaration.scoped = scoped;

    if (match(parser, VC_TOKEN_EQUAL))
    {
        node->as.local_declaration.initializer = parse_expression(parser);
        if (node->as.local_declaration.initializer == NULL)
            return NULL;
    }

    if (consume(parser, VC_TOKEN_SEMICOLON, "';'") == NULL)
        return NULL;
    return node;
}

static VcAstNode *parse_typed_local(VcParser *parser)
{
    return parse_typed_local_ex(parser, false, current_token(parser));
}

static bool check_contextual_scoped(const VcParser *parser)
{
    return check(parser, VC_TOKEN_IDENTIFIER) &&
        token_text_equals(parser, current_token(parser), "scoped");
}

static bool match_contextual_scoped(VcParser *parser)
{
    if (!check_contextual_scoped(parser))
        return false;
    advance_token(parser);
    return true;
}

static VcAstNode *parse_scoped_value_local(VcParser *parser)
{
    const VcToken *start = current_token(parser);
    if (!match_contextual_scoped(parser))
        return NULL;
    if (is_type_keyword(current_kind(parser), false))
    {
        parser_error(parser, "expected 'ref', found '%s'",
            vc_token_kind_name(current_kind(parser)));
        return NULL;
    }
    return parse_typed_local_ex(parser, true, start);
}

static VcAstNode *parse_ref_local(VcParser *parser)
{
    const VcToken *start = current_token(parser);
    const bool scoped = match_contextual_scoped(parser);
    if (consume(parser, VC_TOKEN_KW_REF, "'ref'") == NULL)
        return NULL;
    const bool ref_readonly = match(parser, VC_TOKEN_KW_READONLY);

    VcAstNode *node = new_node(parser, VC_AST_LOCAL_DECLARATION, start->location);
    if (node == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }
    node->as.local_declaration.is_ref = true;
    node->as.local_declaration.ref_readonly = ref_readonly;
    node->as.local_declaration.scoped = scoped;

    if (match(parser, VC_TOKEN_KW_VAR))
        node->as.local_declaration.is_var = true;
    else
    {
        node->as.local_declaration.type = parse_type(parser, false);
        if (node->as.local_declaration.type == NULL)
            return NULL;
    }

    const VcToken *name = consume(parser, VC_TOKEN_IDENTIFIER, "ref local variable name");
    if (name == NULL)
        return NULL;
    node->as.local_declaration.name = copy_token_text(parser, name);
    if (node->as.local_declaration.name == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }

    if (consume(parser, VC_TOKEN_EQUAL, "'=' after ref local declaration") == NULL ||
        consume(parser, VC_TOKEN_KW_REF, "'ref' in ref local initializer") == NULL)
        return NULL;

    node->as.local_declaration.initializer = parse_expression(parser);
    if (node->as.local_declaration.initializer == NULL)
        return NULL;
    if (consume(parser, VC_TOKEN_SEMICOLON, "';'") == NULL)
        return NULL;
    return node;
}

static VcAstNode *parse_var_local(VcParser *parser, const VcToken *keyword)
{
    const VcToken *name = consume(parser, VC_TOKEN_IDENTIFIER, "local variable name");
    if (name == NULL)
        return NULL;

    VcAstNode *node = new_node(parser, VC_AST_LOCAL_DECLARATION, keyword->location);
    if (node == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }
    node->as.local_declaration.is_var = true;
    node->as.local_declaration.name = copy_token_text(parser, name);

    if (match(parser, VC_TOKEN_EQUAL))
    {
        node->as.local_declaration.initializer = parse_expression(parser);
        if (node->as.local_declaration.initializer == NULL)
            return NULL;
    }

    if (consume(parser, VC_TOKEN_SEMICOLON, "';'") == NULL)
        return NULL;
    return node;
}

static VcAstNode *parse_using_declaration(VcParser *parser, const VcToken *keyword, bool is_await)
{
    VcAstNode *node = new_node(parser, VC_AST_LOCAL_DECLARATION, keyword->location);
    if (node == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }

    node->as.local_declaration.is_using_resource = true;
    node->as.local_declaration.is_await_using_resource = is_await;
    if (match(parser, VC_TOKEN_KW_VAR))
    {
        node->as.local_declaration.is_var = true;
    }
    else
    {
        node->as.local_declaration.type = parse_type(parser, false);
        if (node->as.local_declaration.type == NULL)
            return NULL;
    }

    const VcToken *name = consume(parser, VC_TOKEN_IDENTIFIER, "using variable name");
    if (name == NULL || consume(parser, VC_TOKEN_EQUAL, "'='") == NULL)
        return NULL;
    node->as.local_declaration.name = copy_token_text(parser, name);
    if (node->as.local_declaration.name == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }

    node->as.local_declaration.initializer = parse_expression(parser);
    if (node->as.local_declaration.initializer == NULL ||
        consume(parser, VC_TOKEN_SEMICOLON, "';'") == NULL)
        return NULL;
    return node;
}

static bool wrap_using_declarations(VcParser *parser, VcAstNodeList *statements)
{
    for (size_t i = 0; i < statements->count; i++)
    {
        VcAstNode *resource = statements->items[i];
        if (resource == NULL || resource->kind != VC_AST_LOCAL_DECLARATION ||
            !resource->as.local_declaration.is_using_resource)
            continue;

        VcAstNode *body = new_node(parser, VC_AST_BLOCK_STATEMENT, resource->location);
        if (body == NULL)
        {
            out_of_memory(parser);
            return false;
        }
        for (size_t j = i + 1; j < statements->count; j++)
        {
            if (!vc_ast_node_list_push(parser->tree, &body->as.block_statement.statements,
                    statements->items[j]))
            {
                out_of_memory(parser);
                return false;
            }
        }
        if (!wrap_using_declarations(parser, &body->as.block_statement.statements))
            return false;

        VcAstNode *using_statement = new_node(parser, VC_AST_USING_STATEMENT,
            resource->location);
        if (using_statement == NULL)
        {
            out_of_memory(parser);
            return false;
        }
        using_statement->as.using_statement.declaration = resource;
        using_statement->as.using_statement.body = body;
        using_statement->as.using_statement.is_declaration = true;
        using_statement->as.using_statement.is_await =
            resource->as.local_declaration.is_await_using_resource;

        statements->items[i] = using_statement;
        statements->count = i + 1;
        return true;
    }
    return true;
}

static VcAstNode *parse_expression_statement(VcParser *parser)
{
    const VcToken *start = current_token(parser);
    VcAstNode *expression = parse_expression(parser);
    if (expression == NULL)
        return NULL;

    if (consume(parser, VC_TOKEN_SEMICOLON, "';'") == NULL)
        return NULL;

    VcAstNode *node = new_node(parser, VC_AST_EXPRESSION_STATEMENT, start->location);
    if (node == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }
    node->as.expression_statement.expression = expression;
    return node;
}

static VcAstNode *parse_switch_statement(VcParser *parser, const VcToken *keyword)
{
    if (consume(parser, VC_TOKEN_LEFT_PAREN, "'('") == NULL)
        return NULL;

    VcAstNode *expression = parse_expression(parser);
    if (expression == NULL || consume(parser, VC_TOKEN_RIGHT_PAREN, "')'") == NULL ||
        consume(parser, VC_TOKEN_LEFT_BRACE, "'{'") == NULL)
        return NULL;

    VcAstNode *node = new_node(parser, VC_AST_SWITCH_STATEMENT, keyword->location);
    if (node == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }
    node->as.switch_statement.expression = expression;

    while (!check(parser, VC_TOKEN_RIGHT_BRACE) && !at_end(parser))
    {
        if (!check(parser, VC_TOKEN_KW_CASE) && !check(parser, VC_TOKEN_KW_DEFAULT))
        {
            parser_error(parser, "expected 'case', 'default', or '}' in switch");
            return NULL;
        }

        VcAstNode *section = new_node(parser, VC_AST_SWITCH_SECTION,
            current_token(parser)->location);
        if (section == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }

        while (check(parser, VC_TOKEN_KW_CASE) || check(parser, VC_TOKEN_KW_DEFAULT))
        {
            const bool is_default = match(parser, VC_TOKEN_KW_DEFAULT);
            const VcToken *label_token = previous_token(parser);
            if (!is_default)
            {
                if (consume(parser, VC_TOKEN_KW_CASE, "'case'") == NULL)
                    return NULL;
                label_token = previous_token(parser);
            }

            VcAstNode *label = new_node(parser, VC_AST_SWITCH_LABEL,
                label_token->location);
            if (label == NULL)
            {
                out_of_memory(parser);
                return NULL;
            }
            label->as.switch_label.is_default = is_default;

            if (!is_default)
            {
                const VcTokenKind start_kind = current_kind(parser);
                const bool contextual_not = current_contextual_pattern_word(parser, "not");
                const bool obvious_pattern = start_kind == VC_TOKEN_LEFT_BRACE ||
                    start_kind == VC_TOKEN_LEFT_PAREN || start_kind == VC_TOKEN_LESS ||
                    start_kind == VC_TOKEN_LESS_EQUAL || start_kind == VC_TOKEN_GREATER ||
                    start_kind == VC_TOKEN_GREATER_EQUAL || start_kind == VC_TOKEN_NUMBER ||
                    start_kind == VC_TOKEN_STRING || start_kind == VC_TOKEN_CHARACTER ||
                    start_kind == VC_TOKEN_KW_TRUE || start_kind == VC_TOKEN_KW_FALSE ||
                    start_kind == VC_TOKEN_KW_NULL || start_kind == VC_TOKEN_PLUS ||
                    start_kind == VC_TOKEN_MINUS || contextual_not ||
                    (start_kind == VC_TOKEN_IDENTIFIER &&
                     token_at(parser, parser->current + 1)->kind != VC_TOKEN_DOT);
                if (obvious_pattern)
                {
                    label->as.switch_label.is_pattern = true;
                    label->as.switch_label.pattern = parse_pattern_or(parser);
                    if (label->as.switch_label.pattern == NULL)
                        return NULL;
                }
                else
                {
                    label->as.switch_label.value = parse_expression(parser);
                    if (label->as.switch_label.value == NULL)
                        return NULL;
                }
            }

            if (is_default && current_contextual_pattern_word(parser, "when"))
            {
                parser_error(parser, "default switch label cannot have a when guard");
                return NULL;
            }
            if (!is_default && current_contextual_pattern_word(parser, "when"))
            {
                advance_token(parser);
                label->as.switch_label.guard = parse_expression(parser);
                if (label->as.switch_label.guard == NULL)
                    return NULL;
            }

            if (consume(parser, VC_TOKEN_COLON, "':'") == NULL ||
                !vc_ast_node_list_push(parser->tree, &section->as.switch_section.labels, label))
            {
                if (!parser->result->has_error)
                    out_of_memory(parser);
                return NULL;
            }

            if (!check(parser, VC_TOKEN_KW_CASE) && !check(parser, VC_TOKEN_KW_DEFAULT))
                break;
        }

        while (!check(parser, VC_TOKEN_KW_CASE) && !check(parser, VC_TOKEN_KW_DEFAULT) &&
            !check(parser, VC_TOKEN_RIGHT_BRACE) && !at_end(parser))
        {
            VcAstNode *statement = NULL;
            if ((check(parser, VC_TOKEN_KW_USING) &&
                    token_at(parser, parser->current + 1)->kind != VC_TOKEN_LEFT_PAREN) ||
                (check_contextual_await_using(parser) &&
                    token_at(parser, parser->current + 2)->kind != VC_TOKEN_LEFT_PAREN))
            {
                const bool is_await = check_contextual_await_using(parser);
                const VcToken *using_keyword = current_token(parser);
                if (is_await)
                {
                    advance_token(parser);
                    if (consume(parser, VC_TOKEN_KW_USING, "'using'") == NULL)
                        return NULL;
                }
                else
                {
                    advance_token(parser);
                }
                statement = parse_using_declaration(parser, using_keyword, is_await);
            }
            else
            {
                statement = parse_statement(parser);
            }
            if (statement == NULL ||
                !vc_ast_node_list_push(parser->tree, &section->as.switch_section.statements, statement))
            {
                if (!parser->result->has_error)
                    out_of_memory(parser);
                return NULL;
            }
        }
        if (!wrap_using_declarations(parser, &section->as.switch_section.statements))
            return NULL;

        if (!vc_ast_node_list_push(parser->tree, &node->as.switch_statement.sections, section))
        {
            out_of_memory(parser);
            return NULL;
        }
    }

    if (consume(parser, VC_TOKEN_RIGHT_BRACE, "'}'") == NULL)
        return NULL;
    return node;
}

static VcAstNode *parse_using_statement(VcParser *parser, const VcToken *keyword, bool is_await)
{
    if (consume(parser, VC_TOKEN_LEFT_PAREN, "'('") == NULL)
        return NULL;

    VcAstNodeList resources = {0};
    VcAstNode *expression = NULL;

    if (match(parser, VC_TOKEN_KW_VAR))
    {
        const VcToken *var_keyword = previous_token(parser);
        const VcToken *name = consume(parser, VC_TOKEN_IDENTIFIER, "using variable name");
        if (name == NULL || consume(parser, VC_TOKEN_EQUAL, "'='") == NULL)
            return NULL;
        VcAstNode *initializer = parse_expression(parser);
        if (initializer == NULL)
            return NULL;
        if (check(parser, VC_TOKEN_COMMA))
        {
            parser_error(parser, "implicitly typed using statement may declare only one resource");
            return NULL;
        }

        VcAstNode *resource = new_node(parser, VC_AST_LOCAL_DECLARATION, var_keyword->location);
        if (resource == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        resource->as.local_declaration.is_var = true;
        resource->as.local_declaration.is_using_resource = true;
        resource->as.local_declaration.is_await_using_resource = is_await;
        resource->as.local_declaration.name = copy_token_text(parser, name);
        resource->as.local_declaration.initializer = initializer;
        if (resource->as.local_declaration.name == NULL ||
            !vc_ast_node_list_push(parser->tree, &resources, resource))
        {
            out_of_memory(parser);
            return NULL;
        }
    }
    else if (looks_like_typed_local(parser))
    {
        const VcToken *type_start = current_token(parser);
        VcAstTypeRef *type = parse_type(parser, false);
        if (type == NULL)
            return NULL;

        for (;;)
        {
            const VcToken *name = consume(parser, VC_TOKEN_IDENTIFIER, "using variable name");
            if (name == NULL || consume(parser, VC_TOKEN_EQUAL, "'='") == NULL)
                return NULL;
            VcAstNode *initializer = parse_expression(parser);
            if (initializer == NULL)
                return NULL;

            VcAstNode *resource = new_node(parser, VC_AST_LOCAL_DECLARATION, type_start->location);
            if (resource == NULL)
            {
                out_of_memory(parser);
                return NULL;
            }
            resource->as.local_declaration.type = type;
            resource->as.local_declaration.name = copy_token_text(parser, name);
            resource->as.local_declaration.initializer = initializer;
            resource->as.local_declaration.is_using_resource = true;
            resource->as.local_declaration.is_await_using_resource = is_await;
            if (resource->as.local_declaration.name == NULL ||
                !vc_ast_node_list_push(parser->tree, &resources, resource))
            {
                out_of_memory(parser);
                return NULL;
            }

            if (!match(parser, VC_TOKEN_COMMA))
                break;
        }
    }
    else
    {
        expression = parse_expression(parser);
        if (expression == NULL)
            return NULL;
    }

    if (consume(parser, VC_TOKEN_RIGHT_PAREN, "')'") == NULL)
        return NULL;
    VcAstNode *body = parse_statement(parser);
    if (body == NULL)
        return NULL;

    if (resources.count == 0)
    {
        VcAstNode *node = new_node(parser, VC_AST_USING_STATEMENT, keyword->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.using_statement.expression = expression;
        node->as.using_statement.body = body;
        node->as.using_statement.is_await = is_await;
        return node;
    }

    VcAstNode *current_body = body;
    for (size_t i = resources.count; i > 0; i--)
    {
        VcAstNode *node = new_node(parser, VC_AST_USING_STATEMENT, keyword->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.using_statement.declaration = resources.items[i - 1];
        node->as.using_statement.body = current_body;
        node->as.using_statement.is_await = is_await;
        current_body = node;
    }
    return current_body;
}

static VcAstNode *parse_statement(VcParser *parser)
{
    if (check(parser, VC_TOKEN_LEFT_BRACE))
        return parse_block(parser);

    if (match(parser, VC_TOKEN_KW_IF))
    {
        const VcToken *keyword = previous_token(parser);
        if (consume(parser, VC_TOKEN_LEFT_PAREN, "'('") == NULL)
            return NULL;
        VcAstNode *condition = parse_expression(parser);
        if (condition == NULL || consume(parser, VC_TOKEN_RIGHT_PAREN, "')'") == NULL)
            return NULL;
        VcAstNode *then_statement = parse_statement(parser);
        if (then_statement == NULL)
            return NULL;

        VcAstNode *node = new_node(parser, VC_AST_IF_STATEMENT, keyword->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.if_statement.condition = condition;
        node->as.if_statement.then_statement = then_statement;
        if (match(parser, VC_TOKEN_KW_ELSE))
        {
            node->as.if_statement.else_statement = parse_statement(parser);
            if (node->as.if_statement.else_statement == NULL)
                return NULL;
        }
        return node;
    }

    if (match(parser, VC_TOKEN_KW_WHILE))
    {
        const VcToken *keyword = previous_token(parser);
        if (consume(parser, VC_TOKEN_LEFT_PAREN, "'('") == NULL)
            return NULL;
        VcAstNode *condition = parse_expression(parser);
        if (condition == NULL || consume(parser, VC_TOKEN_RIGHT_PAREN, "')'") == NULL)
            return NULL;
        VcAstNode *body = parse_statement(parser);
        if (body == NULL)
            return NULL;

        VcAstNode *node = new_node(parser, VC_AST_WHILE_STATEMENT, keyword->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.while_statement.condition = condition;
        node->as.while_statement.body = body;
        return node;
    }

    if (match(parser, VC_TOKEN_KW_DO))
    {
        const VcToken *keyword = previous_token(parser);
        VcAstNode *body = parse_statement(parser);
        if (body == NULL)
            return NULL;
        if (consume(parser, VC_TOKEN_KW_WHILE, "'while'") == NULL ||
            consume(parser, VC_TOKEN_LEFT_PAREN, "'('") == NULL)
            return NULL;
        VcAstNode *condition = parse_expression(parser);
        if (condition == NULL ||
            consume(parser, VC_TOKEN_RIGHT_PAREN, "')'") == NULL ||
            consume(parser, VC_TOKEN_SEMICOLON, "';'") == NULL)
            return NULL;

        VcAstNode *node = new_node(parser, VC_AST_DO_WHILE_STATEMENT, keyword->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.while_statement.condition = condition;
        node->as.while_statement.body = body;
        return node;
    }

    if (match(parser, VC_TOKEN_KW_FOR))
    {
        const VcToken *keyword = previous_token(parser);
        if (consume(parser, VC_TOKEN_LEFT_PAREN, "'('") == NULL)
            return NULL;

        VcAstNode *initializer = NULL;
        if (match(parser, VC_TOKEN_SEMICOLON))
        {
            initializer = NULL;
        }
        else if (check(parser, VC_TOKEN_KW_REF) || check_contextual_scoped(parser))
        {
            initializer = parse_ref_local(parser);
            if (initializer == NULL)
                return NULL;
        }
        else if (looks_like_typed_local(parser))
        {
            initializer = parse_typed_local(parser);
            if (initializer == NULL)
                return NULL;
        }
        else if (match(parser, VC_TOKEN_KW_VAR))
        {
            initializer = parse_var_local(parser, previous_token(parser));
            if (initializer == NULL)
                return NULL;
        }
        else
        {
            initializer = parse_expression_statement(parser);
            if (initializer == NULL)
                return NULL;
        }

        VcAstNode *condition = NULL;
        if (!check(parser, VC_TOKEN_SEMICOLON))
        {
            condition = parse_expression(parser);
            if (condition == NULL)
                return NULL;
        }
        if (consume(parser, VC_TOKEN_SEMICOLON, "';'") == NULL)
            return NULL;

        VcAstNode *increment = NULL;
        if (!check(parser, VC_TOKEN_RIGHT_PAREN))
        {
            increment = parse_expression(parser);
            if (increment == NULL)
                return NULL;
        }
        if (consume(parser, VC_TOKEN_RIGHT_PAREN, "')'") == NULL)
            return NULL;

        VcAstNode *body = parse_statement(parser);
        if (body == NULL)
            return NULL;

        VcAstNode *node = new_node(parser, VC_AST_FOR_STATEMENT, keyword->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.for_statement.initializer = initializer;
        node->as.for_statement.condition = condition;
        node->as.for_statement.increment = increment;
        node->as.for_statement.body = body;
        return node;
    }

    if (check_contextual_await_using(parser))
    {
        const VcToken *keyword = advance_token(parser);
        if (consume(parser, VC_TOKEN_KW_USING, "'using'") == NULL)
            return NULL;
        if (!check(parser, VC_TOKEN_LEFT_PAREN))
        {
            parser_error(parser, "await using declaration is only valid in a block or switch section");
            return NULL;
        }
        return parse_using_statement(parser, keyword, true);
    }

    if (match(parser, VC_TOKEN_KW_USING))
    {
        const VcToken *keyword = previous_token(parser);
        if (!check(parser, VC_TOKEN_LEFT_PAREN))
        {
            parser_error(parser, "using declaration is only valid in a block or switch section");
            return NULL;
        }
        return parse_using_statement(parser, keyword, false);
    }

    if (match(parser, VC_TOKEN_KW_FIXED))
    {
        const VcToken *keyword = previous_token(parser);
        if (consume(parser, VC_TOKEN_LEFT_PAREN, "'('") == NULL)
            return NULL;

        VcAstTypeRef *type = parse_type(parser, false);
        if (type == NULL)
            return NULL;

        VcAstNodeList declarations = {0};
        for (;;)
        {
            const VcToken *name = consume(parser, VC_TOKEN_IDENTIFIER, "fixed pointer variable name");
            if (name == NULL || consume(parser, VC_TOKEN_EQUAL, "'='") == NULL)
                return NULL;
            VcAstNode *initializer = parse_expression(parser);
            if (initializer == NULL)
                return NULL;

            VcAstNode *node = new_node(parser, VC_AST_FIXED_STATEMENT, keyword->location);
            if (node == NULL)
            {
                out_of_memory(parser);
                return NULL;
            }
            node->as.fixed_statement.type = type;
            node->as.fixed_statement.name = copy_token_text(parser, name);
            node->as.fixed_statement.initializer = initializer;
            if (node->as.fixed_statement.name == NULL ||
                !vc_ast_node_list_push(parser->tree, &declarations, node))
            {
                out_of_memory(parser);
                return NULL;
            }
            if (!match(parser, VC_TOKEN_COMMA))
                break;
        }

        if (consume(parser, VC_TOKEN_RIGHT_PAREN, "')'") == NULL)
            return NULL;
        VcAstNode *body = parse_statement(parser);
        if (body == NULL)
            return NULL;

        for (size_t i = declarations.count; i > 0; i--)
        {
            declarations.items[i - 1]->as.fixed_statement.body = body;
            body = declarations.items[i - 1];
        }
        return body;
    }

    if (match(parser, VC_TOKEN_KW_LOCK))
    {
        const VcToken *keyword = previous_token(parser);
        if (consume(parser, VC_TOKEN_LEFT_PAREN, "'('") == NULL)
            return NULL;
        VcAstNode *expression = parse_expression(parser);
        if (expression == NULL || consume(parser, VC_TOKEN_RIGHT_PAREN, "')'") == NULL)
            return NULL;
        VcAstNode *body = parse_statement(parser);
        if (body == NULL)
            return NULL;

        VcAstNode *node = new_node(parser, VC_AST_LOCK_STATEMENT, keyword->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.lock_statement.expression = expression;
        node->as.lock_statement.body = body;
        return node;
    }

    const VcToken *foreach_keyword = NULL;
    bool await_foreach = false;
    if (current_kind(parser) == VC_TOKEN_IDENTIFIER &&
        token_text_equals(parser, current_token(parser), "await") &&
        token_at(parser, parser->current + 1)->kind == VC_TOKEN_KW_FOREACH)
    {
        foreach_keyword = advance_token(parser);
        advance_token(parser);
        await_foreach = true;
    }
    else if (match(parser, VC_TOKEN_KW_FOREACH))
    {
        foreach_keyword = previous_token(parser);
    }

    if (foreach_keyword != NULL)
    {
        const VcToken *keyword = foreach_keyword;
        if (consume(parser, VC_TOKEN_LEFT_PAREN, "'('") == NULL)
            return NULL;

        const bool is_ref = match(parser, VC_TOKEN_KW_REF);
        const bool ref_readonly = is_ref && match(parser, VC_TOKEN_KW_READONLY);

        bool is_var = false;
        VcAstTypeRef *type = NULL;
        if (match(parser, VC_TOKEN_KW_VAR))
        {
            is_var = true;
        }
        else
        {
            type = parse_type(parser, false);
            if (type == NULL)
                return NULL;
        }

        const VcToken *name = consume(parser, VC_TOKEN_IDENTIFIER, "foreach variable name");
        if (name == NULL || consume(parser, VC_TOKEN_KW_IN, "'in'") == NULL)
            return NULL;

        VcAstNode *collection = parse_expression(parser);
        if (collection == NULL || consume(parser, VC_TOKEN_RIGHT_PAREN, "')'") == NULL)
            return NULL;

        VcAstNode *body = parse_statement(parser);
        if (body == NULL)
            return NULL;

        VcAstNode *node = new_node(parser, VC_AST_FOREACH_STATEMENT, keyword->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.foreach_statement.type = type;
        node->as.foreach_statement.name = copy_token_text(parser, name);
        node->as.foreach_statement.is_var = is_var;
        node->as.foreach_statement.is_await = await_foreach;
        node->as.foreach_statement.is_ref = is_ref;
        node->as.foreach_statement.ref_readonly = ref_readonly;
        node->as.foreach_statement.collection = collection;
        node->as.foreach_statement.body = body;
        return node;
    }

    if (match(parser, VC_TOKEN_KW_SWITCH))
        return parse_switch_statement(parser, previous_token(parser));

    if (match(parser, VC_TOKEN_KW_YIELD))
    {
        const VcToken *keyword = previous_token(parser);
        if (match(parser, VC_TOKEN_KW_RETURN))
        {
            VcAstNode *expression = parse_expression(parser);
            if (expression == NULL || consume(parser, VC_TOKEN_SEMICOLON, "';'") == NULL)
                return NULL;
            VcAstNode *node = new_node(parser, VC_AST_YIELD_RETURN_STATEMENT, keyword->location);
            if (node == NULL)
            {
                out_of_memory(parser);
                return NULL;
            }
            node->as.yield_statement.expression = expression;
            return node;
        }
        if (match(parser, VC_TOKEN_KW_BREAK))
        {
            if (consume(parser, VC_TOKEN_SEMICOLON, "';'") == NULL)
                return NULL;
            VcAstNode *node = new_node(parser, VC_AST_YIELD_BREAK_STATEMENT, keyword->location);
            if (node == NULL) out_of_memory(parser);
            return node;
        }
        parser_error(parser, "expected 'return' or 'break' after 'yield'");
        return NULL;
    }

    if (match(parser, VC_TOKEN_KW_BREAK))
    {
        const VcToken *keyword = previous_token(parser);
        if (consume(parser, VC_TOKEN_SEMICOLON, "';'") == NULL)
            return NULL;
        VcAstNode *node = new_node(parser, VC_AST_BREAK_STATEMENT, keyword->location);
        if (node == NULL) out_of_memory(parser);
        return node;
    }

    if (match(parser, VC_TOKEN_KW_CONTINUE))
    {
        const VcToken *keyword = previous_token(parser);
        if (consume(parser, VC_TOKEN_SEMICOLON, "';'") == NULL)
            return NULL;
        VcAstNode *node = new_node(parser, VC_AST_CONTINUE_STATEMENT, keyword->location);
        if (node == NULL) out_of_memory(parser);
        return node;
    }

    if (match(parser, VC_TOKEN_KW_TRY))
    {
        const VcToken *keyword = previous_token(parser);
        VcAstNode *try_block = parse_block(parser);
        if (try_block == NULL)
            return NULL;

        VcAstNode *node = new_node(parser, VC_AST_TRY_STATEMENT, keyword->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.try_statement.try_block = try_block;

        while (match(parser, VC_TOKEN_KW_CATCH))
        {
            const VcToken *catch_keyword = previous_token(parser);
            VcAstNode *clause = new_node(parser, VC_AST_CATCH_CLAUSE, catch_keyword->location);
            if (clause == NULL)
            {
                out_of_memory(parser);
                return NULL;
            }

            if (match(parser, VC_TOKEN_LEFT_PAREN))
            {
                clause->as.catch_clause.type = parse_type(parser, false);
                if (clause->as.catch_clause.type == NULL)
                    return NULL;
                if (check(parser, VC_TOKEN_IDENTIFIER))
                {
                    const VcToken *name = current_token(parser);
                    advance_token(parser);
                    clause->as.catch_clause.name = copy_token_text(parser, name);
                    if (clause->as.catch_clause.name == NULL)
                    {
                        out_of_memory(parser);
                        return NULL;
                    }
                }
                if (consume(parser, VC_TOKEN_RIGHT_PAREN, "')'") == NULL)
                    return NULL;
            }
            else
            {
                clause->as.catch_clause.catch_all = true;
            }

            clause->as.catch_clause.body = parse_block(parser);
            if (clause->as.catch_clause.body == NULL)
                return NULL;
            if (!vc_ast_node_list_push(parser->tree, &node->as.try_statement.catches, clause))
            {
                out_of_memory(parser);
                return NULL;
            }
        }

        if (match(parser, VC_TOKEN_KW_FINALLY))
        {
            node->as.try_statement.finally_block = parse_block(parser);
            if (node->as.try_statement.finally_block == NULL)
                return NULL;
        }

        if (node->as.try_statement.catches.count == 0 &&
            node->as.try_statement.finally_block == NULL)
        {
            parser_error(parser, "expected 'catch' or 'finally' after try block");
            return NULL;
        }
        return node;
    }

    if (match(parser, VC_TOKEN_KW_THROW))
    {
        const VcToken *keyword = previous_token(parser);
        VcAstNode *expression = NULL;
        if (!check(parser, VC_TOKEN_SEMICOLON))
        {
            expression = parse_expression(parser);
            if (expression == NULL)
                return NULL;
        }
        if (consume(parser, VC_TOKEN_SEMICOLON, "';'") == NULL)
            return NULL;

        VcAstNode *node = new_node(parser, VC_AST_THROW_STATEMENT, keyword->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        node->as.throw_statement.expression = expression;
        return node;
    }

    if (match(parser, VC_TOKEN_KW_RETURN))
    {
        const VcToken *keyword = previous_token(parser);
        VcAstNode *node = new_node(parser, VC_AST_RETURN_STATEMENT, keyword->location);
        if (node == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }

        if (!check(parser, VC_TOKEN_SEMICOLON))
        {
            if (match(parser, VC_TOKEN_KW_REF))
                node->as.return_statement.is_ref = true;
            node->as.return_statement.expression = parse_expression(parser);
            if (node->as.return_statement.expression == NULL)
                return NULL;
        }

        if (consume(parser, VC_TOKEN_SEMICOLON, "';'") == NULL)
            return NULL;
        return node;
    }

    if (check(parser, VC_TOKEN_KW_REF))
        return parse_ref_local(parser);
    if (check_contextual_scoped(parser))
    {
        if (token_at(parser, parser->current + 1)->kind == VC_TOKEN_KW_REF)
            return parse_ref_local(parser);
        return parse_scoped_value_local(parser);
    }

    if (looks_like_typed_local(parser))
        return parse_typed_local(parser);

    if (match(parser, VC_TOKEN_KW_VAR))
        return parse_var_local(parser, previous_token(parser));

    return parse_expression_statement(parser);
}

static bool parse_parameters(VcParser *parser, VcAstNodeList *parameters)
{
    if (!check(parser, VC_TOKEN_RIGHT_PAREN))
    {
        do
        {
            const VcToken *start = current_token(parser);
            VcAstNodeList attributes = {0};
            if (!parse_attribute_lists(parser, &attributes))
                return false;

            bool extension_receiver = false;
            if (check(parser, VC_TOKEN_KW_THIS))
            {
                if (parameters->count != 0)
                {
                    parser_error(parser, "extension receiver modifier 'this' is valid only on the first parameter");
                    return false;
                }
                extension_receiver = true;
                advance_token(parser);
            }

            const bool scoped = match_contextual_scoped(parser);
            VcTokenKind modifier = VC_TOKEN_EOF;
            if (check(parser, VC_TOKEN_KW_REF) || check(parser, VC_TOKEN_KW_OUT) ||
                check(parser, VC_TOKEN_KW_IN) || check(parser, VC_TOKEN_KW_PARAMS))
            {
                modifier = current_kind(parser);
                advance_token(parser);
            }
            if (scoped && modifier == VC_TOKEN_EOF &&
                is_type_keyword(current_kind(parser), false))
            {
                parser_error_at(parser, start->location,
                    "scoped parameter requires ref, in, or out");
                return false;
            }
            VcAstTypeRef *type = parse_type(parser, false);
            if (type == NULL)
                return false;

            const VcToken *name = consume(parser, VC_TOKEN_IDENTIFIER, "parameter name");
            if (name == NULL)
                return false;

            VcAstNode *parameter = new_node(parser, VC_AST_PARAMETER, start->location);
            if (parameter == NULL)
            {
                out_of_memory(parser);
                return false;
            }
            parameter->as.parameter.modifier = modifier;
            parameter->as.parameter.scoped = scoped;
            parameter->as.parameter.extension_receiver = extension_receiver;
            parameter->as.parameter.type = type;
            parameter->as.parameter.name = copy_token_text(parser, name);
            parameter->attributes = attributes;

            if (match(parser, VC_TOKEN_EQUAL))
            {
                parameter->as.parameter.default_value = parse_expression(parser);
                if (parameter->as.parameter.default_value == NULL)
                    return false;
            }

            if (!vc_ast_node_list_push(parser->tree, parameters, parameter))
            {
                out_of_memory(parser);
                return false;
            }
        } while (match(parser, VC_TOKEN_COMMA));
    }

    return true;
}

static VcAstNode *parse_method_after_name(
    VcParser *parser,
    VcSourceLocation location,
    VcAstNodeList attributes,
    uint32_t modifiers,
    VcAstTypeRef *return_type,
    char *name,
    bool is_constructor,
    bool returns_ref,
    bool returns_ref_readonly)
{
    VcAstNode *method = new_node(parser, VC_AST_METHOD_DECLARATION, location);
    if (method == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }
    method->attributes = attributes;
    method->as.method_declaration.modifiers = modifiers;
    method->as.method_declaration.return_type = return_type;
    method->as.method_declaration.returns_ref = returns_ref;
    method->as.method_declaration.returns_ref_readonly = returns_ref_readonly;
    method->as.method_declaration.name = name;
    method->as.method_declaration.is_constructor = is_constructor;
    method->as.method_declaration.is_operator = false;
    method->as.method_declaration.operator_kind = VC_TOKEN_EOF;

    if (!is_constructor && !parse_generic_parameters(parser, &method->as.method_declaration.generic_parameters))
        return NULL;

    if (consume(parser, VC_TOKEN_LEFT_PAREN, "'('") == NULL)
        return NULL;
    if (!parse_parameters(parser, &method->as.method_declaration.parameters))
        return NULL;
    if (consume(parser, VC_TOKEN_RIGHT_PAREN, "')'") == NULL)
        return NULL;
    if (!is_constructor && !parse_generic_constraints(parser,
            &method->as.method_declaration.generic_parameters,
            &method->as.method_declaration.generic_constraints))
        return NULL;

    if (is_constructor && match(parser, VC_TOKEN_COLON))
    {
        if (match(parser, VC_TOKEN_KW_BASE))
        {
            method->as.method_declaration.has_base_constructor_initializer = true;
        }
        else if (match(parser, VC_TOKEN_KW_THIS))
        {
            method->as.method_declaration.has_this_constructor_initializer = true;
        }
        else
        {
            parser_error(parser, "expected 'base' or 'this' constructor initializer");
            return NULL;
        }
        if (consume(parser, VC_TOKEN_LEFT_PAREN, "'('") == NULL)
            return NULL;
        if (!check(parser, VC_TOKEN_RIGHT_PAREN))
        {
            do
            {
                VcAstNode *argument = parse_argument(parser);
                if (argument == NULL)
                    return NULL;
                if (!vc_ast_node_list_push(parser->tree,
                        &method->as.method_declaration.constructor_initializer_arguments,
                        argument))
                {
                    out_of_memory(parser);
                    return NULL;
                }
            } while (match(parser, VC_TOKEN_COMMA));
        }
        if (consume(parser, VC_TOKEN_RIGHT_PAREN, "')'") == NULL)
            return NULL;
    }

    if (match(parser, VC_TOKEN_SEMICOLON))
        return method;

    const bool async_method = (modifiers & VC_AST_MOD_ASYNC) != 0;
    if (match(parser, VC_TOKEN_FAT_ARROW))
    {
        const bool expression_returns_ref = match(parser, VC_TOKEN_KW_REF);
        if (returns_ref && !expression_returns_ref)
        {
            parser_error(parser, "ref-returning expression-bodied method requires '=> ref expression'");
            return NULL;
        }
        if (!returns_ref && expression_returns_ref)
        {
            parser_error(parser, "'ref' expression body requires a ref-returning method");
            return NULL;
        }
        if (async_method)
            parser->async_method_depth++;
        VcAstNode *expression = parse_expression(parser);
        if (async_method)
            parser->async_method_depth--;
        if (expression == NULL || consume(parser, VC_TOKEN_SEMICOLON, "';'") == NULL)
            return NULL;
        const bool returns_value = !is_constructor && return_type != NULL &&
            !(return_type->pointer_depth == 0 && strcmp(return_type->name, "void") == 0);
        method->as.method_declaration.body =
            make_expression_body_block(parser, expression, returns_value, returns_ref);
        return method->as.method_declaration.body != NULL ? method : NULL;
    }

    if (async_method)
        parser->async_method_depth++;
    method->as.method_declaration.body = parse_block(parser);
    if (async_method)
        parser->async_method_depth--;
    if (method->as.method_declaration.body == NULL)
        return NULL;
    return method;
}

static bool is_overloadable_operator(VcTokenKind kind)
{
    switch (kind)
    {
        case VC_TOKEN_PLUS:
        case VC_TOKEN_MINUS:
        case VC_TOKEN_STAR:
        case VC_TOKEN_SLASH:
        case VC_TOKEN_PERCENT:
        case VC_TOKEN_BANG:
        case VC_TOKEN_TILDE:
        case VC_TOKEN_PLUS_PLUS:
        case VC_TOKEN_MINUS_MINUS:
        case VC_TOKEN_EQUAL_EQUAL:
        case VC_TOKEN_BANG_EQUAL:
        case VC_TOKEN_LESS:
        case VC_TOKEN_LESS_EQUAL:
        case VC_TOKEN_GREATER:
        case VC_TOKEN_GREATER_EQUAL:
            return true;
        default:
            return false;
    }
}

static VcAstNode *parse_property_after_name(
    VcParser *parser,
    VcSourceLocation location,
    VcAstNodeList attributes,
    uint32_t modifiers,
    VcAstTypeRef *type,
    char *name,
    bool returns_ref,
    bool returns_ref_readonly)
{
    VcAstNode *property = new_node(parser, VC_AST_PROPERTY_DECLARATION, location);
    if (property == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }

    property->attributes = attributes;
    property->as.property_declaration.modifiers = modifiers;
    property->as.property_declaration.type = type;
    property->as.property_declaration.name = name;
    property->as.property_declaration.returns_ref = returns_ref;
    property->as.property_declaration.returns_ref_readonly = returns_ref_readonly;

    if (match(parser, VC_TOKEN_FAT_ARROW))
    {
        if (returns_ref && !match(parser, VC_TOKEN_KW_REF))
        {
            parser_error_at(parser, location, "ref-returning property expression body requires ref");
            return NULL;
        }
        property->as.property_declaration.has_getter = true;
        property->as.property_declaration.getter_expression = true;
        property->as.property_declaration.getter_body = parse_expression(parser);
        if (property->as.property_declaration.getter_body == NULL)
            return NULL;
        if (consume(parser, VC_TOKEN_SEMICOLON, "';'") == NULL)
            return NULL;
        return property;
    }

    if (consume(parser, VC_TOKEN_LEFT_BRACE, "'{'") == NULL)
        return NULL;

    while (!check(parser, VC_TOKEN_RIGHT_BRACE) && !at_end(parser))
    {
        const uint32_t accessor_modifiers = parse_modifiers(parser, false, NULL);
        const VcToken *accessor = current_token(parser);
        const bool is_getter = accessor->kind == VC_TOKEN_KW_GET;
        const bool is_setter = accessor->kind == VC_TOKEN_KW_SET;
        if (!is_getter && !is_setter)
        {
            parser_error_at(parser, accessor->location, "expected 'get' or 'set' accessor");
            return NULL;
        }
        advance_token(parser);

        if (is_getter && property->as.property_declaration.has_getter)
        {
            parser_error_at(parser, accessor->location, "property already has a getter");
            return NULL;
        }
        if (is_setter && property->as.property_declaration.has_setter)
        {
            parser_error_at(parser, accessor->location, "property already has a setter");
            return NULL;
        }

        bool is_auto = false;
        bool is_expression = false;
        VcAstNode *body = NULL;
        if (match(parser, VC_TOKEN_SEMICOLON))
        {
            is_auto = true;
        }
        else if (match(parser, VC_TOKEN_FAT_ARROW))
        {
            if (is_getter && returns_ref && !match(parser, VC_TOKEN_KW_REF))
            {
                parser_error_at(parser, accessor->location,
                    "ref-returning property getter expression body requires ref");
                return NULL;
            }
            VcAstNode *expression = parse_expression(parser);
            if (expression == NULL || consume(parser, VC_TOKEN_SEMICOLON, "';'") == NULL)
                return NULL;
            if (is_getter)
            {
                is_expression = true;
                body = expression;
            }
            else
            {
                body = make_expression_body_block(parser, expression, false, false);
                if (body == NULL)
                    return NULL;
            }
        }
        else
        {
            body = parse_block(parser);
            if (body == NULL)
                return NULL;
        }

        if (is_getter)
        {
            property->as.property_declaration.has_getter = true;
            property->as.property_declaration.getter_modifiers = accessor_modifiers;
            property->as.property_declaration.getter_auto = is_auto;
            property->as.property_declaration.getter_expression = is_expression;
            property->as.property_declaration.getter_body = body;
        }
        else
        {
            property->as.property_declaration.has_setter = true;
            property->as.property_declaration.setter_modifiers = accessor_modifiers;
            property->as.property_declaration.setter_auto = is_auto;
            property->as.property_declaration.setter_body = body;
        }
    }

    if (consume(parser, VC_TOKEN_RIGHT_BRACE, "'}'") == NULL)
        return NULL;

    if (match(parser, VC_TOKEN_EQUAL))
    {
        property->as.property_declaration.initializer = parse_expression(parser);
        if (property->as.property_declaration.initializer == NULL)
            return NULL;
        if (consume(parser, VC_TOKEN_SEMICOLON, "';'") == NULL)
            return NULL;
    }
    return property;
}

static VcAstNode *parse_member(VcParser *parser, const char *containing_type_name)
{
    VcAstNodeList attributes = {0};
    if (!parse_attribute_lists(parser, &attributes))
        return NULL;

    const uint32_t modifiers = parse_modifiers(parser, true, containing_type_name);
    const VcToken *start = current_token(parser);

    if (match(parser, VC_TOKEN_KW_EVENT))
    {
        VcAstTypeRef *event_type = parse_type(parser, false);
        if (event_type == NULL)
            return NULL;

        const VcToken *name_token = consume(parser, VC_TOKEN_IDENTIFIER, "event name");
        if (name_token == NULL)
            return NULL;

        VcAstNode *event = new_node(parser, VC_AST_FIELD_DECLARATION, start->location);
        if (event == NULL)
        {
            out_of_memory(parser);
            return NULL;
        }
        event->attributes = attributes;
        event->as.field_declaration.modifiers = modifiers;
        event->as.field_declaration.type = event_type;
        event->as.field_declaration.name = copy_token_text(parser, name_token);
        event->as.field_declaration.is_event = true;
        if (event->as.field_declaration.name == NULL)
            return NULL;

        if (match(parser, VC_TOKEN_LEFT_BRACE))
        {
            while (!check(parser, VC_TOKEN_RIGHT_BRACE) && !at_end(parser))
            {
                const VcToken *accessor = current_token(parser);
                const bool is_add = accessor->kind == VC_TOKEN_IDENTIFIER &&
                    token_text_equals(parser, accessor, "add");
                const bool is_remove = accessor->kind == VC_TOKEN_IDENTIFIER &&
                    token_text_equals(parser, accessor, "remove");
                if (!is_add && !is_remove)
                {
                    parser_error_at(parser, accessor->location,
                        "expected 'add' or 'remove' event accessor");
                    return NULL;
                }
                advance_token(parser);

                if (is_add && event->as.field_declaration.has_event_add_accessor)
                {
                    parser_error_at(parser, accessor->location, "event already has an add accessor");
                    return NULL;
                }
                if (is_remove && event->as.field_declaration.has_event_remove_accessor)
                {
                    parser_error_at(parser, accessor->location, "event already has a remove accessor");
                    return NULL;
                }

                VcAstNode *body = NULL;
                if (match(parser, VC_TOKEN_SEMICOLON))
                {
                    body = NULL;
                }
                else if (match(parser, VC_TOKEN_FAT_ARROW))
                {
                    VcAstNode *expression = parse_expression(parser);
                    if (expression == NULL || consume(parser, VC_TOKEN_SEMICOLON, "';'") == NULL)
                        return NULL;
                    body = make_expression_body_block(parser, expression, false, false);
                    if (body == NULL)
                        return NULL;
                }
                else
                {
                    body = parse_block(parser);
                    if (body == NULL)
                        return NULL;
                }

                if (is_add)
                {
                    event->as.field_declaration.has_event_add_accessor = true;
                    event->as.field_declaration.event_add_body = body;
                }
                else
                {
                    event->as.field_declaration.has_event_remove_accessor = true;
                    event->as.field_declaration.event_remove_body = body;
                }
            }
            if (consume(parser, VC_TOKEN_RIGHT_BRACE, "'}'") == NULL)
                return NULL;

            if (match(parser, VC_TOKEN_EQUAL))
            {
                event->as.field_declaration.initializer = parse_expression(parser);
                if (event->as.field_declaration.initializer == NULL)
                    return NULL;
                if (consume(parser, VC_TOKEN_SEMICOLON, "';'") == NULL)
                    return NULL;
            }
            return event;
        }

        if (match(parser, VC_TOKEN_EQUAL))
        {
            event->as.field_declaration.initializer = parse_expression(parser);
            if (event->as.field_declaration.initializer == NULL)
                return NULL;
        }
        if (consume(parser, VC_TOKEN_SEMICOLON, "';'") == NULL)
            return NULL;
        return event;
    }

    if (check(parser, VC_TOKEN_IDENTIFIER) && token_text_equals(parser, current_token(parser), containing_type_name) &&
        token_at(parser, parser->current + 1)->kind == VC_TOKEN_LEFT_PAREN)
    {
        const VcToken *name_token = advance_token(parser);
        char *name = copy_token_text(parser, name_token);
        return parse_method_after_name(parser, start->location, attributes, modifiers, NULL, name, true, false, false);
    }

    if (check(parser, VC_TOKEN_KW_IMPLICIT) || check(parser, VC_TOKEN_KW_EXPLICIT))
    {
        const VcToken *conversion_token = advance_token(parser);
        if (consume(parser, VC_TOKEN_KW_OPERATOR, "'operator'") == NULL)
            return NULL;

        VcAstTypeRef *target_type = parse_type(parser, true);
        if (target_type == NULL)
            return NULL;

        char *name = copy_token_text(parser, conversion_token);
        if (name == NULL)
            return NULL;

        VcAstNode *method = parse_method_after_name(parser, start->location, attributes,
            modifiers, target_type, name, false, false, false);
        if (method == NULL)
            return NULL;
        method->as.method_declaration.is_operator = true;
        method->as.method_declaration.operator_kind = conversion_token->kind;
        return method;
    }

    const bool returns_ref = match(parser, VC_TOKEN_KW_REF);
    const bool returns_ref_readonly = returns_ref && match(parser, VC_TOKEN_KW_READONLY);
    VcAstTypeRef *type = parse_type(parser, true);
    if (type == NULL)
        return NULL;

    if (match(parser, VC_TOKEN_KW_OPERATOR))
    {
        if (returns_ref)
        {
            parser_error_at(parser, start->location, "operators cannot return by ref");
            return NULL;
        }
        const VcToken *operator_token = current_token(parser);
        if (!is_overloadable_operator(operator_token->kind))
        {
            parser_error_at(parser, operator_token->location,
                "expected overloadable operator symbol, found '%s'",
                vc_token_kind_name(operator_token->kind));
            return NULL;
        }
        advance_token(parser);

        char *name = copy_token_text(parser, operator_token);
        if (name == NULL)
            return NULL;

        VcAstNode *method = parse_method_after_name(parser, start->location, attributes, modifiers, type, name, false, false, false);
        if (method == NULL)
            return NULL;
        method->as.method_declaration.is_operator = true;
        method->as.method_declaration.operator_kind = operator_token->kind;
        return method;
    }

    const VcToken *name_token = consume(parser, VC_TOKEN_IDENTIFIER, "member name");
    if (name_token == NULL)
        return NULL;
    char *name = copy_token_text(parser, name_token);
    if (name == NULL)
        return NULL;

    if (check(parser, VC_TOKEN_LEFT_PAREN) || check(parser, VC_TOKEN_LESS))
    {
        return parse_method_after_name(parser, start->location, attributes, modifiers, type, name, false,
            returns_ref, returns_ref_readonly);
    }

    if (check(parser, VC_TOKEN_LEFT_BRACE) || check(parser, VC_TOKEN_FAT_ARROW))
        return parse_property_after_name(parser, start->location, attributes, modifiers, type, name,
            returns_ref, returns_ref_readonly);

    VcAstNode *field = new_node(parser, VC_AST_FIELD_DECLARATION, start->location);
    if (field == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }
    field->attributes = attributes;
    field->as.field_declaration.modifiers = modifiers;
    field->as.field_declaration.type = type;
    field->as.field_declaration.name = name;
    field->as.field_declaration.is_ref = returns_ref;
    field->as.field_declaration.ref_readonly = returns_ref_readonly;

    if (match(parser, VC_TOKEN_EQUAL))
    {
        field->as.field_declaration.initializer = parse_expression(parser);
        if (field->as.field_declaration.initializer == NULL)
            return NULL;
    }

    if (consume(parser, VC_TOKEN_SEMICOLON, "';'") == NULL)
        return NULL;
    return field;
}

static VcAstNode *parse_enum_member(VcParser *parser)
{
    VcAstNodeList attributes = {0};
    if (!parse_attribute_lists(parser, &attributes))
        return NULL;

    const VcToken *name = consume(parser, VC_TOKEN_IDENTIFIER, "enum member name");
    if (name == NULL)
        return NULL;

    VcAstNode *member = new_node(parser, VC_AST_ENUM_MEMBER, name->location);
    if (member == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }
    member->attributes = attributes;
    member->as.enum_member.name = copy_token_text(parser, name);

    if (match(parser, VC_TOKEN_EQUAL))
    {
        member->as.enum_member.value = parse_expression(parser);
        if (member->as.enum_member.value == NULL)
            return NULL;
    }
    return member;
}

static VcAstNode *parse_type_declaration(
    VcParser *parser,
    VcAstNodeList attributes,
    uint32_t modifiers)
{
    const VcToken *keyword = current_token(parser);
    VcAstTypeKind type_kind;

    switch (current_kind(parser))
    {
        case VC_TOKEN_KW_CLASS: type_kind = VC_AST_TYPE_CLASS; break;
        case VC_TOKEN_KW_STRUCT: type_kind = VC_AST_TYPE_STRUCT; break;
        case VC_TOKEN_KW_INTERFACE: type_kind = VC_AST_TYPE_INTERFACE; break;
        case VC_TOKEN_KW_ENUM: type_kind = VC_AST_TYPE_ENUM; break;
        default:
            parser_error(parser, "expected type declaration, found '%s'", vc_token_kind_name(current_kind(parser)));
            return NULL;
    }
    advance_token(parser);

    const VcToken *name_token = consume(parser, VC_TOKEN_IDENTIFIER, "type name");
    if (name_token == NULL)
        return NULL;

    VcAstNode *type = new_node(parser, VC_AST_TYPE_DECLARATION, keyword->location);
    if (type == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }
    type->attributes = attributes;
    type->as.type_declaration.type_kind = type_kind;
    type->as.type_declaration.modifiers = modifiers;
    type->as.type_declaration.name = copy_token_text(parser, name_token);

    if (!parse_generic_parameters(parser, &type->as.type_declaration.generic_parameters))
        return NULL;

    if (match(parser, VC_TOKEN_COLON))
    {
        do
        {
            VcAstTypeRef *base_type = parse_type(parser, false);
            if (base_type == NULL)
                return NULL;
            if (!vc_ast_type_list_push(parser->tree, &type->as.type_declaration.base_types, base_type))
            {
                out_of_memory(parser);
                return NULL;
            }
        } while (match(parser, VC_TOKEN_COMMA));
    }

    if (!parse_generic_constraints(parser, &type->as.type_declaration.generic_parameters,
            &type->as.type_declaration.generic_constraints))
        return NULL;

    if (consume(parser, VC_TOKEN_LEFT_BRACE, "'{'") == NULL)
        return NULL;

    if (type_kind == VC_AST_TYPE_ENUM)
    {
        while (!check(parser, VC_TOKEN_RIGHT_BRACE) && !at_end(parser))
        {
            VcAstNode *member = parse_enum_member(parser);
            if (member == NULL)
                return NULL;
            if (!vc_ast_node_list_push(parser->tree, &type->as.type_declaration.members, member))
            {
                out_of_memory(parser);
                return NULL;
            }

            if (!match(parser, VC_TOKEN_COMMA))
                break;
        }
    }
    else
    {
        while (!check(parser, VC_TOKEN_RIGHT_BRACE) && !at_end(parser))
        {
            VcAstNode *member = parse_member(parser, type->as.type_declaration.name);
            if (member == NULL)
                return NULL;
            if (!vc_ast_node_list_push(parser->tree, &type->as.type_declaration.members, member))
            {
                out_of_memory(parser);
                return NULL;
            }
        }
    }

    if (consume(parser, VC_TOKEN_RIGHT_BRACE, "'}'") == NULL)
        return NULL;
    match(parser, VC_TOKEN_SEMICOLON);
    return type;
}


static VcAstNode *parse_delegate_declaration(
    VcParser *parser,
    VcAstNodeList attributes,
    uint32_t modifiers)
{
    const VcToken *keyword = consume(parser, VC_TOKEN_KW_DELEGATE, "'delegate'");
    if (keyword == NULL)
        return NULL;

    VcAstTypeRef *return_type = parse_type(parser, true);
    if (return_type == NULL)
        return NULL;

    const VcToken *name_token = consume(parser, VC_TOKEN_IDENTIFIER, "delegate name");
    if (name_token == NULL)
        return NULL;

    VcAstNode *type = new_node(parser, VC_AST_TYPE_DECLARATION, keyword->location);
    if (type == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }
    type->attributes = attributes;
    type->as.type_declaration.type_kind = VC_AST_TYPE_DELEGATE;
    type->as.type_declaration.modifiers = modifiers;
    type->as.type_declaration.name = copy_token_text(parser, name_token);
    if (type->as.type_declaration.name == NULL)
        return NULL;

    if (!parse_generic_parameters(parser, &type->as.type_declaration.generic_parameters))
        return NULL;

    VcAstNode *invoke = new_node(parser, VC_AST_METHOD_DECLARATION, keyword->location);
    if (invoke == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }
    invoke->as.method_declaration.modifiers = VC_AST_MOD_PUBLIC |
        (modifiers & VC_AST_MOD_UNSAFE);
    invoke->as.method_declaration.return_type = return_type;
    invoke->as.method_declaration.name = vc_ast_copy_text(parser->tree, "Invoke", 6);
    if (invoke->as.method_declaration.name == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }

    if (consume(parser, VC_TOKEN_LEFT_PAREN, "'('") == NULL)
        return NULL;
    if (!parse_parameters(parser, &invoke->as.method_declaration.parameters))
        return NULL;
    if (consume(parser, VC_TOKEN_RIGHT_PAREN, "')'") == NULL)
        return NULL;
    if (!parse_generic_constraints(parser, &type->as.type_declaration.generic_parameters,
            &type->as.type_declaration.generic_constraints))
        return NULL;
    if (consume(parser, VC_TOKEN_SEMICOLON, "';'") == NULL)
        return NULL;

    if (!vc_ast_node_list_push(parser->tree, &type->as.type_declaration.members, invoke))
    {
        out_of_memory(parser);
        return NULL;
    }
    return type;
}

static VcAstNode *parse_using(VcParser *parser)
{
    const VcToken *keyword = consume(parser, VC_TOKEN_KW_USING, "'using'");
    if (keyword == NULL)
        return NULL;

    VcAstNode *node = new_node(parser, VC_AST_USING_DECLARATION, keyword->location);
    if (node == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }

    if (check(parser, VC_TOKEN_IDENTIFIER) &&
        token_at(parser, parser->current + 1)->kind == VC_TOKEN_EQUAL)
    {
        const VcToken *alias = advance_token(parser);
        parser->current++;
        node->as.using_declaration.alias = copy_token_text(parser, alias);
    }

    node->as.using_declaration.name = parse_qualified_name(parser);
    if (node->as.using_declaration.name == NULL)
        return NULL;

    if (consume(parser, VC_TOKEN_SEMICOLON, "';'") == NULL)
        return NULL;
    return node;
}

static VcAstNode *parse_namespace(VcParser *parser)
{
    const VcToken *keyword = consume(parser, VC_TOKEN_KW_NAMESPACE, "'namespace'");
    if (keyword == NULL)
        return NULL;

    char *name = parse_qualified_name(parser);
    if (name == NULL)
        return NULL;

    VcAstNode *node = new_node(parser, VC_AST_NAMESPACE_DECLARATION, keyword->location);
    if (node == NULL)
    {
        out_of_memory(parser);
        return NULL;
    }
    node->as.namespace_declaration.name = name;

    if (match(parser, VC_TOKEN_SEMICOLON))
    {
        node->as.namespace_declaration.file_scoped = true;
        while (!at_end(parser))
        {
            VcAstNode *declaration = parse_declaration(parser, false);
            if (declaration == NULL)
                return NULL;
            if (!vc_ast_node_list_push(parser->tree, &node->as.namespace_declaration.declarations, declaration))
            {
                out_of_memory(parser);
                return NULL;
            }
        }
        return node;
    }

    if (consume(parser, VC_TOKEN_LEFT_BRACE, "'{' or ';'") == NULL)
        return NULL;

    while (!check(parser, VC_TOKEN_RIGHT_BRACE) && !at_end(parser))
    {
        VcAstNode *declaration = parse_declaration(parser, true);
        if (declaration == NULL)
            return NULL;
        if (!vc_ast_node_list_push(parser->tree, &node->as.namespace_declaration.declarations, declaration))
        {
            out_of_memory(parser);
            return NULL;
        }
    }

    if (consume(parser, VC_TOKEN_RIGHT_BRACE, "'}'") == NULL)
        return NULL;
    match(parser, VC_TOKEN_SEMICOLON);
    return node;
}

static VcAstNode *parse_declaration(VcParser *parser, bool allow_namespace)
{
    if (check(parser, VC_TOKEN_KW_USING))
        return parse_using(parser);

    if (check(parser, VC_TOKEN_KW_NAMESPACE))
    {
        if (!allow_namespace)
        {
            parser_error(parser, "a file-scoped namespace cannot contain another namespace declaration");
            return NULL;
        }
        return parse_namespace(parser);
    }

    VcAstNodeList attributes = {0};
    if (!parse_attribute_lists(parser, &attributes))
        return NULL;
    uint32_t modifiers = parse_modifiers(parser, false, NULL);
    if (check(parser, VC_TOKEN_KW_REF) &&
        token_at(parser, parser->current + 1)->kind == VC_TOKEN_KW_STRUCT)
    {
        modifiers |= VC_AST_MOD_REF;
        advance_token(parser);
    }
    if (check(parser, VC_TOKEN_KW_DELEGATE))
        return parse_delegate_declaration(parser, attributes, modifiers);
    return parse_type_declaration(parser, attributes, modifiers);
}

/* Recover only at a top-level boundary. Invalid syntax never reaches semantics.
 * Keep the first diagnostic and discard the damaged declaration, avoiding cascades. */
static void recover_declaration(VcParser *parser, size_t failed_start)
{
    size_t depth = 0;
    for (size_t i = 0; i < parser->current; i++)
    {
        VcTokenKind kind = token_at(parser, i)->kind;
        if (kind == VC_TOKEN_LEFT_BRACE) depth++;
        else if (kind == VC_TOKEN_RIGHT_BRACE && depth > 0) depth--;
    }
    while (!at_end(parser))
    {
        VcTokenKind kind = current_kind(parser);
        if (depth == 0 && parser->current > failed_start &&
            (kind == VC_TOKEN_KW_PUBLIC || kind == VC_TOKEN_KW_INTERNAL ||
             kind == VC_TOKEN_KW_CLASS || kind == VC_TOKEN_KW_STRUCT ||
             kind == VC_TOKEN_KW_INTERFACE || kind == VC_TOKEN_KW_ENUM ||
             kind == VC_TOKEN_KW_DELEGATE || kind == VC_TOKEN_KW_NAMESPACE)) return;
        advance_token(parser);
        if (kind == VC_TOKEN_LEFT_BRACE) depth++;
        else if (kind == VC_TOKEN_RIGHT_BRACE && depth > 0) depth--;
    }
}

bool vc_parse_source(
    const VcSource *source,
    const VcTokenList *tokens,
    VcAstTree *tree,
    VcParseResult *result)
{
    vc_parse_result_init(result);

    VcParser parser = {
        .source = source,
        .tokens = tokens,
        .tree = tree,
        .result = result
    };

    VcAstNode *root = vc_ast_new_node(tree, VC_AST_COMPILATION_UNIT, tokens->items[0].location);
    if (root == NULL)
    {
        out_of_memory(&parser);
        return false;
    }
    root->span.start = tokens->items[0].span.start;
    root->span.end = tokens->items[tokens->count - 1].span.end;
    tree->root = root;

    while (!at_end(&parser))
    {
        const size_t declaration_start = parser.current;
        VcAstNode *declaration;
        if (check(&parser, VC_TOKEN_KW_NAMESPACE))
            declaration = parse_namespace(&parser);
        else
            declaration = parse_declaration(&parser, true);

        if (declaration == NULL)
        {
            recover_declaration(&parser, declaration_start);
            continue;
        }

        if (!vc_ast_node_list_push(tree, &root->as.compilation_unit.declarations, declaration))
        {
            out_of_memory(&parser);
            return false;
        }

        if (declaration->kind == VC_AST_NAMESPACE_DECLARATION &&
            declaration->as.namespace_declaration.file_scoped && !at_end(&parser))
        {
            parser_error_at(&parser, current_token(&parser)->location,
                "unexpected tokens after file-scoped namespace contents");
            return false;
        }
    }

    return !result->has_error;
}
