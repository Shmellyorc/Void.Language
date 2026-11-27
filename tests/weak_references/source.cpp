#include "../storage_proof.hpp"
#include "void/frontend.hpp"
#include "void/runtime.hpp"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition,const char* message) {
    if(!condition)throw std::runtime_error(message);
}

void reject_forged_operations(const vd::ir::Module& input) {
    using namespace vd::ir;
    auto module=input;
    Type other_cell=Type::none;
    for(auto type:module.nominal_types)
        if(type.definition->weak_target && Type::key(*type.definition->weak_target).ends_with("::resource"))
            other_cell=type;
    require(other_cell!=Type::none,"missing second weak cell type");
    auto reject=[&](Op operation,auto change) {
        module=input;
        for(auto& function:module.functions)for(auto& block:function.blocks)for(auto& op:block.instructions) {
            if(op.op!=operation || (operation==Op::weak_create && op.type==other_cell))continue;
            change(op);
            try { validate(module); } catch(const std::invalid_argument&) { return; }
            throw std::runtime_error("typed IR accepted an invalid weak operation");
        }
        throw std::runtime_error("expected weak operation missing from lowered IR");
    };
    reject(Op::weak_create,[&](Instruction& op){op.type=other_cell;});
    reject(Op::weak_create,[](Instruction& op){op.op=Op::gc_construct;op.a=0;});
    reject(Op::weak_lock,[](Instruction& op){op.type.nullable=false;});
}
}

int main(int argc,char** argv) {
    try {
        require(argc==2 || argc==4,"expected source path and optional native object output");
        auto program=vd::compile_project(argv[1]);
        if(argc==4) {
            storage_fixture::emit(program.module,argv[2],std::string_view(argv[3])=="optimized",false);
            return 0;
        }
        reject_forged_operations(program.module);
        for(bool optimized:{false,true}) {
            vd::Jit jit;
            auto runtime=std::make_shared<vd::Runtime>(1);
            auto code=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);
            require(code->invoke(0)==42,"live source promotion or generic inference failed");
            require(code->invoke(1)==1,"expired cycle or collection roots failed");
            require(code->invoke(2)==6,"deterministic cleanup did not expire the target");
        }
    } catch(const std::exception& failure) {
        std::cerr<<failure.what()<<'\n';
        return 1;
    }
}
