#include "void/tooling.hpp"
#include "void/backend.hpp"
#include "void/path.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
namespace fs=std::filesystem;
std::size_t checks=0;
void check(bool value,const std::string& message){++checks;if(!value)throw std::runtime_error(message);}
const std::string manifest="schema=1\n[project]\nid='org.test.sources'\nname='Sources'\nversion='1.0.0'\nkind='application'\n[application]\nid='org.test.app'\n[sources]\nroots=['src']\n";
const std::string source="func main() -> i32:\n    return 23\n";
struct Fixture {
 fs::path root=fs::temp_directory_path()/vd::utf8_path("VOID S3 sessions 雪 "+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 Fixture(){fs::create_directories(root/"src");write("void.project.toml",manifest);}
 ~Fixture(){std::error_code ec;fs::remove_all(root,ec);}
 void write(const std::string& id,const std::string& text){auto path=root/vd::utf8_path(id);fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);out<<text;check(bool(out),"fixture write");}
};
auto analyze(vd::ToolingSession& session,bool success){auto result=vd::ToolingSession::analyze(session.select());check(session.accept(result),"fresh result accepted");check(result->succeeded()==success,"expected session status");check(bool(session.current_index())==success,"no stale semantic index");if(!success)check(!result->result().program && !result->semantic_analysis(),"failed selection cannot return executable/semantic analysis");return result;}
void sessions(){
 Fixture f;vd::ToolingSession s(vd::ProjectSession{f.root});
 auto empty=analyze(s,false);check(empty->project()->manifest && empty->project()->state->discovery_complete && !empty->project()->state->entry,"no-main error-bearing state");
 s.open("/src/game.void",1,source);auto first=analyze(s,true);check(first->project()->project->entry()==f.root/"src/game.void","dirty new source selected");
 auto retained=s.select();s.open("/src/second.void",1,source);auto ambiguous=analyze(s,false);check(ambiguous->project()->state->discovery.size()==2,"ambiguous D retained");
 check(!s.accept(vd::ToolingSession::analyze(retained)),"delayed earlier success rejected");check(s.current_analysis()==ambiguous && s.last_success()==first,"failed current versus explicit historical success");
 s.unavailable("/src/second.void",2);auto single=analyze(s,true);check(single->project()->project->entry()==first->project()->project->entry(),"tombstone changes inventory");
 s.open("/void.project.toml",1,"# é😀\r\n[=roject]\r\n");auto malformed=analyze(s,false);auto diagnostic=malformed->result().diagnostics.at(0);check(diagnostic.primary.module_identity=="/void.project.toml","manifest has separate canonical owner");check(!malformed->project()->manifest && malformed->project()->state->discovery.empty(),"invalid TOML traverses no source policy");
 s.replace("/void.project.toml",2,manifest);analyze(s,true);check(diagnostic.primary.source->text().find("[=roject]")!=std::string::npos,"diagnostic owns removed dirty manifest bytes");
 auto selected=s.select();s.refresh();check(!s.accept(vd::ToolingSession::analyze(selected)),"disk invalidation rejects delayed selection");analyze(s,true);
 vd::ToolingSession other(vd::ProjectSession{f.root});check(!other.accept(vd::ToolingSession::analyze(s.select())),"cross-session token rejection");
 s.open("/src/child/void.project.toml",1,"malformed boundary");s.open("/src/child/new.void",1,"func helper():\n    pass\n");analyze(s,false);
 s.unavailable("/src/child/void.project.toml",2);analyze(s,true);s.close("/src/child/new.void",2);s.close("/src/child/void.project.toml",3);analyze(s,true);
 s.unavailable("/void.project.toml",3);analyze(s,false);s.close("/void.project.toml",4);analyze(s,true);
 // A root change is a different session; explicit reselection never rebases dirty paths.
 s.reselect("src/game.void");check(analyze(s,true)->project()->state->discovery.empty(),"explicit reselection skips D");check(!s.requires_reselection(),"deliberate selection accepted");
 s.reselect("src/missing.void");analyze(s,false);check(s.requires_reselection(),"invalid explicit entry never falls back");
 s.reselect();analyze(s,true);s.unavailable("/src/game.void",2);analyze(s,false);s.close("/src/game.void",3);analyze(s,false);
 check(first->index()->declaration("/src/game.void::main").has_value(),"historical metadata outlives all revisions");
}
void libraries(){
 Fixture f;auto policy=manifest;policy.replace(policy.find("kind='application'"),18,"kind='library'");policy.replace(policy.find("[application]"),std::string("[application]\nid='org.test.app'").size(),"[package]\nid='org.test.library'\nname='geometry'");f.write("void.project.toml",policy);
 vd::ToolingSession s(vd::ProjectSession{f.root});auto empty=analyze(s,true);check(!empty->result().program && !empty->project()->project->entry() && !empty->semantic_analysis()->executable(),"library has metadata without executable entry");
 bool rejected=false;try{(void)empty->semantic_analysis()->lower();}catch(const std::invalid_argument&){rejected=true;}check(rejected,"library metadata cannot lower IR");
 s.open("/src/a.void",1,"public func value() -> i32:\n    return 7\n");s.open("/src/b.void",1,"use \"./a.void\" as a\nfunc main() -> i32:\n    return a.value()\n");auto indexed=analyze(s,true);check(indexed->index()->definition_at("/src/b.void",std::string("use \"./a.void\" as a\nfunc main() -> i32:\n    return a.").size())->module=="/src/a.void","library import navigation uses declarationless root");
 s.replace("/src/b.void",2,"func read() -> i32:\n    return hidden\n");analyze(s,false);
}
void preflight_ownership(){
 vd::ProjectPreflight rejected;
 fs::path active;
 {
  Fixture f;f.write("src/main.void",source);
  f.write("src/child/void.project.toml","[=roject]");
  f.write("src/child/launcher.void",source);
  active=f.root/"src/child/launcher.void";
  rejected=vd::discover_project_context(f.root,active,f.root,"src/main.void");
  check(rejected.manifest_mode && rejected.root==f.root && rejected.analysis.manifest,"preflight retains pinned manifest metadata");
  check(!rejected.entry && !rejected.analysis.project && !rejected.analysis.state->validated,"rejected active document grants no validated selection");
 }
 const auto& diagnostic=rejected.analysis.diagnostics.at(0);
 check(rejected.analysis.diagnostics.size()==1 && diagnostic.code==vd::DiagnosticCode::project_boundary,"structured nested-project diagnostic retained");
 const auto& span=diagnostic.primary;
 check(span.source && span.source->path()==vd::path_utf8(active) && span.module_identity=="/src/child/launcher.void","diagnostic owns the active source identity after project removal");
 check(span.begin==0 && span.end==0 && span.source->text().empty(),"preflight owns an empty span without reading a denied child source");
}
void reload(){
 for(bool optimize:{false,true}) {
  Fixture f;const std::string body="var state = 0\nfunc main() -> i32:\n    state += 1\n    return state + ";f.write("src/game.void",body+"10\n");vd::ToolingSession s(vd::ProjectSession{f.root});auto before=analyze(s,true);
  vd::Jit jit;vd::ReloadSlot slot(jit.compile(*before->result().program,optimize));check(slot.invoke(0)==11,"original application generation");
  s.open("/src/game.void",1,body+"20\n");auto after=analyze(s,true);check(before->project()->project->selection_key()!=after->project()->project->selection_key(),"body changes selection/build provenance");check(before->project()->project->context_key()==after->project()->project->context_key(),"body retains semantic project context");check(s.invalidation().source_compatibility==vd::SourceCompatibility::exact_match && !s.requires_reselection(),"body remains source-compatible");check(jit.replace(slot,s,*after,optimize) && slot.invoke(0)==22,"actual body reload preserves global cells");
  s.open("/void.project.toml",1,manifest+"# comment-only revision\n");auto trivia=analyze(s,true);check(s.invalidation().source_compatibility==vd::SourceCompatibility::exact_match && !s.requires_reselection(),"manifest trivia is provenance, not semantic metadata");check(jit.replace(slot,s,*trivia,optimize),"comment-only context compatible");
  auto metadata=manifest;metadata.replace(metadata.find("name='Sources'"),14,"name='Changed'");s.replace("/void.project.toml",2,metadata);auto changed=analyze(s,true);check(s.requires_reselection() && s.invalidation().source_compatibility==vd::SourceCompatibility::restart_required,"semantic metadata requires reselection/restart");check(!jit.replace(slot,s,*changed,optimize),"old Runtime rejects changed project source context");
  s.replace("/src/game.void",2,body+"30\n");check(!jit.replace(slot,s,*after,optimize),"stale project generation cannot publish");analyze(s,true);
 }
}
void profiles(){
 auto parsed=vd::project::parse_manifest(vd::SourceFile("manifest",manifest));check(parsed.succeeded(),"profile schema");auto m=parsed.manifest;
 auto release=vd::project::resolve_profile(*m,{},false,false),debug=vd::project::resolve_profile(*m,vd::project::ProfileName::debug,false,false);
 check(release.optimize && !debug.optimize && release.diagnostics.empty() && debug.diagnostics.empty(),"schema-default profiles keep their optimization and stay quiet");
 check(release.debug_info==vd::project::DebugInfo::none && debug.debug_info==vd::project::DebugInfo::none,"implicit debug information mode");
 auto authored=vd::project::parse_manifest(vd::SourceFile("manifest",manifest+"[build.debug]\ndebug_info='full'\n[build.release]\ndebug_info='line'\n"));
 check(authored.succeeded(),"authored debug information modes");
 for(auto [name,mode,spelling]:{std::tuple{vd::project::ProfileName::release,vd::project::DebugInfo::line,"'line'"},
                                std::tuple{vd::project::ProfileName::debug,vd::project::DebugInfo::full,"'full'"}}) {
  const auto profile=vd::project::resolve_profile(*authored.manifest,name,false,false);
  check(profile.debug_info==mode && profile.diagnostics.size()==1,"explicit selected mode retained");
  const auto& warning=profile.diagnostics.front();
  check(warning.severity==vd::Severity::warning && warning.code==vd::DiagnosticCode::project_debug_info,"structured profile warning");
  check(warning.primary.source->text().substr(warning.primary.begin,warning.primary.end-warning.primary.begin)==spelling,"authored profile warning owns exact value span");
 }
 check(!vd::project::resolve_profile(*m,{},true,false).optimize,"explicit no-opt wins");check(vd::project::resolve_profile(*m,{},false,true).name==vd::project::ProfileName::debug,"implicit release yields to cache debug selection");
 bool rejected=false;try{(void)vd::project::resolve_profile(*m,vd::project::ProfileName::release,true,true);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"explicit release cache conflict survives no-opt");
 auto override=vd::project::parse_manifest(vd::SourceFile("dirty",manifest+"[build.debug]\noptimization='O2'\ndebug_info='none'\n[build.release]\noptimization='none'\ndebug_info='full'\n"));check(override.succeeded(),"profile overrides");
 auto selected=vd::project::resolve_profile(*override.manifest,vd::project::ProfileName::debug,false,false);check(selected.optimize && selected.diagnostics.empty(),"none quiet; unselected unsupported setting quiet");
 auto warning=vd::project::resolve_profile(*override.manifest,vd::project::ProfileName::release,false,false).diagnostics.at(0);check(warning.primary.source->text().substr(warning.primary.begin,warning.primary.end-warning.primary.begin)=="'full'","explicit profile warning retains exact value range");
 rejected=false;try{(void)vd::project::resolve_profile(*override.manifest,{},false,true);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"custom O2 debug conflicts with cache");check(!vd::project::resolve_profile(*override.manifest,{},true,true).optimize,"explicit no-opt resolves effective debug optimization");
}
}
int main(){try{sessions();libraries();preflight_ownership();reload();profiles();std::cout<<"project_sessions: "<<checks<<" checks passed\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
