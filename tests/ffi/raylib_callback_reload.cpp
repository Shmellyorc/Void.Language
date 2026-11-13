#include "void/backend.hpp"
#include "void/c_primitive.hpp"
#include "void/frontend.hpp"
#include "void/runtime.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

static void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
static void write(const std::filesystem::path& path,const std::string& text){std::ofstream out(path,std::ios::binary);out<<text;require(bool(out),"write reload source");}
int main(int argc,char** argv){
 try{
    require(argc==6,"expected entry, native root, provider, adapter, descriptor");
    const std::filesystem::path entry=argv[1];const auto original=vd::read_source(entry).text();
    auto contract=vd::CPrimitiveAdmission::admit_conventional(vd::read_source(argv[5]).text(),argv[2],argv[3],argv[4]);
    auto analyze=[&](){auto a=vd::analyze_project(entry,contract);if(!a.succeeded())throw vd::CompilationError(a.diagnostics);return a.program;};
    for(bool optimize:{false,true}){
        write(entry,original);auto before=analyze();vd::Jit jit;auto old=jit.compile(*before,optimize);auto runtime=old->runtime_owner();vd::ReloadSlot slot(old);
        require(slot.invoke(0)==1,"register typed callback");
        auto edited=original;const std::string marker="self.last = self.bias";auto at=edited.find(marker);require(at!=std::string::npos,"reload edit marker");edited.replace(at,marker.size(),marker+" + 100");
        write(entry,edited);auto next=analyze();require(jit.replace(slot,*next,optimize),"compatible typed callback reload");old.reset();
        require(jit.live_generations()==2,"G5 lost acquiring generation");
        {vd::Runtime::Attachment attach(*runtime);vd::Runtime::Managed managed(*runtime);require(runtime->collect()==vd::Runtime::Collection::completed,"GC while callback retained");}
        require(slot.invoke(1)==40,"typed callback changed to replacement code");
        require(slot.invoke(-1)==0,"typed callback unregister");require(jit.live_generations()==1,"G5 did not retire typed callback generation");
        require(slot.invoke(0)==1&&slot.invoke(1)==140,"replacement typed callback generation");require(slot.invoke(-1)==0,"replacement unregister");
        require(!runtime->close(),"typed callback Runtime cleanup");
        std::cout<<"Raylib G5 "<<(optimize?"O2":"normal")<<" capture, GC, compatible reload and retirement passed\n";
    }
    write(entry,original);return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
