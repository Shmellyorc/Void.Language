#ifndef VOIDC_LEXER_H
#define VOIDC_LEXER_H

#include <stdbool.h>
#include <stddef.h>

typedef struct VcSource
{
    char *path;
    char *text;
    size_t length;
} VcSource;

typedef struct VcSourceLocation
{
    size_t offset;
    size_t line;
    size_t column;
} VcSourceLocation;

typedef struct VcSourceSpan
{
    VcSourceLocation start;
    VcSourceLocation end;
} VcSourceSpan;

typedef enum VcTokenKind
{
    VC_TOKEN_EOF,
    VC_TOKEN_IDENTIFIER,
    VC_TOKEN_NUMBER,
    VC_TOKEN_STRING,
    VC_TOKEN_CHARACTER,
    VC_TOKEN_INTERPOLATED_START,
    VC_TOKEN_INTERPOLATED_TEXT,
    VC_TOKEN_INTERPOLATION_START,
    VC_TOKEN_INTERPOLATION_END,
    VC_TOKEN_INTERPOLATED_END,

    VC_TOKEN_KW_ABSTRACT,
    VC_TOKEN_KW_AS,
    VC_TOKEN_KW_BASE,
    VC_TOKEN_KW_BOOL,
    VC_TOKEN_KW_BREAK,
    VC_TOKEN_KW_BYTE,
    VC_TOKEN_KW_CASE,
    VC_TOKEN_KW_CATCH,
    VC_TOKEN_KW_CHAR,
    VC_TOKEN_KW_CLASS,
    VC_TOKEN_KW_CONST,
    VC_TOKEN_KW_CONTINUE,
    VC_TOKEN_KW_DECIMAL,
    VC_TOKEN_KW_DEFAULT,
    VC_TOKEN_KW_DELEGATE,
    VC_TOKEN_KW_DO,
    VC_TOKEN_KW_DOUBLE,
    VC_TOKEN_KW_ELSE,
    VC_TOKEN_KW_ENUM,
    VC_TOKEN_KW_EVENT,
    VC_TOKEN_KW_EXPLICIT,
    VC_TOKEN_KW_EXTERN,
    VC_TOKEN_KW_FALSE,
    VC_TOKEN_KW_FINALLY,
    VC_TOKEN_KW_FIXED,
    VC_TOKEN_KW_FLOAT,
    VC_TOKEN_KW_FOR,
    VC_TOKEN_KW_FOREACH,
    VC_TOKEN_KW_GET,
    VC_TOKEN_KW_IF,
    VC_TOKEN_KW_IMPLICIT,
    VC_TOKEN_KW_IN,
    VC_TOKEN_KW_INT,
    VC_TOKEN_KW_INTERFACE,
    VC_TOKEN_KW_INTERNAL,
    VC_TOKEN_KW_IS,
    VC_TOKEN_KW_LOCK,
    VC_TOKEN_KW_LONG,
    VC_TOKEN_KW_NAMESPACE,
    VC_TOKEN_KW_NATIVE,
    VC_TOKEN_KW_NEW,
    VC_TOKEN_KW_NULL,
    VC_TOKEN_KW_OBJECT,
    VC_TOKEN_KW_OPERATOR,
    VC_TOKEN_KW_OUT,
    VC_TOKEN_KW_OVERRIDE,
    VC_TOKEN_KW_PARAMS,
    VC_TOKEN_KW_PRIVATE,
    VC_TOKEN_KW_PROTECTED,
    VC_TOKEN_KW_PUBLIC,
    VC_TOKEN_KW_READONLY,
    VC_TOKEN_KW_REF,
    VC_TOKEN_KW_RETURN,
    VC_TOKEN_KW_SBYTE,
    VC_TOKEN_KW_SEALED,
    VC_TOKEN_KW_SET,
    VC_TOKEN_KW_SHORT,
    VC_TOKEN_KW_SIZEOF,
    VC_TOKEN_KW_STACKALLOC,
    VC_TOKEN_KW_STATIC,
    VC_TOKEN_KW_STRING,
    VC_TOKEN_KW_STRUCT,
    VC_TOKEN_KW_SWITCH,
    VC_TOKEN_KW_THIS,
    VC_TOKEN_KW_THROW,
    VC_TOKEN_KW_TRUE,
    VC_TOKEN_KW_TRY,
    VC_TOKEN_KW_TYPEOF,
    VC_TOKEN_KW_UINT,
    VC_TOKEN_KW_ULONG,
    VC_TOKEN_KW_UNSAFE,
    VC_TOKEN_KW_USHORT,
    VC_TOKEN_KW_USING,
    VC_TOKEN_KW_VAR,
    VC_TOKEN_KW_VIRTUAL,
    VC_TOKEN_KW_VOID,
    VC_TOKEN_KW_WHERE,
    VC_TOKEN_KW_WHILE,
    VC_TOKEN_KW_YIELD,

    VC_TOKEN_LEFT_PAREN,
    VC_TOKEN_RIGHT_PAREN,
    VC_TOKEN_LEFT_BRACE,
    VC_TOKEN_RIGHT_BRACE,
    VC_TOKEN_LEFT_BRACKET,
    VC_TOKEN_RIGHT_BRACKET,
    VC_TOKEN_COMMA,
    VC_TOKEN_DOT,
    VC_TOKEN_DOT_DOT,
    VC_TOKEN_SEMICOLON,
    VC_TOKEN_COLON,
    VC_TOKEN_COLON_COLON,
    VC_TOKEN_QUESTION,
    VC_TOKEN_QUESTION_DOT,
    VC_TOKEN_QUESTION_QUESTION,
    VC_TOKEN_QUESTION_QUESTION_EQUAL,
    VC_TOKEN_AT,

    VC_TOKEN_PLUS,
    VC_TOKEN_PLUS_PLUS,
    VC_TOKEN_PLUS_EQUAL,
    VC_TOKEN_MINUS,
    VC_TOKEN_MINUS_MINUS,
    VC_TOKEN_MINUS_EQUAL,
    VC_TOKEN_ARROW,
    VC_TOKEN_STAR,
    VC_TOKEN_STAR_EQUAL,
    VC_TOKEN_SLASH,
    VC_TOKEN_SLASH_EQUAL,
    VC_TOKEN_PERCENT,
    VC_TOKEN_PERCENT_EQUAL,

    VC_TOKEN_EQUAL,
    VC_TOKEN_EQUAL_EQUAL,
    VC_TOKEN_FAT_ARROW,
    VC_TOKEN_BANG,
    VC_TOKEN_BANG_EQUAL,
    VC_TOKEN_LESS,
    VC_TOKEN_LESS_EQUAL,
    VC_TOKEN_LESS_LESS,
    VC_TOKEN_LESS_LESS_EQUAL,
    VC_TOKEN_GREATER,
    VC_TOKEN_GREATER_EQUAL,
    VC_TOKEN_GREATER_GREATER,
    VC_TOKEN_GREATER_GREATER_EQUAL,

    VC_TOKEN_AMPERSAND,
    VC_TOKEN_AMPERSAND_AMPERSAND,
    VC_TOKEN_AMPERSAND_EQUAL,
    VC_TOKEN_PIPE,
    VC_TOKEN_PIPE_PIPE,
    VC_TOKEN_PIPE_EQUAL,
    VC_TOKEN_CARET,
    VC_TOKEN_CARET_EQUAL,
    VC_TOKEN_TILDE
} VcTokenKind;

typedef struct VcToken
{
    VcTokenKind kind;
    VcSourceLocation location;
    VcSourceSpan span;
    size_t length;
} VcToken;

typedef enum VcDiagnosticSeverity
{
    VC_DIAGNOSTIC_ERROR,
    VC_DIAGNOSTIC_WARNING,
    VC_DIAGNOSTIC_NOTE,
    VC_DIAGNOSTIC_HELP
} VcDiagnosticSeverity;

/* Bounded, value-owned context; locations borrow compilation source paths. */
#define VC_DIAGNOSTIC_CONTEXT_LIMIT 8
typedef struct VcDiagnosticContext
{
    VcDiagnosticSeverity severity;
    const char *path;
    VcSourceSpan span;
    const VcSource *source;
    char message[512];
} VcDiagnosticContext;

typedef struct VcDiagnostic
{
    const char *path;
    VcSourceSpan span;
    const VcSource *source;
    char message[2048];
    VcDiagnosticSeverity severity;
    const char *code;
    VcDiagnosticContext context[VC_DIAGNOSTIC_CONTEXT_LIMIT];
    size_t context_count;
    bool context_truncated;
} VcDiagnostic;

typedef struct VcTokenList
{
    VcToken *items;
    size_t count;
    size_t capacity;
    bool has_error;
    VcDiagnostic diagnostic;
} VcTokenList;

void vc_source_init(VcSource *source);
void vc_source_destroy(VcSource *source);
bool vc_source_load(const char *path, VcSource *source, char *error, size_t error_size);
VcSourceSpan vc_source_span_at_location(const VcSource *source, VcSourceLocation location);

void vc_token_list_init(VcTokenList *tokens);
void vc_token_list_destroy(VcTokenList *tokens);
bool vc_lex_source(const VcSource *source, VcTokenList *tokens);

const char *vc_token_kind_name(VcTokenKind kind);

#endif
