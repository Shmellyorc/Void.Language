#include "ast.h"
#include "lexer.h"
#include "monomorph.h"
#include "parser.h"
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
        "public sealed class C { "
        "private int Instance(int value) { return value; } "
        "private static int Static(int value) { return value; } "
        "public int Run() { return Instance(1) + this.Instance(2) + Static(3); } } "
        "public static class Program { public static void Main() { C c = new C(); int x = c.Run(); } }";
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
    expect(parsed, "parse implicit receiver model fixture");
    if (parsed)
    {
        VcAstTree *trees[] = {&tree};
        char error[512] = {0};
        bool specialized = vc_monomorphize(trees, 1, error, sizeof(error));
        expect(specialized, "specialize implicit receiver model fixture");
        if (specialized)
        {
            VcSemanticUnit unit = {&source, &tree};
            bool analyzed = vc_semantic_analyze(&unit, 1, false, false, &model, &diagnostic);
            expect(analyzed, "analyze implicit receiver model fixture");
            if (!analyzed) fprintf(stderr, "%s\n", diagnostic.message);
            size_t implicit_instance = 0;
            size_t explicit_instance = 0;
            size_t simple_static = 0;
            for (size_t i = 0; analyzed && i < model.binding_count; i++)
            {
                const VcSemanticBinding *binding = &model.bindings[i];
                if (!binding->has_method || binding->method_index >= model.method_count ||
                    binding->node == NULL || binding->node->kind != VC_AST_CALL_EXPRESSION)
                    continue;
                const VcSemanticMethod *method = &model.methods[binding->method_index];
                const char *name = method->node->as.method_declaration.name;
                const VcAstNode *callee = binding->node->as.call_expression.callee;
                if (strcmp(name, "Instance") == 0 &&
                    callee->kind == VC_AST_IDENTIFIER_EXPRESSION)
                {
                    expect(binding->implicit_receiver, "simple instance call records implicit receiver");
                    implicit_instance++;
                }
                else if (strcmp(name, "Instance") == 0 &&
                    callee->kind == VC_AST_MEMBER_ACCESS_EXPRESSION)
                {
                    expect(!binding->implicit_receiver, "explicit this remains explicit receiver");
                    explicit_instance++;
                }
                else if (strcmp(name, "Static") == 0 &&
                    callee->kind == VC_AST_IDENTIFIER_EXPRESSION)
                {
                    expect(!binding->implicit_receiver, "simple static call has no implicit receiver");
                    simple_static++;
                }
            }
            expect(implicit_instance == 1, "one implicit instance call binding observed");
            expect(explicit_instance == 1, "one explicit instance call binding observed");
            expect(simple_static == 1, "one simple static call binding observed");
        }
    }

    vc_semantic_model_destroy(&model);
    vc_ast_tree_destroy(&tree);
    vc_token_list_destroy(&tokens);
    return failures != 0;
}
