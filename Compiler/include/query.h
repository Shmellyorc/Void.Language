#ifndef VOIDC_QUERY_H
#define VOIDC_QUERY_H

#include "compiler.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct VcQuerySession VcQuerySession;

typedef enum VcQuerySymbolKind
{
    VC_QUERY_SYMBOL_NONE,
    VC_QUERY_SYMBOL_LOCAL,
    VC_QUERY_SYMBOL_PARAMETER,
    VC_QUERY_SYMBOL_FIELD,
    VC_QUERY_SYMBOL_PROPERTY,
    VC_QUERY_SYMBOL_METHOD,
    VC_QUERY_SYMBOL_CONSTRUCTOR,
    VC_QUERY_SYMBOL_TYPE,
    VC_QUERY_SYMBOL_NAMESPACE,
    VC_QUERY_SYMBOL_ENUM_MEMBER,
    VC_QUERY_SYMBOL_EVENT,
    VC_QUERY_SYMBOL_CONSTANT
} VcQuerySymbolKind;

typedef struct VcQuerySymbol
{
    VcQuerySymbolKind kind;
    const char *name;
    const char *type_name;
    char type_name_storage[96];
    const char *path;
    VcSourceSpan span;
    const char *definition_path;
    VcSourceSpan definition_span;
    bool is_static;
    size_t parameter_count;
} VcQuerySymbol;

typedef struct VcQueryMember
{
    VcQuerySymbolKind kind;
    const char *name;
    const char *type_name;
    bool is_static;
    size_t parameter_count;
} VcQueryMember;

typedef struct VcQueryParameter
{
    const char *name;
    const char *type_name;
    char type_name_storage[96];
    const char *modifier;
    bool is_optional;
    bool is_params;
} VcQueryParameter;

typedef struct VcQuerySignature
{
    VcQuerySymbolKind kind;
    const char *name;
    const char *return_type;
    char return_type_storage[96];
    bool is_static;
    size_t generic_parameter_count;
    size_t parameter_count;
} VcQuerySignature;

typedef struct VcQueryCompletion
{
    VcQuerySymbolKind kind;
    const char *name;
    char type_name[96];
    bool is_static;
    size_t parameter_count;
} VcQueryCompletion;

typedef struct VcQueryNamedSymbol
{
    VcQuerySymbolKind kind;
    const char *name;
    const char *container_name;
    const char *path;
    VcSourceSpan span;
} VcQueryNamedSymbol;

typedef struct VcQueryLocation
{
    const char *path;
    VcSourceSpan span;
} VcQueryLocation;

VcQuerySession *vc_query_session_create(
    const VcProject *project,
    const char *tool_root,
    char *error,
    size_t error_size);

VcQuerySession *vc_query_session_create_text(
    const VcProject *project,
    const char *tool_root,
    const char *source_path,
    const char *source_text,
    char *error,
    size_t error_size);

VcQuerySession *vc_query_session_create_texts(
    const VcProject *project,
    const char *tool_root,
    const VcSourceOverride *overrides,
    size_t override_count,
    char *error,
    size_t error_size);

void vc_query_session_destroy(VcQuerySession *session);

bool vc_query_symbol_at(
    const VcQuerySession *session,
    const char *path,
    size_t offset,
    VcQuerySymbol *symbol);

bool vc_query_type_at(
    const VcQuerySession *session,
    const char *path,
    size_t offset,
    const char **type_name);

bool vc_query_definition_at(
    const VcQuerySession *session,
    const char *path,
    size_t offset,
    const char **definition_path,
    VcSourceSpan *definition_span);

size_t vc_query_members_at(
    const VcQuerySession *session,
    const char *path,
    size_t offset,
    VcQueryMember *members,
    size_t capacity);

bool vc_query_parameter_at(
    const VcQuerySession *session,
    const char *path,
    size_t offset,
    size_t parameter_index,
    VcQueryParameter *parameter);

size_t vc_query_signatures_at(
    const VcQuerySession *session,
    const char *path,
    size_t offset,
    VcQuerySignature *signatures,
    size_t capacity,
    size_t *selected_signature,
    size_t *active_parameter);

bool vc_query_signature_parameter_at(
    const VcQuerySession *session,
    const char *path,
    size_t offset,
    size_t signature_index,
    size_t parameter_index,
    VcQueryParameter *parameter);

bool vc_query_signature_generic_parameter_at(
    const VcQuerySession *session,
    const char *path,
    size_t offset,
    size_t signature_index,
    size_t generic_parameter_index,
    const char **name);

size_t vc_query_completions_at(
    const VcQuerySession *session,
    const char *path,
    size_t offset,
    VcQueryCompletion *items,
    size_t capacity);

const char *vc_query_source_text(
    const VcQuerySession *session,
    const char *path);

size_t vc_query_document_symbols(
    const VcQuerySession *session,
    const char *path,
    VcQueryNamedSymbol *symbols,
    size_t capacity);

size_t vc_query_workspace_symbols(
    const VcQuerySession *session,
    const char *query,
    VcQueryNamedSymbol *symbols,
    size_t capacity);

size_t vc_query_references_at(
    const VcQuerySession *session,
    const char *path,
    size_t offset,
    bool include_definition,
    VcQueryLocation *locations,
    size_t capacity);

#endif
