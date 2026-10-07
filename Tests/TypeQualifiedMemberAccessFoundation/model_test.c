#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "monomorph.h"
#include "semantic.h"
#include <stdio.h>
#include <string.h>

static int failures;
static void expect(bool condition, const char *label)
{
    if (!condition) { fprintf(stderr, "FAIL: %s\n", label); failures++; }
    else puts("True");
}

static void check_model(char *text, const char *name, VcSemanticType builtin, bool valid, size_t arguments)
{
    VcSource source = {"model.void", text, strlen(text)};
    VcTokenList tokens;
    VcAstTree tree;
    VcParseResult parse;
    VcSemanticModel model;
    VcSemanticDiagnostic diagnostic = {0};
    vc_token_list_init(&tokens);
    vc_ast_tree_init(&tree);
    vc_parse_result_init(&parse);
    vc_semantic_model_init(&model);
    bool parsed = vc_lex_source(&source, &tokens) && !tokens.has_error &&
        vc_parse_source(&source, &tokens, &tree, &parse) && !parse.has_error;
    expect(parsed, "receiver parses");
    if (parsed)
    {
        VcAstTree *trees[] = {&tree};
        char error[512];
        bool specialized = vc_monomorphize(trees, 1, error, sizeof(error));
        expect(specialized, "receiver specializes");
        if (specialized)
        {
            VcSemanticUnit unit = {&source, &tree};
            bool result = vc_semantic_analyze(&unit, 1, false, false, &model, &diagnostic);
            expect(result == valid, "expected semantic result");
            bool found = false;
            for (size_t i = 0; i < model.binding_count; i++)
            {
                const VcSemanticBinding *binding = &model.bindings[i];
                if (!binding->is_type_receiver) continue;
                char receiver[512];
                if (!vc_ast_receiver_name(binding->node, receiver, sizeof(receiver)) || strcmp(receiver, name) != 0) continue;
                found = true;
                if (builtin != VC_SEM_TYPE_UNKNOWN)
                {
                    expect(binding->type == builtin, "builtin semantic identity");
                    expect(binding->node->kind == VC_AST_TYPE_RECEIVER_EXPRESSION, "keyword is explicit type syntax");
                }
                else
                {
                    expect(vc_semantic_type_is_struct(binding->type), "declaration-backed identity");
                    const VcSemanticStruct *owner = &model.structs[vc_semantic_struct_index(binding->type)];
                    expect(binding->declaration_node == owner->node, "receiver query declaration");
                    if (arguments != 0)
                    {
                        expect(binding->receiver_type_ref != NULL &&
                            binding->receiver_type_ref->generic_arguments.count == arguments,
                            "source generic arguments retained");
                        expect(owner->node->as.type_declaration.generic_arguments.count == arguments,
                            "constructed identity retains arguments");
                        expect(strcmp(binding->receiver_type_ref->generic_arguments.items[0]->name, "int") == 0,
                            "actual argument remains int");
                    }
                }
            }
            expect(found, "queryable receiver binding");
        }
    }
    vc_semantic_model_destroy(&model);
    vc_ast_tree_destroy(&tree);
    vc_token_list_destroy(&tokens);
}

int main(void)
{
    const char *names[] = {"bool", "byte", "sbyte", "short", "ushort", "int", "uint", "long", "ulong", "float", "double", "decimal", "char", "string", "object"};
    const VcSemanticType types[] = {VC_SEM_TYPE_BOOL, VC_SEM_TYPE_BYTE, VC_SEM_TYPE_SBYTE,
        VC_SEM_TYPE_SHORT, VC_SEM_TYPE_USHORT, VC_SEM_TYPE_INT, VC_SEM_TYPE_UINT,
        VC_SEM_TYPE_LONG, VC_SEM_TYPE_ULONG, VC_SEM_TYPE_FLOAT, VC_SEM_TYPE_DOUBLE,
        VC_SEM_TYPE_DECIMAL, VC_SEM_TYPE_CHAR, VC_SEM_TYPE_STRING, VC_SEM_TYPE_OBJECT};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        char text[512];
        snprintf(text, sizeof(text), "public static class Program { public static void Main() { %s.Missing; } }", names[i]);
        check_model(text, names[i], types[i], false, 0);
    }
    char generic[] = "namespace Example { public class Box<T> { public static T Echo(T value) { return value; } } } public static class Program { public static void Main() { int x = Example.Box<int>.Echo(42); } }";
    check_model(generic, "Example.Box", VC_SEM_TYPE_UNKNOWN, true, 1);
    char named[] = "public class Widget { public static int Number; } public static class Program { public static void Main() { int x = Widget.Number; } }";
    check_model(named, "Widget", VC_SEM_TYPE_UNKNOWN, true, 0);
    char type_name[] = "public class Type { public static int Number; } public static class Program { public static void Main() { int x = Type.Number; } }";
    check_model(type_name, "Type", VC_SEM_TYPE_UNKNOWN, true, 0);
    return failures != 0;
}
