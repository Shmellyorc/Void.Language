#include "ast.h"
#include "../../Runtime/include/vc_utf8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VC_ARENA_BLOCK_SIZE 16384u

struct VcArenaBlock
{
    struct VcArenaBlock *next;
    size_t used;
    size_t capacity;
    unsigned char data[];
};

static size_t align_up(size_t value, size_t alignment)
{
    const size_t remainder = value % alignment;
    return remainder == 0 ? value : value + (alignment - remainder);
}

static VcArenaBlock *arena_block_create(size_t minimum_capacity)
{
    size_t capacity = VC_ARENA_BLOCK_SIZE;
    if (capacity < minimum_capacity)
        capacity = align_up(minimum_capacity, VC_ARENA_BLOCK_SIZE);

    VcArenaBlock *block = malloc(sizeof(*block) + capacity);
    if (block == NULL)
        return NULL;

    block->next = NULL;
    block->used = 0;
    block->capacity = capacity;
    return block;
}

void vc_ast_tree_init(VcAstTree *tree)
{
    memset(tree, 0, sizeof(*tree));
}

void vc_ast_tree_destroy(VcAstTree *tree)
{
    VcArenaBlock *block = tree->arena.first;
    while (block != NULL)
    {
        VcArenaBlock *next = block->next;
        free(block);
        block = next;
    }

    memset(tree, 0, sizeof(*tree));
}

void *vc_ast_alloc(VcAstTree *tree, size_t size)
{
    const size_t alignment = sizeof(void *);
    size = align_up(size, alignment);

    VcArenaBlock *block = tree->arena.current;
    if (block == NULL || block->used + size > block->capacity)
    {
        VcArenaBlock *next = arena_block_create(size);
        if (next == NULL)
            return NULL;

        if (tree->arena.first == NULL)
            tree->arena.first = next;
        else
            tree->arena.current->next = next;

        tree->arena.current = next;
        block = next;
    }

    void *memory = block->data + block->used;
    block->used += size;
    memset(memory, 0, size);
    return memory;
}

char *vc_ast_copy_text(VcAstTree *tree, const char *text, size_t length)
{
    char *copy = vc_ast_alloc(tree, length + 1);
    if (copy == NULL)
        return NULL;

    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

VcAstNode *vc_ast_new_node(VcAstTree *tree, VcAstKind kind, VcSourceLocation location)
{
    VcAstNode *node = vc_ast_alloc(tree, sizeof(*node));
    if (node == NULL)
        return NULL;

    node->kind = kind;
    node->location = location;
    node->span.start = location;
    node->span.end = location;
    return node;
}

VcAstTypeRef *vc_ast_new_type(VcAstTree *tree, VcSourceLocation location)
{
    VcAstTypeRef *type = vc_ast_alloc(tree, sizeof(*type));
    if (type != NULL)
    {
        type->location = location;
        type->span.start = location;
        type->span.end = location;
    }
    return type;
}

bool vc_ast_tree_contains(const VcAstTree *tree, const void *pointer)
{
    if (tree == NULL || pointer == NULL)
        return false;

    const unsigned char *value = pointer;
    for (const VcArenaBlock *block = tree->arena.first; block != NULL; block = block->next)
    {
        if (value >= block->data && value < block->data + block->used)
            return true;
    }
    return false;
}

static bool grow_pointer_array(VcAstTree *tree, void ***items, size_t *capacity, size_t count)
{
    if (count < *capacity)
        return true;

    const size_t new_capacity = *capacity == 0 ? 4 : *capacity * 2;
    void **new_items = vc_ast_alloc(tree, new_capacity * sizeof(*new_items));
    if (new_items == NULL)
        return false;

    if (*items != NULL && count > 0)
        memcpy(new_items, *items, count * sizeof(*new_items));

    *items = new_items;
    *capacity = new_capacity;
    return true;
}

bool vc_ast_node_list_push(VcAstTree *tree, VcAstNodeList *list, VcAstNode *node)
{
    void **items = (void **)list->items;
    if (!grow_pointer_array(tree, &items, &list->capacity, list->count))
        return false;

    list->items = (VcAstNode **)items;
    list->items[list->count++] = node;
    return true;
}

bool vc_ast_type_list_push(VcAstTree *tree, VcAstTypeList *list, VcAstTypeRef *type)
{
    void **items = (void **)list->items;
    if (!grow_pointer_array(tree, &items, &list->capacity, list->count))
        return false;

    list->items = (VcAstTypeRef **)items;
    list->items[list->count++] = type;
    return true;
}

bool vc_ast_string_list_push(VcAstTree *tree, VcAstStringList *list, char *value)
{
    void **items = (void **)list->items;
    if (!grow_pointer_array(tree, &items, &list->capacity, list->count))
        return false;

    list->items = (char **)items;
    list->items[list->count++] = value;
    return true;
}

static bool rectangular_initializer_shape_sequence(
    const VcAstNodeList *items,
    VcSourceLocation location,
    size_t dimension,
    size_t rank,
    size_t *lengths,
    bool *lengths_set,
    VcAstRectangularInitializerIssue *issue)
{
    if (!lengths_set[dimension])
    {
        lengths[dimension] = items->count;
        lengths_set[dimension] = true;
    }
    else if (lengths[dimension] != items->count)
    {
        if (issue != NULL)
        {
            issue->kind = VC_AST_RECTANGULAR_INITIALIZER_ISSUE_INCONSISTENT_LENGTH;
            issue->location = items->count == 0 ? location : items->items[0]->location;
            issue->dimension = dimension;
        }
        return false;
    }

    for (size_t i = 0; i < items->count; i++)
    {
        const VcAstNode *item = items->items[i];
        if (item == NULL)
            return false;

        if (dimension + 1 < rank)
        {
            if (item->kind != VC_AST_COLLECTION_INITIALIZER_ELEMENT)
            {
                if (issue != NULL)
                {
                    issue->kind = VC_AST_RECTANGULAR_INITIALIZER_ISSUE_MISSING_GROUP;
                    issue->location = item->location;
                    issue->dimension = dimension + 1;
                }
                return false;
            }
            if (!rectangular_initializer_shape_sequence(
                    &item->as.collection_initializer_element.arguments,
                    item->location, dimension + 1, rank,
                    lengths, lengths_set, issue))
                return false;
        }
        else if (item->kind == VC_AST_COLLECTION_INITIALIZER_ELEMENT)
        {
            if (issue != NULL)
            {
                issue->kind = VC_AST_RECTANGULAR_INITIALIZER_ISSUE_EXTRA_GROUP;
                issue->location = item->location;
                issue->dimension = dimension;
            }
            return false;
        }
    }
    return true;
}

bool vc_ast_rectangular_initializer_shape(
    const VcAstNodeList *items,
    VcSourceLocation location,
    size_t rank,
    size_t *lengths,
    VcAstRectangularInitializerIssue *issue)
{
    if (items == NULL || rank < 2 || lengths == NULL)
        return false;

    bool *lengths_set = calloc(rank, sizeof(*lengths_set));
    if (lengths_set == NULL)
        return false;
    if (issue != NULL)
        memset(issue, 0, sizeof(*issue));

    const bool ok = rectangular_initializer_shape_sequence(items, location, 0, rank,
        lengths, lengths_set, issue);
    if (ok)
    {
        for (size_t i = 0; i < rank; i++)
            if (!lengths_set[i])
                lengths[i] = 0;
    }
    free(lengths_set);
    return ok;
}

static void indent(int depth)
{
    for (int i = 0; i < depth; i++)
        fputs("  ", stdout);
}

static void dump_modifiers(uint32_t modifiers)
{
    static const struct
    {
        uint32_t flag;
        const char *name;
    } names[] = {
        {VC_AST_MOD_PUBLIC, "public"},
        {VC_AST_MOD_PRIVATE, "private"},
        {VC_AST_MOD_PROTECTED, "protected"},
        {VC_AST_MOD_INTERNAL, "internal"},
        {VC_AST_MOD_STATIC, "static"},
        {VC_AST_MOD_SEALED, "sealed"},
        {VC_AST_MOD_ABSTRACT, "abstract"},
        {VC_AST_MOD_READONLY, "readonly"},
        {VC_AST_MOD_UNSAFE, "unsafe"},
        {VC_AST_MOD_EXTERN, "extern"},
        {VC_AST_MOD_VIRTUAL, "virtual"},
        {VC_AST_MOD_OVERRIDE, "override"},
        {VC_AST_MOD_CONST, "const"},
        {VC_AST_MOD_ASYNC, "async"},
        {VC_AST_MOD_REF, "ref"}
    };

    if (modifiers == 0)
        return;

    fputs(" [", stdout);
    bool first = true;
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        if ((modifiers & names[i].flag) == 0)
            continue;

        if (!first)
            putchar(' ');
        fputs(names[i].name, stdout);
        first = false;
    }
    putchar(']');
}

static void dump_type(const VcAstTypeRef *type)
{
    if (type == NULL)
    {
        fputs("<none>", stdout);
        return;
    }

    fputs(type->name != NULL ? type->name : "<type>", stdout);
    if (type->generic_arguments.count > 0)
    {
        putchar('<');
        for (size_t i = 0; i < type->generic_arguments.count; i++)
        {
            if (i > 0)
                fputs(", ", stdout);
            dump_type(type->generic_arguments.items[i]);
        }
        putchar('>');
    }

    if (type->nullable)
        putchar('?');

    for (size_t i = 0; i < type->pointer_depth; i++)
        putchar('*');

    if (type->rectangular_rank > 1)
    {
        putchar('[');
        for (size_t i = 1; i < type->rectangular_rank; i++)
            putchar(',');
        putchar(']');
    }

    for (size_t i = 0; i < type->array_rank; i++)
        fputs("[]", stdout);

}

static const char *type_kind_name(VcAstTypeKind kind)
{
    switch (kind)
    {
        case VC_AST_TYPE_CLASS: return "Class";
        case VC_AST_TYPE_STRUCT: return "Struct";
        case VC_AST_TYPE_INTERFACE: return "Interface";
        case VC_AST_TYPE_DELEGATE: return "Delegate";
        case VC_AST_TYPE_ENUM: return "Enum";
    }
    return "Type";
}

static const char *literal_kind_name(VcAstLiteralKind kind)
{
    switch (kind)
    {
        case VC_AST_LITERAL_NUMBER: return "Number";
        case VC_AST_LITERAL_STRING: return "String";
        case VC_AST_LITERAL_CHARACTER: return "Character";
        case VC_AST_LITERAL_TRUE: return "True";
        case VC_AST_LITERAL_FALSE: return "False";
        case VC_AST_LITERAL_NULL: return "Null";
    }
    return "Literal";
}

static void dump_string_list(const VcAstStringList *list)
{
    if (list->count == 0)
        return;

    putchar('<');
    for (size_t i = 0; i < list->count; i++)
    {
        if (i > 0)
            fputs(", ", stdout);
        fputs(list->items[i], stdout);
    }
    putchar('>');
}

static void dump_node(const VcAstNode *node, int depth)
{
    if (node == NULL)
        return;

    for (size_t i = 0; i < node->attributes.count; i++)
        dump_node(node->attributes.items[i], depth);

    indent(depth);
    if (node->argument_name != NULL)
    {
        printf("NamedArgument %s\n", node->argument_name);
        depth++;
        indent(depth);
    }

    switch (node->kind)
    {
        case VC_AST_COMPILATION_UNIT:
            puts("CompilationUnit");
            for (size_t i = 0; i < node->as.compilation_unit.declarations.count; i++)
                dump_node(node->as.compilation_unit.declarations.items[i], depth + 1);
            break;

        case VC_AST_USING_DECLARATION:
            printf("Using %s", node->as.using_declaration.name);
            if (node->as.using_declaration.alias != NULL)
                printf(" as %s", node->as.using_declaration.alias);
            putchar('\n');
            break;

        case VC_AST_NAMESPACE_DECLARATION:
            printf("Namespace %s%s\n",
                node->as.namespace_declaration.name,
                node->as.namespace_declaration.file_scoped ? " (file)" : "");
            for (size_t i = 0; i < node->as.namespace_declaration.declarations.count; i++)
                dump_node(node->as.namespace_declaration.declarations.items[i], depth + 1);
            break;

        case VC_AST_TYPE_DECLARATION:
            printf("%s %s", type_kind_name(node->as.type_declaration.type_kind), node->as.type_declaration.name);
            dump_string_list(&node->as.type_declaration.generic_parameters);
            dump_modifiers(node->as.type_declaration.modifiers);
            if (node->as.type_declaration.base_types.count > 0)
            {
                fputs(" : ", stdout);
                for (size_t i = 0; i < node->as.type_declaration.base_types.count; i++)
                {
                    if (i > 0)
                        fputs(", ", stdout);
                    dump_type(node->as.type_declaration.base_types.items[i]);
                }
            }
            putchar('\n');
            for (size_t i = 0; i < node->as.type_declaration.generic_constraints.count; i++)
                dump_node(node->as.type_declaration.generic_constraints.items[i], depth + 1);
            for (size_t i = 0; i < node->as.type_declaration.members.count; i++)
                dump_node(node->as.type_declaration.members.items[i], depth + 1);
            break;

        case VC_AST_ENUM_MEMBER:
            printf("EnumMember %s\n", node->as.enum_member.name);
            if (node->as.enum_member.value != NULL)
                dump_node(node->as.enum_member.value, depth + 1);
            break;

        case VC_AST_METHOD_DECLARATION:
            if (node->as.method_declaration.is_constructor)
                printf("Constructor %s", node->as.method_declaration.name);
            else if (node->as.method_declaration.is_operator)
                printf("Operator %s", vc_token_kind_name(node->as.method_declaration.operator_kind));
            else
                printf("Method %s", node->as.method_declaration.name);
            dump_string_list(&node->as.method_declaration.generic_parameters);
            if (node->as.method_declaration.returns_ref)
                fputs(node->as.method_declaration.returns_ref_readonly ?
                    " : ref readonly " : " : ref ", stdout);
            else
                fputs(" : ", stdout);
            dump_type(node->as.method_declaration.return_type);
            dump_modifiers(node->as.method_declaration.modifiers);
            putchar('\n');
            for (size_t i = 0; i < node->as.method_declaration.parameters.count; i++)
                dump_node(node->as.method_declaration.parameters.items[i], depth + 1);
            for (size_t i = 0; i < node->as.method_declaration.generic_constraints.count; i++)
                dump_node(node->as.method_declaration.generic_constraints.items[i], depth + 1);
            if (node->as.method_declaration.has_base_constructor_initializer)
            {
                indent(depth + 1);
                puts("BaseConstructorInitializer");
                for (size_t i = 0;
                     i < node->as.method_declaration.constructor_initializer_arguments.count;
                     i++)
                    dump_node(node->as.method_declaration.constructor_initializer_arguments.items[i], depth + 2);
            }
            else if (node->as.method_declaration.has_this_constructor_initializer)
            {
                indent(depth + 1);
                puts("ThisConstructorInitializer");
                for (size_t i = 0;
                     i < node->as.method_declaration.constructor_initializer_arguments.count;
                     i++)
                    dump_node(node->as.method_declaration.constructor_initializer_arguments.items[i], depth + 2);
            }
            dump_node(node->as.method_declaration.body, depth + 1);
            break;

        case VC_AST_FIELD_DECLARATION:
            printf("%s %s : ", node->as.field_declaration.is_event ? "Event" : "Field",
                node->as.field_declaration.name);
            if (node->as.field_declaration.is_ref)
                fputs(node->as.field_declaration.ref_readonly ? "ref readonly " : "ref ", stdout);
            dump_type(node->as.field_declaration.type);
            dump_modifiers(node->as.field_declaration.modifiers);
            putchar('\n');
            if (node->as.field_declaration.initializer != NULL)
                dump_node(node->as.field_declaration.initializer, depth + 1);
            if (node->as.field_declaration.has_event_add_accessor)
            {
                indent(depth + 1);
                puts("AddAccessor");
                dump_node(node->as.field_declaration.event_add_body, depth + 2);
            }
            if (node->as.field_declaration.has_event_remove_accessor)
            {
                indent(depth + 1);
                puts("RemoveAccessor");
                dump_node(node->as.field_declaration.event_remove_body, depth + 2);
            }
            break;

        case VC_AST_PROPERTY_DECLARATION:
            printf("Property %s : ", node->as.property_declaration.name);
            if (node->as.property_declaration.returns_ref)
                fputs(node->as.property_declaration.returns_ref_readonly ?
                    "ref readonly " : "ref ", stdout);
            dump_type(node->as.property_declaration.type);
            dump_modifiers(node->as.property_declaration.modifiers);
            putchar('\n');
            if (node->as.property_declaration.has_getter)
            {
                indent(depth + 1);
                fputs("Getter", stdout);
                dump_modifiers(node->as.property_declaration.getter_modifiers);
                if (node->as.property_declaration.getter_auto)
                    fputs(" [auto]", stdout);
                putchar('\n');
                if (node->as.property_declaration.getter_body != NULL)
                    dump_node(node->as.property_declaration.getter_body, depth + 2);
            }
            if (node->as.property_declaration.has_setter)
            {
                indent(depth + 1);
                fputs("Setter", stdout);
                dump_modifiers(node->as.property_declaration.setter_modifiers);
                if (node->as.property_declaration.setter_auto)
                    fputs(" [auto]", stdout);
                putchar('\n');
                if (node->as.property_declaration.setter_body != NULL)
                    dump_node(node->as.property_declaration.setter_body, depth + 2);
            }
            if (node->as.property_declaration.initializer != NULL)
            {
                indent(depth + 1);
                fputs("Initializer\n", stdout);
                dump_node(node->as.property_declaration.initializer, depth + 2);
            }
            break;

        case VC_AST_PARAMETER:
            fputs("Parameter ", stdout);
            if (node->as.parameter.extension_receiver)
                fputs("this ", stdout);
            if (node->as.parameter.scoped)
                fputs("scoped ", stdout);
            if (node->as.parameter.modifier != VC_TOKEN_EOF)
                printf("%s ", vc_token_kind_name(node->as.parameter.modifier));
            printf("%s : ", node->as.parameter.name);
            dump_type(node->as.parameter.type);
            putchar('\n');
            if (node->as.parameter.default_value != NULL)
                dump_node(node->as.parameter.default_value, depth + 1);
            break;

        case VC_AST_ATTRIBUTE:
            printf("Attribute %s\n", node->as.attribute.name);
            for (size_t i = 0; i < node->as.attribute.arguments.count; i++)
                dump_node(node->as.attribute.arguments.items[i], depth + 1);
            break;

        case VC_AST_GENERIC_CONSTRAINT:
            printf("GenericConstraint %s", node->as.generic_constraint.parameter);
            if (node->as.generic_constraint.requires_reference_type) fputs(" class", stdout);
            if (node->as.generic_constraint.requires_value_type) fputs(" struct", stdout);
            if (node->as.generic_constraint.requires_unmanaged_type) fputs(" unmanaged", stdout);
            for (size_t i = 0; i < node->as.generic_constraint.type_constraints.count; i++)
            {
                fputs(" ", stdout);
                dump_type(node->as.generic_constraint.type_constraints.items[i]);
            }
            if (node->as.generic_constraint.requires_constructor) fputs(" new()", stdout);
            putchar('\n');
            break;

        case VC_AST_BLOCK_STATEMENT:
            puts("Block");
            for (size_t i = 0; i < node->as.block_statement.statements.count; i++)
                dump_node(node->as.block_statement.statements.items[i], depth + 1);
            break;

        case VC_AST_EXPRESSION_STATEMENT:
            puts("ExpressionStatement");
            dump_node(node->as.expression_statement.expression, depth + 1);
            break;

        case VC_AST_RETURN_STATEMENT:
            puts(node->as.return_statement.is_ref ? "Return ref" : "Return");
            dump_node(node->as.return_statement.expression, depth + 1);
            break;

        case VC_AST_LOCAL_DECLARATION:
            printf("Local %s%s%s%s : ",
                node->as.local_declaration.scoped ? "scoped " : "",
                node->as.local_declaration.is_ref ? "ref " : "",
                node->as.local_declaration.ref_readonly ? "readonly " : "",
                node->as.local_declaration.name);
            if (node->as.local_declaration.is_var)
                fputs("var", stdout);
            else
                dump_type(node->as.local_declaration.type);
            putchar('\n');
            dump_node(node->as.local_declaration.initializer, depth + 1);
            break;

        case VC_AST_IF_STATEMENT:
            puts("If");
            indent(depth + 1);
            puts("Condition");
            dump_node(node->as.if_statement.condition, depth + 2);
            indent(depth + 1);
            puts("Then");
            dump_node(node->as.if_statement.then_statement, depth + 2);
            if (node->as.if_statement.else_statement != NULL)
            {
                indent(depth + 1);
                puts("Else");
                dump_node(node->as.if_statement.else_statement, depth + 2);
            }
            break;

        case VC_AST_WHILE_STATEMENT:
            puts("While");
            indent(depth + 1);
            puts("Condition");
            dump_node(node->as.while_statement.condition, depth + 2);
            indent(depth + 1);
            puts("Body");
            dump_node(node->as.while_statement.body, depth + 2);
            break;

        case VC_AST_DO_WHILE_STATEMENT:
            puts("DoWhile");
            indent(depth + 1);
            puts("Body");
            dump_node(node->as.while_statement.body, depth + 2);
            indent(depth + 1);
            puts("Condition");
            dump_node(node->as.while_statement.condition, depth + 2);
            break;

        case VC_AST_FOR_STATEMENT:
            puts("For");
            if (node->as.for_statement.initializer != NULL)
            {
                indent(depth + 1);
                puts("Initializer");
                dump_node(node->as.for_statement.initializer, depth + 2);
            }
            if (node->as.for_statement.condition != NULL)
            {
                indent(depth + 1);
                puts("Condition");
                dump_node(node->as.for_statement.condition, depth + 2);
            }
            if (node->as.for_statement.increment != NULL)
            {
                indent(depth + 1);
                puts("Increment");
                dump_node(node->as.for_statement.increment, depth + 2);
            }
            indent(depth + 1);
            puts("Body");
            dump_node(node->as.for_statement.body, depth + 2);
            break;

        case VC_AST_FOREACH_STATEMENT:
            fputs("Foreach ", stdout);
            if (node->as.foreach_statement.is_var)
                fputs("var", stdout);
            else
                dump_type(node->as.foreach_statement.type);
            printf(" %s\n", node->as.foreach_statement.name);
            indent(depth + 1);
            puts("Collection");
            dump_node(node->as.foreach_statement.collection, depth + 2);
            indent(depth + 1);
            puts("Body");
            dump_node(node->as.foreach_statement.body, depth + 2);
            break;

        case VC_AST_USING_STATEMENT:
            if (node->as.using_statement.is_await)
            {
                puts(node->as.using_statement.is_declaration ?
                    "AwaitUsingDeclarationStatement" : "AwaitUsingStatement");
            }
            else
            {
                puts(node->as.using_statement.is_declaration ?
                    "UsingDeclarationStatement" : "UsingStatement");
            }
            if (node->as.using_statement.declaration != NULL)
            {
                indent(depth + 1);
                puts("Resource");
                dump_node(node->as.using_statement.declaration, depth + 2);
            }
            else
            {
                indent(depth + 1);
                puts("Expression");
                dump_node(node->as.using_statement.expression, depth + 2);
            }
            indent(depth + 1);
            puts("Body");
            dump_node(node->as.using_statement.body, depth + 2);
            break;

        case VC_AST_LOCK_STATEMENT:
            puts("LockStatement");
            indent(depth + 1);
            puts("Expression");
            dump_node(node->as.lock_statement.expression, depth + 2);
            indent(depth + 1);
            puts("Body");
            dump_node(node->as.lock_statement.body, depth + 2);
            break;

        case VC_AST_FIXED_STATEMENT:
            fputs("Fixed ", stdout);
            dump_type(node->as.fixed_statement.type);
            printf(" %s\n", node->as.fixed_statement.name);
            indent(depth + 1);
            puts("Initializer");
            dump_node(node->as.fixed_statement.initializer, depth + 2);
            indent(depth + 1);
            puts("Body");
            dump_node(node->as.fixed_statement.body, depth + 2);
            break;

        case VC_AST_SWITCH_STATEMENT:
            puts("Switch");
            indent(depth + 1);
            puts("Expression");
            dump_node(node->as.switch_statement.expression, depth + 2);
            for (size_t i = 0; i < node->as.switch_statement.sections.count; i++)
                dump_node(node->as.switch_statement.sections.items[i], depth + 1);
            break;

        case VC_AST_SWITCH_SECTION:
            puts("SwitchSection");
            for (size_t i = 0; i < node->as.switch_section.labels.count; i++)
                dump_node(node->as.switch_section.labels.items[i], depth + 1);
            for (size_t i = 0; i < node->as.switch_section.statements.count; i++)
                dump_node(node->as.switch_section.statements.items[i], depth + 1);
            break;

        case VC_AST_SWITCH_LABEL:
            if (node->as.switch_label.is_default)
                puts("DefaultLabel");
            else if (node->as.switch_label.is_pattern)
            {
                puts("PatternCaseLabel");
                dump_node(node->as.switch_label.pattern, depth + 1);
            }
            else
            {
                puts("CaseLabel");
                dump_node(node->as.switch_label.value, depth + 1);
            }
            if (node->as.switch_label.guard != NULL)
            {
                indent(depth + 1);
                puts("WhenGuard");
                dump_node(node->as.switch_label.guard, depth + 2);
            }
            break;

        case VC_AST_YIELD_RETURN_STATEMENT:
            puts("YieldReturn");
            dump_node(node->as.yield_statement.expression, depth + 1);
            break;

        case VC_AST_YIELD_BREAK_STATEMENT:
            puts("YieldBreak");
            break;

        case VC_AST_THROW_STATEMENT:
            puts(node->as.throw_statement.expression != NULL ? "Throw" : "Rethrow");
            dump_node(node->as.throw_statement.expression, depth + 1);
            break;

        case VC_AST_TRY_STATEMENT:
            puts("Try");
            dump_node(node->as.try_statement.try_block, depth + 1);
            for (size_t i = 0; i < node->as.try_statement.catches.count; i++)
                dump_node(node->as.try_statement.catches.items[i], depth + 1);
            if (node->as.try_statement.finally_block != NULL)
            {
                indent(depth + 1);
                puts("Finally");
                dump_node(node->as.try_statement.finally_block, depth + 2);
            }
            break;

        case VC_AST_CATCH_CLAUSE:
            if (node->as.catch_clause.catch_all)
                puts("Catch");
            else
                printf("Catch %s%s%s\n",
                    node->as.catch_clause.type != NULL ? node->as.catch_clause.type->name : "<unknown>",
                    node->as.catch_clause.name != NULL ? " " : "",
                    node->as.catch_clause.name != NULL ? node->as.catch_clause.name : "");
            dump_node(node->as.catch_clause.body, depth + 1);
            break;

        case VC_AST_BREAK_STATEMENT:
            puts("Break");
            break;

        case VC_AST_CONTINUE_STATEMENT:
            puts("Continue");
            break;

        case VC_AST_TYPE_RECEIVER_EXPRESSION:
            printf("TypeReceiver ");
            dump_type(node->receiver_type);
            putchar('\n');
            break;

        case VC_AST_IDENTIFIER_EXPRESSION:
            printf("Identifier %s\n", node->as.identifier_expression.name);
            break;

        case VC_AST_AWAIT_EXPRESSION:
            puts("Await");
            dump_node(node->as.await_expression.operand, depth + 1);
            if (node->as.await_expression.awaiter_inference_call != NULL)
                dump_node(node->as.await_expression.awaiter_inference_call, depth + 1);
            break;

        case VC_AST_LITERAL_EXPRESSION:
            printf("%s %s\n",
                literal_kind_name(node->as.literal_expression.literal_kind),
                node->as.literal_expression.text != NULL ? node->as.literal_expression.text : "");
            break;

        case VC_AST_MEMBER_ACCESS_EXPRESSION:
            printf("MemberAccess %s%s\n",
                node->as.member_access_expression.null_conditional_direct ? "?." : ".",
                node->as.member_access_expression.member);
            dump_node(node->as.member_access_expression.target, depth + 1);
            break;

        case VC_AST_CALL_EXPRESSION:
            fputs("Call", stdout);
            if (node->as.call_expression.generic_arguments.count != 0)
            {
                putchar('<');
                for (size_t i = 0; i < node->as.call_expression.generic_arguments.count; i++)
                {
                    if (i != 0) fputs(", ", stdout);
                    dump_type(node->as.call_expression.generic_arguments.items[i]);
                }
                putchar('>');
            }
            putchar('\n');
            dump_node(node->as.call_expression.callee, depth + 1);
            for (size_t i = 0; i < node->as.call_expression.arguments.count; i++)
                dump_node(node->as.call_expression.arguments.items[i], depth + 1);
            break;

        case VC_AST_INDEX_EXPRESSION:
            puts(node->as.index_expression.null_conditional_direct ? "Index?[]" : "Index");
            dump_node(node->as.index_expression.target, depth + 1);
            if (node->as.index_expression.indices.count != 0)
            {
                for (size_t i = 0; i < node->as.index_expression.indices.count; i++)
                    dump_node(node->as.index_expression.indices.items[i], depth + 1);
            }
            else
                dump_node(node->as.index_expression.index, depth + 1);
            break;

        case VC_AST_NEW_EXPRESSION:
            fputs("New ", stdout);
            if (node->as.new_expression.target_typed)
                fputs("<target-typed>", stdout);
            else
                dump_type(node->as.new_expression.type);
            if (node->as.new_expression.is_array)
                fputs(" [array]", stdout);
            putchar('\n');
            if (node->as.new_expression.array_lengths.count != 0)
            {
                for (size_t i = 0; i < node->as.new_expression.array_lengths.count; i++)
                    dump_node(node->as.new_expression.array_lengths.items[i], depth + 1);
            }
            else if (node->as.new_expression.array_length != NULL)
                dump_node(node->as.new_expression.array_length, depth + 1);
            for (size_t i = 0; i < node->as.new_expression.arguments.count; i++)
                dump_node(node->as.new_expression.arguments.items[i], depth + 1);
            for (size_t i = 0; i < node->as.new_expression.initializers.count; i++)
                dump_node(node->as.new_expression.initializers.items[i], depth + 1);
            break;

        case VC_AST_OBJECT_INITIALIZER_MEMBER:
            printf("ObjectInit %s\n", node->as.object_initializer_member.member);
            dump_node(node->as.object_initializer_member.value, depth + 1);
            break;

        case VC_AST_COLLECTION_INITIALIZER_ELEMENT:
            fputs("CollectionInit\n", stdout);
            for (size_t i = 0; i < node->as.collection_initializer_element.arguments.count; i++)
                dump_node(node->as.collection_initializer_element.arguments.items[i], depth + 1);
            break;

        case VC_AST_DEFAULT_EXPRESSION:
            fputs("Default", stdout);
            if (node->as.default_expression.type != NULL)
            {
                putchar(' ');
                dump_type(node->as.default_expression.type);
            }
            else
                fputs(" <target-typed>", stdout);
            fputc('\n', stdout);
            break;

        case VC_AST_TYPEOF_EXPRESSION:
            fputs("TypeOf ", stdout);
            dump_type(node->as.typeof_expression.type);
            fputc('\n', stdout);
            break;

        case VC_AST_SIZEOF_EXPRESSION:
            fputs("SizeOf ", stdout);
            dump_type(node->as.sizeof_expression.type);
            fputc('\n', stdout);
            break;

        case VC_AST_STACKALLOC_EXPRESSION:
            fputs("StackAlloc ", stdout);
            dump_type(node->as.stackalloc_expression.type);
            fputc('\n', stdout);
            dump_node(node->as.stackalloc_expression.count, depth + 1);
            break;

        case VC_AST_CAST_EXPRESSION:
            fputs("Cast ", stdout);
            dump_type(node->as.cast_expression.type);
            fputc('\n', stdout);
            dump_node(node->as.cast_expression.expression, depth + 1);
            break;

        case VC_AST_TYPE_RELATION_EXPRESSION:
            if (node->as.type_relation_expression.pattern_logical_kind != VC_TOKEN_EOF)
            {
                const VcTokenKind logical = node->as.type_relation_expression.pattern_logical_kind;
                printf("LogicalPattern %s\n",
                    logical == VC_TOKEN_BANG ? "not" :
                    logical == VC_TOKEN_AMPERSAND_AMPERSAND ? "and" : "or");
                if (node->as.type_relation_expression.expression != NULL)
                    dump_node(node->as.type_relation_expression.expression, depth + 1);
                dump_node(node->as.type_relation_expression.pattern_left, depth + 1);
                if (node->as.type_relation_expression.pattern_right != NULL)
                    dump_node(node->as.type_relation_expression.pattern_right, depth + 1);
            }
            else if (node->as.type_relation_expression.pattern_property)
            {
                fputs("PropertyPattern\n", stdout);
                if (node->as.type_relation_expression.expression != NULL)
                    dump_node(node->as.type_relation_expression.expression, depth + 1);
                for (size_t i = 0; i < node->as.type_relation_expression.pattern_properties.count; i++)
                    dump_node(node->as.type_relation_expression.pattern_properties.items[i], depth + 1);
            }
            else if (node->as.type_relation_expression.pattern_constant != NULL)
            {
                if (node->as.type_relation_expression.pattern_operator_kind != VC_TOKEN_EOF)
                    printf("RelationalPattern %s\n",
                        vc_token_kind_name(node->as.type_relation_expression.pattern_operator_kind));
                else
                    printf("ConstantPattern %s\n",
                        vc_token_kind_name(node->as.type_relation_expression.operator_kind));
                if (node->as.type_relation_expression.expression != NULL)
                    dump_node(node->as.type_relation_expression.expression, depth + 1);
                dump_node(node->as.type_relation_expression.pattern_constant, depth + 1);
            }
            else
            {
                printf("TypeRelation %s ", vc_token_kind_name(node->as.type_relation_expression.operator_kind));
                dump_type(node->as.type_relation_expression.type);
                if (node->as.type_relation_expression.pattern_name != NULL)
                    printf(" %s", node->as.type_relation_expression.pattern_name);
                fputc('\n', stdout);
                if (node->as.type_relation_expression.expression != NULL)
                    dump_node(node->as.type_relation_expression.expression, depth + 1);
            }
            break;

        case VC_AST_PROPERTY_PATTERN_MEMBER:
            printf("PropertyPatternMember %s\n", node->as.property_pattern_member.name);
            dump_node(node->as.property_pattern_member.pattern, depth + 1);
            break;

        case VC_AST_UNARY_EXPRESSION:
            printf("%sUnary %s\n",
                node->as.unary_expression.postfix ? "Postfix" : "",
                vc_token_kind_name(node->as.unary_expression.operator_kind));
            dump_node(node->as.unary_expression.operand, depth + 1);
            break;

        case VC_AST_BINARY_EXPRESSION:
            printf("Binary %s\n", vc_token_kind_name(node->as.binary_expression.operator_kind));
            dump_node(node->as.binary_expression.left, depth + 1);
            dump_node(node->as.binary_expression.right, depth + 1);
            break;

        case VC_AST_RANGE_EXPRESSION:
            puts("Range");
            indent(depth + 1);
            puts("Start");
            dump_node(node->as.range_expression.start, depth + 2);
            indent(depth + 1);
            puts("End");
            dump_node(node->as.range_expression.end, depth + 2);
            break;

        case VC_AST_CONDITIONAL_EXPRESSION:
            puts("Conditional");
            indent(depth + 1);
            puts("Condition");
            dump_node(node->as.conditional_expression.condition, depth + 2);
            indent(depth + 1);
            puts("True");
            dump_node(node->as.conditional_expression.when_true, depth + 2);
            indent(depth + 1);
            puts("False");
            dump_node(node->as.conditional_expression.when_false, depth + 2);
            break;

        case VC_AST_SWITCH_EXPRESSION:
            puts("SwitchExpression");
            indent(depth + 1);
            puts("Value");
            dump_node(node->as.switch_expression.expression, depth + 2);
            for (size_t i = 0; i < node->as.switch_expression.arms.count; i++)
                dump_node(node->as.switch_expression.arms.items[i], depth + 1);
            break;

        case VC_AST_SWITCH_EXPRESSION_ARM:
            printf("SwitchExpressionArm%s\n", node->as.switch_expression_arm.is_discard ? " _" : "");
            if (node->as.switch_expression_arm.pattern != NULL)
            {
                indent(depth + 1);
                puts("Pattern");
                dump_node(node->as.switch_expression_arm.pattern, depth + 2);
            }
            if (node->as.switch_expression_arm.guard != NULL)
            {
                indent(depth + 1);
                puts("WhenGuard");
                dump_node(node->as.switch_expression_arm.guard, depth + 2);
            }
            indent(depth + 1);
            puts("Result");
            dump_node(node->as.switch_expression_arm.result, depth + 2);
            break;

        case VC_AST_ASSIGNMENT_EXPRESSION:
            printf("Assignment %s%s\n", vc_token_kind_name(node->as.assignment_expression.operator_kind),
                node->as.assignment_expression.is_ref ? " ref" : "");
            dump_node(node->as.assignment_expression.left, depth + 1);
            dump_node(node->as.assignment_expression.right, depth + 1);
            break;

        case VC_AST_LAMBDA_EXPRESSION:
            printf("%sLambda%s", node->as.lambda_expression.is_async ? "Async " : "",
                node->as.lambda_expression.expression_body ? " =>" : "");
            if (node->as.lambda_expression.parameters.count > 0)
            {
                fputs(" (", stdout);
                for (size_t i = 0; i < node->as.lambda_expression.parameters.count; i++)
                {
                    if (i > 0) fputs(", ", stdout);
                    fputs(node->as.lambda_expression.parameters.items[i], stdout);
                }
                putchar(')');
            }
            putchar('\n');
            dump_node(node->as.lambda_expression.body, depth + 1);
            break;

        case VC_AST_COMPILER_CLOSURE_FRAME_EXPRESSION:
            puts("CompilerClosureFrame");
            for (size_t i = 0; i < node->as.compiler_closure_frame_expression.captures.count; i++)
                dump_node(node->as.compiler_closure_frame_expression.captures.items[i], depth + 1);
            break;

        case VC_AST_COMPILER_CAPTURE_EXPRESSION:
            printf("CompilerCapture %s\n",
                node->as.compiler_capture_expression.name != NULL
                    ? node->as.compiler_capture_expression.name : "<capture>");
            break;

        case VC_AST_PARENTHESIZED_EXPRESSION:
            puts("Parenthesized");
            dump_node(node->as.parenthesized_expression.expression, depth + 1);
            break;
    }
}

void vc_ast_dump(const VcAstTree *tree, const VcSource *source)
{
    (void)source;
    dump_node(tree->root, 0);
}

/* Only a non-conditional dotted name can denote a type. */
bool vc_ast_receiver_name(const VcAstNode *node, char *name, size_t size)
{
    if (node == NULL || size == 0) return false;
    if (node->kind == VC_AST_TYPE_RECEIVER_EXPRESSION ||
        node->kind == VC_AST_IDENTIFIER_EXPRESSION)
    {
        const char *text = node->kind == VC_AST_TYPE_RECEIVER_EXPRESSION
            ? node->receiver_type->name : node->as.identifier_expression.name;
        if (text == NULL || strlen(text) >= size) return false;
        memcpy(name, text, strlen(text) + 1);
        return true;
    }
    if (node->kind != VC_AST_MEMBER_ACCESS_EXPRESSION ||
        node->as.member_access_expression.target == NULL ||
        node->as.member_access_expression.target->kind == VC_AST_TYPE_RECEIVER_EXPRESSION ||
        node->as.member_access_expression.null_conditional ||
        !vc_ast_receiver_name(node->as.member_access_expression.target, name, size))
        return false;
    const size_t length = strlen(name);
    const char *member = node->as.member_access_expression.member;
    if (length + strlen(member) + 2 > size) return false;
    name[length] = '.';
    memcpy(name + length + 1, member, strlen(member) + 1);
    return true;
}


/* Decode the language's scalar value once for constant evaluation and native
   emission. C multibyte character constants are not Unicode scalar values. */
bool vc_ast_character_scalar(const char *text, uint32_t *value)
{
    if (text == NULL || value == NULL)
        return false;
    const size_t length = strlen(text);
    if (length < 3 || text[0] != '\'' || text[length - 1] != '\'')
        return false;
    if (text[1] != '\\')
    {
        size_t offset = 1;
        return vc_utf8_decode_one(text, length - 1, &offset, value) && offset == length - 1;
    }
    if (length == 4)
    {
        switch (text[2])
        {
            case '0': *value = 0; return true;
            case 'a': *value = 7; return true;
            case 'b': *value = 8; return true;
            case 'f': *value = 12; return true;
            case 'n': *value = 10; return true;
            case 'r': *value = 13; return true;
            case 't': *value = 9; return true;
            case 'v': *value = 11; return true;
            case '\\': *value = 92; return true;
            case '\'': *value = 39; return true;
            case '"': *value = 34; return true;
            default: break;
        }
    }
    size_t start = 2;
    unsigned base = 8;
    if (text[2] == 'x' || text[2] == 'u' || text[2] == 'U')
    {
        base = 16;
        start = 3;
        if ((text[2] == 'u' && length != 8) ||
            (text[2] == 'U' && length != 12))
            return false;
    }
    if (start == length - 1)
        return false;
    uint32_t scalar = 0;
    for (size_t i = start; i < length - 1; i++)
    {
        unsigned digit;
        if (text[i] >= '0' && text[i] <= '9') digit = (unsigned)(text[i] - '0');
        else if (text[i] >= 'a' && text[i] <= 'f') digit = (unsigned)(text[i] - 'a') + 10;
        else if (text[i] >= 'A' && text[i] <= 'F') digit = (unsigned)(text[i] - 'A') + 10;
        else return false;
        if (digit >= base || scalar > (UINT32_C(0x10ffff) - digit) / base)
            return false;
        scalar = scalar * base + digit;
    }
    if (!vc_utf8_scalar_is_valid(scalar))
        return false;
    *value = scalar;
    return true;
}
