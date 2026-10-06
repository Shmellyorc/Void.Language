#include "query.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool ends_with(const char *value, const char *suffix)
{
    const size_t value_length = strlen(value);
    const size_t suffix_length = strlen(suffix);
    return suffix_length <= value_length && strcmp(value + value_length - suffix_length, suffix) == 0;
}

static const char *project_source(const VcProject *project, const char *suffix)
{
    for (size_t i = 0; i < project->source_count; i++)
    {
        if (ends_with(project->sources[i], suffix))
            return project->sources[i];
    }
    return NULL;
}

static char *read_text(const char *path, size_t *length)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return NULL;
    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return NULL;
    }
    const long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return NULL;
    }
    char *text = malloc((size_t)size + 1);
    if (text == NULL)
    {
        fclose(file);
        return NULL;
    }
    if (fread(text, 1, (size_t)size, file) != (size_t)size)
    {
        free(text);
        fclose(file);
        return NULL;
    }
    fclose(file);
    text[size] = '\0';
    *length = (size_t)size;
    return text;
}

static size_t find_offset(const char *text, const char *needle, size_t add)
{
    const char *found = strstr(text, needle);
    return found == NULL ? (size_t)-1 : (size_t)(found - text) + add;
}

static bool has_member(const VcQueryMember *members, size_t count, VcQuerySymbolKind kind, const char *name)
{
    for (size_t i = 0; i < count; i++)
    {
        if (members[i].kind == kind && strcmp(members[i].name, name) == 0)
            return true;
    }
    return false;
}

int main(void)
{
    char error[2048];
    VcProject project;
    vc_project_init(&project);
    if (!vc_resolve_project("Tests/SemanticQueries", &project, error, sizeof(error)))
    {
        fprintf(stderr, "%s\n", error);
        return 1;
    }

    const char *program_path = project_source(&project, "Program.void");
    const char *player_path = project_source(&project, "Player.void");
    if (program_path == NULL || player_path == NULL)
        return 1;

    size_t program_length = 0;
    size_t player_length = 0;
    char *program_text = read_text(program_path, &program_length);
    char *player_text = read_text(player_path, &player_length);
    if (program_text == NULL || player_text == NULL || program_length == 0 || player_length == 0)
        return 1;

    VcQuerySession *session = vc_query_session_create(&project, ".", error, sizeof(error));
    if (session == NULL)
    {
        fprintf(stderr, "%s\n", error);
        return 1;
    }

    const size_t health_offset = find_offset(program_text, "player.Health", strlen("player."));
    VcQuerySymbol health;
    const bool health_ok = vc_query_symbol_at(session, program_path, health_offset, &health) &&
        health.kind == VC_QUERY_SYMBOL_FIELD && strcmp(health.name, "Health") == 0 &&
        strcmp(health.type_name, "int") == 0 && ends_with(health.definition_path, "Player.void");
    puts(health_ok ? "True" : "False");

    const size_t player_offset = find_offset(program_text, "player.Add", 0);
    const char *player_type = NULL;
    const bool type_ok = vc_query_type_at(session, program_path, player_offset, &player_type) &&
        strcmp(player_type, "Player") == 0;
    puts(type_ok ? "True" : "False");

    VcQueryMember members[32];
    const size_t member_count = vc_query_members_at(session, program_path, player_offset, members, 32);
    const bool members_ok = has_member(members, member_count, VC_QUERY_SYMBOL_FIELD, "Health") &&
        has_member(members, member_count, VC_QUERY_SYMBOL_PROPERTY, "Score") &&
        has_member(members, member_count, VC_QUERY_SYMBOL_METHOD, "Add");
    puts(members_ok ? "True" : "False");

    const size_t add_offset = find_offset(program_text, "player.Add", strlen("player."));
    VcQuerySymbol add;
    VcQueryParameter first;
    VcQueryParameter second;
    const bool signature_ok = vc_query_symbol_at(session, program_path, add_offset, &add) &&
        add.kind == VC_QUERY_SYMBOL_METHOD && strcmp(add.name, "Add") == 0 &&
        add.parameter_count == 2 &&
        vc_query_parameter_at(session, program_path, add_offset, 0, &first) &&
        vc_query_parameter_at(session, program_path, add_offset, 1, &second) &&
        strcmp(first.name, "amount") == 0 && strcmp(first.type_name, "int") == 0 &&
        strcmp(second.name, "bonus") == 0 && strcmp(second.type_name, "int") == 0;
    puts(signature_ok ? "True" : "False");

    const size_t total_offset = find_offset(program_text, "WriteLine(total)", strlen("WriteLine("));
    VcQuerySymbol total;
    const bool local_ok = vc_query_symbol_at(session, program_path, total_offset, &total) &&
        total.kind == VC_QUERY_SYMBOL_LOCAL && strcmp(total.name, "total") == 0 &&
        strcmp(total.type_name, "int") == 0 && ends_with(total.definition_path, "Program.void");
    puts(local_ok ? "True" : "False");

    const size_t amount_offset = find_offset(player_text, "Health + amount", strlen("Health + "));
    VcQuerySymbol amount;
    const bool parameter_ok = vc_query_symbol_at(session, player_path, amount_offset, &amount) &&
        amount.kind == VC_QUERY_SYMBOL_PARAMETER && strcmp(amount.name, "amount") == 0 &&
        strcmp(amount.type_name, "int") == 0 && ends_with(amount.definition_path, "Player.void");
    puts(parameter_ok ? "True" : "False");

    const char *definition_path = NULL;
    VcSourceSpan definition_span;
    const bool definition_ok = vc_query_definition_at(
        session, program_path, health_offset, &definition_path, &definition_span) &&
        definition_path != NULL && ends_with(definition_path, "Player.void") &&
        definition_span.start.line == 5;
    puts(definition_ok ? "True" : "False");

    vc_query_session_destroy(session);
    free(player_text);
    free(program_text);
    vc_project_destroy(&project);
    return 0;
}
