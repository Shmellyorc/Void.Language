#include "void/ir.hpp"
#include <iostream>

int main() {
    using namespace vd::ir;
    Module module;module.identity="/main.void";
    Function f;f.source_name="xor";f.result=Type::u64;
    Instruction a{Op::constant};a.type=Type::u64;
    Instruction operation{Op::random_xor,0,0};operation.type=Type::u64;
    f.blocks={{{a,operation},{Terminator::Kind::return_value,1}}};
    auto outcome=std::make_shared<ValueType>();outcome->identity="$random-seed-result";outcome->name="result";
    outcome->result_type=true;outcome->fields={{"tag",Type::i32},{"value",Type::u64},{"error",Type::i32}};
    outcome->cases={{"Ok",1,1},{"Err",2,1}};
    for(auto op:{Op::random_xor,Op::random_seed}) {
        if(op==Op::random_seed) {
            module.coordinated=true;f.source_name="seed";f.result=Type(outcome);
            Instruction seed{Op::random_seed};seed.type=f.result;
            f.blocks={{{seed},{Terminator::Kind::return_value,0}}};
        }
        for(auto owner:{"/@stdlib/random.void","","/random.void","/@stdlib/random.void.fake"}) {
            f.module_identity=owner;f.name=owner[0]?f.module_identity+"::"+f.source_name:f.source_name;
            module.functions={f};
            try{validate(module);std::cout<<(op==Op::random_xor?"xor":"seed")<<" owner='"<<owner<<"' accepted\n";}
            catch(const std::exception& error){std::cout<<(op==Op::random_xor?"xor":"seed")<<" owner='"<<owner<<"' rejected: "<<error.what()<<'\n';}
        }
    }
}
