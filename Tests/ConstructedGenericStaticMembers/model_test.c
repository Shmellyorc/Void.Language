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
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", label);
        failures++;
    }
    else
        puts("True");
}

static void inspect_specializations(
    const VcAstNodeList *declarations,
    size_t *generic334_count,
    size_t *pair_arg_count,
    bool *canonical_generic_name,
    bool *canonical_method_name)
{
    for (size_t i = 0; i < declarations->count; i++)
    {
        const VcAstNode *node = declarations->items[i];
        if (node == NULL)
            continue;
        if (node->kind == VC_AST_NAMESPACE_DECLARATION)
        {
            inspect_specializations(&node->as.namespace_declaration.declarations,
                generic334_count, pair_arg_count, canonical_generic_name, canonical_method_name);
            continue;
        }
        if (node->kind != VC_AST_TYPE_DECLARATION)
            continue;
        if (node->as.type_declaration.original_generic_name != NULL &&
            strcmp(node->as.type_declaration.original_generic_name, "Generic334") == 0)
        {
            (*generic334_count)++;
            if (strcmp(node->as.type_declaration.name,
                    "Generic334__g1_Example__dArg334") == 0)
                *canonical_generic_name = true;
        }
        for (size_t member_index = 0;
            member_index < node->as.type_declaration.members.count;
            member_index++)
        {
            const VcAstNode *member = node->as.type_declaration.members.items[member_index];
            if (member == NULL || member->kind != VC_AST_METHOD_DECLARATION ||
                member->as.method_declaration.original_generic_name == NULL ||
                strcmp(member->as.method_declaration.original_generic_name, "Pair") != 0)
                continue;
            if (strcmp(member->as.method_declaration.name,
                    "Pair__gm1_Example__dArg334") == 0)
            {
                *canonical_method_name = true;
                (*pair_arg_count)++;
            }
        }
    }
}

int main(void)
{
    char text[] =
        "using Example; "
        "namespace Example { public class Arg334 {} "
        "public class Generic334<T> { public static T Value; } } "
        "public class Counter334<T> { public static int Value; } "
        "public class Box334<T> { public static T Current; public static T Property { get; set; } "
        "public static T Echo(T value) { return value; } "
        "public static U Pair<U>(T first, U second) { return second; } } "
        "public class Mapper334<T> { public static T Echo(T value) { return value; } } "
        "public delegate int Transform334(int value); "
        "public static class Program { public static void Main() { "
        "Generic334<Arg334>.Value = null; "
        "Example.Generic334<Example.Arg334>.Value = null; "
        "Counter334<int>.Value = 1; Counter334<string>.Value = 2; "
        "Box334<int>.Current = 3; int a = Box334<int>.Current; "
        "Box334<int>.Property = 4; int b = Box334<int>.Property; "
        "int c = Box334<int>.Echo(5); "
        "Example.Arg334 arg = null; "
        "Example.Arg334 p1 = Box334<int>.Pair<Arg334>(1, arg); "
        "Example.Arg334 p2 = Box334<int>.Pair<Example.Arg334>(1, arg); "
        "Transform334 map = Mapper334<int>.Echo; } }";

    VcSource source = {"model334.void", text, strlen(text)};
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
    expect(parsed, "parse #334 model fixture");
    if (parsed)
    {
        VcAstTree *trees[] = {&tree};
        char error[512] = {0};
        const bool specialized = vc_monomorphize(trees, 1, error, sizeof(error));
        expect(specialized, "monomorphize #334 model fixture");
        if (!specialized)
            fprintf(stderr, "%s\n", error);
        if (specialized)
        {
            size_t generic334_count = 0;
            size_t canonical_pair_count = 0;
            bool canonical_generic_name = false;
            bool canonical_method_name = false;
            inspect_specializations(&tree.root->as.compilation_unit.declarations,
                &generic334_count, &canonical_pair_count,
                &canonical_generic_name, &canonical_method_name);
            expect(generic334_count == 1,
                "equivalent generic argument spellings create one closed specialization");
            expect(canonical_generic_name,
                "closed specialization name uses resolved argument declaration identity");
            expect(canonical_pair_count == 1 && canonical_method_name,
                "equivalent generic method argument spellings share specialization identity");

            VcSemanticUnit unit = {&source, &tree};
            const bool analyzed = vc_semantic_analyze(&unit, 1, false, false,
                &model, &diagnostic);
            expect(analyzed, "analyze #334 model fixture");
            if (!analyzed)
                fprintf(stderr, "%s\n", diagnostic.message);
            if (analyzed)
            {
                size_t generic_owner = (size_t)-1;
                size_t qualified_generic_owner = (size_t)-1;
                size_t counter_int = (size_t)-1;
                size_t counter_string = (size_t)-1;
                bool saw_box_field = false;
                bool saw_box_property = false;
                bool saw_box_method = false;
                bool saw_mapper_delegate = false;

                for (size_t i = 0; i < model.binding_count; i++)
                {
                    const VcSemanticBinding *binding = &model.bindings[i];
                    if (binding->node == NULL)
                        continue;
                    if (binding->is_type_receiver && binding->receiver_type_ref != NULL &&
                        vc_semantic_type_is_struct(binding->type))
                    {
                        const char *name = binding->receiver_type_ref->name;
                        const size_t owner = vc_semantic_struct_index(binding->type);
                        if (strcmp(name, "Generic334") == 0)
                            generic_owner = owner;
                        else if (strcmp(name, "Example.Generic334") == 0)
                            qualified_generic_owner = owner;
                        else if (strcmp(name, "Counter334") == 0 &&
                            binding->receiver_type_ref->generic_arguments.count == 1)
                        {
                            const char *argument = binding->receiver_type_ref->generic_arguments.items[0]->name;
                            if (strcmp(argument, "int") == 0)
                                counter_int = owner;
                            else if (strcmp(argument, "string") == 0)
                                counter_string = owner;
                        }
                    }
                    if ((binding->has_field || binding->has_property || binding->has_method) &&
                        binding->struct_index < model.struct_count)
                    {
                        const VcSemanticStruct *owner = &model.structs[binding->struct_index];
                        if (owner->node != NULL &&
                            owner->node->as.type_declaration.original_generic_name != NULL &&
                            strcmp(owner->node->as.type_declaration.original_generic_name, "Box334") == 0)
                        {
                            if (binding->has_field && binding->type == VC_SEM_TYPE_INT)
                                saw_box_field = true;
                            if (binding->has_property && binding->type == VC_SEM_TYPE_INT)
                                saw_box_property = true;
                        }
                    }
                    if (binding->has_delegate_create &&
                        binding->delegate_method_index < model.method_count)
                    {
                        const VcSemanticMethod *method = &model.methods[binding->delegate_method_index];
                        if (method->is_static && method->parameter_count == 1 &&
                            method->parameter_types[0] == VC_SEM_TYPE_INT &&
                            method->return_type == VC_SEM_TYPE_INT)
                            saw_mapper_delegate = true;
                    }
                }

                for (size_t method_index = 0; method_index < model.method_count; method_index++)
                {
                    const VcSemanticMethod *method = &model.methods[method_index];
                    if (!method->is_static || !method->has_owner_struct ||
                        method->owner_struct_index >= model.struct_count)
                        continue;
                    const VcSemanticStruct *owner = &model.structs[method->owner_struct_index];
                    if (owner->node == NULL ||
                        owner->node->as.type_declaration.original_generic_name == NULL ||
                        strcmp(owner->node->as.type_declaration.original_generic_name, "Box334") != 0 ||
                        strcmp(method->node->as.method_declaration.name, "Echo") != 0)
                        continue;
                    if (method->return_type == VC_SEM_TYPE_INT && method->parameter_count == 1 &&
                        method->parameter_types[0] == VC_SEM_TYPE_INT)
                        saw_box_method = true;
                }

                expect(generic_owner != (size_t)-1 &&
                    generic_owner == qualified_generic_owner,
                    "qualified and unqualified equivalent closed receivers share semantic identity");
                expect(counter_int != (size_t)-1 && counter_string != (size_t)-1 &&
                    counter_int != counter_string,
                    "different generic arguments keep distinct closed semantic identities");
                expect(saw_box_field, "containing T substitutes static field type");
                expect(saw_box_property, "containing T substitutes static property type");
                expect(saw_box_method, "containing T substitutes static method signature");
                expect(saw_mapper_delegate,
                    "closed static method group exposes substituted delegate signature");
            }
        }
    }

    vc_semantic_model_destroy(&model);
    vc_ast_tree_destroy(&tree);
    vc_token_list_destroy(&tokens);
    return failures != 0;
}
