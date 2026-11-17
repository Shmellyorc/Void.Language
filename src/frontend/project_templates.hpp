#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vd::project {
struct TemplateMetadata {
    std::string id, name, description, source_name;
    std::vector<std::string> short_names, tags;
};
bool valid_template_short_name(std::string_view);
bool valid_template_project_name(std::string_view);
TemplateMetadata parse_template_metadata(std::string_view);

std::vector<TemplateMetadata> list_project_templates();
TemplateMetadata install_project_template(const std::filesystem::path& source);
TemplateMetadata install_project_template_git(const std::string& origin);
void uninstall_project_template(std::string_view short_name);
// A missing installed name leaves ordinary project creation to the caller.
std::optional<std::filesystem::path> create_from_project_template(
    std::string_view short_name, const std::optional<std::string>& name, bool nested);
}
