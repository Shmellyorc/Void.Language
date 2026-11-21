#pragma once
#include <optional>
#include <filesystem>
#include <string_view>
namespace vd::crash {
// Closed, non-sensitive vocabulary. Never pass source, URIs, arguments or messages.
enum class Operation : unsigned { startup, cli, frontend, jit_compile, jit_execute,
    aot_build, lsp_request, lsp_analysis, lsp_completion, lsp_hover, lsp_shutdown,
    external_execute, finished, count };
void context(Operation operation) noexcept;
// Returns only in the unsupervised/worker path (nullopt), or with the worker's
// ordinary exit code. A signalled worker makes the POSIX supervisor re-raise it.
// Windows collects owning-process sanitizer findings only, without a debugger;
// unmarked exception termination is unsupported and never inferred from status.
std::optional<int> start(const char* role, const char* executable,
    std::string_view build_identity = {}, bool force = false) noexcept;
// LLVM -2 is abnormal child termination (or timeout when one was configured).
// Ordinary nonzero and launch failures are never reported as crashes. Windows
// LLVM child results lack exception events; their existing diagnostics are kept.
void child_termination(const std::filesystem::path& executable, int status, bool timeout_possible) noexcept;
// Explicit AOT boundary; no instrumentation is linked into generated programs.
int execute_external(int argc, char** argv);
}
