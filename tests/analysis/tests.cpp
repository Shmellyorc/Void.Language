#include "void/frontend.hpp"
#include "void/backend.hpp"
#include <atomic>
#include <fstream>
#include <iostream>
#include <thread>
#include <set>
#include <stdexcept>
namespace {
namespace fs = std::filesystem;
void check(bool b, const std::string& message) { if (!b) throw std::runtime_error(message); }
void write(const fs::path& p, const std::string& s) {
    fs::create_directories(p.parent_path()); std::ofstream out(p,std::ios::binary); out << s; check(bool(out),"fixture write");
}
std::shared_ptr<const vd::ValidatedAnalysis> analyze(const fs::path& root) {
    auto r = vd::analyze_project(root / "main.void");
    if (!r.succeeded()) throw vd::CompilationError(r.diagnostics);
    check(r.diagnostics.empty(),"unexpected successful diagnostics"); return r.program;
}
const std::string main_source = "use \"/worker.void\" as worker\nconst root_bonus = 2\nfunc entry(x: i32) -> i32:\n    return worker.calculate(x)\n";
std::string worker(int add) { return "use \"/values.void\" as values\nconst bonus: i32 = values.base + root_bonus\nfunc calculate(x: i32) -> i32:\n    const stable: i32 = x + bonus\n    return stable + " + std::to_string(add) + "\n"; }
void project(const fs::path& root) { write(root/"main.void",main_source); write(root/"worker.void",worker(1)); write(root/"values.void","const base: i32 = 40\n"); }
bool edge(const vd::ValidatedAnalysis& a, const std::string& from, const std::string& to) {
    for (const auto& r : a.references()) if (r.from==from && r.to==to) return true;
    return false;
}
void run(const fs::path& root) {
    project(root);
    auto before = analyze(root);
    check(before->modules().size()==3 && before->declarations().size()==6,"complete metadata");
    check(edge(*before,"/worker.void::bonus","/values.void::base") && edge(*before,"/worker.void::bonus","/main.void::root_bonus"),"root/import constant edges");
    check(edge(*before,"/main.void::entry","/worker.void::calculate"),"call owner");
    check(edge(*before,"/worker.void::calculate::const::stable::0","/worker.void::bonus"),"local initializer edge");
    check(edge(*before,"/worker.void::calculate","/worker.void::calculate::const::stable::0"),"local readonly reference");
    auto relocated = root.parent_path()/"relocated"; project(relocated);
    check(analyze(relocated)->compatibility_contract()==before->compatibility_contract(),"physical relocation changed identity");
    for (bool optimize : {false,true}) {
        project(root); vd::Jit jit;
        auto old = jit.compile(*before,optimize); vd::ReloadSlot slot(old);
        check(old->invoke(10)==53,"baseline native source");
        write(root/"worker.void",worker(9)); auto body = analyze(root);
        check(body->compatibility_contract()==before->compatibility_contract(),"ordinary body edit with const incompatible");
        check(jit.replace(slot,*body,optimize) && slot.invoke(10)==61 && old->invoke(10)==53,"body replacement did not publish or lost lease");
        auto spaced=worker(9); spaced.replace(spaced.find("bonus: i32 = values.base + root_bonus"),std::string("bonus: i32 = values.base + root_bonus").size(),"bonus : i32 = values.base  + root_bonus # constant trivia");
        write(root/"worker.void","# shifted location\n\n"+spaced); auto trivia=analyze(root);
        check(trivia->compatibility_contract()==body->compatibility_contract(),"comment/location changed compatibility");
        check(trivia->declarations()[3].source.begin!=before->declarations()[3].source.begin,"locations did not update");
        check(jit.replace(slot,*trivia,optimize),"trivia replacement");
        vd::Compatibility layout; layout.layout=1;
        vd::Compatibility captures; captures.captures=1;
        check(!jit.replace(slot,*trivia,optimize,layout) && !jit.replace(slot,*trivia,optimize,captures),"host layout/capture guard lost");
        auto reject = [&](const std::string& file, const std::string& source) {
            project(root); write(root/file,source); auto candidate=analyze(root);
            auto active=slot.acquire(); auto generations=jit.live_generations();
            vd::Compatibility spoof; spoof.source_contract=active->compatibility().source_contract; spoof.constant_contract=active->compatibility().constant_contract; spoof.analysis=before;
            check(!jit.replace(slot,*candidate,optimize,spoof),"incompatible edit published");
            check(slot.acquire()==active && slot.invoke(10)==61 && jit.live_generations()==generations,"preflight damaged active code or generated candidate");
            auto native=jit.compile(*candidate,optimize,spoof);
            check(!slot.replace(native) && native->compatibility().analysis==candidate,"host safety override or missing source ownership");
        };
        reject("values.void","const base: i32 = 41\n"); // transitive constant
        auto local=worker(9); local.replace(local.find("x + bonus"),9,"x - bonus"); reject("worker.void",local);
        reject("worker.void","use \"/values.void\" as values\nconst bonus: i32 = values.base + root_bonus\ninternal func calculate(x: i32) -> i32:\n    const stable: i32 = x + bonus\n    return stable + 9\n");
        auto signature=worker(9); signature.replace(signature.find("calculate(x: i32)"),17,"calculate(x: i32, y: i32)");
        project(root); write(root/"worker.void",signature); write(root/"main.void","use \"/worker.void\" as worker\nconst root_bonus = 2\nfunc entry(x: i32) -> i32:\n    return worker.calculate(x, 0)\n");
        auto changed_signature=analyze(root); check(!jit.replace(slot,*changed_signature,optimize),"signature change accepted");
        reject("worker.void",worker(9)+"func extra() -> i32:\n    return 0\n");
        auto no_local=worker(9); no_local.replace(no_local.find("const stable"),12,"var stable"); reject("worker.void",no_local);
        // Invalid edits never supply candidate metadata, including independent errors.
        project(root); write(root/"worker.void","func calculate(x: i32) -> i32:\n    var bad = unknown\n    var second: bool = 1\n    return x\n");
        auto invalid=vd::analyze_project(root/"main.void"); check(!invalid.program && invalid.diagnostics.size()>=2,"partial invalid analysis exposed");
        auto original=invalid.diagnostics.front().primary.source->text();
        write(root/"worker.void","deleted content\n");
        check(invalid.diagnostics.front().primary.source->text()==original && slot.invoke(10)==61,"diagnostic snapshot or slot lost");
        // Real concurrent native invocation while repeatedly publishing whole generations.
        project(root); auto first=analyze(root); write(root/"worker.void",worker(9)); auto second=analyze(root);
        std::atomic<bool> stop=false,wrong=false; std::atomic<unsigned> calls=0;
        std::vector<std::thread> callers;
        for (int n=0;n<3;++n) callers.emplace_back([&] { while (!stop.load()) { auto lease=slot.acquire(); auto value=lease->invoke(10); if(value!=53 && value!=61) wrong=true; ++calls; } });
        for(int n=0;n<12;++n) check(jit.replace(slot,n%2 ? *first : *second,optimize),"concurrent compatible replacement");
        stop=true; for(auto& t:callers)t.join(); check(!wrong && calls>0 && old->invoke(10)==53,"concurrent call/lease failure");
        // Analyze, then delete all source files. Lower and generate code from retained trees.
        auto retained=slot.acquire()->compatibility().analysis;
        fs::remove(root/"worker.void"); fs::remove(root/"values.void"); fs::remove(root/"main.void");
        check(jit.compile(*before,optimize)->invoke(10)==53 && retained && !retained->modules().empty(),"deleted source invalidated retained analysis");
        std::cout << (optimize?"O2":"normal") << ": body edit 53 -> 61, const edit rejected, old lease 53; concurrent calls " << calls << '\n';
    }
}
void bindings(const fs::path& root) {
    write(root/"one.void","func same(x: i32) -> i32:\n    return x + 1\n");
    write(root/"two.void","func same(x: i32) -> i32:\n    return x + 2\n");
    auto main=[](const std::string& owner) { return "use \"/one.void\" as one\nuse \"/two.void\" as two\nfunc entry(x: i32) -> i32:\n    return "+owner+".same(x)\n"; };
    write(root/"main.void",main("one")); auto a=analyze(root); write(root/"main.void",main("two")); auto b=analyze(root);
    check(a->compatibility_contract()!=b->compatibility_contract() && edge(*a,"/main.void::entry","/one.void::same") && edge(*b,"/main.void::entry","/two.void::same"),"same-name rebinding invisible");
    vd::Jit jit; vd::ReloadSlot slot(jit.compile(*a)); check(!jit.replace(slot,*b) && slot.invoke(0)==1,"rebinding published");
    write(root/"extra.void","# empty module\n"); write(root/"main.void","use \"/extra.void\"\n"+main("one"));
    auto graph=analyze(root); check(!jit.replace(slot,*graph),"module graph expansion accepted");
    write(root/"one.void","func renamed(x: i32) -> i32:\n    return x + 1\n"); auto renamed=main("one"); renamed.replace(renamed.find(".same"),5,".renamed"); write(root/"main.void",renamed);
    check(!jit.replace(slot,*analyze(root)),"rename published");
    // Function-only cycle with both directions actually resolved.
    write(root/"one.void","use \"/two.void\" as two\nfunc same(x: i32) -> i32:\n    if x < 1:\n        return 7\n    return two.same(x - 1)\n");
    write(root/"two.void","use \"/one.void\" as one\nfunc same(x: i32) -> i32:\n    return one.same(x)\n"); write(root/"main.void",main("one")); auto cycle=analyze(root);
    check(edge(*cycle,"/one.void::same","/two.void::same") && edge(*cycle,"/two.void::same","/one.void::same"),"cycle edges");
    for(bool o:{false,true}) check(jit.compile(*cycle,o)->invoke(4)==7,"cycle native execution");
}
void numeric(const fs::path& root) {
    const std::vector<std::string> types={"i8","i16","i32","i64","u8","u16","u32","u64","f32","f64"};
    std::string text;
    for (std::size_t n=0;n<types.size();++n) {
        auto id=std::to_string(n); text+="const c_"+id+": "+types[n]+" = 1\nfunc f_"+id+"(x: "+types[n]+") -> "+types[n]+":\n    return x + c_"+id+"\n";
    }
    write(root/"main.void",text); auto a=analyze(root); std::set<vd::ir::Type> seen;
    for(const auto& d:a->declarations())seen.insert(d.type);
    check(seen.size()==10 && a->declarations().size()==20,"numeric analysis collapsed types");
    auto changed=text; changed.replace(changed.find("c_0: i8"),7,"c_0: u8"); changed.replace(changed.find("f_0(x: i8) -> i8"),std::string("f_0(x: i8) -> i8").size(),"f_0(x: u8) -> u8");
    write(root/"main.void",changed); auto b=analyze(root); check(a->compatibility_contract()!=b->compatibility_contract(),"same-width signatures/constants compatible");
    // Declaration edit with identical evaluated value still requires restart.
    write(root/"main.void","const value = 1\n"); a=analyze(root); write(root/"main.void","const value = 0 + 1\n"); b=analyze(root);
    check(a->constant_contract()!=b->constant_contract(),"constant syntax edit collapsed to equal value");
}
void failures(const fs::path& root) {
    write(root/"main.void","const divisor = 100\nfunc entry(x: i32) -> i32:\n    return divisor / x\n");
    auto old=analyze(root); vd::Jit jit;
    for(bool optimize:{false,true}) {
        auto native=jit.compile(*old,optimize); vd::ReloadSlot slot(native);
        for(unsigned n=0;n<240;++n) {
            const std::vector<std::string> bad={"var q: bool = 1","var q = 1.2q","var q = missing","var q = (1 + )","while x > 0:","var q = x as bool","return true","var q = call_missing(x)"};
            write(root/"main.void","func entry(x: i32) -> i32:\n    "+bad[n%bad.size()]+"\n    var independent = missing_independent\n    return x\n");
            auto result=vd::analyze_project(root/"main.void");
            check(!result.program && !result.diagnostics.empty(),"recovery returned candidate");
            bool independent=false;
            for(const auto& d:result.diagnostics) if(d.description.find("missing_independent")!=std::string::npos) independent=true;
            check(independent,"recovery lost following independent error");
        }
        fs::remove(root/"main.void");
        try { slot.invoke(0); throw std::logic_error("runtime failure disappeared"); }
        catch(const std::runtime_error& e) { check(std::string(e.what()).find("return divisor / x")!=std::string::npos,"old runtime source snapshot lost"); }
        check(native->compatibility().analysis==old && slot.invoke(10)==10,"invalid source damaged generation");
    }
}

}
void analysis_tests(const std::filesystem::path& output) { run(output/"project"); bindings(output/"bindings"); numeric(output/"numeric"); failures(output/"failures"); }
