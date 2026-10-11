#include "void/tooling.hpp"
#include "void/backend.hpp"
#include <fstream>
#include <stdexcept>
namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void write(const std::filesystem::path& p, const std::string& text) { std::ofstream out(p,std::ios::binary); out << text; check(bool(out),"write fixture"); }
template<class F> void rejects(F f) { bool failed=false; try { f(); } catch(const std::invalid_argument&) { failed=true; } check(failed,"invalid document transition accepted"); }
const std::string main_text="use \"/worker.void\" as worker\nuse \"/values.void\" as values\nfunc entry(x: i32) -> i32:\n    return worker.calculate(x) + values.bonus\n";
std::string worker(int n) { return "func calculate(x: i32) -> i32:\n    return x + " + std::to_string(n) + "\n"; }
auto run(vd::ToolingSession& s) { auto r=vd::ToolingSession::analyze(s.select()); check(s.accept(r),"fresh acceptance"); return r; }
}
void session_tests(const std::filesystem::path& root) {
    std::filesystem::create_directories(root);
    write(root/"main.void",main_text); write(root/"worker.void",worker(42)); write(root/"values.void","const bonus = 1\n");
    vd::ToolingSession s(root/"main.void");
    {
        struct RestoreDirectory { std::filesystem::path path=std::filesystem::current_path(); ~RestoreDirectory() { std::filesystem::current_path(path); } } restore;
        vd::ToolingSession relative(std::filesystem::relative(root/"main.void"));
        std::filesystem::current_path(root);
        check(run(relative)->result().succeeded(),"session root changed with working directory");
    }
    auto first=run(s); check(first->result().succeeded() && s.current_index(),"initial analysis");
    auto declaration=s.current_index()->declaration("/worker.void::calculate");
    auto refs=s.current_index()->references_to(declaration->identity); check(refs.size()==1,"initial references");
    check(s.current_index()->definition_at("/main.void",main_text.find("worker.calculate")+7)->identity==declaration->identity,"definition");
    s.open("/worker.void",1,worker(50));
    check(!s.current_index() && !s.is_current(*first),"old result served current");
    check(s.invalidation().requires_full_reanalysis && s.invalidation().source_compatibility==vd::SourceCompatibility::unknown,"unvalidated compatibility");
    auto selected=s.select(); auto delayed=vd::ToolingSession::analyze(selected);
    s.replace("/worker.void",2,worker(51));
    check(!s.accept(delayed),"delayed result accepted");
    check(!s.accept(vd::ToolingSession::analyze(selected)),"late execution of old selection accepted");
    auto body=run(s); check(s.invalidation().source_compatibility==vd::SourceCompatibility::exact_match,"body compatibility");
    check(s.invalidation().dependent_modules==std::vector<std::string>{"/main.void"},"import dependent planning");
    check(!s.accept(body),"duplicate result accepted");
    rejects([&]{s.replace("/worker.void",0,worker(52));});
    rejects([&]{s.replace("/worker.void",2,worker(52));}); rejects([&]{s.replace("/worker.void",1,worker(52));});
    s.replace("/worker.void",3,"# \xc3\xa9\xf0\x9f\x98\x80\r\nfunc calculate(x: i32) -> i32:\r\n    return missing\r\n");
    auto bad=run(s); check(!bad->result().succeeded() && !s.current_index() && s.last_success()==body,"failed analysis phantom success");
    const auto diagnostic=bad->result().diagnostics.front();
    check(diagnostic.primary.source->text().substr(diagnostic.primary.begin,7)=="missing","diagnostic original byte offset");
    auto location=diagnostic.primary.source->location(diagnostic.primary.begin); check(location.line==3 && location.column==12,"UTF8 CRLF location");
    check(s.invalidation().source_compatibility==vd::SourceCompatibility::unknown,"invalid comparison safety");
    check(!s.accept(delayed) && s.current_analysis()==bad,"stale success replaced latest failure");
    s.replace("/worker.void",4,worker(51)); auto repaired=run(s); check(repaired->result().succeeded(),"repair");
    check(diagnostic.primary.source->text().find("missing")!=std::string::npos,"old diagnostics lifetime");
    s.open("/values.void",1,"const bonus = 2\n");
    s.replace("/worker.void",5,worker(52)); auto multiple=run(s);
    check(multiple->result().succeeded() && s.invalidation().source_compatibility==vd::SourceCompatibility::restart_required,"multiple overlays constant restart");
    s.close("/values.void",2); s.close("/worker.void",6); auto disk=run(s);
    check(disk->result().program->compatibility_contract()==first->result().program->compatibility_contract(),"close disk fallback");
    rejects([&]{s.open("/worker.void",6,worker(42));});
    s.unavailable("/worker.void",7); auto deleted=run(s); check(!deleted->result().succeeded(),"deletion fell back to disk");
    check(declaration->source.source->text()==worker(42),"declaration lifetime after deletion");
    s.close("/worker.void",8); check(run(s)->result().succeeded(),"deletion close");
    s.open("/main.void",1,"use \"./new/../new/module.void\" as m\nfunc entry(x: i32) -> i32:\n    return m.calculate(x)\n");
    s.open("/new/module.void",1,worker(9)); auto added=run(s);
    check(added->result().succeeded() && s.invalidation().graph_changed && s.invalidation().added_modules==std::vector<std::string>{"/new/module.void"},"virtual graph discovery");
    s.replace("/new/module.void",2,"use \"/main.void\" as m\n"+worker(9)); check(run(s)->result().succeeded(),"virtual function cycle");
    s.open("/new/Module.void",1,worker(9)); check(!run(s)->result().succeeded(),"virtual case collision");
    s.close("/new/Module.void",2);
    rejects([&]{s.open("/new/./bad.void",1,worker(1));});
    rejects([&]{s.open("/../bad.void",1,worker(1));});
    s.replace("/main.void",2,"use \"/New/module.void\" as m\nfunc entry(x: i32) -> i32:\n    return m.calculate(x)\n");
    check(!run(s)->result().succeeded(),"virtual case mismatch");
    s.close("/main.void",3); s.close("/new/module.void",3); run(s);
    s.open("/worker.void",9,worker(42)); auto equal_before=s.select(); auto equal_old=vd::ToolingSession::analyze(equal_before);
    s.replace("/worker.void",10,worker(42)); check(!s.select().same_revision(equal_before) && !s.accept(equal_old),"equal bytes identity conflation");
    run(s);
    vd::ToolingSession other(root/"main.void"); check(!other.accept(s.latest_accepted()),"cross-session result accepted");
    auto old=s.latest_accepted(); s.refresh(); check(!s.current_index() && !s.accept(old),"disk refresh freshness"); run(s);
    for(unsigned n=11;n<51;++n) {
        s.replace("/worker.void",n,worker(42)); auto r=run(s);
        check(r->result().program->compatibility_contract()==first->result().program->compatibility_contract(),"deterministic transitions");
    }
    s.replace("/worker.void",51,"func calculate(x: i32) -> bool:\n    return true\n"); check(!run(s)->result().succeeded(),"signature type mismatch");
    s.replace("/worker.void",52,worker(42)); run(s);
    s.replace("/worker.void",53,"func calculate(x: i32, y: i32) -> i32:\n    return x + y\n");
    auto changed_call=main_text; changed_call.replace(changed_call.find("calculate(x)"),12,"calculate(x, x)");
    s.open("/main.void",4,changed_call); check(run(s)->result().succeeded() && s.invalidation().source_compatibility==vd::SourceCompatibility::restart_required,"valid signature restart");
    s.close("/main.void",5); s.replace("/worker.void",54,worker(42)); run(s);
    s.open("/other.void",1,worker(42));
    auto rebound=main_text; rebound.replace(rebound.find("/worker.void"),12,"/other.void"); s.open("/main.void",6,rebound); run(s);
    check(s.invalidation().source_compatibility==vd::SourceCompatibility::restart_required,"binding restart");
    check(s.current_index()->references_to("/worker.void::calculate").empty() && s.current_index()->references_to("/other.void::calculate").size()==1,"current reference ownership after rebind");
    // Session analysis still lowers through the same native pipeline; historical
    // analysis and source remain executable after subsequent revisions.
    std::shared_ptr<const vd::SessionAnalysis> survivor;
    { vd::ToolingSession temporary(root/"main.void"); survivor=run(temporary); }
    check(survivor->index()->references_to("/worker.void::calculate").size()==1,"analysis outlives session");
    vd::SourceOverrides overlays;
    overlays.emplace("/virtual.void",vd::SourceFile("/virtual.void",worker(1)));
    check(vd::analyze_project(root/"virtual.void",overlays).succeeded(),"unsaved virtual entry");
    overlays.emplace("/virtual.void/child.void",vd::SourceFile("/virtual.void/child.void",worker(1)));
    check(!vd::analyze_project(root/"virtual.void"/"child.void",overlays).succeeded(),"entry root missing");
    overlays.emplace("/main.void",vd::SourceFile("/main.void","use \"/virtual.void/child.void\" as c\nfunc entry(x: i32) -> i32:\n    return c.calculate(x)\n"));
    check(!vd::analyze_project(root/"main.void",overlays).succeeded(),"virtual file-directory collision");
    overlays.erase("/main.void"); overlays.erase("/virtual.void/child.void");
    std::error_code ec;
    std::filesystem::create_symlink(root/"worker.void",root/"symbolic.void",ec);
    if (!ec || std::filesystem::is_symlink(root/"symbolic.void")) {
        overlays.emplace("/symbolic.void",vd::SourceFile("/symbolic.void",worker(1)));
        check(!vd::analyze_project(root/"symbolic.void",overlays).succeeded(),"overlay bypassed symlink policy");
    }
    ec.clear(); std::filesystem::create_hard_link(root/"worker.void",root/"linked.void",ec);
    if (!ec || std::filesystem::exists(root/"linked.void")) {
        overlays.emplace("/linked.void",vd::SourceFile("/linked.void",worker(1)));
        overlays.emplace("/main.void",vd::SourceFile("/main.void","use \"/worker.void\" as a\nuse \"/linked.void\" as b\nfunc entry(x: i32) -> i32:\n    return a.calculate(x) + b.calculate(x)\n"));
        check(!vd::analyze_project(root/"main.void",overlays).succeeded(),"overlay bypassed hardlink policy");
    }
    vd::Jit jit; auto native=jit.compile(*first->result().program); check(native->invoke(10)==53,"session native lowering");
}
