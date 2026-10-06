#include "voidc.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct JsonReader
{
    const char *text;
    size_t position;
} JsonReader;

static void set_error(char *error, size_t error_size, const char *format, ...)
{
    if (error == NULL || error_size == 0)
        return;

    va_list args;
    va_start(args, format);
    vsnprintf(error, error_size, format, args);
    va_end(args);
}

static char *vc_strdup(const char *value)
{
    if (value == NULL)
        return NULL;

    const size_t length = strlen(value);
    char *copy = malloc(length + 1);
    if (copy == NULL)
        return NULL;

    memcpy(copy, value, length + 1);
    return copy;
}

static bool has_suffix(const char *value, const char *suffix)
{
    const size_t value_length = strlen(value);
    const size_t suffix_length = strlen(suffix);

    if (suffix_length > value_length)
        return false;

    return strcmp(value + value_length - suffix_length, suffix) == 0;
}

static bool path_is_file(const char *path)
{
    struct stat info;
    return stat(path, &info) == 0 && S_ISREG(info.st_mode);
}

static bool path_is_directory(const char *path)
{
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

static bool path_join(const char *left, const char *right, char *output, size_t output_size)
{
    const size_t left_length = strlen(left);
    const bool needs_separator = left_length > 0 && left[left_length - 1] != '/';
    const int written = snprintf(output, output_size, "%s%s%s", left, needs_separator ? "/" : "", right);
    return written >= 0 && (size_t)written < output_size;
}

static const char *path_last_separator(const char *path)
{
    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    const char *backslash = strrchr(path, '\\');
    if (backslash != NULL && (slash == NULL || backslash > slash))
        slash = backslash;
#endif
    return slash;
}

static bool path_parent(const char *path, char *output, size_t output_size)
{
    const char *slash = path_last_separator(path);
    if (slash == NULL)
        return snprintf(output, output_size, ".") > 0;

    if (slash == path)
        return snprintf(output, output_size, "/") > 0;

    size_t length = (size_t)(slash - path);
#ifdef _WIN32
    /* Preserve the separator in a drive root, rather than returning C:. */
    if (length == 2 && path[1] == ':')
        ++length;
#endif
    if (length + 1 > output_size)
        return false;

    memcpy(output, path, length);
    output[length] = '\0';
    return true;
}

static const char *path_filename(const char *path)
{
    const char *slash = path_last_separator(path);
    return slash == NULL ? path : slash + 1;
}

static bool path_equal(const char *left, const char *right)
{
#ifdef _WIN32
    /* Discovery joins with '/', while a native CLI target can use '\\'. */
    while (*left != '\0' && *right != '\0')
    {
        const char a = *left == '\\' ? '/' : *left;
        const char b = *right == '\\' ? '/' : *right;
        if (a != b)
            return false;
        ++left;
        ++right;
    }
    return *left == *right;
#else
    return strcmp(left, right) == 0;
#endif
}

static char *read_file(const char *path, char *error, size_t error_size)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
    {
        set_error(error, error_size, "could not open '%s': %s", path, strerror(errno));
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        set_error(error, error_size, "could not seek '%s'", path);
        fclose(file);
        return NULL;
    }

    const long file_size = ftell(file);
    if (file_size < 0)
    {
        set_error(error, error_size, "could not determine size of '%s'", path);
        fclose(file);
        return NULL;
    }

    rewind(file);

    char *buffer = malloc((size_t)file_size + 1);
    if (buffer == NULL)
    {
        set_error(error, error_size, "out of memory while reading '%s'", path);
        fclose(file);
        return NULL;
    }

    const size_t read_count = fread(buffer, 1, (size_t)file_size, file);
    fclose(file);

    if (read_count != (size_t)file_size)
    {
        set_error(error, error_size, "could not read all of '%s'", path);
        free(buffer);
        return NULL;
    }

    buffer[file_size] = '\0';
    return buffer;
}

static void json_skip_whitespace(JsonReader *reader)
{
    while (isspace((unsigned char)reader->text[reader->position]))
        reader->position++;
}

static bool json_take(JsonReader *reader, char expected)
{
    json_skip_whitespace(reader);
    if (reader->text[reader->position] != expected)
        return false;

    reader->position++;
    return true;
}

static char *json_parse_string(JsonReader *reader)
{
    json_skip_whitespace(reader);
    if (reader->text[reader->position] != '"')
        return NULL;

    reader->position++;
    const size_t start = reader->position;
    size_t decoded_length = 0;
    bool escaped = false;

    while (reader->text[reader->position] != '\0')
    {
        const char c = reader->text[reader->position];
        if (!escaped && c == '"')
            break;

        if (!escaped && c == '\\')
        {
            escaped = true;
            reader->position++;
            decoded_length++;
            continue;
        }

        escaped = false;
        reader->position++;
        decoded_length++;
    }

    if (reader->text[reader->position] != '"')
        return NULL;

    const size_t end = reader->position;
    reader->position++;

    char *value = malloc(decoded_length + 1);
    if (value == NULL)
        return NULL;

    size_t source = start;
    size_t destination = 0;
    while (source < end)
    {
        char c = reader->text[source++];
        if (c == '\\' && source < end)
        {
            const char escaped_char = reader->text[source++];
            switch (escaped_char)
            {
                case '"': c = '"'; break;
                case '\\': c = '\\'; break;
                case '/': c = '/'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                default:
                    free(value);
                    return NULL;
            }
        }

        value[destination++] = c;
    }

    value[destination] = '\0';
    return value;
}

static bool json_parse_bool(JsonReader *reader, bool *value)
{
    json_skip_whitespace(reader);
    const char *text = reader->text + reader->position;

    if (strncmp(text, "true", 4) == 0)
    {
        reader->position += 4;
        *value = true;
        return true;
    }

    if (strncmp(text, "false", 5) == 0)
    {
        reader->position += 5;
        *value = false;
        return true;
    }

    return false;
}

static bool json_parse_int(JsonReader *reader, int *value)
{
    json_skip_whitespace(reader);

    char *end = NULL;
    const long parsed = strtol(reader->text + reader->position, &end, 10);
    if (end == reader->text + reader->position)
        return false;

    reader->position = (size_t)(end - reader->text);
    *value = (int)parsed;
    return true;
}

static bool json_skip_value(JsonReader *reader);

static bool add_library(VcProject *project, const char *name, char *error, size_t error_size)
{
    if (project->library_count == project->library_capacity)
    {
        const size_t new_capacity = project->library_capacity == 0 ? 4 : project->library_capacity * 2;
        char **new_libraries = realloc(project->libraries, new_capacity * sizeof(*new_libraries));
        if (new_libraries == NULL)
        {
            set_error(error, error_size, "out of memory while storing native library");
            return false;
        }

        project->libraries = new_libraries;
        project->library_capacity = new_capacity;
    }

    project->libraries[project->library_count] = vc_strdup(name);
    if (project->libraries[project->library_count] == NULL)
    {
        set_error(error, error_size, "out of memory while storing native library");
        return false;
    }

    project->library_count++;
    return true;
}

static bool valid_library_name(const char *name)
{
    if (name == NULL || name[0] == '\0')
        return false;

    const unsigned char first = (unsigned char)name[0];
    if (!isalnum(first) && first != '_')
        return false;

    for (size_t i = 0; name[i] != '\0'; i++)
    {
        const unsigned char c = (unsigned char)name[i];
        if (!isalnum(c) && c != '_' && c != '-' && c != '.')
            return false;
    }

    return true;
}

static bool parse_library_array(JsonReader *reader, VcProject *project, char *error, size_t error_size)
{
    if (!json_take(reader, '['))
        return false;

    json_skip_whitespace(reader);
    if (reader->text[reader->position] == ']')
    {
        reader->position++;
        return true;
    }

    for (;;)
    {
        char *library = json_parse_string(reader);
        if (library == NULL)
            return false;

        if (!valid_library_name(library))
        {
            set_error(error, error_size, "native library '%s' is not a valid linker library name", library);
            free(library);
            return false;
        }

        for (size_t i = 0; i < project->library_count; i++)
        {
            if (strcmp(project->libraries[i], library) == 0)
            {
                set_error(error, error_size, "native library '%s' is listed more than once", library);
                free(library);
                return false;
            }
        }

        const bool added = add_library(project, library, error, error_size);
        free(library);
        if (!added)
            return false;

        json_skip_whitespace(reader);
        if (reader->text[reader->position] == ']')
        {
            reader->position++;
            return true;
        }

        if (!json_take(reader, ','))
            return false;
    }
}

static bool add_library_path(VcProject *project, const char *path, char *error, size_t error_size)
{
    if (project->library_path_count == project->library_path_capacity)
    {
        const size_t new_capacity = project->library_path_capacity == 0 ? 4 : project->library_path_capacity * 2;
        char **new_paths = realloc(project->library_paths, new_capacity * sizeof(*new_paths));
        if (new_paths == NULL)
        {
            set_error(error, error_size, "out of memory while storing native library path");
            return false;
        }

        project->library_paths = new_paths;
        project->library_path_capacity = new_capacity;
    }

    project->library_paths[project->library_path_count] = vc_strdup(path);
    if (project->library_paths[project->library_path_count] == NULL)
    {
        set_error(error, error_size, "out of memory while storing native library path");
        return false;
    }

    project->library_path_count++;
    return true;
}

static bool valid_library_path(const char *path)
{
    if (path == NULL || path[0] == '\0' || path[0] == '/' || path[0] == '\\')
        return false;

    for (size_t i = 0; path[i] != '\0'; i++)
    {
        const unsigned char c = (unsigned char)path[i];
        if (!isalnum(c) && c != '_' && c != '-' && c != '.' && c != '/')
            return false;

        if (c == '.' && path[i + 1] == '.' &&
            (i == 0 || path[i - 1] == '/') &&
            (path[i + 2] == '\0' || path[i + 2] == '/'))
        {
            return false;
        }
    }

    return true;
}

static bool parse_library_path_array(JsonReader *reader, VcProject *project, char *error, size_t error_size)
{
    if (!json_take(reader, '['))
        return false;

    json_skip_whitespace(reader);
    if (reader->text[reader->position] == ']')
    {
        reader->position++;
        return true;
    }

    for (;;)
    {
        char *path = json_parse_string(reader);
        if (path == NULL)
            return false;

        if (!valid_library_path(path))
        {
            set_error(error, error_size, "native library path '%s' must be a project-relative directory", path);
            free(path);
            return false;
        }

        for (size_t i = 0; i < project->library_path_count; i++)
        {
            if (strcmp(project->library_paths[i], path) == 0)
            {
                set_error(error, error_size, "native library path '%s' is listed more than once", path);
                free(path);
                return false;
            }
        }

        const bool added = add_library_path(project, path, error, error_size);
        free(path);
        if (!added)
            return false;

        json_skip_whitespace(reader);
        if (reader->text[reader->position] == ']')
        {
            reader->position++;
            return true;
        }

        if (!json_take(reader, ','))
            return false;
    }
}

static bool json_skip_array(JsonReader *reader)
{
    if (!json_take(reader, '['))
        return false;

    json_skip_whitespace(reader);
    if (reader->text[reader->position] == ']')
    {
        reader->position++;
        return true;
    }

    for (;;)
    {
        if (!json_skip_value(reader))
            return false;

        json_skip_whitespace(reader);
        if (reader->text[reader->position] == ']')
        {
            reader->position++;
            return true;
        }

        if (!json_take(reader, ','))
            return false;
    }
}

static bool json_skip_object(JsonReader *reader)
{
    if (!json_take(reader, '{'))
        return false;

    json_skip_whitespace(reader);
    if (reader->text[reader->position] == '}')
    {
        reader->position++;
        return true;
    }

    for (;;)
    {
        char *key = json_parse_string(reader);
        if (key == NULL)
            return false;
        free(key);

        if (!json_take(reader, ':') || !json_skip_value(reader))
            return false;

        json_skip_whitespace(reader);
        if (reader->text[reader->position] == '}')
        {
            reader->position++;
            return true;
        }

        if (!json_take(reader, ','))
            return false;
    }
}

static bool json_skip_value(JsonReader *reader)
{
    json_skip_whitespace(reader);
    const char c = reader->text[reader->position];

    if (c == '"')
    {
        char *value = json_parse_string(reader);
        if (value == NULL)
            return false;
        free(value);
        return true;
    }

    if (c == '{')
        return json_skip_object(reader);

    if (c == '[')
        return json_skip_array(reader);

    bool bool_value = false;
    if (json_parse_bool(reader, &bool_value))
        return true;

    if (strncmp(reader->text + reader->position, "null", 4) == 0)
    {
        reader->position += 4;
        return true;
    }

    int int_value = 0;
    return json_parse_int(reader, &int_value);
}

static bool parse_compiler_object(JsonReader *reader, VcProject *project, char *error, size_t error_size)
{
    if (!json_take(reader, '{'))
        return false;

    json_skip_whitespace(reader);
    if (reader->text[reader->position] == '}')
    {
        reader->position++;
        return true;
    }

    for (;;)
    {
        char *key = json_parse_string(reader);
        if (key == NULL || !json_take(reader, ':'))
        {
            free(key);
            return false;
        }

        bool ok = true;
        if (strcmp(key, "unsafe") == 0)
            ok = json_parse_bool(reader, &project->allow_unsafe);
        else if (strcmp(key, "libraries") == 0)
            ok = parse_library_array(reader, project, error, error_size);
        else if (strcmp(key, "libraryPaths") == 0)
            ok = parse_library_path_array(reader, project, error, error_size);
        else
            ok = json_skip_value(reader);

        free(key);
        if (!ok)
            return false;

        json_skip_whitespace(reader);
        if (reader->text[reader->position] == '}')
        {
            reader->position++;
            return true;
        }

        if (!json_take(reader, ','))
            return false;
    }
}

static bool parse_project_json(const char *path, VcProject *project, char *error, size_t error_size)
{
    char *json = read_file(path, error, error_size);
    if (json == NULL)
        return false;

    JsonReader reader = { .text = json, .position = 0 };
    int format = 0;
    bool have_format = false;
    bool have_name = false;
    bool have_output = false;

    if (!json_take(&reader, '{'))
    {
        set_error(error, error_size, "'%s' must contain a JSON object", path);
        free(json);
        return false;
    }

    json_skip_whitespace(&reader);
    if (reader.text[reader.position] != '}')
    {
        for (;;)
        {
            char *key = json_parse_string(&reader);
            if (key == NULL || !json_take(&reader, ':'))
            {
                free(key);
                set_error(error, error_size, "invalid JSON in '%s'", path);
                free(json);
                return false;
            }

            bool ok = true;
            if (strcmp(key, "format") == 0)
            {
                ok = json_parse_int(&reader, &format);
                have_format = ok;
            }
            else if (strcmp(key, "name") == 0)
            {
                free(project->name);
                project->name = json_parse_string(&reader);
                ok = project->name != NULL;
                have_name = ok;
            }
            else if (strcmp(key, "version") == 0)
            {
                free(project->version);
                project->version = json_parse_string(&reader);
                ok = project->version != NULL;
            }
            else if (strcmp(key, "output") == 0)
            {
                char *output = json_parse_string(&reader);
                ok = output != NULL;
                if (ok)
                {
                    if (strcmp(output, "exe") == 0)
                        project->output = VC_OUTPUT_EXE;
                    else if (strcmp(output, "library") == 0)
                        project->output = VC_OUTPUT_LIBRARY;
                    else
                    {
                        set_error(error, error_size, "unsupported output '%s' in '%s'", output, path);
                        ok = false;
                    }
                    have_output = ok;
                }
                free(output);
            }
            else if (strcmp(key, "compiler") == 0)
            {
                ok = parse_compiler_object(&reader, project, error, error_size);
            }
            else
            {
                ok = json_skip_value(&reader);
            }

            free(key);
            if (!ok)
            {
                if (error != NULL && error[0] == '\0')
                    set_error(error, error_size, "invalid value in '%s'", path);
                free(json);
                return false;
            }

            json_skip_whitespace(&reader);
            if (reader.text[reader.position] == '}')
                break;

            if (!json_take(&reader, ','))
            {
                set_error(error, error_size, "invalid JSON in '%s'", path);
                free(json);
                return false;
            }
        }
    }

    if (!json_take(&reader, '}'))
    {
        set_error(error, error_size, "invalid JSON in '%s'", path);
        free(json);
        return false;
    }

    free(json);

    if (!have_format)
    {
        set_error(error, error_size, "project '%s' is missing 'format'", path);
        return false;
    }

    if (format != 1)
    {
        set_error(error, error_size, "project '%s' uses unsupported format %d", path, format);
        return false;
    }

    if (!have_name || project->name[0] == '\0')
    {
        set_error(error, error_size, "project '%s' is missing a valid 'name'", path);
        return false;
    }

    if (!have_output)
    {
        set_error(error, error_size, "project '%s' is missing 'output'", path);
        return false;
    }

    if (project->version == NULL)
    {
        project->version = vc_strdup("0.1.0");
        if (project->version == NULL)
        {
            set_error(error, error_size, "out of memory while loading project");
            return false;
        }
    }

    return true;
}

static bool add_source(VcProject *project, const char *path, char *error, size_t error_size)
{
    if (project->source_count == project->source_capacity)
    {
        const size_t new_capacity = project->source_capacity == 0 ? 16 : project->source_capacity * 2;
        char **new_sources = realloc(project->sources, new_capacity * sizeof(*new_sources));
        if (new_sources == NULL)
        {
            set_error(error, error_size, "out of memory while discovering source files");
            return false;
        }

        project->sources = new_sources;
        project->source_capacity = new_capacity;
    }

    project->sources[project->source_count] = vc_strdup(path);
    if (project->sources[project->source_count] == NULL)
    {
        set_error(error, error_size, "out of memory while storing source path");
        return false;
    }

    project->source_count++;
    return true;
}

static bool should_skip_directory(const char *name)
{
    return strcmp(name, ".") == 0 ||
           strcmp(name, "..") == 0 ||
           name[0] == '.' ||
           strcmp(name, "bin") == 0 ||
           strcmp(name, "obj") == 0 ||
           strcmp(name, "build") == 0;
}

static bool directory_contains_project_file(const char *directory)
{
    DIR *dir = opendir(directory);
    if (dir == NULL)
        return false;

    bool found = false;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (!has_suffix(entry->d_name, ".voidproj"))
            continue;

        char path[PATH_MAX];
        if (path_join(directory, entry->d_name, path, sizeof(path)) && path_is_file(path))
        {
            found = true;
            break;
        }
    }

    closedir(dir);
    return found;
}

static bool discover_sources(const char *directory, VcProject *project, char *error, size_t error_size)
{
    DIR *dir = opendir(directory);
    if (dir == NULL)
    {
        set_error(error, error_size, "could not scan '%s': %s", directory, strerror(errno));
        return false;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        char path[PATH_MAX];
        if (!path_join(directory, entry->d_name, path, sizeof(path)))
        {
            closedir(dir);
            set_error(error, error_size, "path is too long under '%s'", directory);
            return false;
        }

        if (path_is_directory(path))
        {
            if (!should_skip_directory(entry->d_name) &&
                !directory_contains_project_file(path) &&
                !discover_sources(path, project, error, error_size))
            {
                closedir(dir);
                return false;
            }
        }
        else if (path_is_file(path) && has_suffix(entry->d_name, ".void"))
        {
            if (!add_source(project, path, error, error_size))
            {
                closedir(dir);
                return false;
            }
        }
    }

    closedir(dir);
    return true;
}

static bool find_project_file(const char *directory, char *project_path, size_t project_path_size, char *error, size_t error_size)
{
    DIR *dir = opendir(directory);
    if (dir == NULL)
    {
        set_error(error, error_size, "could not scan '%s': %s", directory, strerror(errno));
        return false;
    }

    size_t count = 0;
    char first[PATH_MAX] = {0};
    char second[PATH_MAX] = {0};

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (!has_suffix(entry->d_name, ".voidproj"))
            continue;

        char candidate[PATH_MAX];
        if (!path_join(directory, entry->d_name, candidate, sizeof(candidate)) || !path_is_file(candidate))
            continue;

        count++;
        if (count == 1)
            snprintf(first, sizeof(first), "%s", candidate);
        else if (count == 2)
            snprintf(second, sizeof(second), "%s", candidate);
    }

    closedir(dir);

    if (count > 1)
    {
        set_error(error, error_size,
            "multiple VOID projects found in '%s':\n  %s\n  %s\nSpecify a .voidproj explicitly.",
            directory, path_filename(first), path_filename(second));
        return false;
    }

    if (count == 1)
    {
        snprintf(project_path, project_path_size, "%s", first);
        return true;
    }

    project_path[0] = '\0';
    return true;
}

void vc_project_init(VcProject *project)
{
    memset(project, 0, sizeof(*project));
    project->output = VC_OUTPUT_EXE;
}

void vc_project_destroy(VcProject *project)
{
    free(project->root);
    free(project->project_file);
    free(project->name);
    free(project->version);

    for (size_t i = 0; i < project->library_count; i++)
        free(project->libraries[i]);
    free(project->libraries);

    for (size_t i = 0; i < project->library_path_count; i++)
        free(project->library_paths[i]);
    free(project->library_paths);

    for (size_t i = 0; i < project->source_count; i++)
        free(project->sources[i]);

    free(project->sources);
    memset(project, 0, sizeof(*project));
}

static bool configure_project_file(const char *project_path, VcProject *project, char *error, size_t error_size)
{
    char root[PATH_MAX];
    if (!path_parent(project_path, root, sizeof(root)))
    {
        set_error(error, error_size, "could not determine project directory for '%s'", project_path);
        return false;
    }

    project->root = vc_strdup(root);
    project->project_file = vc_strdup(project_path);
    if (project->root == NULL || project->project_file == NULL)
    {
        set_error(error, error_size, "out of memory while loading project");
        return false;
    }

    if (!parse_project_json(project_path, project, error, error_size))
        return false;

    return discover_sources(project->root, project, error, error_size);
}

static bool configure_projectless(const char *root, const char *program_path, VcProject *project, char *error, size_t error_size)
{
    project->root = vc_strdup(root);
    project->name = vc_strdup("Program");
    project->version = vc_strdup("0.1.0");
    project->projectless = true;
    project->allow_unsafe = false;
    project->output = VC_OUTPUT_EXE;

    if (project->root == NULL || project->name == NULL || project->version == NULL)
    {
        set_error(error, error_size, "out of memory while creating projectless program");
        return false;
    }

    if (!discover_sources(root, project, error, error_size))
        return false;

    bool found_program = false;
    for (size_t i = 0; i < project->source_count; i++)
    {
        if (path_equal(project->sources[i], program_path))
        {
            found_program = true;
            break;
        }
    }

    if (!found_program)
    {
        set_error(error, error_size, "could not include '%s' in source discovery", program_path);
        return false;
    }

    return true;
}

bool vc_resolve_project(const char *target, VcProject *project, char *error, size_t error_size)
{
    if (error != NULL && error_size > 0)
        error[0] = '\0';

    if (target == NULL || target[0] == '\0')
        target = ".";

    if (path_is_file(target))
    {
        if (has_suffix(target, ".voidproj"))
            return configure_project_file(target, project, error, error_size);

        if (has_suffix(target, ".void"))
        {
            char root[PATH_MAX];
            if (!path_parent(target, root, sizeof(root)))
            {
                set_error(error, error_size, "could not determine source directory for '%s'", target);
                return false;
            }

            if (strcmp(path_filename(target), "Program.void") != 0)
            {
                set_error(error, error_size, "projectless execution requires Program.void");
                return false;
            }

            return configure_projectless(root, target, project, error, error_size);
        }

        set_error(error, error_size, "unsupported target '%s'", target);
        return false;
    }

    if (!path_is_directory(target))
    {
        set_error(error, error_size, "target '%s' does not exist or is not a directory", target);
        return false;
    }

    char project_path[PATH_MAX];
    if (!find_project_file(target, project_path, sizeof(project_path), error, error_size))
        return false;

    if (project_path[0] != '\0')
        return configure_project_file(project_path, project, error, error_size);

    char program_path[PATH_MAX];
    if (!path_join(target, "Program.void", program_path, sizeof(program_path)))
    {
        set_error(error, error_size, "project path is too long");
        return false;
    }

    if (path_is_file(program_path))
        return configure_projectless(target, program_path, project, error, error_size);

    set_error(error, error_size,
        "no VOID project found in '%s'\nExpected one of:\n  *.voidproj\n  Program.void",
        target);
    return false;
}
