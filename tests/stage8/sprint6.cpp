#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/native_runtime.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool test,const char* message){if(!test)throw std::runtime_error(message);}
auto analyze(const std::string& text){auto a=vd::analyze_source({"/sprint6.void",text});if(!a.succeeded())throw vd::CompilationError(a.diagnostics);return a.program;}
void empty(vd::Runtime& runtime){vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);require(runtime.collect()==vd::Runtime::Collection::completed && runtime.snapshot().objects==0,"Sprint 6 roots leaked");}
const std::string stress=R"VOID(struct node:
    text: string
value struct payload:
    text: string
    object: node
func make(text: string) -> payload:
    return payload(text = "<{text}>", object = node(text = text))
func entry(input: i32) -> i32:
    var saved = make("🎮\0")
    for i in 0..<120:
        var next = if i % 2 == 0:
            make("{saved.text}")
        else:
            make("other")
        if next.object.text == "":
            return 90
        saved = next
    var empty: [i32] = []
    var selected = if input < 0:
        empty[0]
    else:
        input
    return selected + 1
)VOID";
}
int main(int argc,char** argv){try{
    require(argc==3,"repository and fixture path required");auto source=analyze(stress);
    for(bool optimized:{false,true}){
        vd::Jit jit;auto runtime=std::make_shared<vd::Runtime>(1);auto module=source->lower();
        auto old=jit.compile(module,module.entry_symbol,optimized,{},runtime);
        require(old->invoke(7)==8,"if/interpolation GC stress result");empty(*runtime);
        bool failed=false;try{old->invoke(-1);}catch(const std::runtime_error&){failed=true;}
        require(failed,"selected checked failure did not run");empty(*runtime);
        require(old->invoke(7)==8,"checked failure recovery");empty(*runtime);
        auto edit=stress;edit.replace(edit.rfind("selected + 1"),12,"selected + 2");auto changed=analyze(edit);vd::ReloadSlot slot(old);
        require(jit.replace(slot,*changed,optimized),"Sprint 6 body reload rejected");
        require(slot.acquire()->invoke(7)==9 && old->invoke(7)==8,"Sprint 6 generation replacement changed old code");
        old.reset();slot.acquire()->runtime_owner()->close();
    }
    // Spelling changes in pure templates retain the established restart policy.
    const std::string template_source="const word = \"x\"\nconst text = \"{word}\"\nfunc entry(input: i32) -> i32:\n    if text == \"x\":\n        return input\n    return 90\n";
    auto template_before=analyze(template_source);auto template_edit=template_source;
    template_edit.replace(template_edit.find("\"{word}\""),8,"(\"\" + (word) + \"\")");auto template_after=analyze(template_edit);
    for(bool opt:{false,true}){vd::Jit jit;auto code=jit.compile(*template_before,opt);vd::ReloadSlot slot(code);
        require(code->invoke(7)==7 && !jit.replace(slot,*template_after,opt),"interpolation spelling bypassed constant restart contract");}
    auto async=analyze(R"VOID(func worker(text: string) -> string:
    return "[{text}]"
async func entry(input: i32) -> i32:
    var task = spawn worker(
        "🎮",
    )
    var text = await task
    var again = await task
    var same = if text == again:
        text
    else:
        "wrong"
    if same == "[🎮]":
        return input
    return 90
)VOID");
    for(bool opt:{false,true}){vd::Jit jit;auto code=jit.compile_async(*async,opt);require(code->invoke(7)==7,"Sprint 6 async/repeated await");empty(*code->runtime_owner());}
    const auto directory=std::filesystem::path(argv[2]);std::filesystem::create_directories(directory);auto file=directory/"main.void";
    {std::ofstream stream(file);stream<<stress;}
    auto cached=vd::analyze_project(file);require(cached.succeeded(),"cache source invalid");
    {vd::Jit jit({directory,std::string(vd::compiler_build_identity())});auto first=jit.compile(*cached.program,false);auto second=jit.compile(*cached.program,false);
        require(first->invoke(7)==8 && second->invoke(7)==8,"Sprint 6 cache execution");
        require(jit.cache_status()=="hit" || jit.cache_status()=="unavailable","Sprint 6 Debug cache did not reuse");}
    auto native=vd::read_source(std::filesystem::path(argv[1])/"examples/native_sink/main.void").text();
    native.replace(native.find("sink.copy(original)"),std::string("sink.copy(original)").size(),"sink.copy(\n        \"{original}\",\n    )");
    {std::ofstream stream(file);stream<<native;}
    auto admitted=vd::analyze_project(file,vd::native_sink_project());if(!admitted.succeeded())throw vd::CompilationError(admitted.diagnostics);
    require(!vd::analyze_project(file).succeeded(),"Sprint 6 granted native admission");
    for(bool opt:{false,true}){vd::Jit jit;auto code=jit.compile(*admitted.program,opt);require(code->invoke(0)==7 && !vd::native_sink_live(),"Sprint 6 native owned text/cleanup");empty(*code->runtime_owner());}
    std::cout<<"Sprint 6 GC, checked failure, reload, async and native checks passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
