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
        "Tests/AsyncLambdaSyntaxSemanticFoundationModel/Semantic/Core.void",
        "Tests/AsyncLambdaSyntaxSemanticFoundationModel/Semantic/Tasks.void",
        "Tests/AsyncLambdaSyntaxSemanticFoundationModel/Semantic/Program.void"
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

    size_t async_count = 0;
    size_t sync_count = 0;
    size_t task_count = 0;
    size_t value_task_count = 0;
    size_t async_void_result_count = 0;
    size_t async_int_result_count = 0;
    size_t expression_body_count = 0;
    bool ast_flags_match = true;
    bool target_return_is_preserved = true;
    for (size_t i = 0; i < model.lambda_count; i++)
    {
        const VcSemanticLambda *lambda = &model.lambdas[i];
        ast_flags_match = ast_flags_match &&
            lambda->node->as.lambda_expression.is_async == lambda->is_async;
        expression_body_count += lambda->node->as.lambda_expression.expression_body ? 1u : 0u;
        if (!lambda->is_async)
        {
            sync_count++;
            target_return_is_preserved = target_return_is_preserved &&
                lambda->return_type == lambda->async_result_type;
            continue;
        }

        async_count++;
        target_return_is_preserved = target_return_is_preserved &&
            lambda->return_type != lambda->async_result_type;
        if (lambda->async_returns_value_task)
            value_task_count++;
        else
            task_count++;
        if (lambda->async_result_type == VC_SEM_TYPE_VOID)
            async_void_result_count++;
        if (lambda->async_result_type == VC_SEM_TYPE_INT)
            async_int_result_count++;
    }

    puts(model.lambda_count == 8 ? "True" : "False");
    puts(async_count == 6 ? "True" : "False");
    puts(sync_count == 2 ? "True" : "False");
    puts(task_count == 4 ? "True" : "False");
    puts(value_task_count == 2 ? "True" : "False");
    puts(async_void_result_count == 2 ? "True" : "False");
    puts(async_int_result_count == 4 ? "True" : "False");
    puts(expression_body_count == 3 ? "True" : "False");
    puts(ast_flags_match ? "True" : "False");
    puts(target_return_is_preserved ? "True" : "False");

    size_t await_count = 0;
    size_t await_void_count = 0;
    size_t await_int_count = 0;
    bool await_protocol_metadata = true;
    for (size_t i = 0; i < model.binding_count; i++)
    {
        const VcSemanticBinding *binding = &model.bindings[i];
        if (!binding->has_await_protocol)
            continue;
        await_count++;
        await_void_count += binding->type == VC_SEM_TYPE_VOID ? 1u : 0u;
        await_int_count += binding->type == VC_SEM_TYPE_INT ? 1u : 0u;
        await_protocol_metadata = await_protocol_metadata &&
            binding->await_get_awaiter_method_index < model.method_count &&
            binding->await_on_completed_method_index < model.method_count &&
            binding->await_get_result_method_index < model.method_count;
    }
    puts(await_count == 5 ? "True" : "False");
    puts(await_void_count == 3 ? "True" : "False");
    puts(await_int_count == 2 ? "True" : "False");
    puts(await_protocol_metadata ? "True" : "False");

    vc_semantic_model_destroy(&model);
    for (size_t i = 0; i < 3; i++)
        destroy_unit(&units[i]);
    return 0;
}
