#include "void/tooling.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
auto analyze(const std::filesystem::path& root) {
    auto r=vd::analyze_project(root/"main.void");
    if(!r.succeeded()) throw vd::CompilationError(r.diagnostics);
    return r.program;
}
void edit(const std::filesystem::path& path,const std::string& from,const std::string& to) {
    auto text=vd::read_source(path).text();
    auto position=text.find(from);
    if(position==std::string::npos) throw std::runtime_error("Expected demonstration source edit is absent");
    text.replace(position,from.size(),to);
    std::ofstream out(path,std::ios::binary); out<<text;
    if(!out) throw std::runtime_error("Cannot write demonstration fixture");
}
}
// Run against a disposable COPY of examples/analysis: deliberately edits two files.
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("Supply a disposable copied analysis project directory");
        std::filesystem::path root=argv[1];
        auto before=analyze(root); vd::ToolingIndex index(before);
        auto declaration=index.declaration("/worker.void::calculate");
        if(!declaration) throw std::runtime_error("Missing calculate declaration");
        auto references=index.references_to(declaration->identity);
        std::cout<<declaration->identity<<" declared in "<<declaration->module<<"; references: "<<references.size()<<'\n';
        edit(root/"worker.void","stable + 1","stable + 9");
        auto provisional=vd::plan_invalidation(*before,{"/worker.void"});
        if(!provisional.requires_full_reanalysis || provisional.source_compatibility!=vd::SourceCompatibility::unknown)
            throw std::runtime_error("Unvalidated edit was authorized");
        auto body=analyze(root); auto validated=vd::plan_invalidation(*before,{},body.get());
        if(validated.source_compatibility!=vd::SourceCompatibility::exact_match) throw std::runtime_error("Body edit incompatible");
        std::cout<<"edit plan: full validation required; fresh body source contract: exact match\n";
        edit(root/"values.void","= 40","= 41");
        auto constant=analyze(root); auto incompatible=vd::plan_invalidation(*body,{},constant.get());
        if(incompatible.source_compatibility!=vd::SourceCompatibility::restart_required) throw std::runtime_error("Constant edit accepted");
        std::cout<<"fresh constant source contract: restart required; no native generation modified\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
