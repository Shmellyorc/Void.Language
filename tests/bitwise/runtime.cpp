#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/numeric.hpp"
#include <iostream>
#include <stdexcept>
#include <array>

namespace {
unsigned checks=0;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);++checks;}
void rejects(vd::ir::Module module){bool rejected=false;try{vd::ir::validate(module);}catch(const std::invalid_argument& e){rejected=std::string(e.what()).find("invalid integer shift operation")!=std::string::npos;if(!rejected)std::cerr<<"Unexpected rejection: "<<e.what()<<"\n";}check(rejected,"invalid shift IR accepted");}
}
int main(){
    using namespace vd;using namespace vd::ir;
    try {
        const std::array<Type,8> types{Type::i8,Type::u8,Type::i16,Type::u16,Type::i32,Type::u32,Type::i64,Type::u64};
        for(auto t:types)for(bool left:{false,true}) {
            const auto width=numeric::width(t);const auto sign=numeric::signed_integer(t);
            const auto mask=~std::uint64_t{}>>(64-width);
            const auto bits=sign?((std::uint64_t{1}<<(width-1))|5):mask-4;
            auto value=numeric::integer_text(bits,t);
            const auto glyph=left?"<<":">>";
            auto source="func entry(count: i32) -> i32:\n    var value: "+std::string(type_name(t))+" = "+value+"\n    return ((value "+glyph+" count) & 127) as i32\n";
            auto module=compile_source({"/bitwise.void",source});
            validate(module);
            for(bool optimize:{false,true}) {
                Jit jit;auto code=jit.compile(module,"entry",optimize);
                for(int count:{-1025,-65,-64,-33,-32,-17,-16,-9,-8,-1,0,1,7,8,15,16,31,32,63,64,65,1024}) {
                    const auto n=static_cast<unsigned>(count)&(width-1);
                    // Independent unsigned reference; no host signed-shift or
                    // overflow assumptions, including n=0 and 64-bit values.
                    auto expected=left?(bits<<n)&mask:bits>>n;
                    if(!left && sign && n)expected|=(mask<<(width-n))&mask;
                    check(code->invoke(count)==static_cast<std::int32_t>(expected&127),"JIT dynamic shift differs from bit-pattern model");
                    check((numeric::binary(left?Op::shift_left:Op::shift_right,bits,static_cast<std::uint64_t>(count),t)&127)==(expected&127),"constant kernel differs from independent model");
                }
            }
            auto bad=module;
            bool changed=false;
            for(auto& f:bad.functions)for(auto& b:f.blocks)for(auto& i:b.instructions)if(i.op==Op::shift_left || i.op==Op::shift_right){i.b=i.a;i.immediate=1;changed=true;}
            check(changed,"no shift operation lowered");rejects(std::move(bad));
        }
        // Type-forged operands must fail before entering LLVM.
        for(bool value:{false,true}) {
            Module module;module.identity="bitwise-ir";Function f;f.name="bad";f.result=Type::i32;
            Instruction a{Op::constant};a.type=value?Type::boolean:Type::i32;
            Instruction b{Op::constant};b.type=value?Type::i32:Type::boolean;
            f.blocks={{{a,b,Instruction{Op::shift_right,0,1}},{Terminator::Kind::return_value,2}}};module.functions={f};rejects(std::move(module));
        }
        std::cout<<"G81: "<<checks<<" dynamic JIT/constant-model/IR checks\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
