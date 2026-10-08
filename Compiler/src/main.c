#include "diagnostic.h"
#include "voidc.h"
#include "compiler.h"
#include "ast.h"
#include "lexer.h"
#include "lsp.h"
#include "parser.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define VOIDC_VERSION "0.0.370"

static void print_usage(void)
{
    puts("voidc " VOIDC_VERSION);
    puts("");
    puts("Usage:");
    puts("  voidc <command> [target] [--diagnostics=text|json]");
    puts("");
    puts("Commands:");
    puts("  check      Resolve and validate a VOID project without building it");
    puts("  build      Resolve and build a VOID project");
    puts("  run        Resolve, build, and run a VOID project");
    puts("  publish    Resolve and publish a VOID project");
    puts("  lex        Tokenize VOID source and print the token stream");
    puts("  parse      Parse VOID source and print the syntax tree");
    puts("  lsp        Run the VOID language server over stdio");
    puts("  version    Print compiler version");
    puts("");
    puts("Target:");
    puts("  A directory, .voidproj file, or .void source file.");
    puts("  For build/run/publish, projectless source must be Program.void.");
    puts("  If omitted, the current directory is used.");
}

static bool has_suffix(const char *value, const char *suffix)
{
    const size_t value_length = strlen(value);
    const size_t suffix_length = strlen(suffix);
    return suffix_length <= value_length && strcmp(value + value_length - suffix_length, suffix) == 0;
}

static const char *output_name(VcOutputKind output)
{
    return output == VC_OUTPUT_LIBRARY ? "library" : "exe";
}

#define VC_MAIN_PATH_MAX 4096u

static bool compute_tool_root(const char *argv0, char *output, size_t output_size)
{
    if (argv0 == NULL || output == NULL || output_size == 0)
        return false;

    const char *slash = strrchr(argv0, '/');
    if (slash == NULL)
        return snprintf(output, output_size, ".") > 0;

    char directory[VC_MAIN_PATH_MAX];
    const size_t length = (size_t)(slash - argv0);
    if (length == 0 || length >= sizeof(directory))
        return false;
    memcpy(directory, argv0, length);
    directory[length] = '\0';

    const char *last = strrchr(directory, '/');
    const char *base = last == NULL ? directory : last + 1;
    if (strcmp(base, "bin") != 0)
        return snprintf(output, output_size, "%s", directory) > 0;

    if (last == NULL)
        return snprintf(output, output_size, ".") > 0;
    if (last == directory)
        return snprintf(output, output_size, "/") > 0;

    const size_t parent_length = (size_t)(last - directory);
    if (parent_length + 1 > output_size)
        return false;
    memcpy(output, directory, parent_length);
    output[parent_length] = '\0';
    return true;
}

static int resolve_command(const char *command, const char *target, const char *tool_root)
{
    VcProject project;
    vc_project_init(&project);

    char error[2048];
    if (!vc_resolve_project(target, &project, error, sizeof(error)))
    {
        vc_diagnostic_report_error(error);
        vc_project_destroy(&project);
        return 1;
    }

    printf("VOID project resolved\n");
    printf("  mode:      %s\n", project.projectless ? "projectless" : "project");
    printf("  root:      %s\n", project.root);
    if (project.project_file != NULL)
        printf("  project:   %s\n", project.project_file);
    printf("  name:      %s\n", project.name);
    printf("  version:   %s\n", project.version);
    printf("  output:    %s\n", output_name(project.output));
    printf("  unsafe:    %s\n", project.allow_unsafe ? "enabled" : "disabled");
    printf("  sources:   %zu\n", project.source_count);
    puts("");

    if (strcmp(command, "check") == 0)
    {
        if (!vc_check_project(&project, tool_root, error, sizeof(error)))
        {
            vc_diagnostic_report_error(error);
            vc_project_destroy(&project);
            return 1;
        }

        printf("checked: %s\n", project.name);
        vc_project_destroy(&project);
        return 0;
    }

    if (strcmp(command, "run") == 0 && project.output == VC_OUTPUT_LIBRARY)
    {
        vc_diagnostic_report_error("library projects cannot be run directly");
        vc_project_destroy(&project);
        return 1;
    }

    const VcBuildMode mode = strcmp(command, "publish") == 0 ? VC_BUILD_PUBLISH : VC_BUILD_DEBUG;
    char output_path[4096];
    if (!vc_compile_project(&project, mode, tool_root, output_path, sizeof(output_path), error, sizeof(error)))
    {
        vc_diagnostic_report_error(error);
        vc_project_destroy(&project);
        return 1;
    }

    if (strcmp(command, "publish") == 0)
        printf("published: %s\n", output_path);
    else
        printf("built: %s\n", output_path);

    if (strcmp(command, "run") == 0)
    {
        puts("");
        fflush(stdout);
        unsigned long program_exit_code = 0;
        if (!vc_run_executable(output_path, &program_exit_code, error, sizeof(error)))
        {
            vc_diagnostic_report_error(error);
            vc_project_destroy(&project);
            return 1;
        }
        vc_project_destroy(&project);
        return (int)program_exit_code;
    }

    vc_project_destroy(&project);
    return 0;
}

static void print_escaped_slice(const char *text, size_t length)
{
    putchar('"');
    for (size_t i = 0; i < length; i++)
    {
        const unsigned char c = (unsigned char)text[i];
        switch (c)
        {
            case '\n': fputs("\\n", stdout); break;
            case '\r': fputs("\\r", stdout); break;
            case '\t': fputs("\\t", stdout); break;
            case '\\': fputs("\\\\", stdout); break;
            case '"': fputs("\\\"", stdout); break;
            default:
                if (c >= 32 && c < 127)
                    putchar((char)c);
                else
                    printf("\\x%02X", c);
                break;
        }
    }
    putchar('"');
}

static bool load_and_lex(const char *path, VcSource *source, VcTokenList *tokens)
{
    char error[2048];
    if (!vc_source_load(path, source, error, sizeof(error)))
    {
        vc_diagnostic_report_error(error);
        return false;
    }

    if (!vc_lex_source(source, tokens))
    {
        vc_diagnostic_render(stderr, &tokens->diagnostic);
        return false;
    }

    return true;
}

static int lex_file(const char *path)
{
    VcSource source;
    vc_source_init(&source);

    VcTokenList tokens;
    vc_token_list_init(&tokens);

    if (!load_and_lex(path, &source, &tokens))
    {
        vc_token_list_destroy(&tokens);
        vc_source_destroy(&source);
        return 1;
    }

    printf("== %s ==\n", source.path);
    for (size_t i = 0; i < tokens.count; i++)
    {
        const VcToken *token = &tokens.items[i];
        printf("%zu:%zu-%zu:%zu %-18s ",
            token->span.start.line,
            token->span.start.column,
            token->span.end.line,
            token->span.end.column,
            vc_token_kind_name(token->kind));

        if (token->kind == VC_TOKEN_EOF)
            puts("");
        else
        {
            print_escaped_slice(source.text + token->location.offset, token->length);
            putchar('\n');
        }
    }

    vc_token_list_destroy(&tokens);
    vc_source_destroy(&source);
    return 0;
}

static int parse_file(const char *path)
{
    VcSource source;
    vc_source_init(&source);

    VcTokenList tokens;
    vc_token_list_init(&tokens);

    if (!load_and_lex(path, &source, &tokens))
    {
        vc_token_list_destroy(&tokens);
        vc_source_destroy(&source);
        return 1;
    }

    VcAstTree tree;
    vc_ast_tree_init(&tree);

    VcParseResult result;
    if (!vc_parse_source(&source, &tokens, &tree, &result))
    {
        vc_diagnostic_render(stderr, &result.diagnostic);
        vc_ast_tree_destroy(&tree);
        vc_token_list_destroy(&tokens);
        vc_source_destroy(&source);
        return 1;
    }

    printf("== %s ==\n", source.path);
    vc_ast_dump(&tree, &source);

    vc_ast_tree_destroy(&tree);
    vc_token_list_destroy(&tokens);
    vc_source_destroy(&source);
    return 0;
}

typedef int (*VcSourceCommand)(const char *path);

static int source_command(const char *target, VcSourceCommand command)
{
    if (has_suffix(target, ".void"))
        return command(target);

    VcProject project;
    vc_project_init(&project);

    char error[2048];
    if (!vc_resolve_project(target, &project, error, sizeof(error)))
    {
        vc_diagnostic_report_error(error);
        vc_project_destroy(&project);
        return 1;
    }

    int result = 0;
    for (size_t i = 0; i < project.source_count; i++)
    {
        if (command(project.sources[i]) != 0)
        {
            result = 1;
            break;
        }

        if (i + 1 < project.source_count)
            putchar('\n');
    }

    vc_project_destroy(&project);
    return result;
}

int main(int argc, char **argv)
{
    vc_diagnostic_enable_terminal();
    for (int i = 1; i < argc;)
    {
        if (strncmp(argv[i], "--diagnostics=", 14) != 0) { i++; continue; }
        if (!vc_diagnostic_set_format(argv[i] + 14))
        {
            vc_diagnostic_report_error("diagnostic format must be 'text' or 'json'");
            return 1;
        }
        for (int j = i; j < argc; j++) argv[j] = argv[j + 1];
        argc--;
    }
    if (argc < 2)
    {
        print_usage();
        return 0;
    }

    const char *command = argv[1];

    if (strcmp(command, "version") == 0 || strcmp(command, "--version") == 0)
    {
        puts("voidc " VOIDC_VERSION);
        return 0;
    }

    if (strcmp(command, "help") == 0 || strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0)
    {
        print_usage();
        return 0;
    }

    if (strcmp(command, "lsp") == 0)
    {
        char tool_root[VC_MAIN_PATH_MAX];
        if (!compute_tool_root(argv[0], tool_root, sizeof(tool_root)))
        {
            vc_diagnostic_report_error("could not determine voidc tool root");
            return 1;
        }
        return vc_lsp_run(tool_root, VOIDC_VERSION);
    }

    const char *target = argc >= 3 ? argv[2] : ".";

    if (strcmp(command, "lex") == 0)
        return source_command(target, lex_file);

    if (strcmp(command, "parse") == 0)
        return source_command(target, parse_file);

    if (strcmp(command, "check") == 0 || strcmp(command, "build") == 0 ||
        strcmp(command, "run") == 0 || strcmp(command, "publish") == 0)
    {
        char tool_root[VC_MAIN_PATH_MAX];
        if (!compute_tool_root(argv[0], tool_root, sizeof(tool_root)))
        {
            vc_diagnostic_report_error("could not determine voidc tool root");
            return 1;
        }
        return resolve_command(command, target, tool_root);
    }

    char message[512];
    snprintf(message, sizeof(message), "unknown command '%s'", command);
    vc_diagnostic_report_error(message);
    print_usage();
    return 1;
}
