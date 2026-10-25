#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/native_runtime.hpp"
#include <algorithm>
#include <fstream>
#include <future>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void write(const std::filesystem::path& file,const std::string& text){std::ofstream stream(file);stream<<text;require(bool(stream),"write Sprint 4 source");}
auto analysis(const std::filesystem::path& file,const vd::NativeProject& provider){auto result=vd::analyze_project(file,provider);if(!result.succeeded())throw vd::CompilationError(result.diagnostics);return result.program;}

void empty(vd::Runtime& runtime){vd::Runtime::Attachment attachment(runtime);vd::Runtime::Managed managed(runtime);require(runtime.collect()==vd::Runtime::Collection::completed && runtime.snapshot().objects==0,"provider retained roots after cleanup");}
}
int main(int argc,char** argv){try{
    if(argc==5 && std::string(argv[1])=="--emit"){vd::emit_object(analysis(argv[2],vd::native_sink_project())->lower(),argv[3],std::string(argv[4])=="optimized",true);return 0;}
    require(argc==3,"repository and fixture directory required");const auto program=vd::read_source(std::filesystem::path(argv[1])/"examples/native_sink/main.void").text();std::filesystem::path root=argv[2];std::filesystem::create_directories(root);auto file=root/"main.void";write(file,program);
    auto sink=vd::native_sink_project();auto original=analysis(file,sink);
    require(original->application(),"application identity missing");
    require(!vd::analyze_project(file).succeeded(),"untrusted import admitted");
    for(int mode=0;mode<5;++mode){auto invalid=sink;if(mode==0)invalid.library="void.unknown.v1";if(mode==1)invalid.bindings[0].symbol="missing";if(mode==2)invalid.bindings[0].parameters={"string"};if(mode==3)invalid.bindings[0]=vd::native_fixture_catalog()[2];if(mode==4)invalid.bindings[0].callbacks=true;bool rejected=false;try{vd::validate_native_project(invalid);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"invalid provider descriptor accepted");}
    for(bool optimize:{false,true}){
        vd::Jit jit;auto code=jit.compile(*original,optimize);require(code->invoke(0)==7,"sink JIT result");require(!vd::native_sink_live() && !vd::native_fixture_live(),"provider cleanup leaked");empty(*code->runtime_owner());
        // Entry status changes can reload; changing entry kind cannot.
        write(file,program.substr(0,program.rfind("return result"))+"return result + 1\n");auto changed=analysis(file,sink);vd::ReloadSlot slot(code);require(jit.replace(slot,*changed,optimize) && slot.acquire()->invoke(0)==8 && code->invoke(0)==7,"application reload/lease");
        write(file,"func entry(value: i32) -> i32:\n    return value\n");auto legacy=vd::analyze_project(file);require(legacy.succeeded() && !jit.replace(slot,*legacy.program,optimize),"entry-kind reload accepted");
        write(file,program);
    }
    // Top-level application bodies share managed cleanup and callback lowering.
    std::string top;std::istringstream lines(program);std::string line;
    while(std::getline(lines,line)){if(line=="func main() -> i32:"){top+="if true:\n";continue;}top+=line+"\n";}
    write(file,top);auto top_analysis=analysis(file,sink);
    for(bool optimize:{false,true}){vd::Jit jit;auto code=jit.compile(*top_analysis,optimize);require(code->invoke(0)==7 && !vd::native_sink_live(),"top-level native cleanup");empty(*code->runtime_owner());}
    // Failure unwinding must execute the source defer before releasing roots.
    auto failure=program;auto at=failure.rfind("return result");failure.replace(at,13,"var xs: [i32] = []\n    return xs[0]");write(file,failure);
    for(bool optimize:{false,true}){vd::Jit jit;auto code=jit.compile(*analysis(file,sink),optimize);bool failed=false;try{code->invoke(0);}catch(const std::runtime_error&){failed=true;}require(failed && !vd::native_sink_live(),"failed application leaked sink");empty(*code->runtime_owner());}
    // A provider-specific callback retains captured managed text and its exact
    // old generation, even after publishing a compatible replacement.
    const std::string persistent="use \"/native_sink.void\" as sink\nstruct owner:\n    bias: i32 = 1\n    text: string = \"🎮\"\nfunc (self owner) callback(value: i32) -> i32:\n    return value + self.bias + (self.text.length as i32)\nfunc entry(value: i32) -> i32:\n    var resource = sink.acquire(true) as sink.resource\n    var state = owner()\n    sink.register(resource, state.callback)\n    return value\n";
    write(file,persistent);auto old_analysis=analysis(file,sink);
    for(bool optimize:{false,true}){vd::Jit jit;auto code=jit.compile(*old_analysis,optimize);code->invoke(0);auto runtime=code->runtime_owner();vd::ReloadSlot slot(code);
        auto edit=persistent;edit.replace(edit.find("value + self.bias"),std::string("value + self.bias").size(),"value + 10 + self.bias");write(file,edit);auto changed=analysis(file,sink);require(jit.replace(slot,*changed,optimize),"sink callback reload rejected");code.reset();require(jit.live_generations()==2,"sink lost old generation lease");
        {vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);require(runtime->collect()==vd::Runtime::Collection::completed,"sink callback GC");vd::Runtime::Handle identity(*runtime,vd::native_sink_identity(*runtime));vd::RuntimeFailure error;auto result=vd_native_sink_call(21,5,identity.get(),nullptr,0,nullptr,nullptr,nullptr,"",&error);require(!error.message && result==7,"sink captured root or old callback changed");}
        runtime->close();require(!vd::native_sink_live() && jit.live_generations()==1,"sink shutdown failed to release registration/lease");
    }
    // Provider-local identity checks precede thread checks and all operations.
    write(file,"use \"/native_sink.void\" as sink\nfunc entry(value: i32) -> i32:\n    var resource = sink.acquire(true) as sink.resource\n    return value\n");auto retained=analysis(file,sink);
    {vd::Jit jit;auto code=jit.compile(*retained,false);require(code->invoke(0)==0 && vd::native_sink_live()==1,"sink acquisition");auto runtime=code->runtime_owner();void* identity;
        {vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);identity=vd::native_sink_identity(*runtime);vd::RuntimeFailure failure;vd_native_fixture_call(18,0,identity,nullptr,0,nullptr,nullptr,nullptr,"",&failure);require(failure.message && std::string(failure.message).find("another provider")!=std::string::npos,"provider identity crossover");}
        auto foreign=std::async(std::launch::async,[&]{vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);vd::RuntimeFailure failure;vd_native_sink_call(18,0,identity,nullptr,0,nullptr,nullptr,nullptr,"",&failure);return failure.message && std::string(failure.message).find("owning thread")!=std::string::npos;});require(foreign.get(),"sink thread affinity");
        runtime->close();require(!vd::native_sink_live(),"sink shutdown did not release roots/resource");}
    for(const auto& body:{
        "var forged = sink.resource()\n    return value\n",
        "var r = sink.acquire(true) as sink.resource\n    r.token = 0 as u64\n    return value\n",
        "var r = sink.acquire(true) as sink.resource\n    var cb = func(x: i32) -> i32:\n        return x\n    sink.register(r, cb)\n    return value\n"}){
        write(file,"use \"/native_sink.void\" as sink\nfunc entry(value: i32) -> i32:\n    "+std::string(body));require(!vd::analyze_project(file,sink).succeeded(),"unsafe sink source admitted");}
    // Mutated typed IR must independently reject a symbol from another provider.
    auto module=original->lower();for(auto& f:module.functions)for(auto& b:f.blocks)for(auto& i:b.instructions)if(i.op==vd::ir::Op::native_call && i.callee=="vd_native_sink_call#echo")i.callee="vd_native_fixture_call#i32";
    bool rejected=false;try{vd::ir::validate(module);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"cross-provider IR admitted");
    write(file,program);std::cout<<"Sprint 4 host checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
