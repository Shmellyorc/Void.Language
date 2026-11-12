#pragma once
#include "void/ir.hpp"
#include "void/native_binding.hpp"
#include "void/diagnostic.hpp"
#include "void/project.hpp"
#include <filesystem>
#include <functional>
#include <optional>
#include <map>
#include <set>
namespace vd {
// Records are owned by an immutable successful analysis, never by parser storage.
struct AnalyzedModule { std::string identity; SourceFile source; std::vector<std::string> imports; };
struct AnalyzedParameter {
    std::string name; ir::Type type; SourceSpan source;
    std::optional<SourceSpan> default_source = {};
};
// Plain compiler-owned documentation; tag locations retain the authored snapshot.
struct DocumentationText { std::string description; SourceSpan source; };
struct ParameterDocumentation { std::string name, description; SourceSpan source; };
struct Documentation {
    std::string description;
    std::optional<DocumentationText> returns;
    std::vector<ParameterDocumentation> parameters;
    std::string parameter(const std::string& name) const {
        for(const auto& p:parameters)if(p.name==name && !p.description.empty())return p.description;
        return {};
    }
    std::string text(bool value_result = false) const {
        auto out=description;
        if(value_result && returns){if(!out.empty())out+="\n\n";out+="Returns: "+returns->description;}
        return out;
    }
};
struct AnalyzedDeclaration {
    enum class Kind { function, global_variable, module_constant, local_constant, value_structure, value_field, receiver_method, initializer, managed_structure, managed_field, interface_type, interface_requirement, type_parameter, local_variable, parameter, import_alias, operator_function } kind;
    std::string identity, module, visibility;
    SourceSpan source;
    std::vector<ir::Type> parameters;
    ir::Type type = ir::Type::none;
    std::string constant_syntax; // Exact length-delimited tokens, excluding trivia.
    std::optional<std::uint64_t> value = {}; // Module constants only: typed bit pattern.
    std::string owner = {}; // Containing declaration, or module.
    ir::Type receiver = ir::Type::none;
    bool receiver_mutates = false;
    bool using_field = false; // Stored composition field; does not affect native layout.
    std::string initialization_contract = {};
    std::vector<AnalyzedParameter> initializer_parameters = {};
    std::string capability_contract = {};
    // Private passing contexts, indexed over receiver then ordinary parameters.
    // Required fields are unavailable at managed initializer entry, but proved
    // ready on every successful exit before complete-object publication.
    std::vector<std::vector<unsigned>> construction_parameters = {};
    std::vector<std::string> required_initialization_fields = {};
    std::string base_identity = {}, virtual_slot = {}, override_target = {};
    bool generic_template = false;
    // Editor records never participate in compatibility serialization.
    std::vector<AnalyzedParameter> written_parameters = {};
    std::optional<SourceSpan> extent = {};
    bool rename_safe = false;
    bool builtin = false; // Compiler metadata only; has no filesystem definition target.
    int editor_symbol_kind = 0;
    Documentation documentation = {};
    bool readonly = false;
    std::string operator_glyph = {};
    std::string signature = {}; // Compiler-owned presentation; excluded from native contracts.
    bool static_member = false; // Editor metadata for type-level members.
    bool declaration_only = false; // Native declarations without a VOID body.
};
struct AnalyzedReference {
    std::string from, to, spelling;
    SourceSpan source;
    bool implicit = false; // Dependency edge without a written use site.
    std::string binding_contract = {}; // Exact native identities/specification, before editor normalization.
    std::string signature = {}; // Concrete selected callable, before source identity normalization.
    bool navigation_only = false; // Related target, with no distinct authored token.
};
// Presentation-only metadata. A slot is the same typed slot copied by the
// frontend lowerer. Argument numbers are one-based; zero denotes a local.
struct DebugBinding {
    unsigned slot, argument;
    std::string name;
    SourceLocation source;
};
class ValidatedAnalysis : public std::enable_shared_from_this<ValidatedAnalysis> {
    friend struct AnalysisAccess;
    std::string entry_, entry_symbol_, contract_, constants_;
    bool application_=false, executable_=true;
    std::shared_ptr<const CPrimitiveAdmission> c_primitives_;
    std::shared_ptr<const project::LockedDependencies> locked_dependencies_;
    ApplicationMetadata::Owner application_metadata_;
    std::vector<Diagnostic> warnings_;
    std::vector<AnalyzedModule> modules_;
    std::vector<AnalyzedDeclaration> declarations_;
    std::vector<AnalyzedReference> references_;
    std::vector<AnalyzedDeclaration> editor_declarations_;
    std::vector<AnalyzedReference> editor_references_;
    std::map<std::string,std::vector<DebugBinding>> debug_bindings_;
    std::function<ir::Module()> lower_;
    ValidatedAnalysis() = default;
public:
    ValidatedAnalysis(const ValidatedAnalysis&) = delete;
    ValidatedAnalysis& operator=(const ValidatedAnalysis&) = delete;
    const auto& warnings() const {return warnings_;}
    const std::string& entry_identity() const { return entry_; }
    const std::string& entry_symbol() const { return entry_symbol_; }
    bool application() const { return application_; }
    const ApplicationMetadata::Owner& application_metadata() const { return application_metadata_; }
    const auto& c_primitive_admission() const { return c_primitives_; }
    const auto& locked_dependencies() const { return locked_dependencies_; }
    bool executable() const { return executable_; } // Libraries expose metadata only.
    const auto& modules() const { return modules_; }
    const auto& declarations() const { return declarations_; }
    const auto& references() const { return references_; }
    const auto& editor_declarations() const { return editor_declarations_; }
    const auto& editor_references() const { return editor_references_; }
    const auto& debug_bindings() const { return debug_bindings_; }
    const std::string& compatibility_contract() const { return contract_; }
    const std::string& constant_contract() const { return constants_; }
    // Lowers the retained validated tree; no filesystem reads or reanalysis.
    ir::Module lower(ir::Emission emission = ir::Emission::all) const;
};
struct AnalysisResult {
    std::shared_ptr<const ValidatedAnalysis> program;
    std::vector<Diagnostic> diagnostics;
    bool succeeded() const { return bool(program); }
};
AnalysisResult analyze_source(const SourceFile& source);
// Exact canonical VFS keys. nullopt is an explicit unavailable/deleted source.
using SourceOverrides = std::map<std::string, std::optional<SourceFile>>;
// One configured source root shared by all frontend consumers. No cwd search.
std::filesystem::path standard_library_root();
void configure_standard_library_root(const std::filesystem::path& executable);
struct ProjectSelection { std::filesystem::path entry, root; bool standalone = false; };
ProjectSelection discover_project(const std::filesystem::path& workspace,
    const std::filesystem::path& active = {}, const std::filesystem::path& explicit_entry = {});
void require_module_identity(const std::string& identity);
AnalysisResult analyze_project(const std::filesystem::path& entry);
AnalysisResult analyze_project(const std::filesystem::path& entry,const NativeProject&);
AnalysisResult analyze_project(const std::filesystem::path& entry, std::shared_ptr<const CPrimitiveAdmission>);
AnalysisResult analyze_project(const std::filesystem::path& entry, const SourceOverrides& overrides);
// Manifest-selected mode is separate from the legacy entry-parent adapter above.
// TOML overlays have their own canonical root-relative namespace.
using ManifestOverrides = std::map<std::string, std::optional<SourceFile>>;
// Once an entry is selected, other existing files use their disk bytes for
// entry discovery. Import validation still uses their current editor overlays.
using EditorDiscovery = std::set<std::string>;
// Editor metadata for a single source and its explicit imports. It carries no
// application entry or authority to lower code.
// Context is the project root in manifest mode and the selected legacy entry otherwise.
AnalysisResult analyze_editor_document(const std::filesystem::path& context, const SourceOverrides&,
    const std::string& module, const ManifestOverrides* manifests = nullptr);
void require_manifest_identity(const std::string&);
// Owned observations, including failed selections. Never executable authority.
struct ProjectState {
    std::filesystem::path root;
    std::shared_ptr<const project::Manifest> manifest;
    std::optional<std::filesystem::path> entry;
    std::map<std::string,SourceFile> discovery, execution;
    std::string snapshot_key;
    bool discovery_complete=false, validated=false;
};
class ProjectAnalysis {
    friend struct ProjectAnalysisAccess;
    std::filesystem::path root_;
    std::optional<std::filesystem::path> entry_;
    std::shared_ptr<const project::Manifest> manifest_;
    std::shared_ptr<const ValidatedAnalysis> program_, semantic_;
    std::map<std::string, SourceFile> discovery_, execution_;
    std::string selection_key_, context_key_;
    ProjectAnalysis() = default;
public:
    const auto& root() const { return root_; }
    const auto& entry() const { return entry_; }
    const auto& manifest() const { return manifest_; }
    const auto& program() const { return program_; } // Absent for a checked library.
    const auto& semantic_analysis() const { return semantic_; } // Library lowering rejects.
    const auto& discovery_sources() const { return discovery_; }
    const auto& execution_sources() const { return execution_; }
    const auto& selection_key() const { return selection_key_; }
    const auto& context_key() const { return context_key_; } // Includes locked inputs, excludes root source and manifest trivia.
};
struct ProjectAnalysisResult {
    std::shared_ptr<const ProjectAnalysis> project;
    std::vector<Diagnostic> diagnostics;
    // Validated metadata may accompany failed selection; it grants no entry/IR.
    std::shared_ptr<const project::Manifest> manifest;
    std::shared_ptr<const ProjectState> state;
    bool succeeded() const { return bool(project); }
};
ProjectAnalysisResult analyze_editor_project(const std::filesystem::path&, const std::optional<std::string>&,
    const SourceOverrides&, const ManifestOverrides&, const EditorDiscovery&);
std::vector<Diagnostic> check_project_documents(const std::filesystem::path&,const SourceOverrides&,const ManifestOverrides&,const std::vector<std::string>&);
struct ProjectPreflight {
    std::filesystem::path root;std::optional<std::filesystem::path> entry;
    bool manifest_mode=false,standalone=false;ProjectAnalysisResult analysis;
};
ProjectPreflight discover_project_context(const std::filesystem::path& workspace,const std::filesystem::path& active,const std::filesystem::path& root,const std::filesystem::path& entry);
// Also recognizes malformed/wrong-case manifest presence so callers fail closed.
bool project_manifest_present(const std::filesystem::path& root);
ProjectAnalysisResult analyze_manifest_project(const std::filesystem::path& root,
    const std::optional<std::string>& entry_override = {},
    const SourceOverrides& sources = {}, const ManifestOverrides& manifests = {}, bool authorize_native = false);
struct EditorCandidate { std::string name, detail; int kind = 6; std::string insert_text = {}, documentation = {}; };
struct EditorAssistance {
    std::vector<EditorCandidate> candidates;
    std::string signature, documentation;
    std::vector<std::string> parameter_documentation;
    bool import_context = false, incomplete = false;
    bool constructor_signature = false;
    std::vector<std::string> parameters;
    unsigned active_parameter = 0;
    std::size_t begin = 0, end = 0;
};
// Bounded recovery uses the compiler parser and binder, never produces validated IR.
EditorAssistance editor_assistance(const std::filesystem::path&, const SourceOverrides&,
    const std::string& module, std::size_t cursor);
bool valid_editor_identifier(const std::string&);
EditorAssistance project_editor_assistance(const std::filesystem::path&,const std::optional<std::string>&,const SourceOverrides&,const ManifestOverrides&,const std::string&,std::size_t,const EditorDiscovery& = {});
EditorAssistance document_editor_assistance(const std::filesystem::path& context, const SourceOverrides&,
    const std::string& module, std::size_t cursor, const ManifestOverrides* manifests = nullptr);
std::optional<std::string> format_source(const SourceFile&, bool insert_spaces = true);
// One source pipeline shared by checking, JIT and AOT. Errors contain rendered
// source diagnostics and are raised before invoking any backend.
ir::Module compile_source(const SourceFile& source);
struct SourceProgram { SourceFile entry; ir::Module module; };
// Entry parent is the VFS root; no import depends on the caller working directory.
SourceProgram compile_project(const std::filesystem::path& entry);
SourceFile read_source(const std::filesystem::path& path);
void require_source_entry(const SourceFile& source, const ir::Module& module);
void require_source_entry(const ValidatedAnalysis&);
}
