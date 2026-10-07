#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include <stdio.h>
#include <string.h>

static int failures;

static void expect(bool condition, const char *label)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", label);
        failures++;
    }
    else
        puts("True");
}

static bool parse_source(VcSource *source, VcTokenList *tokens, VcAstTree *tree,
    VcParseResult *parse)
{
    vc_token_list_init(tokens);
    vc_ast_tree_init(tree);
    vc_parse_result_init(parse);
    return vc_lex_source(source, tokens) && !tokens->has_error &&
        vc_parse_source(source, tokens, tree, parse) && !parse->has_error;
}

int main(void)
{
    char library_text[] =
        "namespace Void.Internal { "
        "[BuiltInAssociated(\"int\")] internal static class IntOwner336 { "
        "[PrimitiveConstant(\"min\")] public const int MinValue = default; "
        "[PrimitiveConstant(\"max\")] public const int MaxValue = default; "
        "public static int Property { get { return 7; } } } "
        "[BuiltInAssociated(\"float\")] internal static class FloatOwner336 { "
        "[PrimitiveConstant(\"epsilon\")] public const float Epsilon = default; } "
        "}";
    char user_text[] =
        "public static class Program { public static void Main() { "
        "int minimum = int.MinValue; int maximum = int.MaxValue; "
        "int property = int.Property; float epsilon = float.Epsilon; "
        "} }";

    VcSource sources[2] = {
        {"StandardLibrary/Void/Internal/BuiltInAssociatedMembers.void", library_text, strlen(library_text)},
        {"model336.void", user_text, strlen(user_text)}
    };
    VcTokenList tokens[2];
    VcAstTree trees[2];
    VcParseResult parses[2];
    bool parsed = true;
    for (size_t i = 0; i < 2; i++)
        parsed = parse_source(&sources[i], &tokens[i], &trees[i], &parses[i]) && parsed;
    expect(parsed, "parse primitive-constant associated model");

    VcSemanticModel model;
    VcSemanticDiagnostic diagnostic = {0};
    vc_semantic_model_init(&model);
    if (parsed)
    {
        VcSemanticUnit units[2] = {{&sources[0], &trees[0]}, {&sources[1], &trees[1]}};
        const bool analyzed = vc_semantic_analyze(units, 2, false, false, &model, &diagnostic);
        expect(analyzed, "analyze primitive-constant associated model");
        if (!analyzed)
            fprintf(stderr, "%s\n", diagnostic.message);
        if (analyzed)
        {
            size_t int_owner = (size_t)-1;
            const bool has_int_owner = vc_semantic_builtin_associated_owner(
                &model, VC_SEM_TYPE_INT, &int_owner);
            expect(has_int_owner && int_owner < model.struct_count,
                "int keeps canonical #335 associated owner");

            bool saw_min = false, saw_max = false, saw_property = false;
            if (has_int_owner && int_owner < model.struct_count)
            {
                const VcSemanticStruct *owner = &model.structs[int_owner];
                for (size_t i = 0; i < owner->field_count; i++)
                {
                    const VcSemanticField *field = &owner->fields[i];
                    const char *name = field->node->as.field_declaration.name;
                    if (strcmp(name, "MinValue") == 0)
                        saw_min = field->is_const && field->type == VC_SEM_TYPE_INT &&
                            field->primitive_constant_kind == VC_PRIMITIVE_CONSTANT_MIN;
                    if (strcmp(name, "MaxValue") == 0)
                        saw_max = field->is_const && field->type == VC_SEM_TYPE_INT &&
                            field->primitive_constant_kind == VC_PRIMITIVE_CONSTANT_MAX;
                }
                for (size_t i = 0; i < owner->property_count; i++)
                {
                    const VcSemanticProperty *property = &owner->properties[i];
                    if (strcmp(property->node->as.property_declaration.name, "Property") == 0)
                        saw_property = property->is_static && property->type == VC_SEM_TYPE_INT;
                }
            }
            expect(saw_min, "associated int MinValue carries semantic primitive-min kind");
            expect(saw_max, "associated int MaxValue carries semantic primitive-max kind");
            expect(saw_property, "associated built-in static property uses ordinary property model");

            size_t float_owner = (size_t)-1;
            const bool has_float_owner = vc_semantic_builtin_associated_owner(
                &model, VC_SEM_TYPE_FLOAT, &float_owner);
            bool saw_epsilon = false;
            if (has_float_owner && float_owner < model.struct_count)
            {
                const VcSemanticStruct *owner = &model.structs[float_owner];
                for (size_t i = 0; i < owner->field_count; i++)
                {
                    const VcSemanticField *field = &owner->fields[i];
                    if (strcmp(field->node->as.field_declaration.name, "Epsilon") == 0)
                        saw_epsilon = field->type == VC_SEM_TYPE_FLOAT &&
                            field->primitive_constant_kind == VC_PRIMITIVE_CONSTANT_EPSILON;
                }
            }
            expect(saw_epsilon, "float Epsilon is semantic epsilon kind, not a member-name branch");

            bool property_binding = false;
            bool min_binding = false;
            for (size_t i = 0; i < model.binding_count; i++)
            {
                const VcSemanticBinding *binding = &model.bindings[i];
                if (binding->node == NULL || binding->node->kind != VC_AST_MEMBER_ACCESS_EXPRESSION)
                    continue;
                const char *member = binding->node->as.member_access_expression.member;
                if (member == NULL)
                    continue;
                if (strcmp(member, "Property") == 0)
                    property_binding = binding->has_property && binding->struct_index == int_owner;
                if (strcmp(member, "MinValue") == 0)
                    min_binding = binding->has_constant && binding->struct_index == int_owner;
            }
            expect(property_binding,
                "built-in associated static property binds through ordinary #332 property binding");
            expect(min_binding,
                "built-in primitive constant binds through ordinary constant binding");
        }
    }

    vc_semantic_model_destroy(&model);
    for (size_t i = 0; i < 2; i++)
    {
        vc_ast_tree_destroy(&trees[i]);
        vc_token_list_destroy(&tokens[i]);
    }
    return failures == 0 ? 0 : 1;
}
