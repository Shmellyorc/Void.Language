#include "../../Runtime/include/vc_utf8.h"
#include "lsp.h"
#include "compiler.h"
#include "query.h"
#include "voidc.h"
#include "../../Runtime/include/vc_environment.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct VcLspSlice
{
    const char *start;
    size_t length;
} VcLspSlice;

typedef struct VcLspDocument
{
    char *uri;
    char *path;
    char *text;
    int version;
    char *project_root;
} VcLspDocument;

typedef struct VcLspServer
{
    VcLspDocument *documents;
    size_t document_count;
    size_t document_capacity;
    bool shutdown_received;
    const char *tool_root;
    const char *version;
} VcLspServer;

static char *lsp_strdup(const char *value)
{
    if (value == NULL)
        return NULL;

    const size_t length = strlen(value);
    char *copy = malloc(length + 1);
    if (copy == NULL)
        return NULL;

    memcpy(copy, value, length + 1);
    return copy;
}

static void document_destroy(VcLspDocument *document)
{
    if (document == NULL)
        return;

    free(document->uri);
    free(document->path);
    free(document->text);
    free(document->project_root);
    memset(document, 0, sizeof(*document));
}

static void server_destroy(VcLspServer *server)
{
    for (size_t i = 0; i < server->document_count; i++)
        document_destroy(&server->documents[i]);
    free(server->documents);
}

static void json_skip_ws(const char **cursor, const char *end)
{
    while (*cursor < end && isspace((unsigned char)**cursor))
        (*cursor)++;
}

static bool json_skip_string(const char **cursor, const char *end)
{
    if (*cursor >= end || **cursor != '"')
        return false;

    (*cursor)++;
    while (*cursor < end)
    {
        const char c = **cursor;
        (*cursor)++;
        if (c == '"')
            return true;
        if (c == '\\')
        {
            if (*cursor >= end)
                return false;
            (*cursor)++;
        }
    }

    return false;
}

static bool json_skip_value(const char **cursor, const char *end)
{
    json_skip_ws(cursor, end);
    if (*cursor >= end)
        return false;

    if (**cursor == '"')
        return json_skip_string(cursor, end);

    if (**cursor == '{' || **cursor == '[')
    {
        const char open = **cursor;
        const char close = open == '{' ? '}' : ']';
        int depth = 0;
        do
        {
            if (**cursor == '"')
            {
                if (!json_skip_string(cursor, end))
                    return false;
                continue;
            }

            if (**cursor == open)
                depth++;
            else if (**cursor == close)
                depth--;
            (*cursor)++;
        }
        while (*cursor < end && depth > 0);
        return depth == 0;
    }

    while (*cursor < end && **cursor != ',' && **cursor != '}' && **cursor != ']')
        (*cursor)++;
    return true;
}

static bool json_string_equals(VcLspSlice raw, const char *value)
{
    if (raw.length < 2 || raw.start[0] != '"' || raw.start[raw.length - 1] != '"')
        return false;

    const size_t length = strlen(value);
    return raw.length == length + 2 && memcmp(raw.start + 1, value, length) == 0;
}

static bool json_object_member(VcLspSlice object, const char *name, VcLspSlice *value)
{
    const char *cursor = object.start;
    const char *end = object.start + object.length;
    json_skip_ws(&cursor, end);
    if (cursor >= end || *cursor != '{')
        return false;
    cursor++;

    while (cursor < end)
    {
        json_skip_ws(&cursor, end);
        if (cursor >= end || *cursor == '}')
            return false;

        const char *key_start = cursor;
        if (!json_skip_string(&cursor, end))
            return false;
        VcLspSlice key = { key_start, (size_t)(cursor - key_start) };

        json_skip_ws(&cursor, end);
        if (cursor >= end || *cursor != ':')
            return false;
        cursor++;
        json_skip_ws(&cursor, end);

        const char *value_start = cursor;
        if (!json_skip_value(&cursor, end))
            return false;

        if (json_string_equals(key, name))
        {
            value->start = value_start;
            value->length = (size_t)(cursor - value_start);
            return true;
        }

        json_skip_ws(&cursor, end);
        if (cursor < end && *cursor == ',')
        {
            cursor++;
            continue;
        }
        if (cursor < end && *cursor == '}')
            return false;
        return false;
    }

    return false;
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static bool append_utf8(char *output, size_t output_size, size_t *length, unsigned value)
{
    char encoded[4];
    const size_t width = vc_utf8_encode_scalar((uint32_t)value, encoded);
    if (width == 0 || *length + width >= output_size)
        return false;
    memcpy(output + *length, encoded, width);
    *length += width;
    return true;
}

static char *json_decode_string(VcLspSlice raw)
{
    if (raw.length < 2 || raw.start[0] != '"' || raw.start[raw.length - 1] != '"')
        return NULL;

    char *output = malloc(raw.length + 1);
    if (output == NULL)
        return NULL;

    size_t out = 0;
    for (size_t i = 1; i + 1 < raw.length; i++)
    {
        unsigned char c = (unsigned char)raw.start[i];
        if (c != '\\')
        {
            output[out++] = (char)c;
            continue;
        }

        i++;
        if (i + 1 > raw.length)
        {
            free(output);
            return NULL;
        }

        switch (raw.start[i])
        {
            case '"': output[out++] = '"'; break;
            case '\\': output[out++] = '\\'; break;
            case '/': output[out++] = '/'; break;
            case 'b': output[out++] = '\b'; break;
            case 'f': output[out++] = '\f'; break;
            case 'n': output[out++] = '\n'; break;
            case 'r': output[out++] = '\r'; break;
            case 't': output[out++] = '\t'; break;
            case 'u':
            {
                if (i + 4 >= raw.length)
                {
                    free(output);
                    return NULL;
                }
                unsigned value = 0;
                for (size_t j = 0; j < 4; j++)
                {
                    const int digit = hex_value(raw.start[++i]);
                    if (digit < 0)
                    {
                        free(output);
                        return NULL;
                    }
                    value = (value << 4) | (unsigned)digit;
                }
                if (value >= 0xD800u && value <= 0xDBFFu)
                {
                    if (i + 6 >= raw.length - 1 || raw.start[i + 1] != '\\' ||
                        raw.start[i + 2] != 'u')
                    {
                        free(output);
                        return NULL;
                    }
                    i += 2;
                    unsigned low = 0;
                    for (size_t j = 0; j < 4; j++)
                    {
                        const int digit = hex_value(raw.start[++i]);
                        if (digit < 0)
                        {
                            free(output);
                            return NULL;
                        }
                        low = (low << 4) | (unsigned)digit;
                    }
                    if (low < 0xDC00u || low > 0xDFFFu)
                    {
                        free(output);
                        return NULL;
                    }
                    value = 0x10000u + ((value - 0xD800u) << 10) + (low - 0xDC00u);
                }
                if (!append_utf8(output, raw.length + 1, &out, value))
                {
                    free(output);
                    return NULL;
                }
                break;
            }
            default:
                free(output);
                return NULL;
        }
    }

    output[out] = '\0';
    return output;
}

static bool json_int(VcLspSlice raw, int *value)
{
    char buffer[64];
    if (raw.length == 0 || raw.length >= sizeof(buffer))
        return false;
    memcpy(buffer, raw.start, raw.length);
    buffer[raw.length] = '\0';

    char *end = NULL;
    errno = 0;
    const long parsed = strtol(buffer, &end, 10);
    if (errno != 0 || end == buffer || *end != '\0')
        return false;
    *value = (int)parsed;
    return true;
}

static bool json_bool(VcLspSlice raw, bool *value)
{
    if (raw.length == 4 && memcmp(raw.start, "true", 4) == 0)
    {
        *value = true;
        return true;
    }
    if (raw.length == 5 && memcmp(raw.start, "false", 5) == 0)
    {
        *value = false;
        return true;
    }
    return false;
}

static bool json_array_first(VcLspSlice array, VcLspSlice *value)
{
    const char *cursor = array.start;
    const char *end = array.start + array.length;
    json_skip_ws(&cursor, end);
    if (cursor >= end || *cursor != '[')
        return false;
    cursor++;
    json_skip_ws(&cursor, end);
    if (cursor >= end || *cursor == ']')
        return false;

    const char *start = cursor;
    if (!json_skip_value(&cursor, end))
        return false;
    value->start = start;
    value->length = (size_t)(cursor - start);
    return true;
}

static bool write_message(const char *json)
{
    const size_t length = strlen(json);
    if (printf("Content-Length: %zu\r\n\r\n", length) < 0)
        return false;
    if (fwrite(json, 1, length, stdout) != length)
        return false;
    return fflush(stdout) == 0;
}

typedef struct VcLspPosition
{
    size_t line;
    size_t character;
} VcLspPosition;

static VcLspPosition lsp_position_at(const char *text, size_t offset)
{
    VcLspPosition position = {0, 0};
    if (text == NULL)
        return position;

    size_t index = 0;
    while (text[index] != '\0' && index < offset)
    {
        const unsigned char first = (unsigned char)text[index];
        if (first == '\n')
        {
            position.line++;
            position.character = 0;
            index++;
            continue;
        }

        size_t length = 1;
        unsigned codepoint = first;
        if ((first & 0xE0u) == 0xC0u && text[index + 1] != '\0')
        {
            length = 2;
            codepoint = ((unsigned)(first & 0x1Fu) << 6) |
                (unsigned)((unsigned char)text[index + 1] & 0x3Fu);
        }
        else if ((first & 0xF0u) == 0xE0u && text[index + 1] != '\0' && text[index + 2] != '\0')
        {
            length = 3;
            codepoint = ((unsigned)(first & 0x0Fu) << 12) |
                ((unsigned)((unsigned char)text[index + 1] & 0x3Fu) << 6) |
                (unsigned)((unsigned char)text[index + 2] & 0x3Fu);
        }
        else if ((first & 0xF8u) == 0xF0u && text[index + 1] != '\0' &&
            text[index + 2] != '\0' && text[index + 3] != '\0')
        {
            length = 4;
            codepoint = ((unsigned)(first & 0x07u) << 18) |
                ((unsigned)((unsigned char)text[index + 1] & 0x3Fu) << 12) |
                ((unsigned)((unsigned char)text[index + 2] & 0x3Fu) << 6) |
                (unsigned)((unsigned char)text[index + 3] & 0x3Fu);
        }

        if (index + length > offset)
            break;
        position.character += codepoint > 0xFFFFu ? 2u : 1u;
        index += length;
    }
    return position;
}

static bool lsp_offset_at(
    const char *text,
    size_t target_line,
    size_t target_character,
    size_t *offset)
{
    if (text == NULL || offset == NULL)
        return false;

    size_t line = 0;
    size_t character = 0;
    size_t index = 0;
    while (text[index] != '\0')
    {
        if (line == target_line && character >= target_character)
        {
            *offset = index;
            return character == target_character;
        }

        const unsigned char first = (unsigned char)text[index];
        if (first == '\n')
        {
            if (line == target_line)
                return target_character == character && ((*offset = index), true);
            line++;
            character = 0;
            index++;
            continue;
        }

        size_t length = 1;
        unsigned codepoint = first;
        if ((first & 0xE0u) == 0xC0u && text[index + 1] != '\0')
        {
            length = 2;
            codepoint = ((unsigned)(first & 0x1Fu) << 6) |
                (unsigned)((unsigned char)text[index + 1] & 0x3Fu);
        }
        else if ((first & 0xF0u) == 0xE0u && text[index + 1] != '\0' && text[index + 2] != '\0')
        {
            length = 3;
            codepoint = ((unsigned)(first & 0x0Fu) << 12) |
                ((unsigned)((unsigned char)text[index + 1] & 0x3Fu) << 6) |
                (unsigned)((unsigned char)text[index + 2] & 0x3Fu);
        }
        else if ((first & 0xF8u) == 0xF0u && text[index + 1] != '\0' &&
            text[index + 2] != '\0' && text[index + 3] != '\0')
        {
            length = 4;
            codepoint = ((unsigned)(first & 0x07u) << 18) |
                ((unsigned)((unsigned char)text[index + 1] & 0x3Fu) << 12) |
                ((unsigned)((unsigned char)text[index + 2] & 0x3Fu) << 6) |
                (unsigned)((unsigned char)text[index + 3] & 0x3Fu);
        }

        const size_t units = codepoint > 0xFFFFu ? 2u : 1u;
        if (line == target_line && character + units > target_character)
            return false;
        character += units;
        index += length;
    }

    if (line == target_line && character == target_character)
    {
        *offset = index;
        return true;
    }
    return false;
}

static char *json_escape(const char *value)
{
    size_t size = 1;
    for (size_t i = 0; value[i] != '\0'; i++)
    {
        const unsigned char c = (unsigned char)value[i];
        size += (c == '"' || c == '\\' || c < 32) ? 2 : 1;
    }

    char *output = malloc(size);
    if (output == NULL)
        return NULL;

    size_t out = 0;
    for (size_t i = 0; value[i] != '\0'; i++)
    {
        const unsigned char c = (unsigned char)value[i];
        switch (c)
        {
            case '"': output[out++] = '\\'; output[out++] = '"'; break;
            case '\\': output[out++] = '\\'; output[out++] = '\\'; break;
            case '\n': output[out++] = '\\'; output[out++] = 'n'; break;
            case '\r': output[out++] = '\\'; output[out++] = 'r'; break;
            case '\t': output[out++] = '\\'; output[out++] = 't'; break;
            default: output[out++] = (char)c; break;
        }
    }
    output[out] = '\0';
    return output;
}

static char *path_to_uri(const char *path)
{
    if (path == NULL)
        return NULL;

    bool windows_drive = false;
#ifdef _WIN32
    windows_drive = isalpha((unsigned char)path[0]) && path[1] == ':' &&
        (path[2] == '/' || path[2] == '\\');
#endif
    size_t needed = strlen("file://") + 1 + (windows_drive ? 1 : 0);
    for (size_t i = 0; path[i] != '\0'; i++)
    {
        unsigned char c = (unsigned char)path[i];
#ifdef _WIN32
        if (c == '\\') c = '/';
#endif
        needed += (isalnum(c) || c == '/' || c == '-' || c == '_' || c == '.' || c == '~' ||
                   (windows_drive && i == 1)) ? 1 : 3;
    }

    char *uri = malloc(needed);
    if (uri == NULL)
        return NULL;
    size_t out = 0;
    memcpy(uri + out, "file://", strlen("file://"));
    out += strlen("file://");
    if (windows_drive) uri[out++] = '/';
    static const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; path[i] != '\0'; i++)
    {
        unsigned char c = (unsigned char)path[i];
#ifdef _WIN32
        if (c == '\\') c = '/';
#endif
        if (isalnum(c) || c == '/' || c == '-' || c == '_' || c == '.' || c == '~' ||
            (windows_drive && i == 1))
            uri[out++] = (char)c;
        else
        {
            uri[out++] = '%';
            uri[out++] = hex[c >> 4];
            uri[out++] = hex[c & 0x0Fu];
        }
    }
    uri[out] = '\0';
    return uri;
}

static bool send_result(VcLspSlice id, const char *result)
{
    const size_t needed = id.length + strlen(result) + 40;
    char *message = malloc(needed);
    if (message == NULL)
        return false;
    const int written = snprintf(message, needed,
        "{\"jsonrpc\":\"2.0\",\"id\":%.*s,\"result\":%s}",
        (int)id.length, id.start, result);
    const bool ok = written >= 0 && (size_t)written < needed && write_message(message);
    free(message);
    return ok;
}

static bool send_method_not_found(VcLspSlice id, const char *method)
{
    char *escaped = json_escape(method);
    if (escaped == NULL)
        return false;
    const size_t needed = id.length + strlen(escaped) + 96;
    char *message = malloc(needed);
    if (message == NULL)
    {
        free(escaped);
        return false;
    }
    const int written = snprintf(message, needed,
        "{\"jsonrpc\":\"2.0\",\"id\":%.*s,\"error\":{\"code\":-32601,\"message\":\"Method not found: %s\"}}",
        (int)id.length, id.start, escaped);
    free(escaped);
    const bool ok = written >= 0 && (size_t)written < needed && write_message(message);
    free(message);
    return ok;
}

static bool append_format(char *buffer, size_t capacity, size_t *length, const char *format, ...)
{
    if (*length >= capacity)
        return false;
    va_list arguments;
    va_start(arguments, format);
    const int written = vsnprintf(buffer + *length, capacity - *length, format, arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= capacity - *length)
        return false;
    *length += (size_t)written;
    return true;
}

static bool send_diagnostics(
    const VcLspDocument *document,
    const VcCheckDiagnostic *diagnostic)
{
    char *escaped_uri = json_escape(document->uri);
    if (escaped_uri == NULL)
        return false;

    if (diagnostic == NULL)
    {
        const size_t needed = strlen(escaped_uri) + 160;
        char *message = malloc(needed);
        if (message == NULL)
        {
            free(escaped_uri);
            return false;
        }
        const int written = snprintf(message, needed,
            "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\","
            "\"params\":{\"uri\":\"%s\",\"version\":%d,\"diagnostics\":[]}}",
            escaped_uri, document->version);
        free(escaped_uri);
        const bool ok = written >= 0 && (size_t)written < needed && write_message(message);
        free(message);
        return ok;
    }

    char *escaped_message = json_escape(diagnostic->message);
    if (escaped_message == NULL)
    {
        free(escaped_uri);
        return false;
    }

    const VcLspPosition start = lsp_position_at(document->text, diagnostic->span.start.offset);
    const VcLspPosition end = lsp_position_at(document->text, diagnostic->span.end.offset);
    const size_t needed = strlen(escaped_uri) + strlen(escaped_message) + 384;
    char *message = malloc(needed);
    if (message == NULL)
    {
        free(escaped_uri);
        free(escaped_message);
        return false;
    }
    const int written = snprintf(message, needed,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\","
        "\"params\":{\"uri\":\"%s\",\"version\":%d,\"diagnostics\":[{"
        "\"range\":{\"start\":{\"line\":%zu,\"character\":%zu},"
        "\"end\":{\"line\":%zu,\"character\":%zu}},"
        "\"severity\":1,\"source\":\"voidc\",\"message\":\"%s\"}]}}",
        escaped_uri,
        document->version,
        start.line,
        start.character,
        end.line,
        end.character,
        escaped_message);
    free(escaped_uri);
    free(escaped_message);
    const bool ok = written >= 0 && (size_t)written < needed && write_message(message);
    free(message);
    return ok;
}

static bool uri_to_path(const char *uri, char *path, size_t path_size)
{
    const char prefix[] = "file://";
    if (strncmp(uri, prefix, sizeof(prefix) - 1) != 0)
        return false;

    const char *input = uri + sizeof(prefix) - 1;
    size_t out = 0;
    while (*input != '\0')
    {
        if (out + 1 >= path_size)
            return false;
        if (*input == '%' && isxdigit((unsigned char)input[1]) && isxdigit((unsigned char)input[2]))
        {
            const int high = hex_value(input[1]);
            const int low = hex_value(input[2]);
            path[out++] = (char)((high << 4) | low);
            input += 3;
            continue;
        }
        path[out++] = *input++;
    }
    path[out] = '\0';
#ifdef _WIN32
    /* Strip the URI-only slash after decoding, including an escaped drive colon. */
    if (path[0] == '/' && isalpha((unsigned char)path[1]) && path[2] == ':')
        memmove(path, path + 1, out);
#endif
    return true;
}

static bool path_parent(char *path)
{
    char *slash = strrchr(path, '/');
    if (slash == NULL)
        return false;
    if (slash == path)
    {
        path[1] = '\0';
        return true;
    }
    *slash = '\0';
    return true;
}

static char *discover_project_root(const char *uri)
{
    char path[PATH_MAX];
    if (!uri_to_path(uri, path, sizeof(path)))
        return NULL;
    if (!path_parent(path))
        return NULL;

    for (;;)
    {
        VcProject project;
        vc_project_init(&project);
        char error[1024];
        if (vc_resolve_project(path, &project, error, sizeof(error)))
        {
            char *root = lsp_strdup(project.root);
            vc_project_destroy(&project);
            return root;
        }
        vc_project_destroy(&project);

        if (strcmp(path, "/") == 0 || strcmp(path, ".") == 0 || !path_parent(path))
            break;
    }

    return NULL;
}

static VcLspDocument *find_document(VcLspServer *server, const char *uri)
{
    for (size_t i = 0; i < server->document_count; i++)
    {
        if (strcmp(server->documents[i].uri, uri) == 0)
            return &server->documents[i];
    }
    return NULL;
}

static bool documents_share_project(
    const VcLspDocument *left,
    const VcLspDocument *right)
{
    if (left == NULL || right == NULL)
        return false;
    if (left->project_root != NULL || right->project_root != NULL)
    {
        return left->project_root != NULL && right->project_root != NULL &&
            strcmp(left->project_root, right->project_root) == 0;
    }
    return left->path != NULL && right->path != NULL && strcmp(left->path, right->path) == 0;
}

static bool collect_project_overrides(
    const VcLspServer *server,
    const VcLspDocument *document,
    VcSourceOverride **overrides,
    size_t *override_count)
{
    *overrides = NULL;
    *override_count = 0;

    if (server->document_count == 0)
        return true;

    VcSourceOverride *items = calloc(server->document_count, sizeof(*items));
    if (items == NULL)
        return false;

    for (size_t i = 0; i < server->document_count; i++)
    {
        const VcLspDocument *candidate = &server->documents[i];
        if (!documents_share_project(document, candidate) ||
            candidate->path == NULL || candidate->text == NULL)
            continue;

        items[*override_count].path = candidate->path;
        items[*override_count].text = candidate->text;
        (*override_count)++;
    }

    *overrides = items;
    return true;
}

static bool open_document(VcLspServer *server, const char *uri, const char *text, int version)
{
    VcLspDocument *existing = find_document(server, uri);
    if (existing != NULL)
    {
        char *new_text = lsp_strdup(text);
        if (new_text == NULL)
            return false;
        free(existing->text);
        existing->text = new_text;
        existing->version = version;
        return true;
    }

    if (server->document_count == server->document_capacity)
    {
        const size_t capacity = server->document_capacity == 0 ? 4 : server->document_capacity * 2;
        VcLspDocument *documents = realloc(server->documents, capacity * sizeof(*documents));
        if (documents == NULL)
            return false;
        server->documents = documents;
        server->document_capacity = capacity;
    }

    VcLspDocument *document = &server->documents[server->document_count];
    memset(document, 0, sizeof(*document));
    document->uri = lsp_strdup(uri);
    char path[PATH_MAX];
    if (uri_to_path(uri, path, sizeof(path)))
        document->path = lsp_strdup(path);
    document->text = lsp_strdup(text);
    document->version = version;
    document->project_root = discover_project_root(uri);
    if (document->uri == NULL || document->path == NULL || document->text == NULL)
    {
        document_destroy(document);
        return false;
    }
    server->document_count++;
    return true;
}

static bool publish_document_diagnostics(VcLspServer *server, VcLspDocument *document)
{
    if (document == NULL || document->path == NULL)
        return true;

    VcProject project;
    vc_project_init(&project);
    char error[2048];
    const char *target = document->project_root != NULL ? document->project_root : document->path;
    if (!vc_resolve_project(target, &project, error, sizeof(error)))
    {
        vc_project_destroy(&project);
        return send_diagnostics(document, NULL);
    }

    VcSourceOverride *overrides = NULL;
    size_t override_count = 0;
    if (!collect_project_overrides(server, document, &overrides, &override_count))
    {
        vc_project_destroy(&project);
        return false;
    }

    VcCheckDiagnostic diagnostic;
    const VcCheckStatus status = vc_check_project_texts(
        &project,
        server->tool_root,
        overrides,
        override_count,
        &diagnostic,
        error,
        sizeof(error));
    free(overrides);
    vc_project_destroy(&project);

    if (status == VC_CHECK_DIAGNOSTIC && strcmp(diagnostic.path, document->path) == 0)
        return send_diagnostics(document, &diagnostic);
    return send_diagnostics(document, NULL);
}

static bool publish_project_diagnostics(VcLspServer *server, VcLspDocument *document)
{
    if (document == NULL)
        return true;

    for (size_t i = 0; i < server->document_count; i++)
    {
        if (!documents_share_project(document, &server->documents[i]))
            continue;
        if (!publish_document_diagnostics(server, &server->documents[i]))
            return false;
    }
    return true;
}

static bool change_document(VcLspServer *server, const char *uri, const char *text, int version)
{
    VcLspDocument *document = find_document(server, uri);
    if (document == NULL)
        return open_document(server, uri, text, version);

    char *new_text = lsp_strdup(text);
    if (new_text == NULL)
        return false;
    free(document->text);
    document->text = new_text;
    document->version = version;
    return true;
}

static void close_document(VcLspServer *server, const char *uri)
{
    for (size_t i = 0; i < server->document_count; i++)
    {
        if (strcmp(server->documents[i].uri, uri) != 0)
            continue;
        document_destroy(&server->documents[i]);
        if (i + 1 < server->document_count)
            server->documents[i] = server->documents[server->document_count - 1];
        server->document_count--;
        return;
    }
}

static bool request_document_position(
    VcLspSlice params,
    char **uri,
    size_t *line,
    size_t *character)
{
    VcLspSlice document;
    VcLspSlice raw_uri;
    VcLspSlice position;
    VcLspSlice raw_line;
    VcLspSlice raw_character;
    if (!json_object_member(params, "textDocument", &document) ||
        !json_object_member(document, "uri", &raw_uri) ||
        !json_object_member(params, "position", &position) ||
        !json_object_member(position, "line", &raw_line) ||
        !json_object_member(position, "character", &raw_character))
        return false;

    *uri = json_decode_string(raw_uri);
    if (*uri == NULL)
        return false;

    int parsed_line = 0;
    int parsed_character = 0;
    if (!json_int(raw_line, &parsed_line) || !json_int(raw_character, &parsed_character) ||
        parsed_line < 0 || parsed_character < 0)
    {
        free(*uri);
        *uri = NULL;
        return false;
    }
    *line = (size_t)parsed_line;
    *character = (size_t)parsed_character;
    return true;
}

static VcQuerySession *document_query_session(
    VcLspServer *server,
    const VcLspDocument *document,
    char *error,
    size_t error_size)
{
    if (document == NULL || document->path == NULL || document->text == NULL)
        return NULL;

    VcProject project;
    vc_project_init(&project);
    const char *target = document->project_root != NULL ? document->project_root : document->path;
    if (!vc_resolve_project(target, &project, error, error_size))
    {
        vc_project_destroy(&project);
        return NULL;
    }

    VcSourceOverride *overrides = NULL;
    size_t override_count = 0;
    if (!collect_project_overrides(server, document, &overrides, &override_count))
    {
        vc_project_destroy(&project);
        return NULL;
    }

    VcQuerySession *session = vc_query_session_create_texts(
        &project,
        server->tool_root,
        overrides,
        override_count,
        error,
        error_size);
    free(overrides);
    vc_project_destroy(&project);
    return session;
}

static const char *query_kind_name(VcQuerySymbolKind kind)
{
    switch (kind)
    {
        case VC_QUERY_SYMBOL_LOCAL: return "local";
        case VC_QUERY_SYMBOL_PARAMETER: return "parameter";
        case VC_QUERY_SYMBOL_FIELD: return "field";
        case VC_QUERY_SYMBOL_PROPERTY: return "property";
        case VC_QUERY_SYMBOL_METHOD: return "method";
        case VC_QUERY_SYMBOL_CONSTRUCTOR: return "constructor";
        case VC_QUERY_SYMBOL_TYPE: return "type";
        case VC_QUERY_SYMBOL_NAMESPACE: return "namespace";
        case VC_QUERY_SYMBOL_ENUM_MEMBER: return "enum member";
        case VC_QUERY_SYMBOL_EVENT: return "event";
        case VC_QUERY_SYMBOL_CONSTANT: return "const";
        case VC_QUERY_SYMBOL_NONE: break;
    }
    return "symbol";
}

static bool append_parameter_label(
    char *buffer,
    size_t capacity,
    size_t *length,
    const VcQueryParameter *parameter)
{
    if (parameter->modifier != NULL && parameter->modifier[0] != '\0' &&
        !append_format(buffer, capacity, length, "%s ", parameter->modifier))
        return false;
    if (!append_format(buffer, capacity, length, "%s %s",
            parameter->type_name, parameter->name))
        return false;
    if (parameter->is_optional && !append_format(buffer, capacity, length, " = ..."))
        return false;
    return true;
}

static bool hover_label(
    const VcQuerySession *session,
    const char *path,
    size_t offset,
    const VcQuerySymbol *symbol,
    char *label,
    size_t label_size)
{
    size_t length = 0;
    if (symbol->kind == VC_QUERY_SYMBOL_METHOD || symbol->kind == VC_QUERY_SYMBOL_CONSTRUCTOR)
    {
        if (!append_format(label, label_size, &length, "%s %s(",
                query_kind_name(symbol->kind), symbol->name))
            return false;
        for (size_t i = 0; i < symbol->parameter_count; i++)
        {
            VcQueryParameter parameter;
            if (!vc_query_parameter_at(session, path, offset, i, &parameter))
                return false;
            if (i != 0 && !append_format(label, label_size, &length, ", "))
                return false;
            if (!append_parameter_label(label, label_size, &length, &parameter))
                return false;
        }
        if (!append_format(label, label_size, &length, ")"))
            return false;
        if (symbol->kind == VC_QUERY_SYMBOL_METHOD &&
            !append_format(label, label_size, &length, ": %s", symbol->type_name))
            return false;
        return true;
    }

    return append_format(label, label_size, &length, "%s %s: %s",
        query_kind_name(symbol->kind), symbol->name, symbol->type_name);
}

static bool handle_hover(VcLspServer *server, VcLspSlice id, VcLspSlice params)
{
    char *uri = NULL;
    size_t line = 0;
    size_t character = 0;
    if (!request_document_position(params, &uri, &line, &character))
        return send_result(id, "null");

    VcLspDocument *document = find_document(server, uri);
    free(uri);
    if (document == NULL)
        return send_result(id, "null");

    size_t offset = 0;
    if (!lsp_offset_at(document->text, line, character, &offset))
        return send_result(id, "null");

    char error[2048];
    VcQuerySession *session = document_query_session(server, document, error, sizeof(error));
    if (session == NULL)
        return send_result(id, "null");

    VcQuerySymbol symbol;
    if (!vc_query_symbol_at(session, document->path, offset, &symbol))
    {
        vc_query_session_destroy(session);
        return send_result(id, "null");
    }

    char label[2048];
    if (!hover_label(session, document->path, offset, &symbol, label, sizeof(label)))
    {
        vc_query_session_destroy(session);
        return false;
    }
    char *escaped = json_escape(label);
    if (escaped == NULL)
    {
        vc_query_session_destroy(session);
        return false;
    }

    const VcLspPosition start = lsp_position_at(document->text, symbol.span.start.offset);
    const VcLspPosition end = lsp_position_at(document->text, symbol.span.end.offset);
    char result[4096];
    const int written = snprintf(result, sizeof(result),
        "{\"contents\":{\"kind\":\"plaintext\",\"value\":\"%s\"},"
        "\"range\":{\"start\":{\"line\":%zu,\"character\":%zu},"
        "\"end\":{\"line\":%zu,\"character\":%zu}}}",
        escaped, start.line, start.character, end.line, end.character);
    free(escaped);
    vc_query_session_destroy(session);
    return written >= 0 && (size_t)written < sizeof(result) && send_result(id, result);
}

static bool signature_label(
    const VcQuerySession *session,
    const char *path,
    size_t offset,
    size_t signature_index,
    const VcQuerySignature *signature,
    char *label,
    size_t label_size)
{
    size_t length = 0;
    if (!append_format(label, label_size, &length, "%s", signature->name))
        return false;
    if (signature->generic_parameter_count != 0)
    {
        if (!append_format(label, label_size, &length, "<"))
            return false;
        for (size_t i = 0; i < signature->generic_parameter_count; i++)
        {
            const char *generic_name = NULL;
            if (!vc_query_signature_generic_parameter_at(
                    session, path, offset, signature_index, i, &generic_name))
                return false;
            if (i != 0 && !append_format(label, label_size, &length, ", "))
                return false;
            if (!append_format(label, label_size, &length, "%s", generic_name))
                return false;
        }
        if (!append_format(label, label_size, &length, ">"))
            return false;
    }
    if (!append_format(label, label_size, &length, "("))
        return false;
    for (size_t i = 0; i < signature->parameter_count; i++)
    {
        VcQueryParameter parameter;
        if (!vc_query_signature_parameter_at(
                session, path, offset, signature_index, i, &parameter))
            return false;
        if (i != 0 && !append_format(label, label_size, &length, ", "))
            return false;
        if (!append_parameter_label(label, label_size, &length, &parameter))
            return false;
    }
    if (!append_format(label, label_size, &length, ")"))
        return false;
    if (signature->kind == VC_QUERY_SYMBOL_METHOD && signature->return_type != NULL &&
        !append_format(label, label_size, &length, ": %s", signature->return_type))
        return false;
    return true;
}

static int completion_item_kind(VcQuerySymbolKind kind)
{
    switch (kind)
    {
        case VC_QUERY_SYMBOL_LOCAL:
        case VC_QUERY_SYMBOL_PARAMETER:
            return 6;
        case VC_QUERY_SYMBOL_FIELD:
            return 5;
        case VC_QUERY_SYMBOL_PROPERTY:
            return 10;
        case VC_QUERY_SYMBOL_METHOD:
            return 2;
        case VC_QUERY_SYMBOL_CONSTRUCTOR:
            return 4;
        case VC_QUERY_SYMBOL_TYPE:
            return 7;
        case VC_QUERY_SYMBOL_NAMESPACE:
            return 9;
        case VC_QUERY_SYMBOL_ENUM_MEMBER:
            return 20;
        case VC_QUERY_SYMBOL_EVENT:
            return 23;
        case VC_QUERY_SYMBOL_CONSTANT:
            return 21;
        case VC_QUERY_SYMBOL_NONE:
            break;
    }
    return 1;
}

static int symbol_kind(VcQuerySymbolKind kind)
{
    switch (kind)
    {
        case VC_QUERY_SYMBOL_NAMESPACE: return 3;
        case VC_QUERY_SYMBOL_TYPE: return 5;
        case VC_QUERY_SYMBOL_METHOD: return 6;
        case VC_QUERY_SYMBOL_PROPERTY: return 7;
        case VC_QUERY_SYMBOL_FIELD: return 8;
        case VC_QUERY_SYMBOL_CONSTRUCTOR: return 9;
        case VC_QUERY_SYMBOL_LOCAL:
        case VC_QUERY_SYMBOL_PARAMETER: return 13;
        case VC_QUERY_SYMBOL_CONSTANT: return 14;
        case VC_QUERY_SYMBOL_ENUM_MEMBER: return 22;
        case VC_QUERY_SYMBOL_EVENT: return 24;
        case VC_QUERY_SYMBOL_NONE: break;
    }
    return 13;
}

static bool append_location(
    char *buffer,
    size_t capacity,
    size_t *length,
    const VcQuerySession *session,
    const char *path,
    VcSourceSpan span)
{
    const char *text = vc_query_source_text(session, path);
    if (text == NULL)
        return false;
    char *uri = path_to_uri(path);
    if (uri == NULL)
        return false;
    char *escaped_uri = json_escape(uri);
    free(uri);
    if (escaped_uri == NULL)
        return false;

    const VcLspPosition start = lsp_position_at(text, span.start.offset);
    const VcLspPosition end = lsp_position_at(text, span.end.offset);
    const bool ok = append_format(buffer, capacity, length,
        "{\"uri\":\"%s\",\"range\":{\"start\":{\"line\":%zu,\"character\":%zu},"
        "\"end\":{\"line\":%zu,\"character\":%zu}}}",
        escaped_uri, start.line, start.character, end.line, end.character);
    free(escaped_uri);
    return ok;
}

static bool request_document_uri(VcLspSlice params, char **uri)
{
    VcLspSlice document;
    VcLspSlice raw_uri;
    if (!json_object_member(params, "textDocument", &document) ||
        !json_object_member(document, "uri", &raw_uri))
        return false;
    *uri = json_decode_string(raw_uri);
    return *uri != NULL;
}

static bool handle_definition(VcLspServer *server, VcLspSlice id, VcLspSlice params)
{
    char *uri = NULL;
    size_t line = 0;
    size_t character = 0;
    if (!request_document_position(params, &uri, &line, &character))
        return send_result(id, "null");

    VcLspDocument *document = find_document(server, uri);
    free(uri);
    if (document == NULL)
        return send_result(id, "null");

    size_t offset = 0;
    if (!lsp_offset_at(document->text, line, character, &offset))
        return send_result(id, "null");

    char error[2048];
    VcQuerySession *session = document_query_session(server, document, error, sizeof(error));
    if (session == NULL)
        return send_result(id, "null");

    const char *definition_path = NULL;
    VcSourceSpan definition_span;
    if (!vc_query_definition_at(session, document->path, offset, &definition_path, &definition_span))
    {
        vc_query_session_destroy(session);
        return send_result(id, "null");
    }

    char result[8192];
    size_t length = 0;
    const bool ok = append_location(result, sizeof(result), &length,
        session, definition_path, definition_span);
    vc_query_session_destroy(session);
    return ok && send_result(id, result);
}

static bool append_symbol_information(
    char *buffer,
    size_t capacity,
    size_t *length,
    const VcQuerySession *session,
    const VcQueryNamedSymbol *symbol)
{
    char *name = json_escape(symbol->name != NULL ? symbol->name : "");
    char *container = symbol->container_name != NULL ? json_escape(symbol->container_name) : NULL;
    if (name == NULL || (symbol->container_name != NULL && container == NULL))
    {
        free(name);
        free(container);
        return false;
    }

    bool ok = append_format(buffer, capacity, length,
        "{\"name\":\"%s\",\"kind\":%d,\"location\":",
        name, symbol_kind(symbol->kind));
    free(name);
    if (ok)
        ok = append_location(buffer, capacity, length, session, symbol->path, symbol->span);
    if (ok && container != NULL)
        ok = append_format(buffer, capacity, length, ",\"containerName\":\"%s\"", container);
    free(container);
    return ok && append_format(buffer, capacity, length, "}");
}

static bool handle_document_symbols(VcLspServer *server, VcLspSlice id, VcLspSlice params)
{
    char *uri = NULL;
    if (!request_document_uri(params, &uri))
        return send_result(id, "[]");
    VcLspDocument *document = find_document(server, uri);
    free(uri);
    if (document == NULL)
        return send_result(id, "[]");

    char error[2048];
    VcQuerySession *session = document_query_session(server, document, error, sizeof(error));
    if (session == NULL)
        return send_result(id, "[]");

    VcQueryNamedSymbol symbols[256];
    const size_t total = vc_query_document_symbols(
        session, document->path, symbols, sizeof(symbols) / sizeof(symbols[0]));
    const size_t count = total < sizeof(symbols) / sizeof(symbols[0]) ?
        total : sizeof(symbols) / sizeof(symbols[0]);

    char result[65536];
    size_t length = 0;
    if (!append_format(result, sizeof(result), &length, "["))
        goto fail;
    for (size_t i = 0; i < count; i++)
    {
        if (i != 0 && !append_format(result, sizeof(result), &length, ","))
            goto fail;
        if (!append_symbol_information(result, sizeof(result), &length, session, &symbols[i]))
            goto fail;
    }
    if (!append_format(result, sizeof(result), &length, "]"))
        goto fail;

    vc_query_session_destroy(session);
    return send_result(id, result);

fail:
    vc_query_session_destroy(session);
    return false;
}

static bool handle_workspace_symbols(VcLspServer *server, VcLspSlice id, VcLspSlice params)
{
    if (server->document_count == 0)
        return send_result(id, "[]");

    VcLspSlice raw_query;
    char *query = NULL;
    if (json_object_member(params, "query", &raw_query))
        query = json_decode_string(raw_query);
    if (query == NULL)
        query = lsp_strdup("");
    if (query == NULL)
        return false;

    char error[2048];
    VcQuerySession *session = document_query_session(
        server, &server->documents[0], error, sizeof(error));
    if (session == NULL)
    {
        free(query);
        return send_result(id, "[]");
    }

    VcQueryNamedSymbol symbols[256];
    const size_t total = vc_query_workspace_symbols(
        session, query, symbols, sizeof(symbols) / sizeof(symbols[0]));
    free(query);
    const size_t count = total < sizeof(symbols) / sizeof(symbols[0]) ?
        total : sizeof(symbols) / sizeof(symbols[0]);

    char result[65536];
    size_t length = 0;
    if (!append_format(result, sizeof(result), &length, "["))
        goto fail;
    for (size_t i = 0; i < count; i++)
    {
        if (i != 0 && !append_format(result, sizeof(result), &length, ","))
            goto fail;
        if (!append_symbol_information(result, sizeof(result), &length, session, &symbols[i]))
            goto fail;
    }
    if (!append_format(result, sizeof(result), &length, "]"))
        goto fail;

    vc_query_session_destroy(session);
    return send_result(id, result);

fail:
    vc_query_session_destroy(session);
    return false;
}

static bool handle_references(VcLspServer *server, VcLspSlice id, VcLspSlice params)
{
    char *uri = NULL;
    size_t line = 0;
    size_t character = 0;
    if (!request_document_position(params, &uri, &line, &character))
        return send_result(id, "[]");
    VcLspDocument *document = find_document(server, uri);
    free(uri);
    if (document == NULL)
        return send_result(id, "[]");

    size_t offset = 0;
    if (!lsp_offset_at(document->text, line, character, &offset))
        return send_result(id, "[]");

    bool include_definition = false;
    VcLspSlice context;
    VcLspSlice raw_include;
    if (json_object_member(params, "context", &context) &&
        json_object_member(context, "includeDeclaration", &raw_include))
        (void)json_bool(raw_include, &include_definition);

    char error[2048];
    VcQuerySession *session = document_query_session(server, document, error, sizeof(error));
    if (session == NULL)
        return send_result(id, "[]");

    VcQueryLocation locations[256];
    const size_t total = vc_query_references_at(
        session, document->path, offset, include_definition,
        locations, sizeof(locations) / sizeof(locations[0]));
    const size_t count = total < sizeof(locations) / sizeof(locations[0]) ?
        total : sizeof(locations) / sizeof(locations[0]);

    char result[65536];
    size_t length = 0;
    if (!append_format(result, sizeof(result), &length, "["))
        goto fail;
    for (size_t i = 0; i < count; i++)
    {
        if (i != 0 && !append_format(result, sizeof(result), &length, ","))
            goto fail;
        if (!append_location(result, sizeof(result), &length, session,
                locations[i].path, locations[i].span))
            goto fail;
    }
    if (!append_format(result, sizeof(result), &length, "]"))
        goto fail;

    vc_query_session_destroy(session);
    return send_result(id, result);

fail:
    vc_query_session_destroy(session);
    return false;
}

static bool handle_completion(VcLspServer *server, VcLspSlice id, VcLspSlice params)
{
    char *uri = NULL;
    size_t line = 0;
    size_t character = 0;
    if (!request_document_position(params, &uri, &line, &character))
        return send_result(id, "null");

    VcLspDocument *document = find_document(server, uri);
    free(uri);
    if (document == NULL)
        return send_result(id, "null");

    size_t offset = 0;
    if (!lsp_offset_at(document->text, line, character, &offset))
        return send_result(id, "null");

    char error[2048];
    VcQuerySession *session = document_query_session(server, document, error, sizeof(error));
    if (session == NULL)
        return send_result(id, "null");

    VcQueryCompletion completions[192];
    const size_t total = vc_query_completions_at(
        session, document->path, offset, completions,
        sizeof(completions) / sizeof(completions[0]));
    const size_t count = total < sizeof(completions) / sizeof(completions[0])
        ? total : sizeof(completions) / sizeof(completions[0]);

    char result[65536];
    size_t length = 0;
    if (!append_format(result, sizeof(result), &length,
            "{\"isIncomplete\":%s,\"items\":[",
            total > count ? "true" : "false"))
        goto fail;

    for (size_t i = 0; i < count; i++)
    {
        char *label = json_escape(completions[i].name != NULL ? completions[i].name : "");
        char *detail = json_escape(completions[i].type_name);
        if (label == NULL || detail == NULL)
        {
            free(label);
            free(detail);
            goto fail;
        }
        const bool ok = (i == 0 || append_format(result, sizeof(result), &length, ",")) &&
            append_format(result, sizeof(result), &length,
                "{\"label\":\"%s\",\"kind\":%d,\"detail\":\"%s\"}",
                label, completion_item_kind(completions[i].kind), detail);
        free(label);
        free(detail);
        if (!ok)
            goto fail;
    }
    if (!append_format(result, sizeof(result), &length, "]}"))
        goto fail;

    vc_query_session_destroy(session);
    return send_result(id, result);

fail:
    vc_query_session_destroy(session);
    return false;
}

static bool handle_signature_help(VcLspServer *server, VcLspSlice id, VcLspSlice params)
{
    char *uri = NULL;
    size_t line = 0;
    size_t character = 0;
    if (!request_document_position(params, &uri, &line, &character))
        return send_result(id, "null");

    VcLspDocument *document = find_document(server, uri);
    free(uri);
    if (document == NULL)
        return send_result(id, "null");

    size_t offset = 0;
    if (!lsp_offset_at(document->text, line, character, &offset))
        return send_result(id, "null");

    char error[2048];
    VcQuerySession *session = document_query_session(server, document, error, sizeof(error));
    if (session == NULL)
        return send_result(id, "null");

    VcQuerySignature signatures[16];
    size_t selected = 0;
    size_t active = 0;
    const size_t total = vc_query_signatures_at(
        session,
        document->path,
        offset,
        signatures,
        16,
        &selected,
        &active);
    const size_t count = total < 16 ? total : 16;
    if (count == 0)
    {
        vc_query_session_destroy(session);
        return send_result(id, "null");
    }
    if (selected >= count)
        selected = 0;
    if (signatures[selected].parameter_count == 0)
        active = 0;
    else if (active >= signatures[selected].parameter_count)
        active = signatures[selected].parameter_count - 1;

    char result[16384];
    size_t length = 0;
    if (!append_format(result, sizeof(result), &length, "{\"signatures\":["))
        goto fail;
    for (size_t i = 0; i < count; i++)
    {
        char label[2048];
        if (!signature_label(session, document->path, offset, i, &signatures[i], label, sizeof(label)))
            goto fail;
        char *escaped_label = json_escape(label);
        if (escaped_label == NULL)
            goto fail;
        if (i != 0 && !append_format(result, sizeof(result), &length, ","))
        {
            free(escaped_label);
            goto fail;
        }
        const bool header_ok = append_format(result, sizeof(result), &length,
            "{\"label\":\"%s\",\"parameters\":[", escaped_label);
        free(escaped_label);
        if (!header_ok)
            goto fail;

        for (size_t j = 0; j < signatures[i].parameter_count; j++)
        {
            VcQueryParameter parameter;
            if (!vc_query_signature_parameter_at(
                    session, document->path, offset, i, j, &parameter))
                goto fail;
            char parameter_label[512];
            size_t parameter_length = 0;
            if (!append_parameter_label(
                    parameter_label, sizeof(parameter_label), &parameter_length, &parameter))
                goto fail;
            char *escaped_parameter = json_escape(parameter_label);
            if (escaped_parameter == NULL)
                goto fail;
            if (j != 0 && !append_format(result, sizeof(result), &length, ","))
            {
                free(escaped_parameter);
                goto fail;
            }
            const bool parameter_ok = append_format(result, sizeof(result), &length,
                "{\"label\":\"%s\"}", escaped_parameter);
            free(escaped_parameter);
            if (!parameter_ok)
                goto fail;
        }
        if (!append_format(result, sizeof(result), &length, "]}"))
            goto fail;
    }
    if (!append_format(result, sizeof(result), &length,
            "],\"activeSignature\":%zu,\"activeParameter\":%zu}", selected, active))
        goto fail;

    vc_query_session_destroy(session);
    return send_result(id, result);

fail:
    vc_query_session_destroy(session);
    return false;
}

static bool document_fields(VcLspSlice params, char **uri, char **text, int *version, bool require_text)
{
    VcLspSlice document;
    if (!json_object_member(params, "textDocument", &document))
        return false;

    VcLspSlice raw_uri;
    if (!json_object_member(document, "uri", &raw_uri))
        return false;
    *uri = json_decode_string(raw_uri);
    if (*uri == NULL)
        return false;

    VcLspSlice raw_version;
    *version = 0;
    if (json_object_member(document, "version", &raw_version))
        (void)json_int(raw_version, version);

    *text = NULL;
    if (require_text)
    {
        VcLspSlice raw_text;
        if (!json_object_member(document, "text", &raw_text))
        {
            free(*uri);
            *uri = NULL;
            return false;
        }
        *text = json_decode_string(raw_text);
        if (*text == NULL)
        {
            free(*uri);
            *uri = NULL;
            return false;
        }
    }
    return true;
}

static bool handle_notification(VcLspServer *server, const char *method, VcLspSlice params)
{
    if (strcmp(method, "initialized") == 0)
        return true;

    if (strcmp(method, "textDocument/didOpen") == 0)
    {
        char *uri = NULL;
        char *text = NULL;
        int version = 0;
        const bool parsed = document_fields(params, &uri, &text, &version, true);
        bool ok = parsed && open_document(server, uri, text, version);
        if (ok)
            ok = publish_project_diagnostics(server, find_document(server, uri));
        free(uri);
        free(text);
        return ok;
    }

    if (strcmp(method, "textDocument/didChange") == 0)
    {
        char *uri = NULL;
        char *unused_text = NULL;
        int version = 0;
        if (!document_fields(params, &uri, &unused_text, &version, false))
            return false;

        VcLspSlice changes;
        VcLspSlice first;
        VcLspSlice raw_text;
        if (!json_object_member(params, "contentChanges", &changes) ||
            !json_array_first(changes, &first) ||
            !json_object_member(first, "text", &raw_text))
        {
            free(uri);
            return false;
        }

        char *text = json_decode_string(raw_text);
        bool ok = text != NULL && change_document(server, uri, text, version);
        if (ok)
            ok = publish_project_diagnostics(server, find_document(server, uri));
        free(uri);
        free(text);
        return ok;
    }

    if (strcmp(method, "textDocument/didClose") == 0)
    {
        char *uri = NULL;
        char *text = NULL;
        int version = 0;
        if (!document_fields(params, &uri, &text, &version, false))
            return false;
        VcLspDocument *document = find_document(server, uri);
        if (document != NULL && !send_diagnostics(document, NULL))
        {
            free(uri);
            return false;
        }
        char *project_root = document != NULL ? lsp_strdup(document->project_root) : NULL;
        close_document(server, uri);
        if (project_root != NULL)
        {
            for (size_t i = 0; i < server->document_count; i++)
            {
                if (server->documents[i].project_root == NULL ||
                    strcmp(server->documents[i].project_root, project_root) != 0)
                    continue;
                if (!publish_project_diagnostics(server, &server->documents[i]))
                {
                    free(project_root);
                    free(uri);
                    return false;
                }
                break;
            }
        }
        free(project_root);
        free(uri);
        return true;
    }

    return true;
}

static bool handle_message(VcLspServer *server, const char *body, size_t length, bool *should_exit, int *exit_code)
{
    VcLspSlice root = { body, length };
    VcLspSlice raw_method;
    if (!json_object_member(root, "method", &raw_method))
        return true;

    char *method = json_decode_string(raw_method);
    if (method == NULL)
        return false;

    VcLspSlice params = { "{}", 2 };
    (void)json_object_member(root, "params", &params);

    VcLspSlice id;
    const bool has_id = json_object_member(root, "id", &id);

    bool ok = true;
    if (strcmp(method, "initialize") == 0 && has_id)
    {
        char result[768];
        const int written = snprintf(result, sizeof(result),
            "{\"capabilities\":{\"textDocumentSync\":1,\"hoverProvider\":true,"
            "\"signatureHelpProvider\":{\"triggerCharacters\":[\"(\",\",\"]},"
            "\"completionProvider\":{\"triggerCharacters\":[\".\"]},"
            "\"definitionProvider\":true,\"referencesProvider\":true,"
            "\"documentSymbolProvider\":true,\"workspaceSymbolProvider\":true},"
            "\"serverInfo\":{\"name\":\"voidc\",\"version\":\"%s\"}}",
            server->version);
        ok = written >= 0 && (size_t)written < sizeof(result) && send_result(id, result);
    }
    else if (strcmp(method, "textDocument/hover") == 0 && has_id)
    {
        ok = handle_hover(server, id, params);
    }
    else if (strcmp(method, "textDocument/signatureHelp") == 0 && has_id)
    {
        ok = handle_signature_help(server, id, params);
    }
    else if (strcmp(method, "textDocument/completion") == 0 && has_id)
    {
        ok = handle_completion(server, id, params);
    }
    else if (strcmp(method, "textDocument/definition") == 0 && has_id)
    {
        ok = handle_definition(server, id, params);
    }
    else if (strcmp(method, "textDocument/references") == 0 && has_id)
    {
        ok = handle_references(server, id, params);
    }
    else if (strcmp(method, "textDocument/documentSymbol") == 0 && has_id)
    {
        ok = handle_document_symbols(server, id, params);
    }
    else if (strcmp(method, "workspace/symbol") == 0 && has_id)
    {
        ok = handle_workspace_symbols(server, id, params);
    }
    else if (strcmp(method, "shutdown") == 0 && has_id)
    {
        server->shutdown_received = true;
        ok = send_result(id, "null");
    }
    else if (strcmp(method, "exit") == 0 && !has_id)
    {
        *should_exit = true;
        *exit_code = server->shutdown_received ? 0 : 1;
    }
    else if (!has_id)
    {
        ok = handle_notification(server, method, params);
    }
    else
    {
        ok = send_method_not_found(id, method);
    }

    free(method);
    return ok;
}

static bool read_message(char **body, size_t *body_length)
{
    char line[4096];
    size_t content_length = 0;
    bool have_length = false;

    for (;;)
    {
        if (fgets(line, sizeof(line), stdin) == NULL)
            return false;

        if (strcmp(line, "\r\n") == 0 || strcmp(line, "\n") == 0)
            break;

        if (strncmp(line, "Content-Length:", 15) == 0)
        {
            char *end = NULL;
            errno = 0;
            const unsigned long parsed = strtoul(line + 15, &end, 10);
            if (errno != 0 || end == line + 15)
                return false;
            content_length = (size_t)parsed;
            have_length = true;
        }
    }

    if (!have_length)
        return false;

    char *buffer = malloc(content_length + 1);
    if (buffer == NULL)
        return false;
    if (fread(buffer, 1, content_length, stdin) != content_length)
    {
        free(buffer);
        return false;
    }
    buffer[content_length] = '\0';
    *body = buffer;
    *body_length = content_length;
    return true;
}

int vc_lsp_run(const char *tool_root, const char *version)
{
    /* LSP framing and Content-Length describe bytes, not CRT text lines. */
    if (!vc_native_environment_init()) return 1;
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1) return 1;
#endif
    VcLspServer server;
    memset(&server, 0, sizeof(server));
    server.tool_root = tool_root;
    server.version = version;

    int exit_code = 0;
    for (;;)
    {
        char *body = NULL;
        size_t length = 0;
        if (!read_message(&body, &length))
            break;

        bool should_exit = false;
        if (!handle_message(&server, body, length, &should_exit, &exit_code))
        {
            free(body);
            exit_code = 1;
            break;
        }
        free(body);

        if (should_exit)
            break;
    }

    server_destroy(&server);
    return exit_code;
}
