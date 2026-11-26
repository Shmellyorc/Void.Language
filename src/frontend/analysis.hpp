#pragma once

#include "model.hpp"

namespace vd {

// Successful analysis owns the frozen compiler model used by tools and lowering.
struct AnalysisAccess {
    static std::shared_ptr<const ValidatedAnalysis> build(
        std::map<std::string, std::unique_ptr<frontend::Unit>>, const std::string& entry,
        std::vector<Diagnostic> warnings = {}, const std::string& context_key = {},
        std::shared_ptr<frontend::Unit> library_scope = {}, ApplicationMetadata::Owner = {},
        bool native_authorized = true, std::shared_ptr<const project::LockedDependencies> = {},
        bool editor_only = false, bool presentation = false);
};
}
