#include "void/frontend.hpp"
#include "void/private_fixed_call_research.hpp"
#include <fstream>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=5) return 1;
    try {
        const auto extent=std::stoull(argv[4]);
        auto analysis=vd::analyze_source({"private-fixed-shape.void",
            "func shape(a: [u64, "+std::to_string(extent)+"]) -> [u64, "+std::to_string(extent)+"]:\n    return a\n"});
        if(!analysis.succeeded()) throw vd::CompilationError(analysis.diagnostics);
        auto f=analysis.program->lower().functions.front();
        f.fixed_ownership=true;f.parameters.assign(8,f.result);f.locals.clear();
        f.blocks={{{{vd::ir::Op::argument,static_cast<unsigned>(std::stoul(argv[3]))}},{vd::ir::Terminator::Kind::return_value,0}}};
        vd::ir::Module input{"private-fixed-call",{},{f}};input.coordinated=true;
        vd::llvm_backend::emit_private_fixed_call_research(input,argv[1],std::string(argv[2])=="optimized");
        auto rejected=std::filesystem::path(std::string(argv[1])+".rejected");
        for(unsigned negative=0;negative<3;++negative) {
            auto bad=input;
            if(negative==0) bad.functions[0].fixed_ownership=false;
            else if(negative==1) bad.functions[0].blocks[0].instructions[0].a=8;
            else {
                bad.functions[0].blocks[0].instructions.push_back({vd::ir::Op::fixed_snapshot,0});
                bad.functions[0].blocks[0].end.value=1;
            }
            {std::ofstream sentinel(rejected);sentinel<<"owner sentinel";}
            bool denied=false;
            try {vd::llvm_backend::emit_private_fixed_call_research(bad,rejected,false);}
            catch(const std::invalid_argument&) {denied=true;}
            std::ifstream saved(rejected);std::string contents{std::istreambuf_iterator<char>(saved),{}};
            if(!denied || contents!="owner sentinel") throw std::runtime_error("invalid input changed output");
        }
        std::filesystem::remove(rejected);
        std::cout<<"Private fixed-call object emitted; invalid inputs rejected before output\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
