#include "void/format.hpp"
#include <bit>
#include <cfenv>
#include <clocale>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace vd::formatting;
namespace {
unsigned checks=0;
void check(Number n,std::string_view spec,std::string_view expected) {
    auto actual=number(n,spec);++checks;
    if(actual!=expected)throw std::runtime_error("bits="+std::to_string(n.bits)+" spec="+std::string(spec)+" expected="+std::string(expected)+" actual="+actual);
}
void real(double n,std::string_view spec,std::string_view expected) {check({NumberKind::f64,std::bit_cast<std::uint64_t>(n)},spec,expected);}
void vectors() {
#include "vectors.inc"
}
void shortest_roundtrips() {
    std::uint64_t state=UINT64_C(0x564f4944);
    for(unsigned n=0;n<4096;++n) {
        state^=state<<13;state^=state>>7;state^=state<<17;
        for(bool narrow:{false,true}) {
            Number value{narrow?NumberKind::f32:NumberKind::f64,narrow?static_cast<std::uint32_t>(state):state};
            auto rendered=number(value);
            if(narrow) {
                auto bits=static_cast<std::uint32_t>(value.bits);
                if((bits&UINT32_C(0x7f800000))==UINT32_C(0x7f800000))continue;
                char* end=nullptr;float parsed=std::strtof(rendered.c_str(),&end);
                if(end!=rendered.data()+rendered.size() || std::bit_cast<std::uint32_t>(parsed)!=bits)throw std::runtime_error("f32 shortest roundtrip");
            } else {
                if((value.bits&UINT64_C(0x7ff0000000000000))==UINT64_C(0x7ff0000000000000))continue;
                char* end=nullptr;double parsed=std::strtod(rendered.c_str(),&end);
                if(end!=rendered.data()+rendered.size() || std::bit_cast<std::uint64_t>(parsed)!=value.bits)throw std::runtime_error("f64 shortest roundtrip");
            }
            ++checks;
        }
    }
}
}
int main() {
    try {
        vectors();
        for(int mode:{FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO,FE_TONEAREST}) {
            if(std::fesetround(mode)==0)vectors();
        }
        std::fesetround(FE_TONEAREST);
        for(const char* locale:{"de_DE.UTF-8","fr_FR.UTF-8","C.UTF-8"})if(std::setlocale(LC_ALL,locale))vectors();
        std::setlocale(LC_ALL,"C");
        shortest_roundtrips();
        real(2.5,"0","2");real(3.5,"0","4");real(2.675,"0.00","2.67");
        real(-0.0,"","-0");real(1e-5,"","1e-05");real(1e-4,"","0.0001");
        real(1e15,"","1000000000000000");real(1e16,"","1e+16");
        real(1250.5,"#,##0.00","1,250.50");real(1.125,"0.00#","1.125");real(1.0,"0.00#","1.00");
        real(-1.5,"'USD '0.00","-USD 1.50");real(0.015,"P0","1%");
        real(-1.5,"'USD -'0.00","USD -1.50");real(1.5,"+'USD +'0.00","USD +1.50");
        real(9.999,"0.00e+00","1.00e+01");real(0.001,"E2","1.00E-03");
        real(std::numeric_limits<double>::infinity(),"P2","inf%");
        real(-std::numeric_limits<double>::infinity(),">8|F2","    -inf");
        check({NumberKind::i8,255},"X","FF");check({NumberKind::i8,255},"X4","00FF");
        check({NumberKind::i8,128},"","-128");check({NumberKind::i64,UINT64_C(9223372036854775808)},"","-9223372036854775808");
        check({NumberKind::u64,UINT64_MAX},"","18446744073709551615");
        check({NumberKind::i8,127},"","127");check({NumberKind::i16,32767},"","32767");
        check({NumberKind::i32,UINT64_C(2147483647)},"","2147483647");check({NumberKind::i64,UINT64_C(9223372036854775807)},"","9223372036854775807");
        check({NumberKind::i16,32768},"","-32768");check({NumberKind::i32,UINT64_C(2147483648)},"","-2147483648");
        check({NumberKind::u8,255},"","255");check({NumberKind::u16,65535},"","65535");check({NumberKind::u32,UINT64_C(4294967295)},"","4294967295");
        for(auto k:{NumberKind::i8,NumberKind::i16,NumberKind::i32,NumberKind::i64,NumberKind::u8,NumberKind::u16,NumberKind::u32,NumberKind::u64}){check({k,42},"D4","0042");check({k,42},"B8","00101010");}
        if(align("\xc3\xa9\xf0\x9f\x98\x80",{'^',5})!=" \xc3\xa9\xf0\x9f\x98\x80  ")throw std::runtime_error("scalar alignment");
        for(auto bad:{"0,00","0,,000","0.","0.#0","0.0%foo","0e+0","0e+00%","F-1",">999999999999999999999999999999999999|0",";0","0.0.0"}){
            bool rejected=false;try{parse_numeric(bad);}catch(const Error&){rejected=true;}if(!rejected)throw std::runtime_error("invalid pattern accepted");++checks;
        }
        for(auto bad:{"D","X","B"}){bool rejected=false;try{number({NumberKind::f64,0},bad);}catch(const Error&){rejected=true;}if(!rejected)throw std::runtime_error("float accepted integer pattern");}
        for(auto kind:{Plan::hexadecimal,Plan::binary}){Plan plan;plan.kind=kind;bool rejected=false;try{number({NumberKind::f64,0},plan);}catch(const Error&){rejected=true;}if(!rejected)throw std::runtime_error("float accepted direct base plan");++checks;}
        real(0.0,"F2000",std::string("0.")+std::string(2000,'0'));
        Builder b;b.append("prefix");bool rejected=false;try{b.repeat('x',std::numeric_limits<std::size_t>::max());}catch(const std::length_error&){rejected=true;}if(!rejected||b.size()!=6)throw std::runtime_error("builder overflow");
        std::cout<<checks<<" numeric format checks passed\n";
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
