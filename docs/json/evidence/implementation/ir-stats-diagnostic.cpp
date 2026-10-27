#include "void/frontend.hpp"
#include <iostream>
#include <vector>
#include <tuple>
#include <algorithm>
int main(int argc,char** argv){
 auto analysis=vd::analyze_project(argv[1]);
 if(!analysis.succeeded())throw vd::CompilationError(analysis.diagnostics);
 auto module=analysis.program->lower();
 std::vector<std::tuple<size_t,size_t,size_t,size_t,std::string>> rows;
 for(auto& f:module.functions){size_t count=0,largest=0;for(auto& b:f.blocks){count+=b.instructions.size();largest=std::max(largest,b.instructions.size());}rows.emplace_back(largest,count,f.locals.size(),f.blocks.size(),f.name);}
 std::sort(rows.rbegin(),rows.rend());
 for(auto& [largest,count,locals,blocks,name]:rows)std::cout<<largest<<'\t'<<count<<'\t'<<locals<<'\t'<<blocks<<'\t'<<name<<'\n';
}
