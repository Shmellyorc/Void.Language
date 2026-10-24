#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/tooling.hpp"
#include "void/format.hpp"
#include "void/temporal.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks=0;
void check(bool yes,const std::string& why){if(!yes)throw std::runtime_error(why);++checks;}
std::string replace(std::string s,const std::string& before,const std::string& after){auto at=s.find(before);check(at!=std::string::npos,"missing fixture replacement: "+before);s.replace(at,before.size(),after);return s;}
auto analyze(const std::string& source){auto a=vd::analyze_source({"/reload.void",source});if(!a.succeeded())throw vd::CompilationError(a.diagnostics);return a.program;}
const std::string entry="func entry(input: i32) -> i32:\n";
const std::string type="value struct number:\n    value: i32\n    operator +(const left: number, const right: number) -> number:\n        return number(value = left.value + right.value + 10)\n    operator ==(const left: number, const right: number) -> bool:\n        return false\n    operator %(const left: number, const right: number) -> number:\n        return number(value = 1)\n";
const std::string source=type+entry+"    var value = number(value = input)\n    value += number(value = 1)\n    if value != number():\n        return value.value\n    return 0\n";
void reload(const std::string& from,const std::string& to,int first,int second,bool compatible){auto a=analyze(from),b=analyze(to);check((a->compatibility_contract()==b->compatibility_contract())==compatible,"unexpected compatibility");auto plan=vd::plan_invalidation(*a,{"/reload.void"},b.get());check(plan.source_compatibility==(compatible?vd::SourceCompatibility::exact_match:vd::SourceCompatibility::restart_required),"wrong invalidation verdict");for(bool optimized:{false,true}){vd::Jit jit;auto old=jit.compile(*a,optimized);vd::ReloadSlot slot(old);auto held=slot.acquire();check(held->invoke(2)==first,"initial generation behavior");check(jit.replace(slot,*b,optimized)==compatible,"wrong publication verdict");check(slot.invoke(2)==(compatible?second:first) && held->invoke(2)==first,"reload changed retained/rejected generation");}}
void operators(){
    reload(source,replace(source,"right.value + 10","right.value + 20"),13,23,true);
    reload(source,replace(source,"    operator %","    private operator %"),13,13,false);
    reload(source,replace(source,"right: number) -> number:\n        return number(value = 1)","right: i64) -> number:\n        return number(value = 1)"),13,13,false);
    const std::string extra="    operator -(const left: number) -> number:\n        return number(value = left.value)\n";
    auto added=replace(source,entry,extra+entry);reload(source,added,13,13,false);reload(added,source,13,13,false);
    auto effects="struct number:\n    value: i32\n    operator +(const left: number, const right: number) -> number:\n        return number(value = left.value)\n"+entry+"    return (number(value = input) + number()).value\n";
    reload(effects,replace(effects,"return number(value = left.value)","return left"),2,2,false);
    auto a=analyze(source);bool inequality=false,compound=false;
    for(const auto& r:a->references())if(r.to.find("::operator::")!=std::string::npos){auto spelling=r.source.source->text().substr(r.source.begin,r.source.end-r.source.begin);inequality|=spelling=="!=";compound|=spelling=="+=";check(!r.binding_contract.empty(),"selected operator lost exact binding");}check(inequality && compound,"missing derived/compound targets");
    const std::string generic="value struct box<t>:\n    value: t\n    operator +(const left: box<t>, const right: box<t>) -> box<t>:\n        return box<t>(value = left.value + right.value)\n";
    auto unused=generic+entry+"    return input\n";auto addition=replace(unused,entry,"    operator -(const left: box<t>) -> box<t>:\n        return left\n"+entry);reload(unused,addition,2,2,false);
    reload(unused,replace(unused,"value struct box", "private value struct box"),2,2,false);
    reload(unused,replace(unused,"value struct box", "struct box"),2,2,false);
    reload(unused,replace(unused,"left.value + right.value","left.value - right.value"),2,2,true);
    auto specialized=generic+entry+"    return (box<i32>(value = input) + box<i32>(value = 1)).value\n";
    reload(specialized,replace(specialized,"left.value + right.value","left.value + right.value + 1"),3,4,true);
    const std::string identity="func identity<t>(value: t) -> t:\n    return value\n";
    auto first=analyze(identity+entry+"    return identity<i32>(input)\n");auto second=analyze(identity+entry+"    return identity<i64>(input as i64) as i32\n");
    auto plan=vd::plan_invalidation(*first,{"/reload.void"},second.get());check(std::find(plan.changed_bindings.begin(),plan.changed_bindings.end(),"/reload.void::entry")!=plan.changed_bindings.end(),"concrete generic target normalized out of invalidation");
}
void formats(){
    auto scalar=entry+"    if \"{input:000}\" == \"002\":\n        return 1\n    return 0\n";
    reload(scalar,replace(scalar,"input:000","input:0000"),1,0,false);
    auto ordinary=entry+"    if format(spec = \"000\", value = input) is Ok(var text):\n        return text.length as i32\n    return 0\n";
    reload(ordinary,replace(ordinary,"spec = \"000\"","spec = \"0000\""),3,4,false);
    const std::string hooks="value struct number:\n    value: i32\nfunc text() -> string:\n    return \"old\"\nfunc (self number) string() -> string:\n    return text()\nfunc (self number) format(spec: string) -> result<string, string>:\n    return Ok(text())\n";
    for(auto expression:{"string(number())","\"{number():tag}\""}){auto s=hooks+entry+"    if "+expression+" == \"old\":\n        return 1\n    return 0\n";reload(s,replace(s,"return \"old\"","return \"new\""),1,0,true);if(std::string(expression).find("tag")!=std::string::npos)reload(s,replace(s,":tag",":other"),1,1,false);}
    auto direct=hooks+entry+"    if number().format(\"tag\") is Ok(var value):\n        return value.length as i32\n    return 0\n";
    reload(direct,replace(direct,"format(\"tag\")","format(\"other\")"),3,3,false);
    auto defaults=replace(hooks,"format(spec: string)","format(spec: string = \"tag\")")+entry+"    if number().format() is Ok(var value):\n        return value.length as i32\n    return 0\n";
    reload(defaults,replace(defaults,"spec: string = \"tag\"","spec: string = \"other\""),3,3,false);
    auto constants="const spec = \"000\"\n"+entry+"    if format(input, spec) is Ok(var value):\n        return value.length as i32\n    return 0\n";
    reload(constants,replace(constants,"spec = \"000\"","spec = \"0000\""),3,4,false);
    reload(ordinary,replace(ordinary,"spec = \"000\"","spec = \"00\" + \"00\""),3,4,false);
    auto a=analyze(scalar);check(a->compatibility_contract().find(vd::formatting::abi_version)!=std::string::npos && a->compatibility_contract().find(vd::temporal::abi_version)!=std::string::npos,"missing pinned policy/dependency identity");
}
void project(const std::filesystem::path& root){
    auto path=root/"tests/stage10/operator_reload_project.void";
    auto main="use \"owner.void\" as owner\n"+entry+"    var value = owner.number(value = input)\n    if value == value:\n        return 1\n    return 0\n";
    vd::SourceOverrides files;files.emplace("/operator_reload_project.void",vd::SourceFile(path.string(),main));files.emplace("/owner.void",vd::SourceFile("owner.void","struct number:\n    value: i32\n"));auto a=vd::analyze_project(path,files);if(!a.succeeded())throw vd::CompilationError(a.diagnostics);
    files.insert_or_assign("/owner.void",vd::SourceFile("owner.void","struct number:\n    value: i32\n    operator ==(const left: number, const right: number) -> bool:\n        return false\n"));auto b=vd::analyze_project(path,files);if(!b.succeeded())throw vd::CompilationError(b.diagnostics);
    auto plan=vd::plan_invalidation(*a.program,{"/owner.void"},b.program.get());check(plan.source_compatibility==vd::SourceCompatibility::restart_required,"builtin/custom selection switch compatible");check(std::find(plan.dependent_declarations.begin(),plan.dependent_declarations.end(),"/operator_reload_project.void::entry")!=plan.dependent_declarations.end(),"owner addition did not invalidate importing caller");
    for(bool optimized:{false,true}){vd::Jit jit;auto old=jit.compile(*a.program,optimized);vd::ReloadSlot slot(old);check(!jit.replace(slot,*b.program,optimized) && slot.invoke(2)==1,"owner import selection bypassed restart");}
}
void write(const std::filesystem::path& path,const std::string& source){std::ofstream out(path);out<<source;check(bool(out),"fixture write failed");}
auto file_analysis(const std::filesystem::path& path){auto a=vd::analyze_project(path);if(!a.succeeded())throw vd::CompilationError(a.diagnostics);return a.program;}
void cache(const std::filesystem::path& directory,const std::filesystem::path& repository,const char* fixture="operator_format_cache.void"){
    std::filesystem::remove_all(directory);std::filesystem::create_directories(directory);auto path=directory/"main.void";auto text=vd::read_source(repository/"tests/stage10"/fixture).text();write(path,text);auto a=file_analysis(path);auto build=std::string(vd::compiler_build_identity());check(!build.empty(),"missing actual cache build identity");vd::Jit jit({directory,build});auto old=jit.compile(*a,false);check(jit.cache_status()=="miss" && old->invoke(2)==2,"operator/format cold cache");auto hit=jit.compile(*a,false);check(jit.cache_status()=="hit" && hit->invoke(2)==2 && old->invoke(-3)==-3,"cached Runtime operation binding");
    auto edited=replace(text,"return Ok(input)","return Ok(input + 1)");write(path,edited);auto b=file_analysis(path);check(a->compatibility_contract()==b->compatibility_contract(),"body-only cached edit changed contract");auto next=jit.compile(*b,false);check(jit.cache_status()=="miss" && next->invoke(2)==3,"stale body reused");auto next_hit=jit.compile(*b,false);check(jit.cache_status()=="hit" && next_hit->invoke(2)==3,"new body cache hit");vd::ReloadSlot slot(hit);check(jit.replace(slot,*b,false) && slot.invoke(2)==3 && old->invoke(2)==2,"cached reload/old lease");check(jit.replace(slot,*b,false) && jit.cache_status()=="hit","reload forced-Runtime cache hit");
    for(const auto& file:std::filesystem::directory_iterator(directory/".void/cache/v1"))write(file.path(),"corrupt");auto retry=jit.compile(*a,false);check(jit.cache_status()=="miss" && retry->invoke(2)==2,"corrupt operator/format fallback");
    vd::Jit changed_build({directory,build+"dependency-update"});check(changed_build.compile(*a,false)->invoke(2)==2 && changed_build.cache_status()=="miss","dependency build identity did not invalidate");auto optimized=jit.compile(*a,true);check(optimized->invoke(2)==2 && jit.cache_status()=="disabled","optimized execution/cache policy");
    auto changed_spec=replace(edited,"const spec = \"000\"","const spec = \"00\"");write(path,changed_spec);auto specification=file_analysis(path);check(!jit.replace(slot,*specification,false) && slot.invoke(2)==3,"format constant bypassed restart");auto different=jit.compile(*specification,false);check(jit.cache_status()=="miss" && different->invoke(2)==101,"stale format specification reused");write(path,edited);
    vd::ToolingSession session(path);auto revision=vd::ToolingSession::analyze(session.select());check(session.accept(revision),"revision acceptance");if(revision->result().succeeded() && b->compatibility_contract()!=revision->result().program->compatibility_contract()){write(directory/"before.contract",b->compatibility_contract());write(directory/"revision.contract",revision->result().program->compatibility_contract());}check(revision->result().succeeded(),"fresh revision analysis failed");check(b->compatibility_contract()==revision->result().program->compatibility_contract(),"fresh revision contract differs");for(unsigned repeat=0;repeat<3;++repeat)check(jit.replace(slot,session,*revision,false),"current cached revision rejected");session.refresh();check(!jit.replace(slot,session,*revision,false) && slot.invoke(2)==3,"stale cached analysis publication");
}
}
int main(int argc,char** argv){try{check(argc==3,"expected repository and isolated cache root");operators();formats();project(argv[1]);cache(argv[2],argv[1]);cache(std::filesystem::path(argv[2])/"combined",argv[1],"operator_systems.void");std::cout<<checks<<" operator/format compatibility/cache/reload checks passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
