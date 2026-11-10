#include "void/project.hpp"

// Deliberately no other includes, PCH, unity build or void_core dependency.
// Public declarations and inline operations must be usable by a fresh consumer.
int main() {
    static_assert(vd::project::manifest_byte_limit == 256 * 1024);
    vd::project::Identity identity;
    vd::project::Sources sources;
    vd::project::ParseResult result;
    vd::project::Field field{};
    std::array<vd::project::Profile, 1> profiles{{
        {vd::project::Optimization::O2, vd::project::DebugInfo::line}
    }};
    std::map<std::string, vd::project::Field> fields;
    fields.emplace("source", field);
    std::shared_ptr<const vd::project::Manifest> manifest;
    std::optional<vd::project::SemanticVersion> version{identity.version};
    std::string_view version_text = version->text();
    std::vector<std::string> names{identity.name};
    const std::size_t size = names.size() + fields.size() + profiles.size();
    return size == 3 && version_text == "0.0.0" && sources.roots.size() == 1 &&
        !manifest && !result.succeeded() ? 0 : 1;
}
