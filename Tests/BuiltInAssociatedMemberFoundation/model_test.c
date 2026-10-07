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
        "[BuiltInAssociated(\"bool\")] internal static class BoolOwner335 {} "
        "[BuiltInAssociated(\"byte\")] internal static class ByteOwner335 {} "
        "[BuiltInAssociated(\"sbyte\")] internal static class SByteOwner335 {} "
        "[BuiltInAssociated(\"short\")] internal static class ShortOwner335 {} "
        "[BuiltInAssociated(\"ushort\")] internal static class UShortOwner335 {} "
        "[BuiltInAssociated(\"int\")] internal static class IntOwner335 { "
        "public static int Field; public const int Constant = 7; "
        "public static int Property { get { return 8; } } "
        "public static int Echo(int value) { return value; } "
        "public static string Echo(string value) { return value; } "
        "public static T Identity<T>(T value) { return value; } } "
        "[BuiltInAssociated(\"uint\")] internal static class UIntOwner335 {} "
        "[BuiltInAssociated(\"long\")] internal static class LongOwner335 {} "
        "[BuiltInAssociated(\"ulong\")] internal static class ULongOwner335 {} "
        "[BuiltInAssociated(\"float\")] internal static class FloatOwner335 {} "
        "[BuiltInAssociated(\"double\")] internal static class DoubleOwner335 {} "
        "[BuiltInAssociated(\"decimal\")] internal static class DecimalOwner335 {} "
        "[BuiltInAssociated(\"char\")] internal static class CharOwner335 {} "
        "[BuiltInAssociated(\"string\")] internal static class StringOwner335 { "
        "public static string Empty { get { return \"\"; } } "
        "public static string Concat(string left, string right) { return left; } "
        "public static bool Equals(string left, string right) { return left == right; } "
        "public static int CompareOrdinal(string left, string right) { return 0; } } "
        "[BuiltInAssociated(\"object\")] internal static class ObjectOwner335 {} "
        "}";
    char user_text[] =
        "using Void; public static class Program { public static void Main() { "
        "string empty = string.Empty; "
        "string joined = string.Concat(\"a\", \"b\"); "
        "bool equal = string.Equals(\"a\", \"a\"); "
        "int order = string.CompareOrdinal(\"a\", \"b\"); "
        "int field = int.Field; int constant = int.Constant; int property = int.Property; "
        "int echo = int.Echo(4); string echoText = int.Echo(\"x\"); "
        "} }";

    VcSource sources[2] = {
        {"StandardLibrary/Void/Internal/BuiltInAssociatedMembers.void", library_text, strlen(library_text)},
        {"model335.void", user_text, strlen(user_text)}
    };
    VcTokenList tokens[2];
    VcAstTree trees[2];
    VcParseResult parses[2];
    bool parsed = true;
    for (size_t i = 0; i < 2; i++)
        parsed = parse_source(&sources[i], &tokens[i], &trees[i], &parses[i]) && parsed;
    expect(parsed, "parse built-in associated-owner model");

    VcSemanticModel model;
    VcSemanticDiagnostic diagnostic = {0};
    vc_semantic_model_init(&model);
    if (parsed)
    {
        VcSemanticUnit units[2] = {{&sources[0], &trees[0]}, {&sources[1], &trees[1]}};
        const bool analyzed = vc_semantic_analyze(units, 2, false, false, &model, &diagnostic);
        expect(analyzed, "analyze built-in associated-owner model");
        if (!analyzed)
            fprintf(stderr, "%s\n", diagnostic.message);
        if (analyzed)
        {
            const VcSemanticType builtin_types[] = {
                VC_SEM_TYPE_BOOL, VC_SEM_TYPE_BYTE, VC_SEM_TYPE_SBYTE,
                VC_SEM_TYPE_SHORT, VC_SEM_TYPE_USHORT, VC_SEM_TYPE_INT,
                VC_SEM_TYPE_UINT, VC_SEM_TYPE_LONG, VC_SEM_TYPE_ULONG,
                VC_SEM_TYPE_FLOAT, VC_SEM_TYPE_DOUBLE, VC_SEM_TYPE_DECIMAL,
                VC_SEM_TYPE_CHAR, VC_SEM_TYPE_STRING, VC_SEM_TYPE_OBJECT
            };
            const size_t builtin_count = sizeof(builtin_types) / sizeof(builtin_types[0]);
            size_t owners[sizeof(builtin_types) / sizeof(builtin_types[0])];
            bool all_registered = true;
            bool all_distinct = true;
            for (size_t i = 0; i < builtin_count; i++)
            {
                owners[i] = (size_t)-1;
                if (!vc_semantic_builtin_associated_owner(&model, builtin_types[i], &owners[i]) ||
                    owners[i] >= model.struct_count ||
                    !model.structs[owners[i]].is_builtin_associated_scope ||
                    model.structs[owners[i]].associated_builtin_type != builtin_types[i])
                    all_registered = false;
                for (size_t j = 0; j < i; j++)
                    if (owners[i] == owners[j])
                        all_distinct = false;
            }
            expect(all_registered, "every supported built-in has one canonical associated owner");
            expect(all_distinct, "different built-in semantic types have distinct owners");

            size_t int_owner = (size_t)-1;
            bool int_member_categories = false;
            if (vc_semantic_builtin_associated_owner(&model, VC_SEM_TYPE_INT, &int_owner) &&
                int_owner < model.struct_count && model.structs[int_owner].node != NULL)
            {
                const VcAstNodeList *members =
                    &model.structs[int_owner].node->as.type_declaration.members;
                bool has_field = false, has_constant = false, has_property = false;
                bool has_int_echo = false, has_string_echo = false, has_generic = false;
                for (size_t i = 0; i < members->count; i++)
                {
                    const VcAstNode *member = members->items[i];
                    if (member == NULL) continue;
                    if (member->kind == VC_AST_FIELD_DECLARATION)
                    {
                        const char *name = member->as.field_declaration.name;
                        if (strcmp(name, "Field") == 0) has_field = true;
                        if (strcmp(name, "Constant") == 0 &&
                            (member->as.field_declaration.modifiers & VC_AST_MOD_CONST) != 0)
                            has_constant = true;
                    }
                    else if (member->kind == VC_AST_PROPERTY_DECLARATION &&
                        strcmp(member->as.property_declaration.name, "Property") == 0)
                        has_property = true;
                    else if (member->kind == VC_AST_METHOD_DECLARATION)
                    {
                        const char *name = member->as.method_declaration.name;
                        if (strcmp(name, "Echo") == 0 &&
                            member->as.method_declaration.parameters.count == 1)
                        {
                            const char *type_name = member->as.method_declaration.parameters.items[0]
                                ->as.parameter.type->name;
                            if (strcmp(type_name, "int") == 0) has_int_echo = true;
                            if (strcmp(type_name, "string") == 0) has_string_echo = true;
                        }
                        if (strcmp(name, "Identity") == 0 &&
                            member->as.method_declaration.generic_parameters.count == 1)
                            has_generic = true;
                    }
                }
                int_member_categories = has_field && has_constant && has_property &&
                    has_int_echo && has_string_echo && has_generic;
            }
            expect(int_member_categories,
                "isolated built-in owner supports ordinary fields/constants/properties/overloads/generic methods");

            size_t void_owner = 0;
            expect(!vc_semantic_builtin_associated_owner(&model, VC_SEM_TYPE_VOID, &void_owner),
                "void has no associated-member owner");
            expect(!vc_semantic_builtin_associated_owner(&model, VC_SEM_TYPE_TYPE, &void_owner),
                "compiler Type semantic helper has no keyword associated-member owner");

            size_t string_owner = (size_t)-1;
            const bool has_string_owner = vc_semantic_builtin_associated_owner(
                &model, VC_SEM_TYPE_STRING, &string_owner);
            bool saw_empty = false;
            bool saw_concat = false;
            bool saw_equals = false;
            bool saw_compare = false;
            if (has_string_owner && string_owner < model.struct_count)
            {
                const VcSemanticStruct *owner = &model.structs[string_owner];
                for (size_t i = 0; i < owner->property_count; i++)
                    if (strcmp(owner->properties[i].node->as.property_declaration.name, "Empty") == 0)
                        saw_empty = owner->properties[i].is_static &&
                            owner->properties[i].type == VC_SEM_TYPE_STRING;
                for (size_t i = 0; i < model.method_count; i++)
                {
                    const VcSemanticMethod *method = &model.methods[i];
                    if (!method->has_owner_struct || method->owner_struct_index != string_owner ||
                        !method->is_static || method->node == NULL)
                        continue;
                    const char *name = method->node->as.method_declaration.name;
                    if (strcmp(name, "Concat") == 0)
                        saw_concat = method->return_type == VC_SEM_TYPE_STRING;
                    else if (strcmp(name, "Equals") == 0)
                        saw_equals = method->return_type == VC_SEM_TYPE_BOOL;
                    else if (strcmp(name, "CompareOrdinal") == 0)
                        saw_compare = method->return_type == VC_SEM_TYPE_INT;
                }
            }
            expect(saw_empty, "string.Empty lives in the canonical string associated scope");
            expect(saw_concat && saw_equals && saw_compare,
                "existing string static methods live in the canonical string associated scope");

            size_t type_receiver_count = 0;
            size_t bound_string_member_count = 0;
            for (size_t i = 0; i < model.binding_count; i++)
            {
                const VcSemanticBinding *binding = &model.bindings[i];
                if (binding->is_type_receiver && binding->type == VC_SEM_TYPE_STRING)
                    type_receiver_count++;
                if (binding->has_property && binding->struct_index == string_owner)
                    bound_string_member_count++;
                if (binding->has_method && binding->method_index < model.method_count &&
                    model.methods[binding->method_index].has_owner_struct &&
                    model.methods[binding->method_index].owner_struct_index == string_owner)
                    bound_string_member_count++;
            }
            expect(type_receiver_count >= 4,
                "string keyword receivers bind as the canonical built-in semantic type");
            expect(bound_string_member_count >= 4,
                "string associated members use ordinary property/method bindings");
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
