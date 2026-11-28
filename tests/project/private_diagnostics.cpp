#include "void/backend.hpp"
#include "void/private_diagnostics.hpp"
#include "void/frontend.hpp"
#include "void/project.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
void require(bool ok, const char* what) { if(!ok)throw std::runtime_error(what); }
template<class F> void rejects(F f) { try {f();} catch(const std::invalid_argument&) {return;} throw std::runtime_error("Expected diagnostic rejection"); }
std::string read(const char* path) {
    std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Cannot read qualification input");
    return {std::istreambuf_iterator<char>(in),{}};
}
}
int main(int argc,char** argv) { try {
    using namespace vd::diagnostics;
    if(argc==9 && std::string_view(argv[1])=="emit") {
        auto analysis=vd::analyze_project(argv[2]);
        if(!analysis.succeeded())throw vd::CompilationError(analysis.diagnostics);
        auto module=analysis.program->lower();
        module.async_root=std::string_view(argv[7])=="async";
        bool optimize=std::string_view(argv[6])=="O2";
        auto map=vd::emit_publication_object(module,argv[3],optimize,argv[8]);
        // Python owns final-link/split/strip/audit sequencing. Keep the draft
        // in memory until final bytes exist; no draft map goes to disk.
        std::cout<<"READY\n"<<std::flush;std::string ready;
        if(!std::getline(std::cin,ready) || ready!="FINAL")throw std::runtime_error("No final image");
        map.bind_image(read(argv[5]));auto bytes=map.encode();
        std::ofstream out(argv[4],std::ios::binary);out.write(bytes.data(),bytes.size());out.close();
        require(bool(out),"Private map write failed");
        for(const auto& site:map.sites())require(!site.path.empty(),"Lost original source mapping");
        std::cout<<"MAP "<<map.build_id()<<" "<<map.image_sha256()<<" "<<map.sites().size()<<'\n';return 0;
    }
    if(argc==5 && std::string_view(argv[1])=="resolve") {
        auto map=read(argv[2]),image=read(argv[3]),tokens=read(argv[4]);std::size_t count=0;
        while(!tokens.empty()) {
            auto begin=tokens.find("VOID-D1 ");if(begin==tokens.npos)break;
            tokens.erase(0,begin);auto end=tokens.find('\n');require(end!=tokens.npos,"Truncated public token");
            auto site=PrivateMap::resolve(map,image,std::string_view(tokens).substr(0,end+1));
            std::cout<<static_cast<unsigned>(site.category)<<" "<<site.path<<":"<<site.line<<":"<<site.column<<" "<<site.function<<" "<<site.description<<"\n"<<site.excerpt<<"\n";
            tokens.erase(0,end+1);++count;
        }
        require(count>0,"No diagnostic tokens resolved");return 0;
    }
    if(argc==6 && std::string_view(argv[1])=="development") {
        auto analysis=vd::analyze_project(argv[2]);if(!analysis.succeeded())throw vd::CompilationError(analysis.diagnostics);
        auto module=analysis.program->lower();module.async_root=std::string_view(argv[5])=="async";
        vd::emit_object(module,argv[3],std::string_view(argv[4])=="O2",true,{},vd::project::DebugInfo::full);return 0;
    }
    PrivateMap map(std::string(64,'a'));
    Site original{Category::arithmetic,"/canary.void::secret","/private/canary.void","return value / zero","Division by zero",7,9};
    auto failure=map.add(original),frame=map.add({Category::call,"caller","/private/canary.void","return secret()","source call",11,4});
    require(failure!=frame,"Site identities collide");rejects([&]{map.encode();});
    map.bind_image("exact final image");auto bytes=map.encode();auto round=PrivateMap::decode(bytes);
    require(round.encode()==bytes,"Map round trip changed");
    auto site=PrivateMap::resolve(bytes,"exact final image",failure);
    require(site.path==original.path && site.excerpt==original.excerpt && site.function==original.function && site.line==7 && site.column==9,"Private mapping changed");
    rejects([&]{map.add(original);});rejects([&]{map.bind_image("other");});
    rejects([&]{PrivateMap::resolve({},"exact final image",failure);});
    rejects([&]{PrivateMap::resolve(bytes,"wrong image",failure);});
    PrivateMap other(std::string(64,'b'));auto wrong=other.add(original);other.bind_image("exact final image");
    rejects([&]{PrivateMap::resolve(other.encode(),"exact final image",failure);});
    rejects([&]{PrivateMap::resolve(bytes,"exact final image",wrong);});
    require(category("Division by zero")==Category::arithmetic && category("Null supplied")==Category::null_value && category("index out of bounds")==Category::bounds,"Public category contract changed");
    auto unknown=failure;unknown.replace(unknown.find(" 1 "),3," 999999 ");
    rejects([&]{PrivateMap::resolve(bytes,"exact final image",unknown);});
    auto wrong_category=failure;wrong_category[wrong_category.size()-2]='1';
    rejects([&]{PrivateMap::resolve(bytes,"exact final image",wrong_category);});
    for(auto token:{failure+"x",std::string("VOID-D2 ")+failure.substr(8),failure.substr(0,failure.size()-1)})rejects([&]{PrivateMap::resolve(bytes,"exact final image",token);});
    // Exact tokens reject alternate spellings, overflow, separators and NULs.
    const auto prefix=failure.substr(0,failure.find(" 1 ")+1);
    for(const auto* tail:{"0 2\n","01 2\n","+1 2\n","-1 2\n","4294967296 2\n","1 02\n","1 +2\n","1 2\r\n","1 2\nextra","1 2 \n","1\t2\n","1  2\n","1 7\n"})
        rejects([&]{PrivateMap::resolve(bytes,"exact final image",prefix+tail);});
    rejects([&]{PrivateMap::resolve(bytes,"exact final image",failure+std::string(1,'\0'));});
    auto upper=failure;upper[8]='A';rejects([&]{PrivateMap::resolve(bytes,"exact final image",upper);});
    rejects([&]{PrivateMap empty(std::string(64,'a'));empty.bind_image({});});
    // Serialized identity fields, counts and categories cannot bypass limits.
    auto put=[](std::string& out,std::size_t offset,std::uint32_t value) {
        for(unsigned i=0;i<4;++i)out[offset+i]=char(value>>(8*i));
    };
    for(auto value:{0u,65u,1024u*1024u+1u,0xffffffffu}) {
        auto invalid=bytes;put(invalid,24,value);rejects([&]{PrivateMap::decode(invalid);});
    }
    for(auto value:{65537u,0xffffffffu}) {
        auto invalid=bytes;put(invalid,160,value);rejects([&]{PrivateMap::decode(invalid);});
    }
    for(auto value:{0u,7u,0xffffffffu}) {
        auto invalid=bytes;put(invalid,164,value);rejects([&]{PrivateMap::decode(invalid);});
    }
    PrivateMap total(std::string(64,'d'));
    Site large{Category::runtime,std::string(1024*1024,'x')};
    for(unsigned i=0;i<15;++i)(void)total.add(large);
    rejects([&]{total.add(large);});
    total.bind_image("bounded image");require(total.encode().size()<=map_byte_limit,"Encoded total exceeded limit");
    for(std::size_t n=0;n<bytes.size();++n)rejects([&]{PrivateMap::decode(std::string_view(bytes).substr(0,n));});
    rejects([&]{PrivateMap::decode(bytes+"x");});
    auto corrupt=bytes;corrupt[0]='X';rejects([&]{PrivateMap::decode(corrupt);});
    corrupt=bytes;corrupt[24]=char(0xff);rejects([&]{PrivateMap::decode(corrupt);});
    rejects([&]{PrivateMap::decode(std::string(map_byte_limit+1,'x'));});
    rejects([&]{PrivateMap bad("not-a-digest");});
    PrivateMap bounded(std::string(64,'c'));for(std::size_t i=0;i<site_limit;++i)(void)bounded.add({});rejects([&]{bounded.add({});});
    rejects([&]{PrivateMap m(std::string(64,'c'));m.add({Category::runtime,{}, {},std::string(1024*1024+1,'x')});});
    std::cout<<"Private diagnostic map bounds, identities and round trips passed\n";return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;} }
