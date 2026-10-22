#include "void/backend.hpp"
#include "void/frontend.hpp"
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks=0;
void reject(vd::ir::Module module){try{vd::ir::validate(module);}catch(const std::invalid_argument&){++checks;return;}throw std::runtime_error("malformed temporal IR accepted");}
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::invalid_argument("expected temporal source fixture");
    auto analysis=vd::analyze_project(argv[1]);if(!analysis.succeeded())throw vd::CompilationError(analysis.diagnostics);auto original=analysis.program->lower();
    for(unsigned variant=0;variant<5;++variant){auto module=original;bool changed=false;
        for(auto& function:module.functions)for(auto& block:function.blocks)for(auto& operation:block.instructions)if(operation.op==vd::ir::Op::temporal_native && !changed){
            if(variant==0)operation.initialization=true;
            else if(variant==1)operation.immediate=9999;
            else if(variant==2)operation.array_repeat=true;
            else if(variant==3)operation.arguments.push_back(operation.arguments.at(0));
            else operation.callee="untrusted-temporal-callback";
            changed=true;
        }
        if(!changed)throw std::runtime_error("temporal fixture has no native operation");reject(std::move(module));
    }
    auto source=std::string("use time\nfunc work(input: i32) -> i32:\n    if make_date(2024, 2, 29) is Ok(var value):\n        return string(value).length as i32\n    return 0\nasync func entry(input: i32) -> i32:\n    var task = spawn work(input)\n    return await task\n");
    const auto fixture=std::filesystem::absolute(argv[1]);vd::SourceOverrides overrides;overrides.emplace("/"+fixture.filename().string(),vd::SourceFile(fixture.string(),source));
    auto worker=vd::analyze_project(fixture,overrides);if(!worker.succeeded())throw vd::CompilationError(worker.diagnostics);
    for(bool optimized:{false,true}){vd::Jit jit;if(jit.compile_async(*worker.program,optimized)->invoke(4)!=10)throw std::runtime_error("worker pure temporal formatting");++checks;}
    source="use time\nfunc work(input: i32) -> i32:\n    if now_utc() is Ok(var value):\n        return 1\n    return 0\nasync func entry(input: i32) -> i32:\n    var task = spawn work(input)\n    return await task\n";
    overrides.insert_or_assign("/"+fixture.filename().string(),vd::SourceFile(fixture.string(),source));if(vd::analyze_project(fixture,overrides).succeeded())throw std::runtime_error("worker Runtime clock accepted");++checks;
    std::cout<<checks<<" temporal IR/worker checks passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
