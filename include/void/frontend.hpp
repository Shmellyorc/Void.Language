#pragma once
#include "void/ir.hpp"
#include <filesystem>
namespace vd {
// One source pipeline shared by checking, JIT and AOT. Errors contain rendered
// source diagnostics and are raised before invoking any backend.
ir::Module compile_source(const SourceFile& source);
SourceFile read_source(const std::filesystem::path& path);
void require_source_entry(const SourceFile& source, const ir::Module& module);
}
