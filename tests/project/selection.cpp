#include "void/frontend.hpp"
#include "void/path.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <atomic>
#include <exception>
#ifdef __linux__
#include <sys/inotify.h>
#include <poll.h>
#include <unistd.h>
#endif
namespace {
namespace fs=std::filesystem;
using Code=vd::DiagnosticCode;
std::size_t checks=0;
void check(bool v,const std::string& what){++checks;if(!v)throw std::runtime_error(what);}
const std::string app="schema=1\n[project]\nid='org.example.sources'\nname='Example'\nversion='1.0.0'\nkind='application'\n[application]\nid='org.example.app'\n[sources]\nroots=['src']\n";
const std::string lib="schema=1\n[project]\nid='org.example.sources'\nname='Example'\nversion='1.0.0'\nkind='library'\n[package]\nid='org.example.library'\nname='example'\n[sources]\nroots=['src']\n";
const std::string main_source="func main() -> i32:\n    return 23\n";
struct Fixture {
 fs::path root;
 Fixture(){root=fs::temp_directory_path()/vd::utf8_path("VOID project 雪 "+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root/"src");write("void.project.toml",app);}
 ~Fixture(){std::error_code ec;fs::remove_all(root,ec);}
 void write(const std::string& name,const std::string& text){auto p=root/vd::utf8_path(name);fs::create_directories(p.parent_path());std::ofstream stream(p,std::ios::binary);stream<<text;check(bool(stream),"fixture write");}
 auto analyze(const std::optional<std::string>& entry={},const vd::SourceOverrides& sources={},const vd::ManifestOverrides& manifests={}){return vd::analyze_manifest_project(root,entry,sources,manifests);}
 auto good(const std::optional<std::string>& entry={},const vd::SourceOverrides& sources={},const vd::ManifestOverrides& manifests={}){auto r=analyze(entry,sources,manifests);if(!r.succeeded())throw vd::CompilationError(r.diagnostics);check(bool(r.project),"success snapshot");return r.project;}
 auto bad(Code code,const std::optional<std::string>& entry={},const vd::SourceOverrides& sources={},const vd::ManifestOverrides& manifests={}){auto r=analyze(entry,sources,manifests);check(!r.succeeded() && !r.project,"failure cannot publish a project/entry");bool found=false;for(const auto& d:r.diagnostics){found|=d.code==code;check(d.primary.source && d.primary.begin<=d.primary.end && d.primary.end<=d.primary.source->text().size(),"owned bounded diagnostic");}std::string details;for(const auto& d:r.diagnostics)details+=vd::render_diagnostic(d);check(found,"expected "+vd::diagnostic_code(code)+"; actual diagnostics:\n"+details);return r;}
};
void selection_and_snapshots(){
 Fixture f;f.write("src/features/launcher.void",main_source);f.write("src/unrelated.void","func unused() -> i32:\n    return false\n");
 auto p=f.good();check(p->program()->entry_identity()=="/src/features/launcher.void","deep any-name entry");check(p->root()==fs::canonical(f.root),"manifest root");check(p->discovery_sources().size()==2 && p->execution_sources().size()==1,"D differs from E");
 check(&p->discovery_sources().at("/src/features/launcher.void").text()==&p->execution_sources().at("/src/features/launcher.void").text(),"D/E share captured snapshot");
 auto old=p->selection_key();f.write("src/second.void",main_source);auto ambiguous=f.bad(Code::project_entry);auto again=f.analyze();check(vd::render_diagnostic(ambiguous.diagnostics[0])==vd::render_diagnostic(again.diagnostics[0]),"deterministic ambiguity");check(ambiguous.diagnostics[0].related.size()==2 && ambiguous.diagnostics[0].related[0].span.module_identity=="/src/features/launcher.void" && ambiguous.diagnostics[0].related[0].span.begin==5 && ambiguous.diagnostics[0].related[0].span.end==9,"exact canonical ambiguity source locations");
 check(p->discovery_sources().size()==2 && p->program()->entry_identity()=="/src/features/launcher.void","returned snapshot independent of later disk changes");
 auto explicit_p=f.good("src/features/launcher.void");check(explicit_p->discovery_sources().empty() && explicit_p->selection_key()!=old,"explicit mode distinct without D");
 f.bad(Code::import_read,"src/missing.void");f.bad(Code::project_path,"../outside.void");f.bad(Code::project_path,"/src/features/launcher.void");
 fs::remove(f.root/"src/second.void");f.write("src/features/launcher.void","func main(value: i32) -> i32:\n    return value\n");f.bad(Code::project_entry);
 for(const auto& text:{std::string("async func main():\n    pass\n"),std::string("func main<t>():\n    pass\n"),std::string("func main() -> bool:\n    return false\n"),std::string("func main(value: i32 = 0):\n    pass\n")}){f.write("src/features/launcher.void",text);f.bad(Code::project_entry);}
 f.write("src/features/launcher.void",main_source);f.write("src/unrelated.void","func broken(:\n");f.bad(Code::identifier);f.good("src/features/launcher.void");
 f.write("void.project.toml","[=roject]");f.bad(Code::project_syntax);
}
void boundaries_and_overlays(){
 Fixture f;f.write("src/app.void",main_source);f.write("src/child/void.project.toml","invalid child remains boundary");f.write("src/child/second.void",main_source);f.good();f.bad(Code::project_boundary,"src/child/second.void");
 f.write("src/app.void","use \"/src/child/second.void\"\n"+main_source);f.bad(Code::project_boundary);f.write("src/app.void",main_source);
 vd::SourceOverrides overlays;overlays.emplace("/src/new/deep/game.void",vd::SourceFile("dirty game",main_source));f.bad(Code::project_entry,{},overlays);
 overlays.insert_or_assign("/src/app.void",std::nullopt);auto p=f.good({},overlays);check(p->program()->entry_identity()=="/src/new/deep/game.void","virtual parents and tombstone");
 overlays.insert_or_assign("/src/new/deep/game.void",vd::SourceFile("dirty signature","func main(value: i32):\n    pass\n"));f.bad(Code::project_entry,{},overlays);
 vd::ManifestOverrides manifests;manifests.emplace("/src/void.project.toml",vd::SourceFile("new child","invalid"));f.bad(Code::project_boundary,{}, {},manifests);f.bad(Code::project_boundary,"src/app.void",{},manifests);
 manifests.clear();manifests.emplace("/void.project.toml",std::nullopt);f.bad(Code::project_boundary,{}, {},manifests);
 manifests.clear();manifests.emplace("/src/child/void.project.toml",std::nullopt);f.bad(Code::project_entry,{}, {},manifests);
 vd::SourceOverrides invalid;invalid.emplace("/@stdlib/os.void",vd::SourceFile("external",""));f.bad(Code::project_path,{},invalid);
 f.write("src/other.VOID","not source syntax");f.good();
 std::error_code ec;fs::create_hard_link(f.root/"src/app.void",f.root/"src/alias.void",ec);
 if(!ec){f.bad(Code::import_path);vd::SourceOverrides dirty;dirty.emplace("/src/alias.void",vd::SourceFile("dirty",main_source));f.bad(Code::import_path,{},dirty);fs::remove(f.root/"src/alias.void");
   Fixture outside;outside.write("source.void",main_source);fs::remove(f.root/"src/app.void");fs::create_hard_link(outside.root/"source.void",f.root/"src/app.void");check(vd::analyze_project(f.root/"src/app.void").succeeded(),"legacy single-identity hard-link policy");f.good();fs::remove(f.root/"src/app.void");f.write("src/app.void",main_source);
   fs::create_hard_link(f.root/"void.project.toml",outside.root/"manifest-alias.toml");f.bad(Code::project_boundary);fs::remove(outside.root/"manifest-alias.toml");
 }
 else std::cout<<"UNQUALIFIED: native hard-link fixture "<<ec.message()<<'\n';
 fs::create_symlink(f.root/"src/app.void",f.root/"src/link.void",ec);
 if(!ec){auto r=f.analyze();check(!r.succeeded(),"source symlink rejected");fs::remove(f.root/"src/link.void");}else std::cout<<"UNQUALIFIED: native symlink privilege "<<ec.message()<<'\n';
 fs::create_directory_symlink(f.root/"src/child",f.root/"src/linkdir",ec);
 if(!ec){f.bad(Code::project_boundary);fs::remove(f.root/"src/linkdir");}
#ifdef __linux__
 f.write("src/APP.VOID",main_source);f.bad(Code::import_path);fs::remove(f.root/"src/APP.VOID");
 f.write("src/closed.void","");fs::permissions(f.root/"src/closed.void",fs::perms::none);
 if(geteuid()!=0){try{f.bad(Code::import_read);}catch(...){fs::permissions(f.root/"src/closed.void",fs::perms::owner_all);throw;}}
 fs::permissions(f.root/"src/closed.void",fs::perms::owner_all);fs::remove(f.root/"src/closed.void");
 Fixture manifest_alias;fs::rename(manifest_alias.root/"void.project.toml",manifest_alias.root/"manifest-held.toml");fs::create_symlink(manifest_alias.root/"manifest-held.toml",manifest_alias.root/"void.project.toml");manifest_alias.bad(Code::project_boundary);
#endif
 f.write("src/wrong/void.Project.toml","invalid");f.bad(Code::project_boundary);
}
void library_scope(){
 Fixture f;f.write("void.project.toml",lib);auto empty=f.good();check(!empty->entry() && !empty->program() && empty->execution_sources().empty(),"empty library has no fabricated executable");
 f.write("src/a.void","const hidden_root = 7\nfunc main() -> i32:\n    return hidden_root\n");f.good();
 f.write("src/b.void","func value() -> i32:\n    return hidden_root\n");f.bad(Code::unknown_identifier);
 f.write("src/b.void","use \"./a.void\" as a\nfunc value() -> i32:\n    return a.hidden_root\n");f.good();
 f.write("src/b.void","print(\"must not run\")\n");f.bad(Code::entry);f.bad(Code::project_entry,"src/a.void");
}
void physical_root_policy(){
 Fixture f;auto policy=app;const std::string before="roots=['src']";policy.replace(policy.find(before),before.size(),"roots=['future']");f.write("void.project.toml",policy);
 vd::SourceOverrides virtual_root;virtual_root.emplace("/future/app.void",vd::SourceFile("virtual",main_source));f.bad(Code::project_boundary,{},virtual_root);
#ifdef __linux__
 Fixture local,remote;local.write("sibling/void.project.toml",app);local.write("sibling/src/app.void","func main() -> i32:\n    return 99\n");remote.write("sibling/void.project.toml",app);remote.write("sibling/src/app.void",main_source);fs::create_directories(remote.root/"target");fs::create_directory_symlink(remote.root/"target",local.root/"ancestor");
 auto requested=local.root/"ancestor/../sibling";auto selected=vd::analyze_manifest_project(requested);check(selected.succeeded() && selected.project->root()==fs::canonical(requested),"root .. must retain physical meaning across permitted ancestor aliases");
#endif
}
void non_entry_names(){
 Fixture f;f.write("src/method.void","value struct box:\n    item: i32\nfunc (self box) main() -> i32:\n    return self.item\n");f.bad(Code::project_entry);
 f.write("src/method.void","func outer():\n    var main = func() -> i32:\n        return 23\n");f.bad(Code::project_entry);
}
void import_depth_admission(){
 Fixture f;
 for(unsigned i=0;i<128;++i)f.write("src/module"+std::to_string(i)+".void","use \"./module"+std::to_string(i+1)+".void\"\n");
 f.write("src/module128.void",std::string(1,char(0xff)));
 auto legacy=vd::analyze_project(f.root/"src/module0.void");check(!legacy.succeeded(),"legacy import depth failure");bool guarded=false;
 for(const auto& d:legacy.diagnostics){guarded|=d.code==Code::nesting;check(d.code!=Code::utf8,"over-depth source must not be opened/parsed");}check(guarded,"legacy depth guard before source read");
 f.bad(Code::nesting,"src/module0.void");
}
void tombstone_safety(){
 // Exercise the shared resolve guard through explicit parse/load and discovery
 // promotion into E. Disk bytes remain present and must never be a fallback.
 Fixture f;f.write("src/app.void",main_source);
 vd::SourceOverrides deleted{{"/src/app.void",std::nullopt}};
 auto selected=f.bad(Code::import_read,"src/app.void",deleted);
 check(selected.diagnostics[0].description.find("explicitly unavailable")!=std::string::npos,"explicit tombstone reaches resolver guard");
 auto policy=app;policy.insert(policy.find("[application]"),"entry='src/app.void'\n");f.write("void.project.toml",policy);f.bad(Code::import_read,{},deleted);
 f.write("void.project.toml",app);f.bad(Code::project_entry,{},deleted);
 f.write("src/app.void","use \"./module.void\"\n"+main_source);f.write("src/module.void","func value():\n    pass\n");
 deleted={{"/src/module.void",std::nullopt}};f.bad(Code::import_read,{},deleted);f.bad(Code::import_read,"src/app.void",deleted);
 auto legacy=vd::analyze_project(f.root/"src/app.void",vd::SourceOverrides{{"/module.void",std::nullopt}});check(!legacy.succeeded(),"legacy dirty import tombstone");check(legacy.diagnostics[0].code==Code::import_read,"legacy uses same missing-source diagnostic");
 policy=app;policy+="exclude=['src/module.void']\n";f.write("void.project.toml",policy);f.bad(Code::import_read,{},deleted);
 f.write("void.project.toml",app);f.write("src/module.void","use \"./app.void\"\n");f.bad(Code::import_read,{},deleted);
 // A tombstone on the already loaded entry also prevents a cycle from reading
 // cached/disk bytes. A fresh revision, not concurrent mutation of the map.
 f.good();deleted={{"/src/app.void",std::nullopt}};f.bad(Code::import_read,"src/module.void",deleted);
 deleted.clear();auto retained=f.good();f.write("src/module.void","invalid disk after snapshot");
 deleted={{"/src/module.void",std::nullopt}};auto failure=f.bad(Code::import_read,"src/app.void",deleted);
 check(retained->execution_sources().at("/src/module.void").text()=="use \"./app.void\"\n","returned snapshot survives later tombstone revision");
 fs::remove_all(f.root);deleted.clear();
 check(vd::render_diagnostic(failure.diagnostics[0]).find("explicitly unavailable")!=std::string::npos,"missing-source diagnostic owns its source after fixture/overlay destruction");
}
#ifdef __linux__
template<class Mutation> void during_capture(Fixture& f,Mutation mutate,Code expected){
 // Lexing this captured source leaves time for the notified mutation to finish
 // before recapture. No public test hook or mutable overlay map is needed.
 f.write("src/zz-slow.void",std::string(2*1024*1024,' '));
 f.write("libs/tool/tool.void","func value():\n    pass\n");
 f.write("src/app.void","use \"/libs/tool/tool.void\"\nuse \"./zz-slow.void\"\n"+main_source);
 f.write("void.project.toml",app+"exclude=['src/zz-slow.void']\n");
 int fd=inotify_init1(IN_CLOEXEC);check(fd>=0,"recapture inotify admission");
 int watch=inotify_add_watch(fd,(f.root/"src/zz-slow.void").c_str(),IN_ACCESS);check(watch>=0,"recapture read notification");
 std::atomic<bool> changed=false;std::exception_ptr problem;
 std::thread writer([&]{try{pollfd wait{fd,POLLIN,0};if(poll(&wait,1,5000)>0){char data[4096];if(read(fd,data,sizeof data)>0){mutate();changed=true;}}}catch(...){problem=std::current_exception();}});
 auto result=f.analyze();writer.join();close(fd);if(problem)std::rethrow_exception(problem);
 check(changed && !result.succeeded(),"recapture mutation completed and failed closed");
 std::string details;bool found=false;
 for(const auto& d:result.diagnostics){found|=d.code==expected;details+=vd::render_diagnostic(d);check(d.primary.source && d.primary.end<=d.primary.source->text().size(),"recapture owns bounded diagnostic");}
 check(found,"recapture expected "+vd::diagnostic_code(expected)+"; got:\n"+details);
}
void recapture_classification(){
 {Fixture f;f.write("src/app.void",main_source);during_capture(f,[&]{f.write("src/app.void",main_source+"# changed\n");},Code::project_changed);}
 {Fixture f;f.write("src/app.void",main_source);during_capture(f,[&]{f.write("void.project.toml","[=roject]");},Code::project_changed);}
 {Fixture f;f.write("src/app.void",main_source);during_capture(f,[&]{fs::remove(f.root/"src/app.void");},Code::project_changed);}
 {Fixture f;f.write("src/app.void",main_source);during_capture(f,[&]{f.write("libs/tool/tool.void","func value():\n    pass\n# changed E-only bytes\n");},Code::project_changed);}
 {Fixture f;f.write("src/app.void",main_source);Fixture external;external.write("target.void",main_source);
  during_capture(f,[&]{fs::remove(f.root/"src/app.void");fs::create_symlink(external.root/"target.void",f.root/"src/app.void");},Code::project_boundary);}
 if(geteuid()!=0){Fixture f;f.write("src/app.void",main_source);
  try{during_capture(f,[&]{fs::permissions(f.root/"src/app.void",fs::perms::none);},Code::import_read);}catch(...){fs::permissions(f.root/"src/app.void",fs::perms::owner_all);throw;}
  fs::permissions(f.root/"src/app.void",fs::perms::owner_all);
  {Fixture manifest;manifest.write("src/app.void",main_source);
   try{during_capture(manifest,[&]{fs::permissions(manifest.root/"void.project.toml",fs::perms::none);},Code::import_read);}catch(...){fs::permissions(manifest.root/"void.project.toml",fs::perms::owner_all);throw;}
   fs::permissions(manifest.root/"void.project.toml",fs::perms::owner_all);
  }
  {Fixture directory;directory.write("src/app.void",main_source);
   try{during_capture(directory,[&]{fs::permissions(directory.root/"src",fs::perms::none);},Code::project_boundary);}catch(...){fs::permissions(directory.root/"src",fs::perms::owner_all);throw;}
   fs::permissions(directory.root/"src",fs::perms::owner_all);
  }
 }else std::cout<<"UNQUALIFIED: root cannot exercise source permission denial\n";
}
#endif
void bounds_and_changes(){
 Fixture f;f.write("src/app.void",main_source);vd::SourceOverrides overlays;
 for(std::size_t i=0;i<4095;++i)overlays.emplace("/src/file"+std::to_string(i)+".void",vd::SourceFile("empty",""));
 auto p=f.good({},overlays);check(p->discovery_sources().size()==4096,"4096-file inclusive bound");
 overlays.emplace("/src/overflow.void",vd::SourceFile("overflow",""));f.bad(Code::limit,{},overlays);
 overlays.clear();std::string id="/src";for(unsigned i=0;i<64;++i)id+="/d";overlays.emplace(id+"/too_deep.void",vd::SourceFile("deep",""));f.bad(Code::limit,{},overlays);
 const std::string with_import="use \"/libs/tool/tool.void\"\n"+main_source;f.write("src/app.void",with_import);f.write("libs/tool/tool.void","func value():\n    pass\n");
 overlays.clear();overlays.emplace("/src/huge.void",vd::SourceFile("huge",std::string(64*1024*1024-with_import.size(),' ')));auto exact_bytes=f.good({},overlays);check(exact_bytes->discovery_sources().size()==2 && exact_bytes->execution_sources().size()==2,"exact 64 MiB D bound permits additional E-only imports");exact_bytes.reset();
 overlays.insert_or_assign("/src/huge.void",vd::SourceFile("huge",std::string(64*1024*1024,' ')));f.bad(Code::limit,{},overlays);
 f.write("src/app.void",main_source);
 f.write("src/slow.void",std::string(2*1024*1024,' '));f.write("src/changing.void","# "+std::string(32,'0')+"\n");std::atomic<bool> stop=false;std::atomic<unsigned> writes=0;
 std::thread writer([&]{while(!stop){auto digits=std::to_string(++writes);std::fstream s(f.root/"src/changing.void",std::ios::in|std::ios::out|std::ios::binary);s<<"# "<<std::string(32-digits.size(),'0')<<digits<<'\n';}});
 bool rejected=false;
 for(unsigned i=0;i<12 && !rejected;++i){auto r=f.analyze();for(const auto& d:r.diagnostics)rejected|=d.code==Code::project_changed || d.code==Code::project_boundary;}
 stop=true;writer.join();check(writes>0 && rejected,"concurrent membership mutation cannot be accepted as stable");
 Fixture entries;entries.write("src/app.void",main_source);
 for(unsigned i=0;i<16381;++i){std::ofstream s(entries.root/"src"/("entry"+std::to_string(i)+".txt"));if(!s)throw std::runtime_error("entry bound fixture creation");}
 entries.good();entries.write("src/one-more.txt","");entries.bad(Code::limit);
#ifdef __linux__
 // A real read event changes a previously observed child boundary after capture
 // starts, before acceptance. The hook is the OS notification, not a parser seam.
 Fixture boundary;boundary.write("src/app.void",main_source);boundary.write("src/race-child/helper.void","");boundary.write("src/slow.void",std::string(2*1024*1024,' '));
 int fd=inotify_init1(IN_CLOEXEC);check(fd>=0,"inotify admission");int watch=inotify_add_watch(fd,(boundary.root/"src/slow.void").c_str(),IN_ACCESS);check(watch>=0,"read notification");std::atomic<bool> changed=false;
 std::thread child([&]{pollfd wait{fd,POLLIN,0};if(poll(&wait,1,5000)>0){char data[4096];if(read(fd,data,sizeof data)>0){std::ofstream stream(boundary.root/"src/race-child/void.project.toml");stream<<"invalid child boundary";stream.close();changed=bool(stream);}}});
 auto changed_result=boundary.analyze();child.join();close(fd);bool retry=false;for(const auto& d:changed_result.diagnostics)retry|=d.code==Code::project_changed;check(changed && !changed_result.succeeded() && retry,"concurrent child boundary causes exact retry diagnostic");
#endif
}
}
int main(){try{selection_and_snapshots();boundaries_and_overlays();library_scope();physical_root_policy();non_entry_names();import_depth_admission();tombstone_safety();
#ifdef __linux__
recapture_classification();
#endif
bounds_and_changes();std::cout<<"project_selection: "<<checks<<" checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
