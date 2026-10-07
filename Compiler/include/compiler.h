#ifndef VOIDC_COMPILER_H
#define VOIDC_COMPILER_H

#include "lexer.h"
#include "voidc.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum VcBuildMode
{
    VC_BUILD_DEBUG,
    VC_BUILD_PUBLISH
} VcBuildMode;

typedef enum VcCheckStatus
{
    VC_CHECK_VALID,
    VC_CHECK_DIAGNOSTIC,
    VC_CHECK_ERROR
} VcCheckStatus;

typedef struct VcCheckDiagnostic
{
    char path[4096];
    VcSourceSpan span;
    char message[512];
} VcCheckDiagnostic;

typedef struct VcSourceOverride
{
    const char *path;
    const char *text;
} VcSourceOverride;

bool vc_check_project(
    const VcProject *project,
    const char *tool_root,
    char *error,
    size_t error_size);

VcCheckStatus vc_check_project_text(
    const VcProject *project,
    const char *tool_root,
    const char *source_path,
    const char *source_text,
    VcCheckDiagnostic *diagnostic,
    char *error,
    size_t error_size);

VcCheckStatus vc_check_project_texts(
    const VcProject *project,
    const char *tool_root,
    const VcSourceOverride *overrides,
    size_t override_count,
    VcCheckDiagnostic *diagnostic,
    char *error,
    size_t error_size);

bool vc_compile_project(
    const VcProject *project,
    VcBuildMode mode,
    const char *tool_root,
    char *output_path,
    size_t output_path_size,
    char *error,
    size_t error_size);

bool vc_run_executable(const char *path, unsigned long *program_exit_code, char *error, size_t error_size);

#endif
