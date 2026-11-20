#include "../managed/probe.hpp"
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include <iostream>
#include <algorithm>
using managed_test::require;
int main(int argc,char** argv) {
    try {
        require(argc==2,"collection equality source root required");
        auto program=vd::compile_project(std::filesystem::path(argv[1])/"tests/collection_equality/lifecycle.void");
        auto imported=vd::compile_project(std::filesystem::path(argv[1])/"tests/collection_equality/imported/main.void");
        auto reject=[](const vd::ir::Module& module) {bool rejected=false;try{vd::ir::validate(module);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"forged derived equality provenance accepted");};
        const auto& valid=imported.module;
        auto adapter=std::find_if(valid.functions.begin(),valid.functions.end(),[](const auto& f){return f.name.find("::$equality$")!=std::string::npos;});
        require(adapter!=valid.functions.end(),"generic collection equality adapter missing");
        auto bad=valid;
        for(auto& f:bad.functions)if(f.name==adapter->name)f.lexical_parent.clear();
        reject(bad);
        bad=valid;bool forged=false;
        for(auto& f:bad.functions)if(f.name!=adapter->lexical_parent)for(auto& b:f.blocks)for(auto& i:b.instructions)
            if(i.op==vd::ir::Op::call && i.callee==adapter->lexical_parent){i.callee=adapter->name;forged=true;}
        require(forged,"independent wrong-parent call not exercised");reject(bad);
        bad=valid;for(auto& f:bad.functions)if(f.name==adapter->name)f.visibility="public";reject(bad);
        vd::Jit jit;
        for(bool optimized:{false,true}) {
            auto runtime=std::make_shared<vd::Runtime>(1);
            auto code=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);
            auto independent=jit.compile(program.module,program.module.entry_symbol,optimized);
            require(independent->runtime_owner()!=runtime,"collection equality Runtime isolation");
            auto empty=[&] {vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);runtime->collect();require(runtime->snapshot().objects==0,"collection comparison retained cycle/storage");};
            for(bool forced:{false,true}) {
                managed_test::Probe p;p.inspect_roots=true;p.collect_before=forced;
                {managed_test::Observe observe(p);require(code->invoke(17)==17,"collection equality GC/alias/custom result");}
                require(p.roots_valid && p.roots_checked && p.commits_valid && p.frames.empty(),"collection equality precise roots/publication");empty();
            }
            managed_test::Probe p;p.inspect_roots=true;p.fail_construction=15;p.fail_allocation=0;bool failed=false;
            {managed_test::Observe observe(p);try{code->invoke(17);}catch(const std::exception&){failed=true;}}
            require(failed && p.roots_valid && p.frames.empty(),"collection equality failure cleanup");empty();
            require(code->invoke(17)==17 && independent->invoke(29)==29,"collection equality failure recovery/isolation");empty();
        }
        std::cout<<"Collection equality: precise roots, cycles, aliases, custom allocation, failure/retry and Runtime isolation passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
