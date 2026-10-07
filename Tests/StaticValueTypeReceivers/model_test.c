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
        "namespace A { public class Box<T> { public static T Field; public const int Constant = 7; public static T Property { get; set; } } "
        "public class Widget { public static int Field; } } "
        "namespace B { public class Widget { public static int Field; } } "
        "public readonly struct Limits { public static int Property { get; set; } } "
        "public class Setter { public static int Value { private get; set; } } "
        "public static class Program { public static void Main() { "
        "A.Box<int>.Field = 1; int a = A.Box<int>.Field; "
        "A.Box<string>.Field = string.Empty; string b = A.Box<string>.Field; "
        "A.Box<int>.Property = 2; int c = A.Box<int>.Property; "
        "A.Box<string>.Property = string.Empty; string d = A.Box<string>.Property; "
        "int e = A.Box<int>.Constant; int f = A.Box<string>.Constant; "
        "A.Widget.Field = 3; B.Widget.Field = 4; Limits.Property = 5; Setter.Value = 6; } }";
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
    expect(parsed, "parse static value fixture");
    if (parsed)
    {
        VcAstTree *trees[] = {&tree};
        char error[512];
        bool specialized = vc_monomorphize(trees, 1, error, sizeof(error));
        expect(specialized, "existing specialization");
        if (specialized)
        {
            VcSemanticUnit unit = {&source, &tree};
            bool analyzed = vc_semantic_analyze(&unit, 1, false, false, &model, &diagnostic);
            expect(analyzed, "analyze static value fixture");
            if (!analyzed) fprintf(stderr, "%s\n", diagnostic.message);
            size_t int_owner = (size_t)-1, string_owner = (size_t)-1;
            size_t a_widget = (size_t)-1, b_widget = (size_t)-1;
            size_t members = 0, constants = 0;
            for (size_t i = 0; i < model.binding_count; i++)
            {
                const VcSemanticBinding *member = &model.bindings[i];
                if (member->node == NULL || member->node->kind != VC_AST_MEMBER_ACCESS_EXPRESSION ||
                    (!member->has_field && !member->has_constant && !member->has_property)) continue;
                const VcAstNode *target = member->node->as.member_access_expression.target;
                const VcSemanticBinding *receiver = vc_semantic_binding(&model, target);
                expect(receiver != NULL && receiver->is_type_receiver, "value member has a type receiver");
                if (receiver == NULL) continue;
                expect(receiver->type == vc_semantic_struct_type(member->struct_index), "receiver and member owner identity");
                members++;
                char name[512];
                expect(vc_ast_receiver_name(target, name, sizeof(name)), "receiver source name");
                if (strcmp(name, "A.Box") == 0)
                {
                    expect(receiver->receiver_type_ref != NULL && receiver->receiver_type_ref->generic_arguments.count == 1,
                        "constructed arguments retained on receiver");
                    const VcSemanticStruct *owner = &model.structs[member->struct_index];
                    expect(owner->node->as.type_declaration.generic_arguments.count == 1, "constructed declaration identity");
                    const char *argument = receiver->receiver_type_ref->generic_arguments.items[0]->name;
                    const bool integer = strcmp(argument, "int") == 0;
                    expect(integer || strcmp(argument, "string") == 0, "actual source argument retained");
                    if (integer) int_owner = member->struct_index;
                    else string_owner = member->struct_index;
                    if (member->has_constant)
                    {
                        constants++;
                        expect(!member->has_field && !member->has_property, "constant does not become storage");
                        expect(member->type == VC_SEM_TYPE_INT, "constant declared type");
                        expect(owner->fields[member->field_index].is_const, "ordinary constant field metadata");
                        expect(!owner->fields[member->field_index].is_static, "constant has no static storage flag");
                        expect(owner->fields[member->field_index].const_analysis_state == 2, "compile-time analysis complete");
                    }
                    else expect(member->type == (integer ? VC_SEM_TYPE_INT : VC_SEM_TYPE_STRING), "existing member substitution");
                }
                else if (strcmp(name, "A.Widget") == 0) a_widget = member->struct_index;
                else if (strcmp(name, "B.Widget") == 0) b_widget = member->struct_index;
                else if (strcmp(name, "Setter") == 0)
                {
                    const VcSemanticProperty *property = &model.structs[member->struct_index].properties[member->property_index];
                    expect(property->setter_reachable && !property->getter_reachable, "direct write reaches only setter");
                }
                else if (strcmp(name, "Limits") == 0)
                {
                    const VcSemanticProperty *property = &model.structs[member->struct_index].properties[member->property_index];
                    expect(property->is_static && !property->getter_readonly, "static getter has no readonly instance receiver");
                }
            }
            expect(members == 14 && constants == 2, "all static value bindings inspected");
            expect(int_owner != (size_t)-1 && string_owner != (size_t)-1 && int_owner != string_owner, "closed receiver identities distinct");
            expect(a_widget != (size_t)-1 && b_widget != (size_t)-1 && a_widget != b_widget, "same short names remain distinct");
        }
    }
    vc_semantic_model_destroy(&model);
    vc_ast_tree_destroy(&tree);
    vc_token_list_destroy(&tokens);
    return failures != 0;
}
