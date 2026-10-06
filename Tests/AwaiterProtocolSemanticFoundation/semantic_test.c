#include "ast.h"
#include "lexer.h"
#include "monomorph.h"
#include "parser.h"
#include "semantic.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct TestUnit
{
    VcSource source;
    VcTokenList tokens;
    VcAstTree tree;
    VcParseResult parse;
    VcSemanticUnit semantic;
} TestUnit;

static bool load_unit(TestUnit *unit, const char *path)
{
    char error[512];
    vc_source_init(&unit->source);
    vc_token_list_init(&unit->tokens);
    vc_ast_tree_init(&unit->tree);
    vc_parse_result_init(&unit->parse);
    if (!vc_source_load(path, &unit->source, error, sizeof(error)))
        return false;
    if (!vc_lex_source(&unit->source, &unit->tokens) || unit->tokens.has_error)
        return false;
    if (!vc_parse_source(&unit->source, &unit->tokens, &unit->tree, &unit->parse) ||
        unit->parse.has_error)
        return false;
    unit->semantic.source = &unit->source;
    unit->semantic.tree = &unit->tree;
    return true;
}

static void destroy_unit(TestUnit *unit)
{
    vc_ast_tree_destroy(&unit->tree);
    vc_token_list_destroy(&unit->tokens);
    vc_source_destroy(&unit->source);
}

int main(void)
{
    const char *paths[] = {
        "Tests/AwaiterProtocolSemanticFoundation/Semantic/Core.void",
        "Tests/AwaiterProtocolSemanticFoundation/Semantic/Tasks.void",
        "Tests/AwaiterProtocolSemanticFoundation/Semantic/Program.void"
    };
    TestUnit units[3] = {0};
    VcSemanticUnit semantic_units[3] = {0};
    bool loaded = true;
    for (size_t i = 0; i < 3; i++)
    {
        loaded = loaded && load_unit(&units[i], paths[i]);
        semantic_units[i] = units[i].semantic;
    }
    puts(loaded ? "True" : "False");
    if (!loaded)
        return 1;

    VcAstTree *trees[3] = {&units[0].tree, &units[1].tree, &units[2].tree};
    char mono_error[512] = {0};
    const bool monomorphized = vc_monomorphize(trees, 3, mono_error, sizeof(mono_error));
    puts(monomorphized ? "True" : "False");
    if (!monomorphized)
    {
        fprintf(stderr, "%s\n", mono_error);
        for (size_t i = 0; i < 3; i++)
            destroy_unit(&units[i]);
        return 1;
    }

    VcSemanticModel model = {0};
    model.async_prelower = true;
    VcSemanticDiagnostic diagnostic = {0};
    const bool valid = vc_semantic_analyze(
        semantic_units, 3, false, false, &model, &diagnostic);
    puts(valid ? "True" : "False");
    if (!valid)
    {
        fprintf(stderr, "%s\n", diagnostic.message);
        vc_semantic_model_destroy(&model);
        for (size_t i = 0; i < 3; i++)
            destroy_unit(&units[i]);
        return 1;
    }

    size_t await_count = 0;
    bool saw_int = false;
    bool saw_void = false;
    bool member_indices_valid = true;
    bool dispatch_metadata_valid = true;
    for (size_t i = 0; i < model.binding_count; i++)
    {
        const VcSemanticBinding *binding = &model.bindings[i];
        if (!binding->has_await_protocol)
            continue;
        await_count++;
        saw_int = saw_int || binding->type == VC_SEM_TYPE_INT;
        saw_void = saw_void || binding->type == VC_SEM_TYPE_VOID;
        member_indices_valid = member_indices_valid &&
            binding->await_get_awaiter_method_index < model.method_count &&
            binding->await_is_completed_struct_index < model.struct_count &&
            binding->await_is_completed_property_index <
                model.structs[binding->await_is_completed_struct_index].property_count &&
            binding->await_on_completed_method_index < model.method_count &&
            binding->await_get_result_method_index < model.method_count;
        dispatch_metadata_valid = dispatch_metadata_valid &&
            !binding->await_get_awaiter_interface_dispatch &&
            !binding->await_get_awaiter_virtual_dispatch &&
            !binding->await_is_completed_interface_dispatch &&
            !binding->await_is_completed_virtual_dispatch &&
            !binding->await_on_completed_interface_dispatch &&
            !binding->await_on_completed_virtual_dispatch &&
            !binding->await_get_result_interface_dispatch &&
            !binding->await_get_result_virtual_dispatch;
    }
    puts(await_count == 2 ? "True" : "False");
    puts(saw_int ? "True" : "False");
    puts(saw_void ? "True" : "False");
    puts(member_indices_valid ? "True" : "False");
    puts(dispatch_metadata_valid ? "True" : "False");

    vc_semantic_model_destroy(&model);
    for (size_t i = 0; i < 3; i++)
        destroy_unit(&units[i]);
    return 0;
}
