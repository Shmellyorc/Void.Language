// Optional acceptance probe. Link against the same void_core library as voidls.
#include "void/frontend.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <algorithm>
namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    auto root=fs::absolute(argv[1]);if(!fs::create_directory(root))throw std::runtime_error("Probe directory must not already exist");
    auto write=[](const fs::path& p,const std::string& t){fs::create_directories(p.parent_path());std::ofstream(p)<<t;};
    const std::string main="func entry(x: i32) -> i32:\n    return x\n";
    write(root/"main.void",main);
    for(int count:{10,1000,10000}) {
        fs::remove_all(root/"files");fs::create_directories(root/"files");
        for(int n=0;n<count;++n)write(root/"files"/("file"+std::to_string(100000+n)+".void"),"invalid candidate bodies must not be parsed\n");
        auto irrelevant=root/"irrelevant";for(int d=0;d<40;++d)irrelevant/="deep";
        write(irrelevant/"ignored.void",main);
        vd::SourceOverrides overlays;overlays["/main.void"]=vd::SourceFile("main.void","use \"files/");
        for(int n=0;n<1000;++n)overlays["/virtual/deep/overlay"+std::to_string(n)+".void"]=vd::SourceFile("virtual.void",main);
        std::vector<double> metadata,semantic;
        auto samples=[&](auto query,std::vector<double>& values){for(int n=0;n<7;++n){auto start=Clock::now();query();values.push_back(std::chrono::duration<double,std::milli>(Clock::now()-start).count());}};
        std::size_t results=0;bool incomplete=false;
        samples([&]{auto q=vd::editor_assistance(root/"main.void",overlays,"/main.void",11);results=q.candidates.size();incomplete=q.incomplete;if(!q.import_context || results!=std::min(count,256))throw std::runtime_error("invalid metadata query");},metadata);
        samples([&]{if(!vd::analyze_project(root/"main.void").program)throw std::runtime_error("semantic baseline");},semantic);
        auto prefix="use \"files/file"+std::to_string(100000+count-1);
        overlays["/main.void"]=vd::SourceFile("main.void",prefix);
        if(vd::editor_assistance(root/"main.void",overlays,"/main.void",prefix.size()).candidates.size()!=1)throw std::runtime_error("prefix lost beyond cap");
        auto cold=metadata.front();std::sort(metadata.begin(),metadata.end());std::sort(semantic.begin(),semantic.end());
        std::cout<<"{\"siblings\":"<<count<<",\"overlays\":1000,\"metadata_first_ms\":"<<cold<<",\"metadata_median_ms\":"<<metadata[3]<<",\"metadata_max_ms\":"<<metadata.back()<<",\"semantic_median_ms\":"<<semantic[3]<<",\"results\":"<<results<<",\"incomplete\":"<<(incomplete?"true":"false")<<"}\n";
    }
    fs::remove_all(root);
}
