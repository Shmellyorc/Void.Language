// Compiler-owned manifest admission, discovery snapshots and E selection.
// Uses SourceGraph::parse/load and the ordinary Parser/validator below the API.
namespace {
namespace project_fs = std::filesystem;
constexpr std::size_t project_file_limit=4096, project_source_limit=64*1024*1024;
constexpr std::size_t project_visit_limit=16384, project_depth_limit=64;
bool project_native_alias(const project_fs::path& path) {
    std::error_code ec;auto status=project_fs::symlink_status(path,ec);
    if(ec==std::errc::no_such_file_or_directory)return false;
    if(ec)throw SourceReadError(path_utf8(path),"Cannot inspect project path.");
    if(project_fs::is_symlink(status))return true;
#ifdef _WIN32
    auto attributes=GetFileAttributesW(path.c_str());
    if(attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_REPARSE_POINT))return true;
#endif
    return false;
}
[[noreturn]] void project_failure(const SourceFile& source,Code code,const std::string& message,const std::string& hint="Correct the project path or source policy and retry.") {
    error(source,{TokenKind::end,"",0},code,message,hint);
}
struct ProjectCapture {
    project_fs::path root;
    const SourceOverrides& overlays;
    const ManifestOverrides& manifest_overlays;
    const ProjectCapture* original;
    SourceFile anchor;
    std::shared_ptr<const project::Manifest> manifest;
    CPrimitiveAdmission::Owner native;
    project::Sources creation_policy;
    std::map<std::string,SourceFile> sources;
    std::map<std::string,std::vector<std::string>> listings;
    std::map<std::string,bool> boundaries;
    std::map<std::string,std::set<std::string>> virtual_children;
    std::set<std::string> virtual_directories;
    std::size_t visited=0,bytes=0;
    Diagnostics diagnostics;
    SourceGraph graph;
    Unit from;
    ProjectCapture(const project_fs::path& requested,const SourceOverrides& o,const ManifestOverrides& m,const ProjectCapture* previous=nullptr,bool inventory_only=false)
      :root(admit_root(requested)),overlays(bounded_overlays(o)),manifest_overlays(m),original(previous),anchor(path_utf8(root/"void.project.toml"),""),graph(root,diagnostics,overlays),from(anchor,"/") {
        changed(original && root!=original->root);
        if(manifest_overlays.size()>project_visit_limit)project_failure(anchor,Code::limit,"Manifest overlay work limit exceeded.");
        for(const auto& [id,value]:overlays) {
            require_module_identity(id);
            std::string parent;
            for(const auto& component:utf8_path(id).relative_path()) {
                auto name=path_utf8(component);virtual_children[parent].insert(name);
                parent+="/"+name;
                if(value && parent!=id)virtual_directories.insert(parent);
            }
        }
        for(const auto& [id,value]:manifest_overlays) {
            (void)value;
            if(id.empty() || id.front()!='/' || path_utf8(utf8_path(id).filename())!="void.project.toml")
                project_failure(anchor,Code::project_path,"Manifest overlay must name an exact root-relative void.project.toml.");
            for(const auto& component:utf8_path(id).relative_path())if(component=="." || component==".." || !portable_path_component(path_utf8(component)))
                project_failure(anchor,Code::project_path,"Invalid manifest overlay identity.");
            if(path_utf8(utf8_path(id).lexically_normal())!=id)project_failure(anchor,Code::project_path,"Manifest overlay identity is not canonical.");
        }
        graph.admit=[this](const auto& path,const auto& id,bool leaf){admit(path,id,leaf);};
        graph.reader=[this](const auto& path,const auto& id){return capture(path,id);};
        if(inventory_only) {
            if(boundary(""))project_failure(anchor,Code::project_boundary,"A project manifest is present.");
            return;
        }
        if(!boundary(""))project_failure(anchor,Code::project_boundary,"Selected project manifest is unavailable.","Select a directory containing an exact regular void.project.toml; a deleted manifest cannot fall back to legacy mode.");
        auto overlay=manifest_overlays.find("/void.project.toml");
        if(overlay!=manifest_overlays.end() && overlay->second->text().size()>project::manifest_byte_limit)project_failure(anchor,Code::limit,"Project manifest byte limit exceeded.");
        auto source=overlay==manifest_overlays.end()?read_bounded(root/"void.project.toml",project::manifest_byte_limit):SourceFile(path_utf8(root/"void.project.toml"),overlay->second->text());
        changed(original && source.text()!=original->manifest->source().text());
        auto parsed=project::parse_manifest(source);
        if(!parsed.succeeded())throw CompilationError(std::move(parsed.diagnostics));
        manifest=std::move(parsed.manifest);
        anchor=manifest->source();from.source=anchor;
        if (!manifest->dependencies().empty())
            project_failure(anchor, Code::project_reserved, "Dependency compilation is not supported in this compiler.",
                "Use voidc resolve --project <directory> to verify the local dependency graph.");
        if (!manifest->native().empty()) {
#ifdef _WIN32
            constexpr auto platform = "windows";
            constexpr auto target = "x86_64-pc-windows-msvc";
#else
            constexpr auto platform = "linux";
            constexpr auto target = "x86_64-unknown-linux-gnu";
#endif
            const auto selected = manifest->native().find(platform);
            if (selected == manifest->native().end())
                project_failure(anchor, Code::project_reserved, "No native provider declared for this host.");
            const auto& declaration = selected->second;
            const auto directory = root / "native";
            const auto path = directory / utf8_path(declaration.descriptor);
            if (!project_fs::is_regular_file(path))
                project_failure(anchor, Code::project_path, "Missing native descriptor: " + path_utf8(path));
            auto descriptor = read_bounded(path, 16384);
            native = CPrimitiveAdmission::admit_conventional(descriptor.text(), directory, declaration.library, declaration.adapter);
            if (native->target() != target)
                project_failure(anchor, Code::project_reserved, "Native descriptor target does not match this host.");
            if (overlays.contains(native->module()))
                project_failure(anchor, Code::project_boundary, "Generated C binding module cannot be overridden.");
            changed(original && (!original->native || native->dynamic_contract() != original->native->dynamic_contract()));
            graph.generated_primitives(native);
        }
        for(const auto& name:manifest->sources().roots) {
            if(name!=".") {
                graph.resolve(from,{TokenKind::path,"/"+name+"/.void",0},true);
                std::error_code ec;
                if(!project_fs::is_directory(root/utf8_path(name),ec) || ec)project_failure(anchor,Code::project_boundary,"Configured source root must be an existing physical directory: '"+name+"'.", "Named overlays may add virtual descendants beneath admitted physical roots, not invent a source root.");
            }
        }
        // Absent exclusions are valid. Inspect only their existing parent spelling;
        // never enumerate the excluded subtree to search for boundaries or errors.
        for(const auto& excluded:manifest->sources().exclude) {
            std::string parent;
            for(const auto& part:utf8_path(excluded)) {
                auto name=path_utf8(part);auto names=directory(parent);
                std::set<std::string> matches;for(const auto& child:names)if(folded(child)==folded(name))matches.insert(child);
                if(matches.empty())break;
                if(matches.size()!=1 || *matches.begin()!=name)project_failure(anchor,Code::project_path,"Source exclusion has wrong case or a case collision: '"+excluded+"'.");
                parent+="/"+name;
                if(parent!="/"+excluded) {
                    auto p=root/utf8_path(parent).relative_path();
                    if(project_native_alias(p))project_failure(anchor,Code::project_boundary,"Source exclusion parent contains an alias.");
                    std::error_code ec;if(!project_fs::is_directory(p,ec))break;
                }
            }
        }
    }
    void changed(bool mismatch) const {
        if(mismatch)project_failure(original?original->anchor:anchor,Code::project_changed,"Project changed during discovery; retry.","Retry after filesystem changes finish; no partial selection or cached entry was accepted.");
    }
    static const SourceOverrides& bounded_overlays(const SourceOverrides& sources) {
        if(sources.size()>project_visit_limit)project_failure(SourceFile("<project overlays>",""),Code::limit,"Source overlay work limit exceeded.");
        return sources;
    }
    static project_fs::path admit_root(const project_fs::path& requested) {
        // Canonicalize the actual OS path before removing parent references:
        // link/../project need not mean the lexical sibling of link.
        auto absolute=project_fs::absolute(requested);
        while(absolute.has_relative_path() && (absolute.filename().empty() || absolute.filename()=="."))absolute=absolute.parent_path();
        SourceFile site(path_utf8(absolute),"");
        if(project_native_alias(absolute))project_failure(site,Code::project_boundary,"Selected project root is an alias.");
        auto root=project_fs::canonical(absolute);
        if(!project_fs::is_directory(root))project_failure(site,Code::project_boundary,"Selected project root is not a directory.");
        // Canonical ancestors are permitted; a root's own spelling is exact.
        if(root!=root.root_path()) {
            auto expected=path_utf8(absolute.filename()==".."?root.filename():absolute.filename());std::set<std::string> matches;
            for(const auto& child:project_fs::directory_iterator(root.parent_path()))if(folded(path_utf8(child.path().filename()))==folded(expected))matches.insert(path_utf8(child.path().filename()));
            if(matches.size()!=1 || *matches.begin()!=expected)project_failure(site,Code::project_boundary,"Selected project root has wrong case or a case collision.");
        }
        return root;
    }
    SourceFile read_bounded(const project_fs::path& path,std::size_t limit) {
        std::ifstream stream(path,std::ios::binary);
        if(!stream)throw SourceReadError(path_utf8(path),"Cannot read admitted project source.");
        std::string text;char buffer[4096];
        while(stream) {
            stream.read(buffer,sizeof buffer);auto count=static_cast<std::size_t>(stream.gcount());
            if(count>limit-text.size())project_failure(anchor,Code::limit,"Project source/input byte limit exceeded.","Reduce the admitted source set or manifest size; selection is never truncated.");
            text.append(buffer,count);
        }
        if(!stream.eof())throw SourceReadError(path_utf8(path),"Cannot finish reading admitted project source.");
        return SourceFile(path_utf8(path),std::move(text));
    }
    const std::vector<std::string>& directory(const std::string& id) {
        if(auto found=listings.find(id);found!=listings.end())return found->second;
        auto path=root/utf8_path(id).relative_path();std::error_code ec;std::set<std::string> names;
        for(project_fs::directory_iterator it(path,ec),end;!ec && it!=end;it.increment(ec)) {
            if(++visited>project_visit_limit)project_failure(anchor,Code::limit,"Project directory-entry limit exceeded.");
            names.insert(path_utf8(it->path().filename()));
        }
        if(ec && !(ec==std::errc::no_such_file_or_directory && virtual_directories.contains(id)))
            project_failure(anchor,Code::project_boundary,"Cannot enumerate project directory '"+id+"'.");
        if(auto virtuals=virtual_children.find(id);virtuals!=virtual_children.end())for(const auto& name:virtuals->second)if(names.insert(name).second && ++visited>project_visit_limit)
            project_failure(anchor,Code::limit,"Project directory-entry limit exceeded by named overlays.");
        std::vector<std::string> observed(names.begin(),names.end());
        if(original)if(auto old=original->listings.find(id);old!=original->listings.end())changed(observed!=old->second);
        return listings.emplace(id,std::move(observed)).first->second;
    }
    bool boundary(const std::string& id) {
        if(auto found=boundaries.find(id);found!=boundaries.end())return found->second;
        const auto& names=directory(id);std::set<std::string> matches;
        for(const auto& name:names)if(folded(name)=="void.project.toml")matches.insert(name);
        if(matches.size()>1 || (!matches.empty() && *matches.begin()!="void.project.toml"))
            project_failure(anchor,Code::project_boundary,"Manifest must use exact case without case siblings in '"+id+"'.");
        auto path=root/utf8_path(id).relative_path()/"void.project.toml";
        if(!matches.empty())admit_file(path,true);
        bool present=!matches.empty();
        if(auto o=manifest_overlays.find(id+"/void.project.toml");o!=manifest_overlays.end())present=bool(o->second);
        if(original)if(auto old=original->boundaries.find(id);old!=original->boundaries.end())changed(present!=old->second);
        boundaries.emplace(id,present);return present;
    }
    void admit_file(const project_fs::path& path,bool manifest_file=false) {
        std::error_code ec;
        if(project_native_alias(path) || !project_fs::is_regular_file(path,ec) || ec || project_fs::canonical(path,ec)!=path || ec)
            project_failure(anchor,Code::project_boundary,"Project file must be regular, exact-case and free of symlink/reparse aliases: '"+path_utf8(path)+"'.");
        if(manifest_file && (project_fs::hard_link_count(path,ec)!=1 || ec))project_failure(anchor,Code::project_boundary,"Project manifest must have exactly one native link: '"+path_utf8(path)+"'.");
        // SourceGraph checks conflicting physical identities across parsed D and
        // loaded E, including overlaid host files. Retain its source-file policy.
    }
    void admit(const project_fs::path& path,const std::string& id,bool leaf) {
        if(project_native_alias(path))project_failure(anchor,Code::project_boundary,"Project source path contains an alias: '"+id+"'.");
        if(leaf) {std::error_code ec;if(project_fs::exists(path,ec))admit_file(path);}
        else if(boundary(id))project_failure(anchor,Code::project_boundary,"Source path crosses nested project boundary '"+id+"'.", "Select the child project explicitly; parent imports and entry overrides cannot cross it.");
    }
    SourceFile capture(const project_fs::path& path,const std::string& id,bool discovery=false) {
        if(standard_module(id))return read_source(path);
        if(auto found=sources.find(id);found!=sources.end())return found->second;
        // D is inventoried before E expansion. Its new work/byte budgets must
        // not replace the existing import/parser limits for E-only sources.
        if(discovery && sources.size()>=project_file_limit)project_failure(anchor,Code::limit,"Project discovery source-file limit exceeded.");
        auto o=overlays.find(id);
        if(discovery && o!=overlays.end() && o->second->text().size()>project_source_limit-bytes)project_failure(anchor,Code::limit,"Project discovery aggregate source-byte limit exceeded.");
        auto source=native && id==native->module()?SourceFile(path_utf8(path),native->source()):
            o!=overlays.end()?SourceFile(path_utf8(path),o->second->text()):discovery?read_bounded(path,project_source_limit-bytes):read_source(path);
        if(original)if(auto old=original->sources.find(id);old!=original->sources.end())changed(source.text()!=old->second.text());
        if(discovery)bytes+=source.text().size();sources.emplace(id,source);return source;
    }
    bool excluded(const std::string& id) const {
        if(native && (id=="/native" || id.starts_with("/native/")))return true;
        auto relative=id.substr(1);
        for(const auto& part:utf8_path(relative))if(auto s=folded(path_utf8(part));s==".git" || s==".void" || s=="build" || s=="libs")return true;
        const auto& policy=manifest?manifest->sources():creation_policy;
        for(const auto& item:policy.exclude)if(relative==item || (relative.starts_with(item) && relative.size()>item.size() && relative[item.size()]=='/'))return true;
        return false;
    }
    void inventory_directory(const std::string& id,std::set<std::string>& result,std::size_t depth) {
        if(depth>project_depth_limit || static_cast<std::size_t>(std::count(id.begin(),id.end(),'/'))>project_depth_limit)project_failure(anchor,Code::limit,"Project directory-depth limit exceeded.");
        std::set<std::string> children;for(const auto& n:directory(id))children.insert(n);
        if(auto found=virtual_children.find(id);found!=virtual_children.end())children.insert(found->second.begin(),found->second.end());
        for(const auto& name:children) {
            auto child=id+"/"+name;if(excluded(child))continue;
            auto o=overlays.find(child);if(o!=overlays.end() && !o->second)continue;
            auto path=root/utf8_path(child).relative_path();std::error_code ec;auto status=project_fs::symlink_status(path,ec);
            if(ec==std::errc::no_such_file_or_directory)ec.clear();
            if(ec)project_failure(anchor,Code::project_boundary,"Cannot inspect inventory path '"+child+"'.");
            if(project_native_alias(path))project_failure(anchor,Code::project_boundary,"Alias in project inventory: '"+child+"'.");
            if(project_fs::is_directory(status) || virtual_directories.contains(child)) {
                if(!portable_path_component(name))project_failure(anchor,Code::project_path,"Invalid project directory component '"+name+"'.");
                if(boundary(child))continue;
                graph.resolve(from,{TokenKind::path,child+"/.void",0},true);
                inventory_directory(child,result,depth+1);
            } else if(name.ends_with(".void")) {
                auto [key,physical]=graph.resolve(from,{TokenKind::path,child,0});
                if(manifest)capture(physical,key,true);
                else {
                    if(result.size()>=project_file_limit)project_failure(anchor,Code::limit,"Creation inventory source-file limit exceeded.");
                    if(project_fs::hard_link_count(physical)!=1 || !std::ifstream(physical,std::ios::binary))
                        project_failure(anchor,Code::project_boundary,"Creation inventory source cannot be admitted: '"+key+"'.");
                }
                result.insert(key);
            }
        }
    }
    std::set<std::string> inventory() {
        std::set<std::string> result;
        for(const auto& name:(manifest?manifest->sources():creation_policy).roots) {
            auto id=name=="."?std::string{}:"/"+name;
            if(id.empty() || !excluded(id))inventory_directory(id,result,0);
        }
        return result;
    }
    SourceFile recapture_source(const std::string& id) {
        Token path{TokenKind::path,id,0};
        if(standard_module(id)) {path.kind=TokenKind::name;path.text=id.substr(9,id.size()-9-5);}
        auto [key,physical]=graph.resolve(from,path);return capture(physical,key);
    }
};
std::string project_snapshot_key(const ProjectCapture& capture,const std::string& entry,bool automatic) {
    llvm::SHA256 hash;
    auto field=[&](std::string_view s){auto length=std::to_string(s.size())+":";hash.update(llvm::StringRef(length));hash.update(llvm::StringRef(s.data(),s.size()));};
    field("void-project-selection-v1");field(path_utf8(capture.root));field(capture.manifest->source().text());field(entry);field(automatic?"automatic":"explicit");
    if(capture.native)field(capture.native->dynamic()->bytes());
    for(const auto& [id,source]:capture.sources){field(id);field(source.text());}
    for(const auto& [id,present]:capture.boundaries){field(id);field(present?"present":"absent");}
    for(const auto& [id,names]:capture.listings){field(id);for(const auto& name:names)field(name);field("end-directory");}
    constexpr char hex[]="0123456789abcdef";std::string result;for(auto byte:hash.final()){result+=hex[byte>>4];result+=hex[byte&15];}return result;
}
std::string project_context_key(const ProjectCapture& capture,const std::string& entry) {
    // Semantic context is independent of source/build provenance and TOML trivia.
    std::string result="project-context-v1";
    auto field=[&](const std::string& value){result+=std::to_string(value.size())+":"+value;};
    auto identity=[&](const project::Identity& value){field(value.id);field(value.name);field(value.version.text());};
    const auto& m=*capture.manifest;
    field(path_utf8(capture.root));identity(m.project());field(std::to_string(static_cast<int>(m.kind())));
    if(m.application())identity(*m.application());if(m.package())identity(*m.package());
    field(entry);field(std::to_string(m.sources().roots.size()));for(const auto& root:m.sources().roots)field(root);
    field(std::to_string(m.sources().exclude.size()));for(const auto& excluded:m.sources().exclude)field(excluded);
    for(const auto& profile:{m.debug(),m.release()}){field(std::to_string(static_cast<int>(profile.optimization)));field(std::to_string(static_cast<int>(profile.debug_info)));}
    return result;
}
}
struct ProjectAnalysisAccess {
    static std::shared_ptr<const ProjectAnalysis> build(ProjectCapture& capture,const std::set<std::string>& seeds,const std::string& entry,bool automatic,std::shared_ptr<Unit> library_scope,bool authorize_native) {
        auto result=std::shared_ptr<ProjectAnalysis>(new ProjectAnalysis);
        result->root_=capture.root;result->manifest_=capture.manifest;
        for(const auto& id:seeds)result->discovery_.emplace(id,capture.sources.at(id));
        for(const auto& [id,unit]:capture.graph.units)result->execution_.emplace(id,unit->source);
        result->selection_key_=project_snapshot_key(capture,entry,automatic);
        result->context_key_=project_context_key(capture,entry);
        if(!entry.empty()) {
            result->entry_=capture.root/utf8_path(entry).relative_path();
            const auto& project = capture.manifest->project();
            const auto& app = *capture.manifest->application();
            auto metadata = ApplicationMetadata::make({project.id, project.name, project.version.text(), app.id, app.name, app.version.text()});
            result->program_=AnalysisAccess::build(std::move(capture.graph.units),entry,capture.diagnostics.warnings(),result->context_key_,{},std::move(metadata),!capture.native || authorize_native);
            result->semantic_=result->program_;
        } else {
            result->semantic_=AnalysisAccess::build(std::move(capture.graph.units),entry,capture.diagnostics.warnings(),result->context_key_,std::move(library_scope));
        }
        return result;
    }
};
void require_manifest_identity(const std::string& id) {
    if(id.empty() || id.front()!='/' || path_utf8(utf8_path(id).filename())!="void.project.toml")throw std::invalid_argument("Expected exact root-relative void.project.toml identity.");
    for(const auto& part:utf8_path(id).relative_path())if(part=="." || part==".." || !portable_path_component(path_utf8(part)))throw std::invalid_argument("Invalid manifest overlay identity.");
    if(path_utf8(utf8_path(id).lexically_normal())!=id)throw std::invalid_argument("Manifest overlay identity is not canonical.");
}
bool project_manifest_present(const std::filesystem::path& root) {
    for(const auto& item:std::filesystem::directory_iterator(root))if(folded(path_utf8(item.path().filename()))=="void.project.toml")return true;
    return false;
}
ProjectAnalysisResult analyze_manifest_project_impl(const std::filesystem::path& root,const std::optional<std::string>& entry_override,const SourceOverrides& sources,const ManifestOverrides& manifests,EditorProbe* editor,bool authorize_native) {
    ProjectAnalysisResult result;
    std::unique_ptr<ProjectCapture> owned;
    std::set<std::string> seeds;std::string entry;bool automatic=false,complete=false;
    auto retain=[&] {
        auto state=std::make_shared<ProjectState>();std::error_code ec;state->root=owned?owned->root:std::filesystem::canonical(root,ec);if(ec){ec.clear();state->root=std::filesystem::absolute(root,ec);}if(ec)state->root=root;
        state->manifest=result.manifest;state->discovery_complete=complete;state->validated=result.succeeded();
        if(owned) {
            for(const auto& id:seeds)if(auto source=owned->sources.find(id);source!=owned->sources.end())state->discovery.emplace(id,source->second);
            for(const auto& [id,unit]:owned->graph.units)state->execution.emplace(id,unit->source);
            if(!entry.empty())state->entry=owned->root/utf8_path(entry).relative_path();
            state->snapshot_key=project_snapshot_key(*owned,entry,automatic);
        }
        if(result.project){state->discovery=result.project->discovery_sources();state->execution=result.project->execution_sources();state->snapshot_key=result.project->selection_key();}
        auto bind=[&](SourceSpan& span){
            if(!span.module_identity.empty())return;
            auto relative=span.source?utf8_path(span.source->path()).lexically_relative(state->root):std::filesystem::path{};
            auto id="/"+path_utf8(relative);
            if(!relative.empty())try{if(id.ends_with("/void.project.toml"))require_manifest_identity(id);else require_module_identity(id);span.module_identity=id;return;}catch(const std::invalid_argument&){}
            span.module_identity="/void.project.toml";
        };
        for(auto& d:result.diagnostics){bind(d.primary);for(auto& related:d.related)bind(related.span);}
        result.state=std::move(state);
    };
    try {
        owned=std::make_unique<ProjectCapture>(root,sources,manifests);auto& capture=*owned;result.manifest=capture.manifest;capture.diagnostics.editor=editor;
        const bool library=capture.manifest->kind()==project::Kind::library;
        if(library && entry_override)project_failure(capture.anchor,Code::project_entry,"A library project cannot select an application entry.");
        auto explicit_entry=entry_override?entry_override:capture.manifest->entry();
        automatic=!explicit_entry;
        if(automatic) {
            seeds=capture.inventory();
            complete=true;
            for(const auto& id:seeds)capture.graph.parse(capture.from,{TokenKind::path,id,0});
            capture.diagnostics.finish();
            if(!library) {
                std::vector<std::pair<Unit*,const Function*>> candidates;
                for(const auto& [id,unit]:capture.graph.parsed)for(const auto* functions:{&unit->functions,&unit->generic_functions})for(const auto& f:*functions)if(f.token.text=="main" && !f.receiver && !f.anonymous) {
                    if(f.asynchronous || !f.generics.empty() || !f.parameters.empty() || (f.result!=Type::none && f.result!=Type::i32)) {
                        try {error(unit->source,f.token,Code::project_entry,"Automatic project main must be synchronous, nongeneric, parameterless and return no result or exact i32.","Correct this declaration or use an explicit project entry.");}catch(const CompilationError& failure){capture.diagnostics.add(failure);}
                    } else candidates.emplace_back(unit.get(),&f);
                }
                capture.diagnostics.finish();
                std::sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){return std::tie(a.first->identity,a.second->token.offset)<std::tie(b.first->identity,b.second->token.offset);});
                if(candidates.empty())project_failure(capture.manifest->source(),Code::project_entry,"No eligible main() in application project.","Add a valid main() in the admitted source roots or set project.entry/--entry for an explicit script or source.");
                if(candidates.size()!=1) {
                    std::vector<RelatedLocation> locations;
                    for(std::size_t n=0;n<std::min<std::size_t>(candidates.size(),128);++n) {
                        auto location=span(candidates[n].first->source,candidates[n].second->token);location.module_identity=candidates[n].first->identity;
                        locations.push_back({std::move(location),"Candidate "+candidates[n].first->identity});
                    }
                    auto message="Ambiguous project entry: "+std::to_string(candidates.size())+" eligible main() declarations.";
                    if(candidates.size()>128)message+=" Locations truncated to 128; all candidates were considered.";
                    error(capture.manifest->source(),{TokenKind::end,"",0},Code::project_entry,message,"Set project.entry/--entry or narrow sources.roots/exclude.",std::move(locations));
                }
                entry=candidates[0].first->identity;
            }
        } else {
            entry="/"+*explicit_entry;
            if(explicit_entry->empty() || explicit_entry->front()=='/' || entry.find('\\')!=std::string::npos)project_failure(capture.anchor,Code::project_path,"Explicit entry must be a portable manifest-relative .void path.");
            try {require_module_identity(entry);}
            catch(const std::invalid_argument&) {project_failure(capture.manifest->source(),Code::project_path,"Explicit entry must be a canonical portable manifest-relative .void path.","Use an exact relative path without ., .., drive prefixes or reserved components; no fallback is attempted.");}
        }
        if(library)for(const auto& id:seeds)capture.graph.load(capture.from,{TokenKind::path,id,0});
        else capture.graph.load(capture.from,{TokenKind::path,entry,0});
        capture.diagnostics.finish();
        std::shared_ptr<Unit> library_scope;
        if(library) {library_scope=std::make_shared<Unit>(capture.manifest->source(),"");validate_graph(capture.graph.units,"",capture.diagnostics,library_scope.get());}
        else validate_graph(capture.graph.units,entry,capture.diagnostics);
        // Recapture membership, boundary observations, spelling/aliases and bytes.
        // No second source parse, no OS-transaction claim, and no stale success.
        // Compare observations before downstream validation can reinterpret new
        // membership/bytes. Failed admission/read operations retain their own
        // diagnostics; an exception alone is not evidence of a changed snapshot.
        ProjectCapture fresh(root,sources,manifests,&capture);
        fresh.changed(automatic && fresh.inventory()!=seeds);
        for(const auto& [id,unit]:capture.graph.units)fresh.changed(fresh.recapture_source(id).text()!=unit->source.text());
        fresh.changed(fresh.manifest->source().text()!=capture.manifest->source().text() || fresh.root!=capture.root || fresh.boundaries!=capture.boundaries || fresh.listings!=capture.listings || fresh.sources.size()!=capture.sources.size());
        for(const auto& [id,source]:capture.sources) {
            auto found=fresh.sources.find(id);
            fresh.changed(found==fresh.sources.end() || found->second.text()!=source.text());
        }
        result.project=ProjectAnalysisAccess::build(capture,seeds,entry,automatic,std::move(library_scope),authorize_native);result.diagnostics=capture.diagnostics.warnings();retain();return result;
    } catch(const CompilationError& failure) {result.diagnostics=failure.diagnostics();retain();return result;}
      catch(const std::filesystem::filesystem_error& failure) {Diagnostic d{Code::project_boundary};d.primary={std::make_shared<const SourceFile>(path_utf8(root),""),{},0,0};d.description=failure.what();d.hint="Check the selected project root, permissions and exact path spelling.";result.diagnostics.push_back(std::move(d));retain();return result;}
      catch(const std::invalid_argument& failure) {Diagnostic d{Code::project_path};d.primary={std::make_shared<const SourceFile>(path_utf8(root),""),{},0,0};d.description=failure.what();d.hint="Use exact canonical root-relative source identities and a manifest-relative .void entry.";result.diagnostics.push_back(std::move(d));retain();return result;}
}

ProjectAnalysisResult analyze_manifest_project(const std::filesystem::path& root,const std::optional<std::string>& entry,const SourceOverrides& sources,const ManifestOverrides& manifests,bool authorize_native) {
    return analyze_manifest_project_impl(root,entry,sources,manifests,nullptr,authorize_native);
}

static std::vector<Diagnostic> check_project_documents_impl(const std::filesystem::path& root,const SourceOverrides& sources,const ManifestOverrides& manifests,const std::vector<std::string>& documents,bool read_physical) {
    std::vector<Diagnostic> records;
    try {
        ProjectCapture capture(root,sources,manifests);
        for(const auto& id:documents) {
            try {
                require_module_identity(id);
                auto path=capture.graph.resolve(capture.from,{TokenKind::path,id,0}).second;
                // Preflight supplies a virtual name for an unsaved document.
                // An existing host file must still be readable.
                if(read_physical) {
                    std::error_code ec;
                    if(std::filesystem::exists(path,ec) && !std::ifstream(path,std::ios::binary))
                        throw SourceReadError(path_utf8(path),"Cannot read admitted project source.");
                    if(ec)throw SourceReadError(path_utf8(path),"Cannot inspect admitted project source.");
                }
            }
            catch(const CompilationError& failure) {
                for(auto diagnostic:failure.diagnostics()) {
                    auto overlay=sources.find(id);auto source=overlay!=sources.end() && overlay->second?*overlay->second:SourceFile(path_utf8(capture.root/utf8_path(id).relative_path()),"");
                    diagnostic.primary={std::make_shared<const SourceFile>(std::move(source)),id,0,0};records.push_back(std::move(diagnostic));
                    if(records.size()==project::manifest_diagnostic_limit)return records;
                }
            }
        }
    }catch(const CompilationError& failure){records=failure.diagnostics();for(auto& d:records)if(d.primary.module_identity.empty())d.primary.module_identity="/void.project.toml";}
    catch(const std::filesystem::filesystem_error& failure){Diagnostic d(Code::project_boundary);d.primary={std::make_shared<const SourceFile>(path_utf8(root/"void.project.toml"),""),"/void.project.toml",0,0};d.description=failure.what();records.push_back(std::move(d));}
    return records;
}
std::vector<Diagnostic> check_project_documents(const std::filesystem::path& root,const SourceOverrides& sources,const ManifestOverrides& manifests,const std::vector<std::string>& documents) {
    return check_project_documents_impl(root,sources,manifests,documents,false);
}
std::vector<std::filesystem::path> project_ancestor_directories(const std::filesystem::path&,const std::filesystem::path&);
project::CreationInventory project::discover_creation(const std::filesystem::path& workspace,const std::filesystem::path& active) {
    auto root=inspect_creation_root(workspace);
    CreationInventory result;result.root=root;
    for(const auto& directory:project_ancestor_directories(root.path,active))
        if(creation_manifest_present(directory)){result.manifest_present=true;return result;}
    for(auto directory=root.path.parent_path();!directory.empty();) {
        if(creation_manifest_present(directory)){result.manifest_present=true;return result;}
        auto parent=directory.parent_path();if(parent==directory)break;directory=std::move(parent);
    }
    try {
        const SourceOverrides sources;
        const ManifestOverrides manifests;
        ProjectCapture inventory(root.path,sources,manifests,nullptr,true);
        auto identities=inventory.inventory();
        ProjectCapture again(root.path,sources,manifests,&inventory,true);
        if(again.inventory()!=identities || again.listings!=inventory.listings || again.boundaries!=inventory.boundaries ||
            inspect_creation_root(root.path).identity!=root.identity)return result;
        result.eligible_count=identities.size();
        std::size_t bytes=0;
        for(const auto& id:identities)bytes+=id.size();
        if(identities.size()>16 || bytes>8192)return result;
        result.sources.assign(identities.begin(),identities.end());
        result.complete=true;
    } catch(const CompilationError&) {
        // An incomplete observation cannot authorize an automatic offer.
    } catch(const std::filesystem::filesystem_error&) {}
    return result;
}
ProjectPreflight discover_project_context(const std::filesystem::path& workspace,const std::filesystem::path& active,const std::filesystem::path& selected_root,const std::filesystem::path& selected_entry) {
    ProjectPreflight result;auto root=selected_root;
    if(root.empty() && selected_entry.empty() && !workspace.empty())
        for(const auto& directory:project_ancestor_directories(workspace,active))if(project_manifest_present(directory)){root=directory;break;}
    if(root.empty()) {
        auto legacy=discover_project(workspace,active,selected_entry);
        result.root=legacy.root;result.entry=legacy.entry;result.standalone=legacy.standalone;return result;
    }
    std::optional<std::string> entry;
    if(!selected_entry.empty())entry=path_utf8(selected_entry);
    result.manifest_mode=true;result.analysis=analyze_manifest_project(root,entry);
    result.root=result.analysis.state->root;
    if(result.analysis.manifest && !active.empty()) {
        SourceFile active_source(path_utf8(active),"");
        std::string identity;
        std::vector<Diagnostic> failures;
        try {
            auto relative=std::filesystem::absolute(active).lexically_relative(result.root);
            if(relative.empty() || *relative.begin()=="..")
                project_failure(active_source,Code::project_boundary,"Active document is outside the pinned project boundary.");
            auto id="/"+path_utf8(relative);
            if(id.ends_with("/void.project.toml")) {
                require_manifest_identity(id);
                identity=id;
                if(id!="/void.project.toml")
                    project_failure(active_source,Code::project_boundary,"Active manifest is outside the pinned project boundary: '"+id+"'.");
            }
            else {
                require_module_identity(id);
                SourceOverrides overlay{{id,active_source}};
                ManifestOverrides manifest{{"/void.project.toml",result.analysis.manifest->source()}};
                failures=check_project_documents_impl(root,overlay,manifest,{id},true);
            }
        }catch(const CompilationError& failure) {
            failures=failure.diagnostics();
            for(auto& diagnostic:failures)diagnostic.primary={std::make_shared<const SourceFile>(active_source),identity,0,0};
        }catch(const std::filesystem::filesystem_error& failure) {
            Diagnostic diagnostic(Code::project_boundary);
            diagnostic.primary={std::make_shared<const SourceFile>(active_source),{},0,0};
            diagnostic.description=failure.what();failures.push_back(std::move(diagnostic));
        }catch(const std::invalid_argument& failure) {
            Diagnostic diagnostic(Code::project_path);
            diagnostic.primary={std::make_shared<const SourceFile>(active_source),{},0,0};
            diagnostic.description=failure.what();failures.push_back(std::move(diagnostic));
        }
        if(!failures.empty()) {
            for(auto& diagnostic:failures)result.analysis.diagnostics.push_back(std::move(diagnostic));
            result.analysis.project.reset();
            auto state=std::make_shared<ProjectState>(*result.analysis.state);state->validated=false;result.analysis.state=std::move(state);
        }
    }
    if(result.analysis.project)result.entry=result.analysis.project->entry();
    return result;
}
