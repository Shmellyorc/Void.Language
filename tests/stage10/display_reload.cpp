#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/tooling.hpp"
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks=0;
auto analyze(const std::string& source){auto result=vd::analyze_source({"/display_reload.void",source});if(!result.succeeded())throw vd::CompilationError(result.diagnostics);return result.program;}
void check(bool valid,const char* why){if(!valid)throw std::runtime_error(why);++checks;}
}
int main(){try{
    for(bool formatted:{false,true}) {
        std::string source="value struct number:\n    private const value: i32 = 4\nfunc (self number) string() -> string:\n    return \"old\"\nfunc (self number) format(spec: string) -> result<string, string>:\n    return Ok(\"old\")\nfunc entry(input: i32) -> i32:\n    if ";
        source+=formatted?"\"{number():custom}\"":"string(number())";source+=" == \"old\":\n        return 1\n    return 2\n";
        auto before=analyze(source);auto modified=source;const auto position=modified.find(formatted?"Ok(\"old\")":"return \"old\"");modified.replace(position,formatted?9:12,formatted?"Ok(\"new\")":"return \"new\"");auto after=analyze(modified);
        check(before->compatibility_contract()==after->compatibility_contract(),"hook body edit changed compatibility");
        vd::ToolingIndex index(before);bool selected=false,created=false;
        for(const auto& reference:index.references())if(reference.from.find("::$display$")!=std::string::npos && reference.to.ends_with(formatted?"::method::format":"::method::string")){check(reference.implicit,"generated hook dependency became written reference");selected=true;}
        for(const auto& reference:index.references())if(reference.from=="/display_reload.void::entry" && reference.to.find("::$display$")!=std::string::npos){check(reference.implicit,"generated adapter became written reference");created=true;}
        check(selected && created,"missing adapter/hook dependency chain");
        for(const auto& declaration:index.declarations())check(declaration.identity.find("::$display$")==std::string::npos,"generated adapter leaked into editor declarations");
        auto plan=vd::plan_invalidation(*before,{"/display_reload.void"},after.get());check(plan.source_compatibility==vd::SourceCompatibility::exact_match,"hook body invalidation contract");
        for(bool optimized:{false,true}){vd::Jit jit;auto old=jit.compile(*before,optimized);vd::ReloadSlot slot(old);check(old->invoke(0)==1,"old hook behavior");check(jit.replace(slot,*after,optimized),"compatible hook reload rejected");check(slot.acquire()->invoke(0)==2 && old->invoke(0)==1,"hook generation lease/target");
            auto visibility=modified;visibility.insert(visibility.find("func (self number) string"),"private ");auto incompatible=analyze(visibility);check(!jit.replace(slot,*incompatible,optimized),"hook visibility edit did not require restart");check(slot.acquire()->invoke(0)==2,"rejected hook reload changed published generation");}
    }
    std::cout<<checks<<" display dependency/reload checks passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
