#include "void/ir.hpp"
#include "void/math_ops.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
using namespace vd::ir;
unsigned checks=0;
Module fixture(Type type, const vd::math::Operation& contract) {
    Module module;
    module.identity="math-independent";
    Function function;
    function.module_identity=std::string(vd::math::source_module);
    function.source_name="probe";
    function.name=function.module_identity+"::probe";
    function.parameters.clear();
    function.result=type;
    Instruction first{Op::constant};first.type=type;
    Instruction second=first;
    Instruction operation{Op::math,0,contract.arity==2?1u:0u};
    operation.type=type;
    operation.immediate=static_cast<std::int64_t>(contract.kind);
    function.blocks={{{first,second,operation},{Terminator::Kind::return_value,2}}};
    module.functions={function};
    return module;
}
void reject(Module module,const std::function<void(Function&,Instruction&)>& mutate) {
    auto& function=module.functions.at(0);
    mutate(function,function.blocks.at(0).instructions.at(2));
    try { validate(module); }
    catch(const std::invalid_argument&) {++checks;return;}
    throw std::runtime_error("forged scalar Math IR accepted");
}
}

int main() {
    try {
        // Exercise every tag/precision without frontend analysis or lowering.
        for(auto type:{Type::f32,Type::f64}) for(const auto& contract:vd::math::operations) {
            const auto valid=fixture(type,contract);
            validate(valid);++checks;
            // Const scalar parameters can carry readonly storage metadata. The
            // operation copies their scalar value and returns an ordinary value.
            auto readonly=valid;
            readonly.functions[0].blocks[0].instructions[0].type.readonly=true;
            if(contract.arity==2)readonly.functions[0].blocks[0].instructions[1].type.readonly=true;
            validate(readonly);++checks;
            for(const auto* owner:{"","/main.void","/math.void","/@stdlib/math.void.fake","/@stdlib/math.void::forged"}) {
                reject(valid,[&](auto& function,auto&) {
                    function.module_identity=owner;
                    function.name=function.module_identity.empty()?function.source_name:function.module_identity+"::"+function.source_name;
                });
            }
            reject(valid,[](auto&,auto& instruction){instruction.immediate=-1;});
            reject(valid,[](auto&,auto& instruction){instruction.immediate=9999;});
            reject(valid,[](auto&,auto& instruction){instruction.a=99;});
            reject(valid,[](auto&,auto& instruction){instruction.type=Type::i32;});
            reject(valid,[&](auto&,auto& instruction){instruction.type=type==Type::f32?Type::f64:Type::f32;});
            reject(valid,[](auto&,auto& instruction){instruction.type.readonly=true;});
            reject(valid,[](auto&,auto& instruction){instruction.type.nullable=true;});
            reject(valid,[](auto&,auto& instruction){instruction.type.construction=true;});
            reject(valid,[](auto&,auto& instruction){instruction.type.preparation="forged";});
            reject(valid,[](auto&,auto& instruction){instruction.type.readonly_references.insert(ReferencePath{});});
            reject(valid,[](auto&,auto& instruction){instruction.arguments={0};});
            reject(valid,[](auto&,auto& instruction){instruction.callee="user-selected-native-symbol";});
            reject(valid,[](auto&,auto& instruction){instruction.receiver=Place{0,{}};});
            reject(valid,[](auto&,auto& instruction){instruction.initialization=true;});
            reject(valid,[](auto&,auto& instruction){instruction.array_repeat=true;});
            reject(valid,[](auto&,auto& instruction){instruction.operator_glyph="+";});
            reject(valid,[](auto& function,auto&){function.blocks[0].instructions[0].type=Type::i32;});
            reject(valid,[&](auto& function,auto&){function.blocks[0].instructions[0].type=type==Type::f32?Type::f64:Type::f32;});
            if(contract.arity==1) {
                reject(valid,[](auto&,auto& instruction){instruction.b=1;});
            } else {
                reject(valid,[](auto&,auto& instruction){instruction.b=99;});
                reject(valid,[](auto& function,auto&){function.blocks[0].instructions[1].type=Type::i32;});
                reject(valid,[&](auto& function,auto&){function.blocks[0].instructions[1].type=type==Type::f32?Type::f64:Type::f32;});
            }
        }
        // Same-width integers, booleans, nominal values and vectors cannot gain
        // access by forging agreement between both operands and the result.
        const auto& identity=vd::math::operations.front();
        for(auto type:{Type::i8,Type::i16,Type::i32,Type::i64,Type::u8,Type::u16,Type::u32,Type::u64,Type::boolean}) {
            auto invalid=fixture(type,identity);
            reject(std::move(invalid),[](auto&,auto&){});
        }
        auto nominal=std::make_shared<ValueType>();nominal->identity=nominal->name="forged-nominal";
        auto aggregate=fixture(Type(nominal),identity);
        aggregate.functions[0].blocks[0].instructions[0].op=Op::aggregate;
        aggregate.functions[0].blocks[0].instructions[1].op=Op::aggregate;
        reject(std::move(aggregate),[](auto&,auto&){});
        auto vector=std::make_shared<ValueType>();
        vector->identity=vector->name=array_identity(Type::f32,2);
        vector->element=FieldType(Type::f32);vector->extent=2;
        // aggregate builds a legal vector before the forged operation, so the
        // Math boundary itself has to reject its complete operand/result shape.
        auto invalid=fixture(Type(vector,Type::fixed_array),identity);
        auto& instructions=invalid.functions[0].blocks[0].instructions;
        instructions[0].op=Op::fixed_array;instructions[0].array_repeat=true;
        Instruction zero{Op::constant};zero.type=Type::f32;
        instructions.insert(instructions.begin(),zero);
        instructions[1].arguments={0};
        instructions[2]=instructions[1];
        instructions[3].a=1;
        invalid.functions[0].blocks[0].end.value=3;
        try {validate(invalid);throw std::runtime_error("forged vector Math IR accepted");}
        catch(const std::invalid_argument&) {++checks;}
        std::cout<<checks<<" independent scalar Math IR checks passed\n";
        return 0;
    } catch(const std::exception& failure) {
        std::cerr<<failure.what()<<'\n';
        return 1;
    }
}
