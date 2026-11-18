#include "project_templates.hpp"
#include "project_digest.hpp"
#include "project_git.hpp"
#include "void/frontend.hpp"
#include "void/path.hpp"
#include "void/project_resolution.hpp"
#include "void/utf8.hpp"
#include <algorithm>

namespace vd::project {
namespace {
namespace fs = std::filesystem;
constexpr std::string_view metadata_path = ".void/template.toml";
constexpr std::string_view manifest_path = "void.project.toml";
constexpr std::string_view receipt_name = "registration";
constexpr std::size_t template_bytes = 64*1024*1024;
constexpr std::size_t template_entries = 8192;

fs::path template_store() {
    auto data = user_data_directory();
    return data.empty() ? data : data/"templates";
}

fs::path required_store() {
    auto store = template_store();
    if(store.empty())throw std::runtime_error("Cannot locate the platform user data directory for templates.");
    return store;
}

bool letters_or_digits(std::string_view value, std::string_view extra) {
    return std::all_of(value.begin(),value.end(),[&](char c) {
        return (c>='a' && c<='z') || (c>='0' && c<='9') || extra.find(c)!=extra.npos;
    });
}

bool has_name(const TemplateMetadata& info, std::string_view name) {
    return std::ranges::find(info.short_names,name)!=info.short_names.end();
}

bool text_file(std::string_view path) {
    return path.ends_with(".void") || path.ends_with(".toml") || path.ends_with(".md") ||
           path.ends_with(".txt") || path.ends_with(".json");
}

void require_template_path(std::string_view path, bool metadata) {
    if(metadata && (path==metadata_path || path==".void/"))return;
    if(path.ends_with('/'))path.remove_suffix(1);
    if(!valid_git_subdir(path) || path.empty())
        throw std::runtime_error("Invalid or reserved template path: "+std::string(path));
    // The root manifest controls the entire starter. Lockfiles and nested
    // manifests cannot import pre-existing identity or dependency authority.
    auto folded=fold_project_name(path);
    const auto leaf=folded.substr(folded.find_last_of('/')+1);
    if(leaf==lock_name || (leaf==manifest_path && path!=manifest_path))
        throw std::runtime_error("Invalid or reserved template path: "+std::string(path));
}

const std::string& file(const ProjectFiles& files, std::string_view path) {
    auto entry=files.find(std::string(path));
    if(entry==files.end())throw std::runtime_error("Template is missing "+std::string(path)+".");
    return entry->second;
}

std::shared_ptr<const Manifest> template_manifest(const ProjectFiles& files) {
    const auto& text=file(files,manifest_path);
    auto parsed=parse_manifest(SourceFile(std::string(manifest_path),text));
    if(!parsed.succeeded())throw CompilationError(std::move(parsed.diagnostics));
    const auto& manifest=*parsed.manifest;
    if(!manifest.dependencies().empty() || !manifest.native().empty() || manifest.native_requirement())
        throw std::runtime_error("Template manifests must be source projects. Add dependencies and native requirements after creation.");
    const auto check_file=[&](const auto& name) { if(name)(void)file(files,*name); };
    check_file(manifest.entry());
    check_file(manifest.package_entry());
    for(const auto& resource:manifest.resources())(void)file(files,resource);
    bool has_source=false;
    for(const auto& root:manifest.sources().roots) {
        const auto prefix=root=="."?std::string{}:root+"/";
        if(!prefix.empty())(void)file(files,prefix);
        auto source=files.lower_bound(prefix);
        for(;source!=files.end() && source->first.starts_with(prefix);++source)
            if(source->first.ends_with(".void")) {has_source=true;break;}
    }
    if(!has_source)throw std::runtime_error("Template requires at least one .void file inside its declared source roots.");
    return std::move(parsed.manifest);
}

TemplateMetadata validate_template(const ProjectFiles& files) {
    if(files.size()>template_entries)throw std::runtime_error("Template entry limit exceeded.");
    std::size_t bytes=0;
    for(const auto& [path,data]:files) {
        require_template_path(path,true);
        if(data.size()>template_bytes-bytes)throw std::runtime_error("Template byte limit exceeded.");
        bytes+=data.size();
        if(text_file(path) && (!valid_utf8(data) || data.find('\0')!=data.npos))
            throw std::runtime_error("Template text file must be UTF-8 without NUL: "+path);
    }
    auto metadata=parse_template_metadata(file(files,metadata_path));
    (void)template_manifest(files);
    return metadata;
}

ProjectFiles select_template(const GitSnapshot& snapshot, const std::string& subdir) {
    ProjectFiles files;
    const auto prefix=subdir.empty()?std::string{}:subdir+"/";
    for(const auto& directory:snapshot.directories)
        if(directory!=subdir && directory.starts_with(prefix))
            files.emplace(directory.substr(prefix.size())+"/","");
    for(const auto& [path,bytes]:snapshot.files)
        if(path.starts_with(prefix))files.emplace(path.substr(prefix.size()),bytes);
    return files;
}

std::string receipt_digest(std::string_view provenance, const ProjectFiles& files) {
    Digest hash;
    hash.field("void-template-registration-v1");
    hash.field(provenance);
    for(const auto& [path,bytes]:files) {
        hash.field(path);
        hash.field(bytes);
    }
    return hash.finish();
}

struct Registration {
    TemplateMetadata info;
    ProjectFiles files;
    std::string receipt;
};

Registration read_registration(const fs::path& path) {
    const auto root=inspect_creation_root(path);
    auto receipt=read_project_input(root.path/"metadata"/receipt_name,16384);
    std::string_view provenance(receipt);
    if(provenance.size()<65 || provenance[64]!='\n')
        throw std::runtime_error("Invalid template registration receipt.");
    provenance.remove_prefix(65);
    ProjectFiles files;
    std::string_view payload;
    if(provenance.starts_with("local\n") && utf8_path(provenance.substr(6)).is_absolute()) {
        payload="source";
        files=read_package_directory(root.path/payload,{},DirectoryInventory::template_registration);
    } else if(provenance.starts_with("git\n")) {
        payload=".void";
        auto row=provenance.substr(4);
        const auto origin=row.find('\n');
        if(origin==row.npos)throw std::runtime_error("Invalid template Git receipt.");
        GitPin pin{std::string(row.substr(0,origin)),{}};
        row.remove_prefix(origin+1);
        const auto revision=row.find('\n');
        if(revision==row.npos)throw std::runtime_error("Invalid template Git receipt.");
        pin.revision=row.substr(0,revision);
        auto subdir=std::string(row.substr(revision+1));
        if(!pin.origin.starts_with("https://") || !valid_git_pin(pin) || !valid_git_subdir(subdir))
            throw std::runtime_error("Invalid template Git provenance.");
        auto snapshot=read_git_snapshot(git_snapshot_path(root.path,pin),pin);
        if(!snapshot)throw std::runtime_error("Missing verified template Git snapshot.");
        files=select_template(*snapshot,subdir);
    } else throw std::runtime_error("Invalid template registration provenance.");
    for(const auto& child:fs::directory_iterator(root.path))
        if(child.path().filename()!="metadata" && child.path().filename()!=payload)
            throw std::runtime_error("Unexpected files in template registration.");
    for(const auto& child:fs::directory_iterator(root.path/"metadata"))
        if(child.path().filename()!=receipt_name)
            throw std::runtime_error("Unexpected files in template registration metadata.");
    if(receipt.compare(0,64,receipt_digest(provenance,files))!=0)
        throw std::runtime_error("Template registration changed: "+path_utf8(path.filename()));
    auto metadata=validate_template(files);
    if(metadata.id!=path_utf8(path.filename()) || inspect_creation_root(root.path).identity!=root.identity)
        throw std::runtime_error("Template registration identity changed.");
    return {std::move(metadata),std::move(files),std::move(receipt)};
}

template<class Visit>
void scan_templates(const CreationRoot& root, Visit visit) {
    std::set<std::string> names;
    std::size_t count=0;
    for(const auto& child:fs::directory_iterator(root.path)) {
        const auto id=path_utf8(child.path().filename());
        if(id==".void")continue;
        if(++count>64 || !valid_identity(id))
            throw std::runtime_error("Invalid or excessive entries in the template store.");
        auto registration=read_registration(child.path());
        for(const auto& name:registration.info.short_names)
            if(!names.insert(name).second)
                throw std::runtime_error("Duplicate short name in the template store: "+name);
        visit(std::move(registration));
    }
}

TemplateMetadata install(const ProjectFiles& files, std::string provenance,
                         const std::function<void(const CreationRoot&)>& publish_source) {
    auto metadata=validate_template(files);
    if(provenance.size()>16000 || !valid_utf8(provenance) || provenance.find('\0')!=provenance.npos)
        throw std::runtime_error("Invalid template provenance.");
    auto receipt=receipt_digest(provenance,files)+'\n'+provenance;
    with_user_package_store(required_store(),true,[&](const CreationRoot& root) {
        bool already=false;
        std::size_t registrations=0;
        scan_templates(root,[&](Registration existing) {
            ++registrations;
            if(existing.info.id==metadata.id) {
                if(existing.receipt!=receipt)
                    throw std::runtime_error("Template identity already installed. Uninstall it before replacing: "+metadata.id);
                already=true;
            } else for(const auto& name:metadata.short_names)
                if(has_name(existing.info,name))throw std::runtime_error("Template short name already installed: "+name);
        });
        if(already)return;
        if(registrations==64)throw std::runtime_error("Template store registration limit exceeded.");
        publish_user_registration(root,metadata.id,[&](const fs::path& stage) {
            const auto admitted=inspect_creation_root(stage);
            publish_source(admitted);
            // The creation helper writes the receipt in the private registration
            // using the same exclusive file machinery as a project file.
            publish_project_directory(admitted,"metadata",{{std::string(receipt_name),receipt}},[&](const auto& path,const auto& reader) {
                if(reader.read(path/receipt_name,16384)!=receipt)
                    throw std::runtime_error("Template receipt changed while staging.");
            });
        });
    });
    return metadata;
}

std::string substitute(std::string_view text, std::string_view source, std::string_view target, std::size_t limit) {
    std::string output;
    auto append=[&](std::string_view part) {
        if(part.size()>limit-output.size())
            throw std::runtime_error("Template substitution exceeds its output size limit.");
        output.append(part);
    };
    for(auto hit=text.find(source);hit!=text.npos;hit=text.find(source)) {
        append(text.substr(0,hit));
        append(target);
        text.remove_prefix(hit+source.size());
    }
    append(text);
    return output;
}

std::string substitute_manifest(const Manifest& authored, const Manifest& generated,
                                std::string_view placeholder, std::size_t limit) {
    struct Edit { SourceSpan span; std::string_view value; };
    std::vector<Edit> edits;
    auto assign=[&](const std::string& key, std::string_view value) {
        edits.push_back({authored.fields().at(key).value,value});
    };
    assign("project.id",generated.project().id);
    assign("project.name",generated.project().name);
    if(generated.application())assign("application.id",generated.application()->id);
    else {
        assign("package.id",generated.package()->id);
        assign("package.name",generated.package()->name);
    }

    // Substitute only outside the generated identity fields so those values
    // reach manifest validation in their canonical form.
    std::ranges::sort(edits,{},[](const Edit& edit) { return edit.span.begin; });
    const std::string_view text=authored.source().text();
    const auto& name=generated.project().name;
    std::string output;
    std::size_t offset=0;
    for(const auto& edit:edits) {
        output+=substitute(text.substr(offset,edit.span.begin-offset),placeholder,name,limit-output.size());
        if(edit.value.size()+2>limit-output.size())
            throw std::runtime_error("Template substitution exceeds its output size limit.");
        output+='"';
        output+=edit.value;
        output+='"';
        offset=edit.span.end;
    }
    output+=substitute(text.substr(offset),placeholder,name,limit-output.size());
    return output;
}

ProjectFiles instantiate(const Registration& registered, const Manifest& authored,
                         const CreationPlan& plan, const std::string& name) {
    ProjectFiles result;
    std::set<std::string> paths;
    std::size_t bytes=0;
    const auto& placeholder=registered.info.source_name;
    for(const auto& [path,data]:registered.files) {
        if(path==metadata_path || path==".void/")continue;
        auto renamed=substitute(path,placeholder,name,4096);
        require_template_path(renamed,false);
        const auto folded=fold_project_name(renamed.ends_with('/')?renamed.substr(0,renamed.size()-1):renamed);
        if(!paths.insert(folded).second)
            throw std::runtime_error("Template filename collision after substitution: "+renamed);
        std::string output;
        if(path==manifest_path)output=substitute_manifest(authored,*plan.manifest,placeholder,template_bytes-bytes);
        else if(text_file(path))output=substitute(data,placeholder,name,template_bytes-bytes);
        else output=data;
        if(output.size()>template_bytes-bytes)throw std::runtime_error("Template output byte limit exceeded.");
        bytes+=output.size();
        result.emplace(std::move(renamed),std::move(output));
    }

    (void)template_manifest(result);
    return result;
}
}

bool valid_template_short_name(std::string_view name) {
    constexpr std::string_view reserved[]={"app","classlib","lib","list","install","uninstall","help","check","run","build","emit",
        "new","init","package","add","remove","tidy","resolve","deps","native","native-bind"};
    return !name.empty() && name.size()<=32 && name.front()>='a' && name.front()<='z' &&
        !name.ends_with('-') && letters_or_digits(name,"-") && portable_path_component(name) &&
        std::ranges::find(reserved,name)==std::end(reserved);
}

bool valid_template_project_name(std::string_view name) {
    if(name.empty() || name.size()>64 || !portable_path_component(name))return false;
    const auto folded=fold_project_name(name);
    return folded.front()>='a' && folded.front()<='z' && letters_or_digits(folded,"_") &&
        valid_editor_identifier(folded) && fold_project_name(name)!="build";
}

std::vector<TemplateMetadata> list_project_templates() {
    std::vector<TemplateMetadata> result;
    const auto store=template_store();
    if(!store.empty())with_user_package_store(store,false,[&](const auto& root) {
        scan_templates(root,[&](Registration entry){result.push_back(std::move(entry.info));});
    });
    std::ranges::sort(result,{},&TemplateMetadata::id);
    return result;
}

TemplateMetadata install_project_template(const fs::path& source) {
    auto admitted=admit_source_directory(fs::current_path(),source);
    auto files=read_package_directory(admitted,{},DirectoryInventory::template_source);
    return install(files,"local\n"+path_utf8(admitted),[&](const CreationRoot& stage) {
        std::map<std::string,std::string_view> views(files.begin(),files.end());
        publish_project_directory(stage,"source",views,[&](const auto& path,const auto& reader) {
            if(read_package_directory(path,reader)!=files)
                throw std::runtime_error("Template source changed while staging.");
        });
    });
}

TemplateMetadata install_project_template_git(const std::string& origin) {
    if(!origin.starts_with("https://") || !valid_git_origin(origin))
        throw std::invalid_argument("Template Git source must be a canonical HTTPS repository or GitHub tree URL.");
    auto selection=normalize_git_source({{origin,{}},{}});
    auto root=fs::current_path();
    GitSnapshot snapshot;
    if(selection.pin.revision.empty()) {
        auto [pin,head]=acquire_git_head(root,selection.pin.origin);
        selection.pin=std::move(pin);
        snapshot=std::move(head);
    } else snapshot=acquire_git_snapshot(root,selection.pin);
    auto files=select_template(snapshot,selection.subdir);
    return install(files,"git\n"+selection.pin.origin+'\n'+selection.pin.revision+'\n'+selection.subdir,
        [&](const CreationRoot& stage){publish_git_snapshot(stage,selection.pin,snapshot);});
}

void uninstall_project_template(std::string_view short_name) {
    if(!valid_template_short_name(short_name))throw std::invalid_argument("Invalid template short name.");
    std::string id;
    with_user_package_store(required_store(),false,[&](const auto& root) {
        scan_templates(root,[&](Registration entry) {
            if(has_name(entry.info,short_name))id=entry.info.id;
        });
        if(!id.empty())remove_user_registration(root,id);
    });
    if(id.empty())throw std::runtime_error("No installed template named "+std::string(short_name)+".");
}

std::optional<fs::path> create_from_project_template(std::string_view short_name,
                                                    const std::optional<std::string>& name, bool nested) {
    const auto store=template_store();
    if(store.empty())return {};
    std::optional<Registration> selected;
    with_user_package_store(store,false,[&](const auto& root) {
        scan_templates(root,[&](Registration entry) {
            if(has_name(entry.info,short_name))selected=std::move(entry);
        });
    });
    if(!selected)return {};
    if(!name)throw std::invalid_argument("Use --name NAME to create from an installed template or new -- PATH for a literal directory.");
    if(!valid_template_project_name(*name))
        throw std::invalid_argument("Template project name must be a portable identifier beginning with a letter.");
    require_outside_user_package_store(utf8_path(*name));
    require_outside_native_approval_store(utf8_path(*name),true);
    const auto manifest=template_manifest(selected->files);
    CreationOptions options;
    options.new_directory=true;
    options.nested=nested;
    options.kind=manifest->kind();
    if(options.kind==Kind::library)options.package_name=fold_project_name(*name);
    const auto plan=plan_creation(utf8_path(*name),options);
    publish_new_project_files(plan,instantiate(*selected,*manifest,plan,*name));
    return plan.root;
}
}
