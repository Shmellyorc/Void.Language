#pragma once

#include "void/frontend.hpp"

namespace vd::frontend {

std::optional<EditorAssistance> editor_namespace_assistance(const ValidatedAnalysis&, const SourceFile&,
    const std::string& module, std::size_t cursor);
}
