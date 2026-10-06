#include "diagnostic.h"
#include <string.h>
static bool terminal_enabled;
static bool json_output;
static const char *severity_name(VcDiagnosticSeverity severity)
{
    switch (severity)
    {
        case VC_DIAGNOSTIC_WARNING: return "warning";
        case VC_DIAGNOSTIC_NOTE: return "note";
        case VC_DIAGNOSTIC_HELP: return "help";
        default: return "error";
    }
}
const char *vc_diagnostic_code_name(VcDiagnosticCode code)
{
    static const char *const names[] = {
#define VC_DIAGNOSTIC_CODE(name, code, meaning) code,
#include "diagnostic_codes.def"
#undef VC_DIAGNOSTIC_CODE
    };
    if ((unsigned)code >= VC_DIAG_CODE_COUNT) return names[VC_DIAG_DRIVER];
    return names[code];
}
void vc_diagnostic_set_code(VcDiagnostic *diagnostic, VcDiagnosticCode code)
{
    diagnostic->code = vc_diagnostic_code_name(code);
}
void vc_diagnostic_init(VcDiagnostic *diagnostic, const char *path, VcSourceSpan span)
{
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->path = path;
    diagnostic->span = span;
    vc_diagnostic_set_code(diagnostic, VC_DIAG_DRIVER);
}
bool vc_diagnostic_add_context(VcDiagnostic *diagnostic, VcDiagnosticSeverity severity,
    const char *path, VcSourceSpan span, const char *message)
{
    if (severity != VC_DIAGNOSTIC_NOTE && severity != VC_DIAGNOSTIC_HELP) return false;
    if (diagnostic->context_count >= VC_DIAGNOSTIC_CONTEXT_LIMIT)
    {
        diagnostic->context_truncated = true;
        return false;
    }
    VcDiagnosticContext *context = &diagnostic->context[diagnostic->context_count++];
    context->severity = severity;
    context->path = path;
    context->span = span;
    snprintf(context->message, sizeof(context->message), "%s", message);
    return true;
}
bool vc_diagnostic_add_source_context(VcDiagnostic *diagnostic, VcDiagnosticSeverity severity,
    const VcSource *source, VcSourceSpan span, const char *message)
{
    if (!vc_diagnostic_add_context(diagnostic, severity, source != NULL ? source->path : NULL,
        span, message)) return false;
    diagnostic->context[diagnostic->context_count - 1].source = source;
    return true;
}
static void readable_text(FILE *output, const char *text)
{
    for (const unsigned char *p = (const unsigned char *)text; *p != 0; p++)
    {
        if (*p == '\n') fputs("\\n", output);
        else if (*p == '\r') fputs("\\r", output);
        else if (*p == '\t') fputs("\\t", output);
        else if (*p < 32 || *p == 127) fprintf(output, "\\x%02x", *p);
        else fputc(*p, output);
    }
}
static void render_location(FILE *output, const char *path, VcSourceSpan span)
{
    if (path != NULL)
    {
        readable_text(output, path);
        fprintf(output, ":%zu:%zu-%zu:%zu: ", span.start.line,
            span.start.column, span.end.line, span.end.column);
    }
}
/* Source spans are half-open byte offsets, not terminal columns. */
static size_t display_width(const unsigned char *text, size_t length, size_t column,
    size_t *bytes)
{
    unsigned c = text[0];
    *bytes = 1;
    if (c == '\t') return 4 - column % 4;
    if (c < 32 || c == 127) return 1;
    if (c >= 0xc2 && c <= 0xf4)
    {
        size_t count = c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
        unsigned scalar = c & (count == 2 ? 31 : count == 3 ? 15 : 7);
        if (count > length) return 1;
        for (size_t i = 1; i < count; i++)
        {
            if ((text[i] & 0xc0) != 0x80) return 1;
            scalar = (scalar << 6) | (text[i] & 63);
        }
        *bytes = count;
        if ((scalar >= 0x300 && scalar <= 0x36f) ||
            (scalar >= 0xfe00 && scalar <= 0xfe0f)) return 0;
        if ((scalar >= 0x1100 && scalar <= 0x115f) ||
            (scalar >= 0x2e80 && scalar <= 0xa4cf) ||
            (scalar >= 0xac00 && scalar <= 0xd7a3) ||
            (scalar >= 0xf900 && scalar <= 0xfaff) ||
            (scalar >= 0xff01 && scalar <= 0xff60) ||
            (scalar >= 0x1f300 && scalar <= 0x1faff) ||
            (scalar >= 0x20000 && scalar <= 0x3fffd)) return 2;
    }
    return 1;
}
static size_t columns(const char *text, size_t length)
{
    size_t column = 0;
    for (size_t i = 0; i < length;)
    {
        size_t bytes;
        column += display_width((const unsigned char *)text + i, length - i, column, &bytes);
        i += bytes;
    }
    return column;
}
static void render_snippet(FILE *output, const char *path, VcSourceSpan span, const VcSource *snapshot)
{
    if (path == NULL || span.start.line == 0) return;
    VcSource source;
    if (snapshot != NULL) source = *snapshot;
    else
    {
        vc_source_init(&source);
        char error[256];
        if (!vc_source_load(path, &source, error, sizeof(error))) return;
    }
    size_t start = span.start.offset < source.length ? span.start.offset : source.length;
    size_t end = span.end.offset < source.length ? span.end.offset : source.length;
    if (end < start) end = start;
    size_t offset = 0, line = 1, shown = 0;
    while (offset <= source.length)
    {
        size_t next = offset;
        while (next < source.length && source.text[next] != '\n') next++;
        size_t visible_end = next;
        if (visible_end > offset && source.text[visible_end - 1] == '\r') visible_end--;
        bool intersects = start <= next && (end > offset || (start == end && start >= offset));
        if (intersects)
        {
            if (shown++ == 8) { fputs("       | ...\n", output); break; }
            fprintf(output, "%6zu | ", line);
            size_t column = 0;
            for (size_t i = offset; i < visible_end;)
            {
                size_t bytes;
                size_t width = display_width((const unsigned char *)source.text + i,
                    visible_end - i, column, &bytes);
                unsigned char c = (unsigned char)source.text[i];
                if (c == '\t') for (size_t j = 0; j < width; j++) fputc(' ', output);
                else if (c < 32 || c == 127) fputc('?', output);
                else fwrite(source.text + i, 1, bytes, output);
                i += bytes;
                column += width;
            }
            fputs("\n       | ", output);
            size_t first = start > offset ? start : offset;
            if (first > visible_end) first = visible_end;
            size_t last = end < visible_end ? end : visible_end;
            if (last < first) last = first;
            size_t before = columns(source.text + offset, first - offset);
            size_t after = columns(source.text + offset, last - offset);
            for (size_t i = 0; i < before; i++) fputc(' ', output);
            fputc('^', output);
            for (size_t i = before + 1; i < after; i++) fputc('~', output);
            fputc('\n', output);
        }
        if (next == source.length || next >= end) break;
        offset = next + 1;
        line++;
    }
    if (snapshot == NULL) vc_source_destroy(&source);
}
/* Reject invalid UTF-8 sequences rather than writing invalid JSON bytes. */
static size_t utf8_sequence(const unsigned char *text)
{
    unsigned c = text[0];
    size_t n = c >= 0xc2 && c <= 0xdf ? 2 : c >= 0xe0 && c <= 0xef ? 3
        : c >= 0xf0 && c <= 0xf4 ? 4 : 0;
    if (n == 0) return 0;
    for (size_t i = 1; i < n; i++)
        if (text[i] == 0 || (text[i] & 0xc0) != 0x80) return 0;
    if ((c == 0xe0 && text[1] < 0xa0) || (c == 0xed && text[1] >= 0xa0) ||
        (c == 0xf0 && text[1] < 0x90) || (c == 0xf4 && text[1] >= 0x90)) return 0;
    return n;
}
static void json_string(FILE *output, const char *text)
{
    if (text == NULL) { fputs("null", output); return; }
    fputc('"', output);
    const unsigned char *p = (const unsigned char *)text;
    while (*p != 0)
    {
        unsigned c = *p;
        if (c == '"' || c == '\\') { fputc('\\', output); fputc((int)c, output); p++; }
        else if (c < 32 || c == 127) { fprintf(output, "\\u%04x", c); p++; }
        else if (c < 128) { fputc((int)c, output); p++; }
        else
        {
            size_t n = utf8_sequence(p);
            if (n == 0) { fprintf(output, "\\u%04x", c); p++; }
            else { fwrite(p, 1, n, output); p += n; }
        }
    }
    fputc('"', output);
}
static void json_span(FILE *output, VcSourceSpan span)
{
    fprintf(output, "{\"start\":{\"offset\":%zu,\"line\":%zu,\"column\":%zu},"
        "\"end\":{\"offset\":%zu,\"line\":%zu,\"column\":%zu}}",
        span.start.offset, span.start.line, span.start.column,
        span.end.offset, span.end.line, span.end.column);
}
static void json_context(FILE *output, const VcDiagnosticContext *context, size_t index)
{
    fprintf(output, "{\"index\":%zu,\"severity\":", index);
    json_string(output, severity_name(context->severity));
    fputs(",\"message\":", output); json_string(output, context->message);
    fputs(",\"file\":", output); json_string(output, context->path);
    fputs(",\"span\":", output); json_span(output, context->span);
    fputc('}', output);
}
static void render_json(FILE *output, const VcDiagnostic *diagnostic)
{
    fputs("{\"schemaVersion\":1,\"code\":", output);
    json_string(output, diagnostic->code != NULL ? diagnostic->code : vc_diagnostic_code_name(VC_DIAG_SEMANTIC));
    fputs(",\"severity\":", output); json_string(output, severity_name(diagnostic->severity));
    fputs(",\"message\":", output); json_string(output, diagnostic->message);
    fputs(",\"file\":", output); json_string(output, diagnostic->path);
    fputs(",\"span\":", output); json_span(output, diagnostic->span);
    for (size_t group = 0; group < 3; group++)
    {
        fputs(group == 0 ? ",\"notes\":[" : group == 1 ? ",\"help\":[" : ",\"relatedLocations\":[", output);
        bool comma = false;
        for (size_t i = 0; i < diagnostic->context_count; i++)
        {
            const VcDiagnosticContext *context = &diagnostic->context[i];
            bool include = group == 0 ? context->severity == VC_DIAGNOSTIC_NOTE
                : group == 1 ? context->severity == VC_DIAGNOSTIC_HELP : context->path != NULL;
            if (!include) continue;
            if (comma) fputc(',', output);
            json_context(output, context, i);
            comma = true;
        }
        fputc(']', output);
    }
    fprintf(output, ",\"contextTruncated\":%s}\n", diagnostic->context_truncated ? "true" : "false");
}
bool vc_diagnostic_set_format(const char *format)
{
    if (strcmp(format, "json") == 0) { json_output = true; return true; }
    if (strcmp(format, "text") == 0) { json_output = false; return true; }
    return false;
}
void vc_diagnostic_render(FILE *output, const VcDiagnostic *diagnostic)
{
    if (json_output) { render_json(output, diagnostic); return; }
    render_location(output, diagnostic->path, diagnostic->span);
    fprintf(output, "%s: ", severity_name(diagnostic->severity));
    readable_text(output, diagnostic->message);
    fputc('\n', output);
    fprintf(output, "       code: %s\n", diagnostic->code != NULL ? diagnostic->code : vc_diagnostic_code_name(VC_DIAG_SEMANTIC));
    render_snippet(output, diagnostic->path, diagnostic->span, diagnostic->source);
    for (size_t i = 0; i < diagnostic->context_count; i++)
    {
        const VcDiagnosticContext *context = &diagnostic->context[i];
        render_location(output, context->path, context->span);
        fprintf(output, "%s: ", severity_name(context->severity));
        readable_text(output, context->message);
        fputc('\n', output);
        render_snippet(output, context->path, context->span, context->source);
    }
    if (diagnostic->context_truncated) fputs("note: Additional diagnostic context was omitted.\n", output);
}
void vc_diagnostic_enable_terminal(void) { terminal_enabled = true; }
bool vc_diagnostic_report(const VcDiagnostic *diagnostic)
{
    if (!terminal_enabled) return false;
    vc_diagnostic_render(stderr, diagnostic);
    return true;
}
void vc_diagnostic_report_error(const char *message)
{
    if (message == NULL || message[0] == '\0') return;
    VcDiagnostic diagnostic;
    VcSourceSpan span = {0};
    vc_diagnostic_init(&diagnostic, NULL, span);
    snprintf(diagnostic.message, sizeof(diagnostic.message), "%s", message);
    vc_diagnostic_render(stderr, &diagnostic);
}
