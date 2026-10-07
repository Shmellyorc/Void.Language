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

int main(void)
{
    char text[] =
        "namespace A { public class Utility { "
        "public static int Add(int a, int b) { return a + b; } "
        "public static int Double(int x) { return x * 2; } "
        "public static int Pick(int x) { return x; } "
        "public static int Pick(long x) { return (int)x; } } "
        "public class Box<T> { public static T Echo(T value) { return value; } } } "
        "public delegate int Transform(int value); "
        "public static class Program { public static void Main() { "
        "int a = A.Utility.Add(1, 2); Transform direct = A.Utility.Double; "
        "Transform overload = A.Utility.Pick; int b = A.Box<int>.Echo(3); } }";
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
    expect(parsed, "parse static callable fixture");
    if (parsed)
    {
        VcAstTree *trees[] = {&tree};
        char error[512] = {0};
        bool specialized = vc_monomorphize(trees, 1, error, sizeof(error));
        expect(specialized, "specialize constructed receiver");
        if (specialized)
        {
            VcSemanticUnit unit = {&source, &tree};
            bool analyzed = vc_semantic_analyze(&unit, 1, false, false, &model, &diagnostic);
            expect(analyzed, "analyze static callable fixture");
            if (!analyzed) fprintf(stderr, "%s\n", diagnostic.message);
            size_t static_calls = 0;
            size_t static_groups = 0;
            bool saw_closed_receiver = false;
            bool saw_overload_exact = false;
            for (size_t i = 0; analyzed && i < model.binding_count; i++)
            {
                const VcSemanticBinding *binding = &model.bindings[i];
                if (binding->node == NULL) continue;
                if (binding->has_method && binding->method_index < model.method_count)
                {
                    const VcSemanticMethod *method = &model.methods[binding->method_index];
                    if (method->is_static && binding->node->kind == VC_AST_CALL_EXPRESSION)
                    {
                        static_calls++;
                        expect(method->has_owner_struct, "static call keeps declaring type identity");
                    }
                }
                if (binding->has_delegate_create && binding->delegate_method_index < model.method_count)
                {
                    const VcSemanticMethod *method = &model.methods[binding->delegate_method_index];
                    if (method->is_static)
                    {
                        static_groups++;
                        expect(!binding->delegate_has_target, "static delegate group has no runtime target");
                        if (strcmp(method->node->as.method_declaration.name, "Pick") == 0)
                        {
                            saw_overload_exact = true;
                            expect(method->parameter_count == 1 && method->parameter_types[0] == VC_SEM_TYPE_INT,
                                "delegate target type selects exact overload");
                        }
                    }
                }
                if (binding->is_type_receiver && binding->receiver_type_ref != NULL &&
                    strcmp(binding->receiver_type_ref->name, "A.Box") == 0)
                {
                    saw_closed_receiver = true;
                    expect(binding->receiver_type_ref->generic_arguments.count == 1,
                        "constructed callable receiver retains generic argument");
                    expect(strcmp(binding->receiver_type_ref->generic_arguments.items[0]->name, "int") == 0,
                        "constructed callable receiver retains int identity");
                }
            }
            expect(static_calls >= 2, "static call bindings observed");
            expect(static_groups == 2, "static method-group bindings observed");
            expect(saw_overload_exact, "overloaded static method group resolved");
            expect(saw_closed_receiver, "constructed static callable receiver observed");
        }
    }
    vc_semantic_model_destroy(&model);
    vc_ast_tree_destroy(&tree);
    vc_token_list_destroy(&tokens);
    return failures != 0;
}
