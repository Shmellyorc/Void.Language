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
        "Tests/AwaitUsingSyntaxStructuralAsyncDisposeSemanticsModel/Semantic/Core.void",
        "Tests/AwaitUsingSyntaxStructuralAsyncDisposeSemanticsModel/Semantic/Tasks.void",
        "Tests/AwaitUsingSyntaxStructuralAsyncDisposeSemanticsModel/Semantic/Program.void"
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
        for (size_t i = 0; i < 3; i++) destroy_unit(&units[i]);
        return 1;
    }

    VcSemanticModel model = {0};
    model.async_prelower = true;
    VcSemanticDiagnostic diagnostic = {0};
    const bool valid = vc_semantic_analyze(semantic_units, 3, false, false, &model, &diagnostic);
    puts(valid ? "True" : "False");
    if (!valid)
    {
        fprintf(stderr, "%s\n", diagnostic.message);
        vc_semantic_model_destroy(&model);
        for (size_t i = 0; i < 3; i++) destroy_unit(&units[i]);
        return 1;
    }

    size_t async_using_count = 0;
    size_t await_protocol_count = 0;
    bool using_metadata = true;
    bool protocol_metadata = true;
    bool all_dispose_async = true;
    bool all_get_result_void = true;
    bool saw_custom_completion = false;
    bool saw_task_completion = false;
    bool saw_value_task_completion = false;

    for (size_t i = 0; i < model.binding_count; i++)
    {
        const VcSemanticBinding *binding = &model.bindings[i];
        if (binding->has_using && binding->using_async)
        {
            async_using_count++;
            using_metadata = using_metadata &&
                binding->using_dispose_method_index < model.method_count &&
                binding->node != NULL && binding->node->kind == VC_AST_USING_STATEMENT &&
                binding->node->as.using_statement.is_await &&
                binding->node->as.using_statement.dispose_await_protocol != NULL;
            if (binding->using_dispose_method_index < model.method_count)
            {
                const VcSemanticMethod *method = &model.methods[binding->using_dispose_method_index];
                const char *method_name = method->node != NULL &&
                    method->node->kind == VC_AST_METHOD_DECLARATION
                    ? method->node->as.method_declaration.name : NULL;
                all_dispose_async = all_dispose_async && method_name != NULL &&
                    strcmp(method_name, "DisposeAsync") == 0;
            }
        }
        if (binding->has_await_protocol)
        {
            await_protocol_count++;
            protocol_metadata = protocol_metadata &&
                binding->await_get_awaiter_method_index < model.method_count &&
                binding->await_on_completed_method_index < model.method_count &&
                binding->await_get_result_method_index < model.method_count;
            all_get_result_void = all_get_result_void && binding->type == VC_SEM_TYPE_VOID;
            const char *name = vc_semantic_type_name(&model, binding->awaitable_type);
            saw_custom_completion = saw_custom_completion || strcmp(name, "CustomCompletion275") == 0;
            saw_task_completion = saw_task_completion || strcmp(name, "Void.Threading.Tasks.Task") == 0 || strcmp(name, "Task") == 0;
            saw_value_task_completion = saw_value_task_completion || strcmp(name, "Void.Threading.Tasks.ValueTask") == 0 || strcmp(name, "ValueTask") == 0;
        }
    }

    puts(async_using_count == 4 ? "True" : "False");
    puts(await_protocol_count == 4 ? "True" : "False");
    puts(using_metadata ? "True" : "False");
    puts(protocol_metadata ? "True" : "False");
    puts(all_dispose_async ? "True" : "False");
    puts(all_get_result_void ? "True" : "False");
    puts(saw_custom_completion ? "True" : "False");
    puts(saw_task_completion ? "True" : "False");
    puts(saw_value_task_completion ? "True" : "False");

    vc_semantic_model_destroy(&model);
    for (size_t i = 0; i < 3; i++) destroy_unit(&units[i]);
    return 0;
}
