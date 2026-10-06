#include "diagnostic.h"
#include "lexer.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct VcLexer
{
    const VcSource *source;
    VcTokenList *tokens;
    size_t position;
    size_t line;
    size_t column;
} VcLexer;

typedef struct Keyword
{
    const char *text;
    VcTokenKind kind;
} Keyword;

static const Keyword KEYWORDS[] = {
    {"abstract", VC_TOKEN_KW_ABSTRACT},
    {"as", VC_TOKEN_KW_AS},
    {"base", VC_TOKEN_KW_BASE},
    {"bool", VC_TOKEN_KW_BOOL},
    {"break", VC_TOKEN_KW_BREAK},
    {"byte", VC_TOKEN_KW_BYTE},
    {"case", VC_TOKEN_KW_CASE},
    {"catch", VC_TOKEN_KW_CATCH},
    {"char", VC_TOKEN_KW_CHAR},
    {"class", VC_TOKEN_KW_CLASS},
    {"const", VC_TOKEN_KW_CONST},
    {"continue", VC_TOKEN_KW_CONTINUE},
    {"decimal", VC_TOKEN_KW_DECIMAL},
    {"default", VC_TOKEN_KW_DEFAULT},
    {"delegate", VC_TOKEN_KW_DELEGATE},
    {"do", VC_TOKEN_KW_DO},
    {"double", VC_TOKEN_KW_DOUBLE},
    {"else", VC_TOKEN_KW_ELSE},
    {"enum", VC_TOKEN_KW_ENUM},
    {"event", VC_TOKEN_KW_EVENT},
    {"explicit", VC_TOKEN_KW_EXPLICIT},
    {"extern", VC_TOKEN_KW_EXTERN},
    {"false", VC_TOKEN_KW_FALSE},
    {"finally", VC_TOKEN_KW_FINALLY},
    {"fixed", VC_TOKEN_KW_FIXED},
    {"float", VC_TOKEN_KW_FLOAT},
    {"for", VC_TOKEN_KW_FOR},
    {"foreach", VC_TOKEN_KW_FOREACH},
    {"get", VC_TOKEN_KW_GET},
    {"if", VC_TOKEN_KW_IF},
    {"implicit", VC_TOKEN_KW_IMPLICIT},
    {"in", VC_TOKEN_KW_IN},
    {"int", VC_TOKEN_KW_INT},
    {"interface", VC_TOKEN_KW_INTERFACE},
    {"internal", VC_TOKEN_KW_INTERNAL},
    {"is", VC_TOKEN_KW_IS},
    {"lock", VC_TOKEN_KW_LOCK},
    {"long", VC_TOKEN_KW_LONG},
    {"namespace", VC_TOKEN_KW_NAMESPACE},
    {"native", VC_TOKEN_KW_NATIVE},
    {"new", VC_TOKEN_KW_NEW},
    {"null", VC_TOKEN_KW_NULL},
    {"object", VC_TOKEN_KW_OBJECT},
    {"operator", VC_TOKEN_KW_OPERATOR},
    {"out", VC_TOKEN_KW_OUT},
    {"override", VC_TOKEN_KW_OVERRIDE},
    {"params", VC_TOKEN_KW_PARAMS},
    {"private", VC_TOKEN_KW_PRIVATE},
    {"protected", VC_TOKEN_KW_PROTECTED},
    {"public", VC_TOKEN_KW_PUBLIC},
    {"readonly", VC_TOKEN_KW_READONLY},
    {"ref", VC_TOKEN_KW_REF},
    {"return", VC_TOKEN_KW_RETURN},
    {"sbyte", VC_TOKEN_KW_SBYTE},
    {"sealed", VC_TOKEN_KW_SEALED},
    {"set", VC_TOKEN_KW_SET},
    {"short", VC_TOKEN_KW_SHORT},
    {"sizeof", VC_TOKEN_KW_SIZEOF},
    {"stackalloc", VC_TOKEN_KW_STACKALLOC},
    {"static", VC_TOKEN_KW_STATIC},
    {"string", VC_TOKEN_KW_STRING},
    {"struct", VC_TOKEN_KW_STRUCT},
    {"switch", VC_TOKEN_KW_SWITCH},
    {"this", VC_TOKEN_KW_THIS},
    {"throw", VC_TOKEN_KW_THROW},
    {"true", VC_TOKEN_KW_TRUE},
    {"try", VC_TOKEN_KW_TRY},
    {"typeof", VC_TOKEN_KW_TYPEOF},
    {"uint", VC_TOKEN_KW_UINT},
    {"ulong", VC_TOKEN_KW_ULONG},
    {"unsafe", VC_TOKEN_KW_UNSAFE},
    {"ushort", VC_TOKEN_KW_USHORT},
    {"using", VC_TOKEN_KW_USING},
    {"var", VC_TOKEN_KW_VAR},
    {"virtual", VC_TOKEN_KW_VIRTUAL},
    {"void", VC_TOKEN_KW_VOID},
    {"where", VC_TOKEN_KW_WHERE},
    {"while", VC_TOKEN_KW_WHILE},
    {"yield", VC_TOKEN_KW_YIELD}
};

static void set_error_text(char *error, size_t error_size, const char *format, ...)
{
    if (error == NULL || error_size == 0)
        return;

    va_list args;
    va_start(args, format);
    vsnprintf(error, error_size, format, args);
    va_end(args);
}

static char *duplicate_string(const char *value)
{
    const size_t length = strlen(value);
    char *copy = malloc(length + 1);
    if (copy == NULL)
        return NULL;

    memcpy(copy, value, length + 1);
    return copy;
}

void vc_source_init(VcSource *source)
{
    memset(source, 0, sizeof(*source));
}

void vc_source_destroy(VcSource *source)
{
    free(source->path);
    free(source->text);
    memset(source, 0, sizeof(*source));
}

bool vc_source_load(const char *path, VcSource *source, char *error, size_t error_size)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
    {
        set_error_text(error, error_size, "could not open '%s': %s", path, strerror(errno));
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        set_error_text(error, error_size, "could not seek '%s'", path);
        fclose(file);
        return false;
    }

    const long file_size = ftell(file);
    if (file_size < 0)
    {
        set_error_text(error, error_size, "could not determine size of '%s'", path);
        fclose(file);
        return false;
    }

    rewind(file);

    char *text = malloc((size_t)file_size + 1);
    char *path_copy = duplicate_string(path);
    if (text == NULL || path_copy == NULL)
    {
        free(text);
        free(path_copy);
        fclose(file);
        set_error_text(error, error_size, "out of memory while reading '%s'", path);
        return false;
    }

    const size_t read_count = fread(text, 1, (size_t)file_size, file);
    fclose(file);

    if (read_count != (size_t)file_size)
    {
        free(text);
        free(path_copy);
        set_error_text(error, error_size, "could not read all of '%s'", path);
        return false;
    }

    text[file_size] = '\0';
    source->path = path_copy;
    source->text = text;
    source->length = (size_t)file_size;
    return true;
}

void vc_token_list_init(VcTokenList *tokens)
{
    memset(tokens, 0, sizeof(*tokens));
}

void vc_token_list_destroy(VcTokenList *tokens)
{
    free(tokens->items);
    memset(tokens, 0, sizeof(*tokens));
}

static bool is_at_end(const VcLexer *lexer)
{
    return lexer->position >= lexer->source->length;
}

static char current(const VcLexer *lexer)
{
    return is_at_end(lexer) ? '\0' : lexer->source->text[lexer->position];
}

static char peek(const VcLexer *lexer, size_t distance)
{
    const size_t position = lexer->position + distance;
    return position >= lexer->source->length ? '\0' : lexer->source->text[position];
}

static char advance(VcLexer *lexer)
{
    if (is_at_end(lexer))
        return '\0';

    const char c = lexer->source->text[lexer->position++];
    if (c == '\n')
    {
        lexer->line++;
        lexer->column = 1;
    }
    else
    {
        lexer->column++;
    }

    return c;
}

static bool match(VcLexer *lexer, char expected)
{
    if (current(lexer) != expected)
        return false;

    advance(lexer);
    return true;
}

static VcSourceLocation location(const VcLexer *lexer)
{
    VcSourceLocation result = {
        .offset = lexer->position,
        .line = lexer->line,
        .column = lexer->column
    };
    return result;
}

static void fail(VcLexer *lexer, VcSourceLocation where, const char *format, ...)
{
    vc_diagnostic_init(&lexer->tokens->diagnostic, lexer->source->path,
        (VcSourceSpan){where, location(lexer)});
    lexer->tokens->diagnostic.source = lexer->source;
    vc_diagnostic_set_code(&lexer->tokens->diagnostic, VC_DIAG_LEXICAL);
    lexer->tokens->has_error = true;
    lexer->tokens->diagnostic.path = lexer->source->path;
    lexer->tokens->diagnostic.span.start = where;
    lexer->tokens->diagnostic.span.end = location(lexer);

    va_list args;
    va_start(args, format);
    vsnprintf(lexer->tokens->diagnostic.message, sizeof(lexer->tokens->diagnostic.message), format, args);
    va_end(args);
}

static bool push_token(VcLexer *lexer, VcTokenKind kind, VcSourceLocation start)
{
    VcTokenList *tokens = lexer->tokens;
    if (tokens->count == tokens->capacity)
    {
        const size_t new_capacity = tokens->capacity == 0 ? 64 : tokens->capacity * 2;
        VcToken *items = realloc(tokens->items, new_capacity * sizeof(*items));
        if (items == NULL)
        {
            fail(lexer, start, "out of memory while storing tokens");
            return false;
        }

        tokens->items = items;
        tokens->capacity = new_capacity;
    }

    VcToken token = {
        .kind = kind,
        .location = start,
        .span = {
            .start = start,
            .end = location(lexer)
        },
        .length = lexer->position - start.offset
    };
    tokens->items[tokens->count++] = token;
    return true;
}

static bool is_identifier_start(unsigned char c)
{
    return c == '_' || isalpha(c) || c >= 0x80;
}

static bool is_identifier_continue(unsigned char c)
{
    return c == '_' || isalnum(c) || c >= 0x80;
}

static VcTokenKind keyword_kind(const char *text, size_t length)
{
    for (size_t i = 0; i < sizeof(KEYWORDS) / sizeof(KEYWORDS[0]); i++)
    {
        if (strlen(KEYWORDS[i].text) == length && strncmp(text, KEYWORDS[i].text, length) == 0)
            return KEYWORDS[i].kind;
    }

    return VC_TOKEN_IDENTIFIER;
}

static bool lex_identifier(VcLexer *lexer, VcSourceLocation start)
{
    while (is_identifier_continue((unsigned char)current(lexer)))
        advance(lexer);

    const size_t length = lexer->position - start.offset;
    const VcTokenKind kind = keyword_kind(lexer->source->text + start.offset, length);
    return push_token(lexer, kind, start);
}

static bool is_digit_for_base(char c, int base)
{
    if (base == 2)
        return c == '0' || c == '1';

    if (base == 16)
        return isxdigit((unsigned char)c) != 0;

    return isdigit((unsigned char)c) != 0;
}

static void consume_digits(VcLexer *lexer, int base)
{
    while (is_digit_for_base(current(lexer), base) || current(lexer) == '_')
        advance(lexer);
}

static bool lex_number(VcLexer *lexer, VcSourceLocation start)
{
    if (current(lexer) == '0' && (peek(lexer, 1) == 'x' || peek(lexer, 1) == 'X'))
    {
        advance(lexer);
        advance(lexer);
        consume_digits(lexer, 16);
    }
    else if (current(lexer) == '0' && (peek(lexer, 1) == 'b' || peek(lexer, 1) == 'B'))
    {
        advance(lexer);
        advance(lexer);
        consume_digits(lexer, 2);
    }
    else
    {
        consume_digits(lexer, 10);

        if (current(lexer) == '.' && isdigit((unsigned char)peek(lexer, 1)))
        {
            advance(lexer);
            consume_digits(lexer, 10);
        }

        if (current(lexer) == 'e' || current(lexer) == 'E')
        {
            size_t distance = 1;
            if (peek(lexer, distance) == '+' || peek(lexer, distance) == '-')
                distance++;

            if (isdigit((unsigned char)peek(lexer, distance)))
            {
                advance(lexer);
                if (current(lexer) == '+' || current(lexer) == '-')
                    advance(lexer);
                consume_digits(lexer, 10);
            }
        }
    }

    while (strchr("uUlLfFdDmM", current(lexer)) != NULL && current(lexer) != '\0')
        advance(lexer);

    return push_token(lexer, VC_TOKEN_NUMBER, start);
}

static bool lex_quoted(VcLexer *lexer, VcSourceLocation start, char quote, VcTokenKind kind)
{
    advance(lexer);

    while (!is_at_end(lexer))
    {
        const char c = current(lexer);
        if (c == quote)
        {
            advance(lexer);
            return push_token(lexer, kind, start);
        }

        if (c == '\n' || c == '\r')
        {
            fail(lexer, start, quote == '"' ? "unterminated string literal" : "unterminated character literal");
            vc_diagnostic_set_code(&lexer->tokens->diagnostic, VC_DIAG_UNTERMINATED_LITERAL);
            vc_diagnostic_add_context(&lexer->tokens->diagnostic, VC_DIAGNOSTIC_HELP, NULL,
                (VcSourceSpan){0}, quote == '"'
                    ? "Close the string with a double quote before the line ends; use an escape for a newline."
                    : "Close the character literal with a single quote before the line ends.");
            return false;
        }

        if (c == '\\')
        {
            advance(lexer);
            if (is_at_end(lexer))
                break;
            advance(lexer);
            continue;
        }

        advance(lexer);
    }

    fail(lexer, start, quote == '"' ? "unterminated string literal" : "unterminated character literal");
            vc_diagnostic_set_code(&lexer->tokens->diagnostic, VC_DIAG_UNTERMINATED_LITERAL);
    vc_diagnostic_add_context(&lexer->tokens->diagnostic, VC_DIAGNOSTIC_HELP, NULL,
        (VcSourceSpan){0}, quote == '"' ? "Close the string with a double quote."
        : "Close the character literal with a single quote.");
    return false;
}

static bool skip_whitespace_and_comments(VcLexer *lexer)
{
    for (;;)
    {
        const char c = current(lexer);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v')
        {
            advance(lexer);
            continue;
        }

        if (c == '/' && peek(lexer, 1) == '/')
        {
            advance(lexer);
            advance(lexer);
            while (!is_at_end(lexer) && current(lexer) != '\n')
                advance(lexer);
            continue;
        }

        if (c == '/' && peek(lexer, 1) == '*')
        {
            const VcSourceLocation start = location(lexer);
            advance(lexer);
            advance(lexer);

            while (!is_at_end(lexer) && !(current(lexer) == '*' && peek(lexer, 1) == '/'))
                advance(lexer);

            if (is_at_end(lexer))
            {
                fail(lexer, start, "unterminated block comment");
                return false;
            }

            advance(lexer);
            advance(lexer);
            continue;
        }

        return true;
    }
}

static bool lex_operator_or_punctuation(VcLexer *lexer, VcSourceLocation start)
{
    const char c = advance(lexer);
    VcTokenKind kind;

    switch (c)
    {
        case '(' : kind = VC_TOKEN_LEFT_PAREN; break;
        case ')' : kind = VC_TOKEN_RIGHT_PAREN; break;
        case '{' : kind = VC_TOKEN_LEFT_BRACE; break;
        case '}' : kind = VC_TOKEN_RIGHT_BRACE; break;
        case '[' : kind = VC_TOKEN_LEFT_BRACKET; break;
        case ']' : kind = VC_TOKEN_RIGHT_BRACKET; break;
        case ',' : kind = VC_TOKEN_COMMA; break;
        case '.' : kind = match(lexer, '.') ? VC_TOKEN_DOT_DOT : VC_TOKEN_DOT; break;
        case ';' : kind = VC_TOKEN_SEMICOLON; break;
        case '@' : kind = VC_TOKEN_AT; break;
        case '~' : kind = VC_TOKEN_TILDE; break;

        case ':':
            kind = match(lexer, ':') ? VC_TOKEN_COLON_COLON : VC_TOKEN_COLON;
            break;

        case '?':
            if (match(lexer, '?'))
                kind = match(lexer, '=') ? VC_TOKEN_QUESTION_QUESTION_EQUAL : VC_TOKEN_QUESTION_QUESTION;
            else
                kind = match(lexer, '.') ? VC_TOKEN_QUESTION_DOT : VC_TOKEN_QUESTION;
            break;

        case '+':
            if (match(lexer, '+')) kind = VC_TOKEN_PLUS_PLUS;
            else if (match(lexer, '=')) kind = VC_TOKEN_PLUS_EQUAL;
            else kind = VC_TOKEN_PLUS;
            break;

        case '-':
            if (match(lexer, '-')) kind = VC_TOKEN_MINUS_MINUS;
            else if (match(lexer, '=')) kind = VC_TOKEN_MINUS_EQUAL;
            else if (match(lexer, '>')) kind = VC_TOKEN_ARROW;
            else kind = VC_TOKEN_MINUS;
            break;

        case '*':
            kind = match(lexer, '=') ? VC_TOKEN_STAR_EQUAL : VC_TOKEN_STAR;
            break;

        case '/':
            kind = match(lexer, '=') ? VC_TOKEN_SLASH_EQUAL : VC_TOKEN_SLASH;
            break;

        case '%':
            kind = match(lexer, '=') ? VC_TOKEN_PERCENT_EQUAL : VC_TOKEN_PERCENT;
            break;

        case '=':
            if (match(lexer, '=')) kind = VC_TOKEN_EQUAL_EQUAL;
            else if (match(lexer, '>')) kind = VC_TOKEN_FAT_ARROW;
            else kind = VC_TOKEN_EQUAL;
            break;

        case '!':
            kind = match(lexer, '=') ? VC_TOKEN_BANG_EQUAL : VC_TOKEN_BANG;
            break;

        case '<':
            if (match(lexer, '<'))
                kind = match(lexer, '=') ? VC_TOKEN_LESS_LESS_EQUAL : VC_TOKEN_LESS_LESS;
            else
                kind = match(lexer, '=') ? VC_TOKEN_LESS_EQUAL : VC_TOKEN_LESS;
            break;

        case '>':
            if (match(lexer, '>'))
                kind = match(lexer, '=') ? VC_TOKEN_GREATER_GREATER_EQUAL : VC_TOKEN_GREATER_GREATER;
            else
                kind = match(lexer, '=') ? VC_TOKEN_GREATER_EQUAL : VC_TOKEN_GREATER;
            break;

        case '&':
            if (match(lexer, '&')) kind = VC_TOKEN_AMPERSAND_AMPERSAND;
            else if (match(lexer, '=')) kind = VC_TOKEN_AMPERSAND_EQUAL;
            else kind = VC_TOKEN_AMPERSAND;
            break;

        case '|':
            if (match(lexer, '|')) kind = VC_TOKEN_PIPE_PIPE;
            else if (match(lexer, '=')) kind = VC_TOKEN_PIPE_EQUAL;
            else kind = VC_TOKEN_PIPE;
            break;

        case '^':
            kind = match(lexer, '=') ? VC_TOKEN_CARET_EQUAL : VC_TOKEN_CARET;
            break;

        default:
            fail(lexer, start, "unexpected character '%c'", c);
                vc_diagnostic_set_code(&lexer->tokens->diagnostic, VC_DIAG_UNEXPECTED_CHARACTER);
            return false;
    }

    return push_token(lexer, kind, start);
}

static bool lex_interpolated(VcLexer *lexer, VcSourceLocation start)
{
    advance(lexer); /* '$' */
    if (current(lexer) != '"')
    {
        fail(lexer, start, "expected '\"' after '$' in interpolated string");
        return false;
    }
    advance(lexer); /* opening quote */
    if (!push_token(lexer, VC_TOKEN_INTERPOLATED_START, start))
        return false;

    VcSourceLocation text_start = location(lexer);
    while (!is_at_end(lexer))
    {
        const char c = current(lexer);
        if (c == '\\')
        {
            advance(lexer);
            if (is_at_end(lexer))
                break;
            advance(lexer);
            continue;
        }
        if (c == '\n' || c == '\r')
        {
            fail(lexer, start, "unterminated interpolated string literal");
            return false;
        }
        if (c == '"')
        {
            if (location(lexer).offset != text_start.offset &&
                !push_token(lexer, VC_TOKEN_INTERPOLATED_TEXT, text_start))
                return false;
            const VcSourceLocation end_start = location(lexer);
            advance(lexer);
            return push_token(lexer, VC_TOKEN_INTERPOLATED_END, end_start);
        }
        if (c == '{')
        {
            if (peek(lexer, 1) == '{')
            {
                advance(lexer);
                advance(lexer);
                continue;
            }
            if (location(lexer).offset != text_start.offset &&
                !push_token(lexer, VC_TOKEN_INTERPOLATED_TEXT, text_start))
                return false;

            const VcSourceLocation expression_start = location(lexer);
            advance(lexer);
            if (!push_token(lexer, VC_TOKEN_INTERPOLATION_START, expression_start))
                return false;

            size_t brace_depth = 0;
            for (;;)
            {
                if (!skip_whitespace_and_comments(lexer))
                    return false;
                if (is_at_end(lexer))
                {
                    fail(lexer, expression_start, "unterminated interpolation expression");
                    return false;
                }

                const VcSourceLocation token_start = location(lexer);
                const unsigned char expression_c = (unsigned char)current(lexer);
                if (expression_c == '}' && brace_depth == 0)
                {
                    advance(lexer);
                    if (!push_token(lexer, VC_TOKEN_INTERPOLATION_END, token_start))
                        return false;
                    break;
                }
                if (is_identifier_start(expression_c))
                {
                    if (!lex_identifier(lexer, token_start))
                        return false;
                    continue;
                }
                if (isdigit(expression_c))
                {
                    if (!lex_number(lexer, token_start))
                        return false;
                    continue;
                }
                if (expression_c == '"')
                {
                    if (!lex_quoted(lexer, token_start, '"', VC_TOKEN_STRING))
                        return false;
                    continue;
                }
                if (expression_c == '\'')
                {
                    if (!lex_quoted(lexer, token_start, '\'', VC_TOKEN_CHARACTER))
                        return false;
                    continue;
                }
                if (expression_c == '$' && peek(lexer, 1) == '"')
                {
                    if (!lex_interpolated(lexer, token_start))
                        return false;
                    continue;
                }

                if (expression_c == '{')
                    brace_depth++;
                else if (expression_c == '}')
                    brace_depth--;
                if (!lex_operator_or_punctuation(lexer, token_start))
                    return false;
            }

            text_start = location(lexer);
            continue;
        }
        if (c == '}')
        {
            if (peek(lexer, 1) != '}')
            {
                fail(lexer, location(lexer), "unescaped '}' in interpolated string literal");
                return false;
            }
            advance(lexer);
            advance(lexer);
            continue;
        }
        advance(lexer);
    }

    fail(lexer, start, "unterminated interpolated string literal");
    return false;
}

bool vc_lex_source(const VcSource *source, VcTokenList *tokens)
{
    VcLexer lexer = {
        .source = source,
        .tokens = tokens,
        .position = 0,
        .line = 1,
        .column = 1
    };

    while (!is_at_end(&lexer))
    {
        if (!skip_whitespace_and_comments(&lexer))
            return false;

        if (is_at_end(&lexer))
            break;

        const VcSourceLocation start = location(&lexer);
        const unsigned char c = (unsigned char)current(&lexer);

        if (is_identifier_start(c))
        {
            if (!lex_identifier(&lexer, start))
                return false;
            continue;
        }

        if (isdigit(c))
        {
            if (!lex_number(&lexer, start))
                return false;
            continue;
        }

        if (c == '$' && peek(&lexer, 1) == '"')
        {
            if (!lex_interpolated(&lexer, start))
                return false;
            continue;
        }

        if (c == '"')
        {
            if (!lex_quoted(&lexer, start, '"', VC_TOKEN_STRING))
                return false;
            continue;
        }

        if (c == '\'')
        {
            if (!lex_quoted(&lexer, start, '\'', VC_TOKEN_CHARACTER))
                return false;
            continue;
        }

        if (!lex_operator_or_punctuation(&lexer, start))
            return false;
    }

    const VcSourceLocation end = location(&lexer);
    return push_token(&lexer, VC_TOKEN_EOF, end);
}

VcSourceSpan vc_source_span_at_location(const VcSource *source, VcSourceLocation where)
{
    VcSourceSpan span = {
        .start = where,
        .end = where
    };

    if (source == NULL)
        return span;

    VcTokenList tokens;
    vc_token_list_init(&tokens);
    if (!vc_lex_source(source, &tokens))
    {
        vc_token_list_destroy(&tokens);
        return span;
    }

    for (size_t i = 0; i < tokens.count; i++)
    {
        if (tokens.items[i].location.offset == where.offset)
        {
            span = tokens.items[i].span;
            break;
        }
    }

    vc_token_list_destroy(&tokens);
    return span;
}

const char *vc_token_kind_name(VcTokenKind kind)
{
    static const char *const names[] = {
        [VC_TOKEN_EOF] = "eof",
        [VC_TOKEN_IDENTIFIER] = "identifier",
        [VC_TOKEN_NUMBER] = "number",
        [VC_TOKEN_STRING] = "string",
        [VC_TOKEN_CHARACTER] = "character",
        [VC_TOKEN_INTERPOLATED_START] = "interpolated-string-start",
        [VC_TOKEN_INTERPOLATED_TEXT] = "interpolated-string-text",
        [VC_TOKEN_INTERPOLATION_START] = "interpolation-start",
        [VC_TOKEN_INTERPOLATION_END] = "interpolation-end",
        [VC_TOKEN_INTERPOLATED_END] = "interpolated-string-end",
        [VC_TOKEN_KW_ABSTRACT] = "abstract",
        [VC_TOKEN_KW_AS] = "as",
        [VC_TOKEN_KW_BASE] = "base",
        [VC_TOKEN_KW_BOOL] = "bool",
        [VC_TOKEN_KW_BREAK] = "break",
        [VC_TOKEN_KW_BYTE] = "byte",
        [VC_TOKEN_KW_CASE] = "case",
        [VC_TOKEN_KW_CATCH] = "catch",
        [VC_TOKEN_KW_CHAR] = "char",
        [VC_TOKEN_KW_CLASS] = "class",
        [VC_TOKEN_KW_CONST] = "const",
        [VC_TOKEN_KW_CONTINUE] = "continue",
        [VC_TOKEN_KW_DECIMAL] = "decimal",
        [VC_TOKEN_KW_DEFAULT] = "default",
        [VC_TOKEN_KW_DELEGATE] = "delegate",
        [VC_TOKEN_KW_DO] = "do",
        [VC_TOKEN_KW_DOUBLE] = "double",
        [VC_TOKEN_KW_ELSE] = "else",
        [VC_TOKEN_KW_ENUM] = "enum",
        [VC_TOKEN_KW_EVENT] = "event",
        [VC_TOKEN_KW_EXPLICIT] = "explicit",
        [VC_TOKEN_KW_EXTERN] = "extern",
        [VC_TOKEN_KW_FALSE] = "false",
        [VC_TOKEN_KW_FINALLY] = "finally",
        [VC_TOKEN_KW_FIXED] = "fixed",
        [VC_TOKEN_KW_FLOAT] = "float",
        [VC_TOKEN_KW_FOR] = "for",
        [VC_TOKEN_KW_FOREACH] = "foreach",
        [VC_TOKEN_KW_GET] = "get",
        [VC_TOKEN_KW_IF] = "if",
        [VC_TOKEN_KW_IMPLICIT] = "implicit",
        [VC_TOKEN_KW_IN] = "in",
        [VC_TOKEN_KW_INT] = "int",
        [VC_TOKEN_KW_INTERFACE] = "interface",
        [VC_TOKEN_KW_INTERNAL] = "internal",
        [VC_TOKEN_KW_IS] = "is",
        [VC_TOKEN_KW_LOCK] = "lock",
        [VC_TOKEN_KW_LONG] = "long",
        [VC_TOKEN_KW_NAMESPACE] = "namespace",
        [VC_TOKEN_KW_NATIVE] = "native",
        [VC_TOKEN_KW_NEW] = "new",
        [VC_TOKEN_KW_NULL] = "null",
        [VC_TOKEN_KW_OBJECT] = "object",
        [VC_TOKEN_KW_OPERATOR] = "operator",
        [VC_TOKEN_KW_OUT] = "out",
        [VC_TOKEN_KW_OVERRIDE] = "override",
        [VC_TOKEN_KW_PARAMS] = "params",
        [VC_TOKEN_KW_PRIVATE] = "private",
        [VC_TOKEN_KW_PROTECTED] = "protected",
        [VC_TOKEN_KW_PUBLIC] = "public",
        [VC_TOKEN_KW_READONLY] = "readonly",
        [VC_TOKEN_KW_REF] = "ref",
        [VC_TOKEN_KW_RETURN] = "return",
        [VC_TOKEN_KW_SBYTE] = "sbyte",
        [VC_TOKEN_KW_SEALED] = "sealed",
        [VC_TOKEN_KW_SET] = "set",
        [VC_TOKEN_KW_SHORT] = "short",
        [VC_TOKEN_KW_SIZEOF] = "sizeof",
        [VC_TOKEN_KW_STACKALLOC] = "stackalloc",
        [VC_TOKEN_KW_STATIC] = "static",
        [VC_TOKEN_KW_STRING] = "string",
        [VC_TOKEN_KW_STRUCT] = "struct",
        [VC_TOKEN_KW_SWITCH] = "switch",
        [VC_TOKEN_KW_THIS] = "this",
        [VC_TOKEN_KW_THROW] = "throw",
        [VC_TOKEN_KW_TRUE] = "true",
        [VC_TOKEN_KW_TRY] = "try",
        [VC_TOKEN_KW_TYPEOF] = "typeof",
        [VC_TOKEN_KW_UINT] = "uint",
        [VC_TOKEN_KW_ULONG] = "ulong",
        [VC_TOKEN_KW_UNSAFE] = "unsafe",
        [VC_TOKEN_KW_USHORT] = "ushort",
        [VC_TOKEN_KW_USING] = "using",
        [VC_TOKEN_KW_VAR] = "var",
        [VC_TOKEN_KW_VIRTUAL] = "virtual",
        [VC_TOKEN_KW_VOID] = "void",
        [VC_TOKEN_KW_WHERE] = "where",
        [VC_TOKEN_KW_WHILE] = "while",
        [VC_TOKEN_KW_YIELD] = "yield",
        [VC_TOKEN_LEFT_PAREN] = "(",
        [VC_TOKEN_RIGHT_PAREN] = ")",
        [VC_TOKEN_LEFT_BRACE] = "{",
        [VC_TOKEN_RIGHT_BRACE] = "}",
        [VC_TOKEN_LEFT_BRACKET] = "[",
        [VC_TOKEN_RIGHT_BRACKET] = "]",
        [VC_TOKEN_COMMA] = ",",
        [VC_TOKEN_DOT] = ".",
        [VC_TOKEN_DOT_DOT] = "..",
        [VC_TOKEN_SEMICOLON] = ";",
        [VC_TOKEN_COLON] = ":",
        [VC_TOKEN_COLON_COLON] = "::",
        [VC_TOKEN_QUESTION] = "?",
        [VC_TOKEN_QUESTION_DOT] = "?.",
        [VC_TOKEN_QUESTION_QUESTION] = "??",
        [VC_TOKEN_QUESTION_QUESTION_EQUAL] = "?\?=",
        [VC_TOKEN_AT] = "@",
        [VC_TOKEN_PLUS] = "+",
        [VC_TOKEN_PLUS_PLUS] = "++",
        [VC_TOKEN_PLUS_EQUAL] = "+=",
        [VC_TOKEN_MINUS] = "-",
        [VC_TOKEN_MINUS_MINUS] = "--",
        [VC_TOKEN_MINUS_EQUAL] = "-=",
        [VC_TOKEN_ARROW] = "->",
        [VC_TOKEN_STAR] = "*",
        [VC_TOKEN_STAR_EQUAL] = "*=",
        [VC_TOKEN_SLASH] = "/",
        [VC_TOKEN_SLASH_EQUAL] = "/=",
        [VC_TOKEN_PERCENT] = "%",
        [VC_TOKEN_PERCENT_EQUAL] = "%=",
        [VC_TOKEN_EQUAL] = "=",
        [VC_TOKEN_EQUAL_EQUAL] = "==",
        [VC_TOKEN_FAT_ARROW] = "=>",
        [VC_TOKEN_BANG] = "!",
        [VC_TOKEN_BANG_EQUAL] = "!=",
        [VC_TOKEN_LESS] = "<",
        [VC_TOKEN_LESS_EQUAL] = "<=",
        [VC_TOKEN_LESS_LESS] = "<<",
        [VC_TOKEN_LESS_LESS_EQUAL] = "<<=",
        [VC_TOKEN_GREATER] = ">",
        [VC_TOKEN_GREATER_EQUAL] = ">=",
        [VC_TOKEN_GREATER_GREATER] = ">>",
        [VC_TOKEN_GREATER_GREATER_EQUAL] = ">>=",
        [VC_TOKEN_AMPERSAND] = "&",
        [VC_TOKEN_AMPERSAND_AMPERSAND] = "&&",
        [VC_TOKEN_AMPERSAND_EQUAL] = "&=",
        [VC_TOKEN_PIPE] = "|",
        [VC_TOKEN_PIPE_PIPE] = "||",
        [VC_TOKEN_PIPE_EQUAL] = "|=",
        [VC_TOKEN_CARET] = "^",
        [VC_TOKEN_CARET_EQUAL] = "^=",
        [VC_TOKEN_TILDE] = "~"
    };

    const size_t count = sizeof(names) / sizeof(names[0]);
    if ((size_t)kind >= count || names[kind] == NULL)
        return "unknown";

    return names[kind];
}
