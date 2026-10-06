#include "parser.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    char text[] = "public class Bad { void M() { int x = 1 } } public class Good {}";
    VcSource source = {"recovery.void", text, sizeof(text) - 1};
    VcTokenList tokens;
    vc_token_list_init(&tokens);
    assert(vc_lex_source(&source, &tokens));
    VcAstTree tree;
    vc_ast_tree_init(&tree);
    VcParseResult result;
    assert(!vc_parse_source(&source, &tokens, &tree, &result));
    assert(result.has_error);
    assert(strstr(result.diagnostic.message, "expected ';'") != NULL);
    assert(result.diagnostic.context_count == 1);
    assert(tree.root->as.compilation_unit.declarations.count == 1);
    assert(strcmp(tree.root->as.compilation_unit.declarations.items[0]->as.type_declaration.name, "Good") == 0);
    vc_ast_tree_destroy(&tree);
    vc_token_list_destroy(&tokens);
    return 0;
}
