#ifndef VOIDC_H
#define VOIDC_H

#include <stdbool.h>
#include <stddef.h>

typedef enum VcOutputKind
{
    VC_OUTPUT_EXE,
    VC_OUTPUT_LIBRARY
} VcOutputKind;

typedef struct VcProject
{
    char *root;
    char *project_file;
    char *name;
    char *version;
    VcOutputKind output;
    bool allow_unsafe;
    bool projectless;

    char **libraries;
    size_t library_count;
    size_t library_capacity;

    char **library_paths;
    size_t library_path_count;
    size_t library_path_capacity;

    char **sources;
    size_t source_count;
    size_t source_capacity;
} VcProject;

void vc_project_init(VcProject *project);
void vc_project_destroy(VcProject *project);

bool vc_resolve_project(const char *target, VcProject *project, char *error, size_t error_size);

#endif
