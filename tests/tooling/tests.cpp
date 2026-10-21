#include "void/tooling.hpp"
#include "void/backend.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
namespace fs=std::filesystem;
void check(bool ok,const std::string& message) { if(!ok) throw std::runtime_error(message); }
void write(const fs::path& p,const std::string& text) { fs::create_directories(p.parent_path()); std::ofstream out(p,std::ios::binary); out<<text; check(bool(out),"fixture write"); }
auto analyze(const fs::path& root) { auto r=vd::analyze_project(root/"main.void"); if(!r.succeeded()) throw vd::CompilationError(r.diagnostics); return r.program; }
bool has(const std::vector<std::string>& keys,const std::string& key) { return std::binary_search(keys.begin(),keys.end(),key); }
std::string leaf(int add=1) { return "const bonus: i32 = 40\ninternal func calculate(x: i32) -> i32:\n    const stable: i32 = x + bonus + root_bonus\n    return stable + "+std::to_string(add)+"\n"; }
const std::string main_text="# UTF-8: \xc3\xa9 \xf0\x9f\x98\x80\nuse \"/middle.void\"\nuse \"/leaf.void\" as direct\nuse \"./leaf.void\" as direct\nuse \"/other.void\" as other\nconst root_bonus = 2\nfunc entry(x: i32) -> i32:\n    if x < 0:\n        return other.calculate(x)\n    return wrapper(x)\nfunc extra(x: i32) -> i32:\n    return direct.calculate(x)\n";
void project(const fs::path& root) {
    write(root/"main.void",main_text); write(root/"leaf.void",leaf());
    write(root/"middle.void","use \"/leaf.void\" as aliased\nfunc wrapper(x: i32) -> i32:\n    return aliased.calculate(x)\n");
    write(root/"other.void","private const hidden = 0\nfunc calculate(x: i32) -> i32:\n    return x\n");
}
std::size_t at(const vd::ValidatedAnalysis& a,const std::string& module,const std::string& text) {
    for(const auto& m:a.modules()) if(m.identity==module) { auto p=m.source.text().find(text); check(p!=std::string::npos,"reference spelling absent"); return p; }
    throw std::runtime_error("module absent");
}
void queries(const fs::path& root) {
    project(root); auto a=analyze(root); vd::ToolingIndex index(a);
    auto d=index.declaration("/leaf.void::calculate");
    check(d && d->module=="/leaf.void" && d->visibility=="internal" && d->parameters==std::vector<vd::ir::Type>{vd::ir::Type::i32} && d->type==vd::ir::Type::i32,"signature/visibility");
    auto refs=index.references_to(d->identity); check(refs.size()==2,"multiple modules/callers");
    auto position=at(*a,"/main.void","direct.calculate")+7;
    check(index.definition_at("/main.void",position)->identity==d->identity,"aliased definition");
    check(index.definition_at("/main.void",position+8)->identity==d->identity,"identifier interior");
    check(!index.definition_at("/main.void",position+9),"half-open identifier end");
    check(!index.definition_at("/main.void",position-1),"qualifier punctuation fabricated symbol");
    check(!index.definition_at("/main.void",main_text.size()+200),"EOF lookup fabricated definition");
    auto wrapper=at(*a,"/main.void","wrapper(x)");
    check(index.definition_at("/main.void",wrapper)->identity=="/middle.void::wrapper","direct unaliased import");
    auto other=at(*a,"/main.void","other.calculate")+6;
    check(index.definition_at("/main.void",other)->identity=="/other.void::calculate","same name owner confusion");
    auto bonus=at(*a,"/leaf.void","+ root_bonus")+2;
    check(index.definition_at("/leaf.void",bonus)->identity=="/main.void::root_bonus","root constant definition");
    auto local=at(*a,"/leaf.void","return stable")+7;
    check(index.definition_at("/leaf.void",local)->identity=="/leaf.void::calculate::const::stable::0","readonly local definition");
    check(index.declaration("/other.void::hidden")->visibility=="private","private declaration metadata");
    check(index.declarations_in("/leaf.void").size()==5,"module declarations");
    check(!index.declaration("missing") && index.references_to("missing").empty() && index.declarations_in("missing").empty(),"missing query fabricated data");
    check(!index.declaration("/main.void::x") && index.definition_at("/main.void",at(*a,"/main.void","x < 0"))->kind==vd::AnalyzedDeclaration::Kind::parameter,"mutable local navigation invented");
    for(const auto& r:a->references()) {
        check(index.definition_at(r.source.module_identity,r.source.begin)->identity==r.to &&
              index.definition_at(r.source.module_identity,r.source.end-1)->identity==r.to,"reference index changed resolved target");
    }
    auto relocated=root.parent_path()/"relocated queries"; project(relocated); auto relocated_analysis=analyze(relocated);
    vd::ToolingIndex relocated_index(relocated_analysis);
    for(unsigned n=0;n<30;++n) {
        auto repeated=relocated_index.references_to(d->identity);
        check(repeated.size()==refs.size(),"relocated query count");
        for(std::size_t i=0;i<refs.size();++i) check(repeated[i].from==refs[i].from && repeated[i].to==refs[i].to && repeated[i].source.module_identity==refs[i].source.module_identity && repeated[i].source.begin==refs[i].source.begin && repeated[i].source.end==refs[i].source.end,"query order unstable across relocation");
    }
    auto location=index.definition_at("/main.void",position)->source;
    check(location.source->text().substr(location.begin,location.end-location.begin)=="calculate","declaration span");
    check(position==main_text.find("direct.calculate")+7 && position>main_text.find("direct.calculate")-4,"UTF8 byte position");
    // Confirm a scalar-offset approximation from the multibyte comment is incorrect.
    check(!index.definition_at("/main.void",position-4) || index.definition_at("/main.void",position-4)->identity!=d->identity,"interpreted scalar offsets as bytes");
    auto moved=leaf(); write(root/"leaf.void","# moved\n\n"+moved); auto b=analyze(root); vd::ToolingIndex fresh(b);
    auto newer=fresh.declaration(d->identity); check(newer->source.begin==d->source.begin+9,"moved declaration span");
    check(index.declaration(d->identity)->source.begin==d->source.begin,"old index changed");
    auto copy=refs.front(); a.reset(); b.reset();
    fs::remove(root/"leaf.void"); fs::remove(root/"main.void"); fs::remove(root/"middle.void"); fs::remove(root/"other.void");
    check(index.declaration(d->identity)->source.source->text()==leaf() && copy.source.source && !copy.source.source->text().empty(),"snapshot lifetime");
    check(fresh.declaration(d->identity)->source.source->text()=="# moved\n\n"+leaf(),"second index lifetime");
    try { vd::ToolingIndex invalid(nullptr); throw std::logic_error("failed analysis indexed"); } catch(const std::invalid_argument&) {}
    std::cout<<"queries: aliased/direct definitions, 2 callers, root/local constants, byte spans and retained snapshots\n";
}
void planning(const fs::path& root) {
    project(root); auto a=analyze(root);
    auto old=vd::plan_invalidation(*a,{"/leaf.void","/leaf.void"});
    check(old.directly_changed_modules==std::vector<std::string>{"/leaf.void"},"hint canonical dedup");
    check(has(old.affected_declarations,"/leaf.void::calculate") && has(old.dependent_declarations,"/middle.void::wrapper") && has(old.dependent_declarations,"/main.void::entry"),"transitive declaration invalidation");
    check(has(old.dependent_modules,"/main.void") && has(old.dependent_modules,"/middle.void"),"transitive import invalidation");
    check(old.requires_full_reanalysis && old.requires_graph_discovery && !old.has_validated_comparison && old.source_compatibility==vd::SourceCompatibility::unknown,"old graph authorized reload");
    auto unknown=vd::plan_invalidation(*a,{"/new.void"});
    check(unknown.unknown_modules==std::vector<std::string>{"/new.void"} && has(unknown.dependent_declarations,"/other.void::calculate"),"unknown module not conservative");
    auto root_change=vd::plan_invalidation(*a,{"/main.void"});
    check(has(root_change.dependent_declarations,"/other.void::calculate"),"implicit root namespace ignored");
    write(root/"leaf.void",leaf(9)); auto b=analyze(root); auto body=vd::plan_invalidation(*a,{},b.get());
    check(body.directly_changed_modules==std::vector<std::string>{"/leaf.void"} && body.changed_declarations.empty() && body.changed_bindings.empty(),"body summary invents signature changes");
    check(body.has_validated_comparison && !body.requires_full_reanalysis && !body.requires_graph_discovery && !body.graph_changed && body.source_compatibility==vd::SourceCompatibility::exact_match,"validated body comparison");
    check(!has(body.dependent_declarations,"/other.void::calculate"),"fresh summary lost independent module precision");
    vd::Jit jit; vd::ReloadSlot slot(jit.compile(*a)); auto lease=slot.acquire();
    vd::ToolingIndex index(a); auto definition=index.declaration("/leaf.void::calculate");
    check(jit.replace(slot,*b) && slot.invoke(10)==61 && lease->invoke(10)==53,"53 -> 61 proof");
    check(index.declaration(definition->identity)->source.source->text()==leaf(),"native publish altered index");
    auto constant=leaf(9); constant.replace(constant.find("= 40"),4,"= 41"); write(root/"leaf.void",constant); auto c=analyze(root);
    auto changed=vd::plan_invalidation(*b,{},c.get());
    check(has(changed.changed_declarations,"/leaf.void::bonus") && has(changed.dependent_declarations,"/main.void::entry") && changed.source_compatibility==vd::SourceCompatibility::restart_required,"constant comparison");
    check(!jit.replace(slot,*c) && slot.invoke(10)==61 && lease->invoke(10)==53,"constant tooling plan bypassed reload gate");
    auto same=vd::plan_invalidation(*a,{},a.get()); check(same.directly_changed_modules.empty() && same.dependent_declarations.empty() && same.source_compatibility==vd::SourceCompatibility::exact_match,"identical snapshots");
    auto shifted=leaf(); write(root/"leaf.void","# comment\n"+shifted); auto comment=vd::plan_invalidation(*a,{},analyze(root).get());
    check(comment.source_compatibility==vd::SourceCompatibility::exact_match && comment.changed_declarations.empty(),"comment restart");
    project(root); auto signature=leaf();
    signature.replace(signature.find("calculate(x: i32)"),std::string("calculate(x: i32)").size(),"calculate(x: i32, flag: bool)");
    write(root/"leaf.void",signature);
    auto caller=vd::read_source(root/"middle.void").text(); caller.replace(caller.find("calculate(x)"),std::string("calculate(x)").size(),"calculate(x, true)"); write(root/"middle.void",caller);
    caller=main_text; caller.replace(caller.find("direct.calculate(x)"),std::string("direct.calculate(x)").size(),"direct.calculate(x, true)"); write(root/"main.void",caller);
    auto sig=analyze(root); auto sigplan=vd::plan_invalidation(*a,{},sig.get());
    check(has(sigplan.changed_declarations,"/leaf.void::calculate") && sigplan.source_compatibility==vd::SourceCompatibility::restart_required && !jit.replace(slot,*sig),"signature safety weakened");
    project(root); auto visibility=leaf(); visibility.replace(visibility.find("internal"),8,"public"); write(root/"leaf.void",visibility);
    check(vd::plan_invalidation(*a,{},analyze(root).get()).source_compatibility==vd::SourceCompatibility::restart_required,"visibility safety weakened");
    project(root); auto duplicates=main_text; duplicates.erase(duplicates.find("use \"./leaf.void\" as direct\n"),std::string("use \"./leaf.void\" as direct\n").size()); write(root/"main.void",duplicates);
    auto deduplicated=analyze(root); auto dedup=vd::plan_invalidation(*a,{},deduplicated.get());
    check(!dedup.graph_changed && dedup.changed_bindings.empty() && dedup.source_compatibility==vd::SourceCompatibility::exact_match,"normalized duplicate import changed dependencies");
    // Changed import targets/rebinding, with both owners reachable in both graphs.
    project(root); auto main=main_text; main.replace(main.find("use \"/leaf.void\" as direct"),std::string("use \"/leaf.void\" as direct\nuse \"./leaf.void\" as direct").size(),"use \"/other.void\" as direct");
    write(root/"main.void",main); auto rebound=analyze(root); auto bindings=vd::plan_invalidation(*a,{},rebound.get());
    check(bindings.graph_changed && has(bindings.changed_bindings,"/main.void::extra") && bindings.source_compatibility==vd::SourceCompatibility::restart_required,"rebinding/import target ignored");
    // New reachable module, then removal/rename. Union edges retain old dependents.
    project(root); write(root/"new.void","func added() -> i32:\n    return 1\n"); write(root/"main.void","use \"/new.void\"\n"+main_text); auto added=analyze(root);
    auto expansion=vd::plan_invalidation(*a,{},added.get()); check(expansion.added_modules==std::vector<std::string>{"/new.void"} && expansion.graph_changed && has(expansion.changed_declarations,"/new.void::added"),"new module missing");
    fs::remove(root/"renamed.void"); fs::rename(root/"new.void",root/"renamed.void"); write(root/"main.void","use \"/renamed.void\"\n"+main_text); auto renamed=analyze(root); auto rename=vd::plan_invalidation(*added,{},renamed.get());
    check(rename.added_modules==std::vector<std::string>{"/renamed.void"} && rename.removed_modules==std::vector<std::string>{"/new.void"},"module rename not add/remove");
    write(root/"main.void",main_text); auto removed=vd::plan_invalidation(*renamed,{},analyze(root).get()); check(removed.removed_modules==std::vector<std::string>{"/renamed.void"},"removed module summary");
    write(root/"leaf.void","func calculate(x: i32) -> i32:\n    var a = missing_a\n    var b = missing_b\n    return x\n"); auto invalid=vd::analyze_project(root/"main.void");
    check(!invalid.program && invalid.diagnostics.size()>=2,"invalid graph supplied tooling metadata");
    auto failed=vd::plan_invalidation(*a,{"/leaf.void"},invalid.program.get()); check(failed.source_compatibility==vd::SourceCompatibility::unknown && failed.requires_full_reanalysis,"invalid graph confirmed");
    fs::remove(root/"leaf.void"); invalid=vd::analyze_project(root/"main.void"); check(!invalid.program && !invalid.diagnostics.empty(),"missing module accepted");
    for(int n=0;n<40;++n) check(vd::plan_invalidation(*a,{"/leaf.void","/leaf.void"})==old,"nondeterministic plan");
    auto relocated=root.parent_path()/"relocated"; project(relocated); auto moved=analyze(relocated);
    check(vd::plan_invalidation(*moved,{"/leaf.void"})==old,"relocation changed plan");
    check(vd::plan_invalidation(*a,{},moved.get()).directly_changed_modules.empty(),"physical source paths count as edits");
    std::cout<<"plan: /leaf.void -> /middle.void::wrapper -> /main.void::entry; validated 53 -> 61, const restart required\n";
}
void containment(const fs::path& root) {
    write(root/"main.void","use \"/values.void\" as values\nfunc entry(x: i32) -> i32:\n    const unused = values.amount + x\n    return x\n");
    write(root/"values.void","const amount = 1\n"); auto before=analyze(root);
    write(root/"values.void","# changed source snapshot only\nconst amount = 1\n"); auto after=analyze(root);
    auto plan=vd::plan_invalidation(*before,{},after.get());
    check(has(plan.dependent_declarations,"/main.void::entry::const::unused::0") && has(plan.dependent_declarations,"/main.void::entry"),"unused readonly initializer lost containing function");
}
void cycles(const fs::path& root) {
    write(root/"main.void","use \"/a.void\" as a\nfunc entry(x: i32) -> i32:\n    return a.first(x)\n");
    write(root/"a.void","use \"/b.void\" as b\nfunc first(x: i32) -> i32:\n    if x < 1:\n        return 7\n    return b.second(x - 1)\n");
    write(root/"b.void","use \"/a.void\" as a\nfunc second(x: i32) -> i32:\n    return a.first(x)\n");
    auto a=analyze(root); vd::ToolingIndex index(a);
    for(int n=0;n<100;++n) {
        auto plan=vd::plan_invalidation(*a,{"/b.void"});
        check(plan.dependent_modules==std::vector<std::string>({"/a.void","/main.void"}) && plan.dependent_declarations==std::vector<std::string>({"/a.void::first","/main.void::entry"}),"cycle traversal");
        check(index.references_to("/a.void::first").size()==2,"cycle reference query");
    }
    auto return_refs=vd::analyze_source({"memory.void","func forward(x: i32) -> i32:\n    return later(x)\nfunc later(x: i32) -> i32:\n    return x\n"});
    vd::ToolingIndex forward(return_refs.program); check(forward.references_to("memory.void::later").size()==1,"forward reference");
    auto escaped=[] {
        auto r=vd::analyze_source({"owned.void","const answer: u64 = 18446744073709551615\n"});
        vd::ToolingIndex temporary(r.program);
        return temporary.declaration("owned.void::answer");
    }();
    check(escaped && escaped->type==vd::ir::Type::u64 && escaped->value==UINT64_MAX && escaped->source.source->text().find("18446744073709551615")!=std::string::npos,"result outlived neither index nor analysis");
    auto copied=forward.references_to("memory.void::later"); return_refs.program.reset();
    check(copied[0].source.source->text().find("later(x)")!=std::string::npos,"copied result lifetime");
}
void editor_metadata(const fs::path& root) {
    const std::string text="func helper(x: i32) -> i32:\n    return x\nfunc entry(x: i32) -> i32:\n    var total = x\n    var callback = func(number: i32):\n        total += number\n    for item in 0..<x:\n        total += item\n    if x > 0:\n        var separate = x\n        total += separate\n    return helper(total)\n";
    write(root/"main.void",text);auto program=analyze(root);vd::ToolingIndex index(program);
    auto captured=index.definition_at("/main.void",text.find("total += number"));
    check(captured && captured->source.begin==text.find("total = x"),"capture lost its exact outer declaration");
    check(index.references_to(captured->identity).size()==4,"mutable reads/writes/capture references incomplete");
    auto loop=index.definition_at("/main.void",text.find("+= item")+3);
    check(loop && loop->source.begin==text.find("item in"),"loop binder identity");
    auto parameter=index.definition_at("/main.void",text.find("+= number")+3);
    check(parameter && parameter->source.begin==text.find("number: i32"),"anonymous parameter identity");
    auto unavailable=vd::editor_assistance(root/"main.void",{{"/main.void",std::nullopt}},"/main.void",0);
    check(unavailable.candidates.empty() && unavailable.signature.empty(),"editor probe ignored an unavailable overlay");
    auto partial=text;auto cursor=partial.find("return helper(total)");partial.replace(cursor,std::string("return helper(total)").size(),"return sep");
    vd::SourceOverrides overrides{{"/main.void",vd::SourceFile("/main.void",partial)}};
    auto result=vd::editor_assistance(root/"main.void",overrides,"/main.void",cursor+10);
    check(std::none_of(result.candidates.begin(),result.candidates.end(),[](const auto& c){return c.name=="separate";}),"completion leaked a disjoint scope");
    check(!vd::analyze_project(root/"main.void",overrides).program,"editor recovery authorized invalid compilation");
    auto formatted=vd::format_source(vd::SourceFile("format.void","# preserve é😀\r\nfunc entry(x:i32)->i32:\r\n  return x # keep\r\n"));
    check(formatted && formatted->find("    return x # keep\r\n")!=std::string::npos,"formatter comments/CRLF");
    check(vd::format_source({"format.void",*formatted})==formatted,"formatter idempotence");
    const vd::SourceFile literal("literal.void","func entry(x: i32) -> i32:\n  var values: {string: i32} = {\n    \"a\": x\n  }\n  var callback = func(number: i32) -> i32:\n    return number + values[\"a\"]\n  return callback(2)\n");
    auto canonical=vd::format_source(literal);check(bool(canonical),"braced literal/anonymous block formatting unavailable");
    auto before=vd::analyze_source(literal),after=vd::analyze_source({"literal.void",*canonical});
    check(before.program && after.program,"formatter changed valid program structure");
    for(bool optimize:{false,true}){vd::Jit jit;check(jit.compile(*before.program,optimize)->invoke(3)==5 && jit.compile(*after.program,optimize)->invoke(3)==5,"formatter changed native execution");}
    check(vd::format_source({"literal.void",*canonical})==canonical,"braced formatter not idempotent");
    check(!vd::format_source({"bad.void","\tfunc entry(:\n"}),"formatter accepted invalid lexical source");
}

void discovery(const fs::path& root) {
    fs::remove_all(root);fs::create_directories(root);
    auto library=fs::absolute(root/"standard");auto project=fs::absolute(root/"project");
    auto setroot=[](const std::string& value) {
#ifdef _WIN32
        _putenv_s("VOID_STDLIB_ROOT",value.c_str());
#else
        if(value.empty())unsetenv("VOID_STDLIB_ROOT");else setenv("VOID_STDLIB_ROOT",value.c_str(),1);
#endif
    };
    std::string previous=std::getenv("VOID_STDLIB_ROOT")?std::getenv("VOID_STDLIB_ROOT"):"";
    struct Restore {decltype(setroot)& set;std::string value;~Restore(){set(value);}} restore{setroot,previous};
    setroot(library.string());
    write(library/"io/files.void","use io\n## Read a fixture.\n## @param x fixture value\nfunc read(x: i32) -> i32:\n    return x + 1\ninternal func hidden(x: i32) -> i32:\n    return x\nprivate func secret(x: i32) -> i32:\n    return x\n");
    write(library/"io.void","use io/files as files\nfunc read(x: i32) -> i32:\n    return files.hidden(x)\n");
    write(library/"for.void","func value(x: i32) -> i32:\n    return x\n");
    write(project/"app/systems/player.void","use \"../helper.void\" as helper\nfunc update(x: i32) -> i32:\n    return helper.update(x)\n");
    write(project/"app/helper.void","func update(x: i32) -> i32:\n    return x\n");
    write(project/"io/files.void","func read(x: i32) -> i32:\n    return x + 100\n");
    const std::string main="use \"/io/files.void\" as project_files\nuse io/files as files\nuse io\nuse for as loop\nuse \"app/systems/player.void\" as player\nfunc entry(x: i32) -> i32:\n    return files.read(x) + player.update(x) + project_files.read(x)\n";
    write(project/"main.void",main);
    auto program=analyze(project);vd::ToolingIndex index(program);
    check(program->modules().size()==7,"nested domains/keyword path/diamond identity");
    auto definition=index.definition_at("/main.void",main.find("files.read")+7);
    check(definition && definition->module=="/@stdlib/io/files.void" && definition->documentation.find("Read a fixture.")!=std::string::npos,"standard definition/docs");
    vd::Jit jit;check(jit.compile(*program,false)->invoke(4)==113,"nested modules JIT");
    auto probe=[&](std::string text,std::size_t cursor=std::string::npos,const vd::SourceOverrides& extra=vd::SourceOverrides{}) {
        auto overlays=extra;overlays.insert_or_assign("/main.void",vd::SourceFile((project/"main.void").string(),text));
        return vd::editor_assistance(project/"main.void",overlays,"/main.void",cursor==std::string::npos?text.size():cursor);
    };
    auto hasname=[](const auto& query,const std::string& name){return std::any_of(query.candidates.begin(),query.candidates.end(),[&](const auto& c){return c.name==name;});};
    check(hasname(probe("use "),"io") && hasname(probe("use "),"io/"),"standard module and same-stem folder");
    check(hasname(probe("use io/"),"files"),"nested standard completion");
    check(hasname(probe("use \""),"app/"),"project folder completion");
    check(hasname(probe("\xef\xbb\xbfuse \""),"app/"),"BOM import completion");
    check(hasname(probe("use \"app/systems/"),"player.void"),"deep project completion");
    write(project/"app/worker space.void","");
    check(hasname(probe("use \"app/worker s"),"worker space.void"),"portable spaces in quoted prefix");
    auto apply=[&](std::string text,std::size_t cursor,const std::string& name) {auto q=probe(text,cursor);for(const auto& c:q.candidates)if(c.name==name){text.replace(q.begin,q.end-q.begin,c.insert_text);return text;}throw std::runtime_error("missing import edit");};
    auto edited=apply("use \"app/systems/plXYZ.void\" as player # é😀\r\n",19,"player.void");
    check(edited=="use \"app/systems/player.void\" as player # é😀\r\n","middle import suffix/alias/comment edit");
    auto newfile=apply("use \"app/systems/",17,"player.void");check(newfile=="use \"app/systems/player.void\"","closing quote edit");
    vd::SourceOverrides overlays{{"/virtual/deep/new.void",vd::SourceFile("new.void","")}};
    check(hasname(probe("use \"",std::string::npos,overlays),"virtual/"),"overlay-only folder");
    check(hasname(probe("use \"virtual/deep/",std::string::npos,overlays),"new.void"),"overlay-only source");
    overlays["/app/helper.void"]=std::nullopt;
    check(!hasname(probe("use \"app/",std::string::npos,overlays),"helper.void"),"tombstone discovery");
    check(probe("# use ").candidates.empty() && probe("func entry(x: i32) -> i32:\n    var s = \"use ").candidates.empty(),"non-import contexts");
    auto completion=probe("use io/files as files\nfunc entry(x: i32) -> i32:\n    return files.re");
    check(hasname(completion,"read") && !hasname(completion,"hidden") && !hasname(completion,"secret"),"documented visible API completion");
    check(completion.candidates.front().documentation.find("Read a fixture.")!=std::string::npos,"completion docs");
    auto signature=probe("use io/files as files\nfunc entry(x: i32) -> i32:\n    return files.read(");
    check(signature.documentation.find("Read a fixture.")!=std::string::npos && signature.parameter_documentation==std::vector<std::string>{"fixture value"},"signature docs");
    auto reject=[&](const std::string& source){return !vd::analyze_project(project/"main.void",{{"/main.void",vd::SourceFile("main.void",source)}}).program;};
    check(reject("use io/files as files\nfunc entry(x: i32) -> i32:\n    return files.hidden(x)\n"),"library internal escaped");
    check(reject("use IO/files\n") && reject("use io/\n") && reject("use io.files\n") && reject("use \"/@stdlib/io/files.void\"\n"),"invalid import spelling/domain escape");
    write(library/"leak.void","func leak(x: i32) -> i32:\n    return project_secret\n");
    check(reject("use leak\nconst project_secret = 7\n"),"project constant leaked into standard source");
    write(library/"leak.void","use \"/app/helper.void\"\n");
    check(reject("use leak\n"),"standard source admitted a project dependency");
    write(library/"leak.void","func helper(x: i32) -> i32:\n    return x\nfunc value(x: i32) -> i32:\n    var f = helper\n    return f(x)\n");
    check(!reject("use leak\nconst helper = 7\n"),"project root constant interfered with library function value");
    setroot((root/"missing-standard-root").string());check(reject("use io/files\n"),"missing configured standard root");
    setroot("relative-standard-root");check(reject("use io/files\n"),"relative standard root admitted");setroot(library.string());
    auto selected=vd::discover_project(project,project/"app/systems/player.void");check(selected.entry==project/"main.void" && !selected.standalone,"nearest main");
    write(project/"app/main.void","invalid source does not trigger fallback\n");
    check(vd::discover_project(project,project/"app/systems/player.void").entry==project/"app/main.void","nested main selection");
    check(vd::discover_project(project,project/"app/helper.void",project/"main.void").entry==project/"main.void","explicit selection wins");
    fs::remove(project/"app/main.void");fs::remove(project/"main.void");
    check(vd::discover_project(project,project/"app/helper.void").standalone,"labeled standalone fallback");
    bool outside=false;try{(void)vd::discover_project(project,library/"io.void");}catch(const std::invalid_argument&){outside=true;}check(outside,"selection escaped workspace");
    std::error_code links;fs::create_hard_link(project/"io/files.void",project/"linked.void",links);
    if(!links) {
        auto alias=vd::analyze_project(project/"new.void",{{"/new.void",vd::SourceFile("diagnostic-only-main", "use \"/io/files.void\" as first\nuse \"/linked.void\" as second\n")},{"/io/files.void",vd::SourceFile("diagnostic-only-label","func read(x: i32) -> i32:\n    return x\n")}});
        check(!alias.program && std::any_of(alias.diagnostics.begin(),alias.diagnostics.end(),[](const auto& d){return d.description.find("Hard-link aliases")!=std::string::npos;}),"overlaid diagnostic filename bypassed hard-link identity");
    }
    std::error_code ec;fs::create_directory_symlink(library,project/"alias",ec);
    if(!ec)check(!hasname(probe("use \""),"alias/"),"symlink discovery");
}

}
void tooling_tests(const fs::path& output) { discovery(output/"discovery"); queries(output/"queries"); planning(output/"planning"); cycles(output/"cycles"); containment(output/"containment"); editor_metadata(output/"editor"); }
