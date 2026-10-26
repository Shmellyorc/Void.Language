#include "void/tooling.hpp"
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks=0;
void check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);++checks;}
void composites(){
    const std::string source=R"VOID(value struct box<t>:
    private hidden: i32
    const value: t
enum parcel:
    empty
    ## Active payload documentation.
    data(item: box<[i32]>, note: string)
func inspect(const view: result<[i32], string>) -> string:
    if view is Ok(var captured):
        return "é😀 {captured}"
    return "error"
func entry(score: i32) -> i32:
    var items = [score]
    var mutable: result<[i32], string> = Ok(items)
    var chosen = parcel.data(box<[i32]>(value = items), "é😀")
    var outcome: result<parcel, string> = Ok(chosen)
    var text = "{outcome}"
    print(items, chosen, outcome)
    if outcome is Ok(var selected):
        if selected is parcel . data(var boxed, var note):
            return boxed.value.count as i32
    return score
)VOID";
    auto analysis=vd::analyze_source({"/main.void",source});if(!analysis.program)throw vd::CompilationError(analysis.diagnostics);
    vd::ToolingIndex index(analysis.program);
    auto data=index.definition_at("/main.void",source.find("parcel.data(")+7);
    auto pattern=index.definition_at("/main.void",source.find("parcel . data(")+9);
    check(data && pattern && data->identity==pattern->identity,"enum payload pattern/source identity");
    check(data->written_parameters.size()==2 && data->written_parameters[0].name=="item" && data->written_parameters[1].name=="note","enum payload names");
    check(data->documentation.description.find("Active payload")!=std::string::npos,"enum payload docs");
    check(index.references_to(data->identity).size()==2,"enum constructor/pattern references");
    auto success=index.definition_at("/main.void",source.find("Ok(chosen)"));
    auto tested=index.definition_at("/main.void",source.find("outcome is Ok(")+11);
    check(success && tested && success->identity==tested->identity && success->builtin,"typed result constructor/pattern metadata");
    check(success->written_parameters[0].name=="value" && index.references_to(success->identity).size()==2,"typed result payload refs");
    auto output=index.definition_at("/main.void",source.find("print(items"));
    check(output && output->signature.find("*values")!=std::string::npos && output->signature.find("$value")==std::string::npos && output->signature.find("file: console.output")!=std::string::npos,"print public signature");
    auto boxed=index.definition_at("/main.void",source.find("return boxed")+7);
    check(boxed && boxed->source.begin==source.find("var boxed")+4 && vd::ir::type_name(boxed->type).find("box<")!=std::string::npos,"typed pattern capture definition");
    for(const auto& d:index.declarations())check(d.identity.find("::$display$")==std::string::npos && d.source.source->text().find("$collection_")==std::string::npos,"synthetic display declaration leaked");
    auto readonly_ok=index.definition_at("/main.void",source.find("view is Ok(")+8);
    auto mutable_ok=index.definition_at("/main.void",source.find("Ok(items)"));
    check(readonly_ok && mutable_ok && !vd::ir::readonly_paths(readonly_ok->type).empty() && vd::ir::readonly_paths(mutable_ok->type).empty() && readonly_ok->identity!=mutable_ok->identity,"result metadata conflated readonly views");
    auto captures=index.definition_at("/main.void",source.find("{captured}")+1);
    check(captures && !vd::ir::readonly_paths(captures->type).empty(),"readonly payload hover authority lost");
    vd::ir::validate(analysis.program->lower());++checks;
    auto changed=source;changed.insert(0,"# metadata must not affect bindings\n");auto after=vd::analyze_source({"/main.void",changed});
    check(after.program && analysis.program->compatibility_contract()==after.program->compatibility_contract(),"editor sum metadata changed source contract");
}

}
int main(){try{
    composites();
    const std::string source=R"(value struct box<t>:
    value: t
    ## Preserve source owner documentation.
    operator +(const left: box<t>, const right: box<t>) -> box<t>:
        return left
    operator %(const left: box<t>, const right: box<t>) -> box<t>:
        return left
    operator ==(const left: box<t>, const right: box<t>) -> bool:
        return true
func entry(input: i32) -> i32:
    var a = box<i32>(value = input)
    var b = a + a
    b %= a
    var x = box<i64>(value = 4)
    var y = x + x
    if b != a:
        return 0
    return input
)";
    auto analyzed=vd::analyze_source({"/main.void",source});if(!analyzed.program)throw vd::CompilationError(analyzed.diagnostics);
    vd::ToolingIndex index(analyzed.program);
    auto first=index.definition_at("/main.void",source.find("a + a")+2);
    auto second=index.definition_at("/main.void",source.find("x + x")+2);
    check(first && second,"selected operator missing from index");
    check(first->identity==second->identity,"generic source navigation identity fragmented");
    check(first->signature.find("box<i32>")!=std::string::npos && second->signature.find("box<i64>")!=std::string::npos,"concrete operator signature erased");
    check(first->documentation.description.find("Preserve source")!=std::string::npos && first->signature.find("Effects: synchronous")!=std::string::npos,"operator docs/effects missing");
    check(!first->rename_safe,"operator glyph rename allowed");
    check(index.references_to(first->identity).size()==2,"generic source references fragmented or duplicated");
    for(const auto& [spelling,glyph]:{std::pair{"b %= a","%"},std::pair{"b != a","=="}}){auto found=index.definition_at("/main.void",source.find(spelling)+2);check(found && found->operator_glyph==glyph && !found->rename_safe,"derived/compound definition");auto refs=index.references_to(found->identity);check(refs.size()==1 && refs[0].source.end-refs[0].source.begin==2,"derived/compound original glyph span");}
    auto declaration=index.definition_at("/main.void",first->source.begin);
    // Declarations are a distinct query path in the LSP; verify the indexed template.
    auto templ=index.declaration(first->identity);check(templ && templ->signature.find("operator +(")!=std::string::npos && templ->signature.find("specialization")!=std::string::npos,"generic declaration signature missing");
    auto original=analyzed.program->compatibility_contract();
    auto changed=source;changed.insert(0,"# unrelated source trivia\n");
    auto after=vd::analyze_source({"/main.void",changed});check(after.program && original==after.program->compatibility_contract(),"presentation changed source compatibility");
    auto plan=vd::plan_invalidation(*analyzed.program,{},after.program.get());check(plan.source_compatibility==vd::SourceCompatibility::exact_match && plan.changed_bindings.empty(),"presentation polluted invalidation");
    vd::ir::validate(analyzed.program->lower());++checks;
    check(first->source.source->text()==source,"immutable source snapshot changed");
    auto generic_source=source.substr(0,source.find("func entry"))+R"(value struct other:
    value: i32
    operator +(const left: other, const right: other) -> other:
        return left
func sum<t>(const left: t, const right: t) -> t:
    return left + right
func entry(input: i32) -> i32:
    var a = sum(box<i32>(), box<i32>())
    var b = sum(box<i64>(), box<i64>())
    var c = sum(other(), other())
    return input
)";
    auto generic=vd::analyze_source({"/main.void",generic_source});if(!generic.program)throw vd::CompilationError(generic.diagnostics);
    vd::ToolingIndex specialized(generic.program);auto offset=generic_source.find("return left + right")+12;
    auto targets=specialized.definitions_at("/main.void",offset);
    check(targets.size()==2 && !specialized.definition_at("/main.void",offset),"generic body silently chose one definition");
    bool aggregate=false;for(const auto& target:targets){check(specialized.references_to(target.identity).size()==1,"generic body duplicated written references");aggregate|=target.signature.find("box<i32>")!=std::string::npos && target.signature.find("box<i64>")!=std::string::npos && target.signature.find("Selected specializations")!=std::string::npos;}
    check(aggregate,"generic body lost exact concrete signatures");
    vd::ir::validate(generic.program->lower());++checks;
    std::cout<<checks<<" Stage 10 tooling metadata checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
