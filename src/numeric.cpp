#include <map>
#include "void/numeric.hpp"
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APSInt.h>
#include <llvm/Support/Error.h>
#include <stdexcept>
namespace vd::numeric {
using ir::Type;
formatting::Number display_number(std::uint64_t bits,Type t) {
    switch(t.kind) {
#define VD_DISPLAY(name) case Type::name:return {formatting::NumberKind::name,bits}
    VD_DISPLAY(i8);VD_DISPLAY(i16);VD_DISPLAY(i32);VD_DISPLAY(i64);
    VD_DISPLAY(u8);VD_DISPLAY(u16);VD_DISPLAY(u32);VD_DISPLAY(u64);
    VD_DISPLAY(f32);VD_DISPLAY(f64);
#undef VD_DISPLAY
    default:throw std::invalid_argument("Display requires a numeric type");
    }
}
bool floating(Type t) { return t == Type::f32 || t == Type::f64; }
bool signed_integer(Type t) { return t == Type::i8 || t == Type::i16 || t == Type::i32 || t == Type::i64; }
bool integer(Type t) { return signed_integer(t) || t == Type::u8 || t == Type::u16 || t == Type::u32 || t == Type::u64; }
bool number(Type t) { return integer(t) || floating(t); }
unsigned width(Type t) {
    switch(t.kind) {
    case Type::i8: case Type::u8: return 8;
    case Type::i16: case Type::u16: return 16;
    case Type::i32: case Type::u32: case Type::f32: return 32;
    case Type::i64: case Type::u64: case Type::f64: return 64;
    case Type::boolean: return 1;
    default: throw std::invalid_argument("Not a numeric type");
    }
}
bool supports(ir::Op op, Type t) {
    using ir::Op;
    switch (op) {
    case Op::equal: case Op::not_equal: return number(t) || t == Type::boolean;
    case Op::remainder: return integer(t);
    case Op::add: case Op::subtract: case Op::multiply: case Op::divide:
    case Op::less: case Op::less_equal: case Op::greater: case Op::greater_equal:
    case Op::negate: return number(t);
    case Op::logical_not: return t == Type::boolean;
    default: return false;
    }
}
static const llvm::fltSemantics& semantics(Type t) { return t == Type::f32 ? llvm::APFloat::IEEEsingle() : llvm::APFloat::IEEEdouble(); }
static llvm::APFloat fp(std::uint64_t v, Type t) { return llvm::APFloat(semantics(t), llvm::APInt(width(t), v)); }
static std::uint64_t bits(const llvm::APFloat& v) { return v.bitcastToAPInt().getZExtValue(); }
static constexpr auto nearest = llvm::APFloat::rmNearestTiesToEven;
std::uint64_t literal(std::string text, Type t, bool explicit_rounding) {
    bool decimal = text.find('.') != std::string::npos;
    if (!text.empty() && (text.back() == 'f' || text.back() == 'd')) { decimal = true; text.pop_back(); }
    if (decimal) {
        if (!floating(t)) throw std::invalid_argument("Floating literal requires a floating type; use an explicit as conversion");
        llvm::APFloat v(semantics(t));
        auto status = v.convertFromString(text, nearest);
        if (!status) { llvm::consumeError(status.takeError()); throw std::invalid_argument("Invalid floating literal"); }
        if (v.isInfinity()) throw std::invalid_argument("Floating literal is outside the finite destination range");
        return bits(v);
    }
    bool negative = !text.empty() && text.front() == '-';
    if (negative || (!text.empty() && text.front() == '+')) text.erase(0, 1);
    // Four bits per decimal digit plus a sign bit is a strict upper bound.
    llvm::APInt magnitude(static_cast<unsigned>(text.size() * 4 + 2), text, 10);
    auto value = negative ? -magnitude : magnitude;
    if (integer(t)) {
        bool fits = signed_integer(t) ? value.isSignedIntN(width(t)) : (!negative || magnitude.isZero()) && magnitude.isIntN(width(t));
        if (!fits) throw std::invalid_argument("Integer literal is outside the " + std::string(ir::type_name(t)) + " range; " + (negative && !signed_integer(t) ? "unsigned types cannot represent negative values" : "literal magnitude is too large for this destination"));
        return value.sextOrTrunc(width(t)).getZExtValue();
    }
    if (!floating(t)) throw std::invalid_argument("Type mismatch: numeric literal requires a numeric type");
    llvm::APFloat v(semantics(t));
    auto status = v.convertFromAPInt(value, true, nearest);
    // Context selects floating negation before a directly signed literal folds.
    // An integer binding has no signed zero, but a contextual floating -0 does.
    if (negative && magnitude.isZero()) v.changeSign();
    if (v.isInfinity() || (!explicit_rounding && (status & llvm::APFloat::opInexact)))
        throw std::invalid_argument("Integer literal is not exactly representable in " + std::string(ir::type_name(t)) + "; use as for explicit rounding");
    return bits(v);
}
std::uint64_t convert(std::uint64_t v, Type from, Type to) {
    if (!number(from) || !number(to)) throw std::invalid_argument("Numeric as requires numeric source and destination types");
    if (from == to) return v;
    if (integer(from)) {
        llvm::APInt a(width(from), v);
        if (integer(to)) {
            auto extended = signed_integer(from) ? a.sext(128) : a.zext(128);
            if (!(signed_integer(to) ? extended.isSignedIntN(width(to)) : !extended.isNegative() && extended.isIntN(width(to))))
                throw std::invalid_argument("Checked numeric conversion is out of range");
            return extended.trunc(width(to)).getZExtValue();
        }
        llvm::APFloat result(semantics(to)); result.convertFromAPInt(a, signed_integer(from), nearest); return bits(result);
    }
    auto a = fp(v, from);
    if (integer(to)) {
        llvm::APSInt result(width(to), !signed_integer(to)); bool exact;
        auto status = a.convertToInteger(result, llvm::APFloat::rmTowardZero, &exact);
        if (status & llvm::APFloat::opInvalidOp) throw std::invalid_argument("Checked numeric conversion is out of range or non-finite");
        return result.getZExtValue();
    }
    bool finite = a.isFinite(), loses;
    a.convert(semantics(to), nearest, &loses);
    if (finite && a.isInfinity()) throw std::invalid_argument("Checked numeric conversion overflows to infinity");
    return bits(a);
}
std::uint64_t negate(std::uint64_t a, Type t) {
    if (floating(t)) { auto v = fp(a,t); v.changeSign(); return bits(v); }
    return (-llvm::APInt(width(t), a)).getZExtValue();
}
std::uint64_t binary(ir::Op op, std::uint64_t x, std::uint64_t y, Type t) {
    using ir::Op;
    if (floating(t)) {
        auto a = fp(x,t), b = fp(y,t);
        switch(op) {
        case Op::add: a.add(b,nearest); return bits(a);
        case Op::subtract: a.subtract(b,nearest); return bits(a);
        case Op::multiply: a.multiply(b,nearest); return bits(a);
        case Op::divide: a.divide(b,nearest); return bits(a);
        default: break;
        }
        auto c = a.compare(b);
        switch(op) {
        case Op::equal: return c == llvm::APFloat::cmpEqual;
        case Op::not_equal: return c != llvm::APFloat::cmpEqual;
        case Op::less: return c == llvm::APFloat::cmpLessThan;
        case Op::less_equal: return c == llvm::APFloat::cmpLessThan || c == llvm::APFloat::cmpEqual;
        case Op::greater: return c == llvm::APFloat::cmpGreaterThan;
        case Op::greater_equal: return c == llvm::APFloat::cmpGreaterThan || c == llvm::APFloat::cmpEqual;
        default: throw std::invalid_argument("Illegal floating operator");
        }
    }
    llvm::APInt a(width(t),x), b(width(t),y);
    bool sign = signed_integer(t);
    switch(op) {
    case Op::add: return (a+b).getZExtValue();
    case Op::subtract: return (a-b).getZExtValue();
    case Op::multiply: return (a*b).getZExtValue();
    case Op::divide: case Op::remainder:
        if (b.isZero()) throw std::invalid_argument("Division or remainder by zero");
        if (sign && a.isMinSignedValue() && b.isAllOnes()) return op == Op::divide ? x : 0;
        return (op == Op::divide ? (sign ? a.sdiv(b) : a.udiv(b)) : (sign ? a.srem(b) : a.urem(b))).getZExtValue();
    case Op::equal: return a == b;
    case Op::not_equal: return a != b;
    case Op::less: return sign ? a.slt(b) : a.ult(b);
    case Op::less_equal: return sign ? a.sle(b) : a.ule(b);
    case Op::greater: return sign ? a.sgt(b) : a.ugt(b);
    case Op::greater_equal: return sign ? a.sge(b) : a.uge(b);
    default: throw std::invalid_argument("Illegal numeric operator");
    }
}
}

namespace vd::numeric {
const std::vector<Member>& members() {
    static const std::vector<Member> entries{
        {"min_value",false,{},"The most negative finite value (zero for unsigned integers)."},
        {"max_value",false,{},"The largest finite value of this exact numeric type."},
        {"abs",true,{"value"},"Absolute value. Signed minimum fails; floating sign is cleared, including negative zero. Type and receiver forms share one implementation."},
        {"clamp",true,{"value","lower","upper"},"Clamp to inclusive bounds. Validate bounds first: reversed or NaN bounds fail. NaN input propagates; infinite bounds are allowed; in-range signed zero is preserved."}
    };return entries;
}
const Member* member(std::string_view name) {for(const auto& m:members())if(m.name==name)return &m;return nullptr;}
ir::Type primitive(std::string_view name) {
    for(auto t:{Type::i8,Type::u8,Type::i16,Type::u16,Type::i32,Type::u32,Type::i64,Type::u64,Type::f32,Type::f64})if(ir::type_name(t)==name)return t;
    return Type::none;
}
std::string symbol(Type t,std::string_view name) {return "$numeric::"+ir::type_name(t)+"::"+std::string(name);}
std::uint64_t limit(Type t,bool maximum) {
    if(floating(t))return bits(llvm::APFloat::getLargest(semantics(t),!maximum));
    if(!integer(t))throw std::invalid_argument("Numeric limits require a numeric type");
    if(signed_integer(t))return (maximum?llvm::APInt::getSignedMaxValue(width(t)):llvm::APInt::getSignedMinValue(width(t))).getZExtValue();
    return maximum?llvm::APInt::getMaxValue(width(t)).getZExtValue():0;
}
std::uint64_t absolute(std::uint64_t value,Type t) {
    if(floating(t)){auto v=fp(value,t);v.clearSign();return bits(v);}
    if(!signed_integer(t))return value;
    auto v=llvm::APInt(width(t),value);
    if(v.isMinSignedValue())throw std::invalid_argument("Signed minimum abs is out of range");
    return (v.isNegative()?-v:v).getZExtValue();
}
std::uint64_t clamp(std::uint64_t value,std::uint64_t lower,std::uint64_t upper,Type t) {
    using ir::Op;
    if(binary(Op::not_equal,lower,lower,t) || binary(Op::not_equal,upper,upper,t) || binary(Op::greater,lower,upper,t))throw std::invalid_argument("Clamp bounds are reversed or NaN");
    if(binary(Op::less,value,lower,t))return lower;
    if(binary(Op::greater,value,upper,t))return upper;
    return value;
}
void append_prelude(ir::Module& module) {
    using ir::Op;using End=ir::Terminator::Kind;
    std::map<std::string,SourceLocation> requested;
    for(const auto& f:module.functions)for(const auto& b:f.blocks)for(const auto& i:b.instructions)if(i.op==Op::call && i.callee.starts_with("$numeric::"))requested.emplace(i.callee,i.source);
    for(const auto& [name,site]:requested) {
        auto begin=std::string("$numeric::").size(),end=name.find("::",begin);
        auto t=primitive(name.substr(begin,end-begin));auto api=member(name.substr(end+2));
        if(t==Type::none || !api || !api->callable)throw std::logic_error("Invalid numeric prelude identity");
        ir::Function f;f.name=name;f.source=site;f.module_identity="$numeric";f.visibility="private";f.source_name=name.substr(std::string("$numeric::").size());f.result=t;module.source_imports.try_emplace(f.module_identity);f.parameters.assign(api->parameters.size(),t);
        auto block=[&] {f.blocks.emplace_back();return unsigned(f.blocks.size()-1);};
        auto emit=[&](unsigned b,ir::Instruction i){i.source=site;f.blocks[b].instructions.push_back(std::move(i));return unsigned(f.blocks[b].instructions.size()-1);};
        auto arg=[&](unsigned b,unsigned n){return emit(b,{Op::argument,n});};
        auto constant=[&](unsigned b,Type type,std::uint64_t value){ir::Instruction i{Op::constant};i.type=type;i.immediate=static_cast<std::int64_t>(value);return emit(b,std::move(i));};
        auto ret=[&](unsigned b,unsigned v){f.blocks[b].end={End::return_value,v};};
        auto branch=[&](unsigned b,unsigned condition,unsigned yes,unsigned no){f.blocks[b].end={End::branch,condition,yes,no};};
        if(api->name=="abs") {
            auto entry=block();auto x=arg(entry,0);
            if(floating(t)) {
                auto raw=t==Type::f32?Type::u32:Type::u64;
                ir::Instruction cast{Op::bitcast,x};cast.type=raw;auto pattern=emit(entry,cast);
                auto mask=constant(entry,raw,std::uint64_t{1}<<(width(t)-1));
                auto magnitude=emit(entry,{Op::remainder,pattern,mask});cast.a=magnitude;cast.type=t;ret(entry,emit(entry,cast));
            } else if(!signed_integer(t))ret(entry,x);
            else {
                auto sign=block(),negative=block(),positive=block(),failure=block();
                auto minimum=constant(entry,t,limit(t,false));branch(entry,emit(entry,{Op::equal,x,minimum}),failure,sign);
                auto value=arg(sign,0),zero=constant(sign,t,0);branch(sign,emit(sign,{Op::less,value,zero}),negative,positive);
                ret(negative,emit(negative,{Op::negate,arg(negative,0)}));ret(positive,arg(positive,0));
                ir::Instruction fail{Op::panic};fail.callee="Signed minimum abs is out of range";emit(failure,fail);f.blocks[failure].end={End::unreachable};
            }
        } else {
            for(unsigned n=0;n<9;++n)block();
            auto lower=arg(0,1);branch(0,emit(0,{Op::not_equal,lower,lower}),8,1);
            auto upper=arg(1,2);branch(1,emit(1,{Op::not_equal,upper,upper}),8,2);
            auto lo=arg(2,1),hi=arg(2,2);branch(2,emit(2,{Op::greater,lo,hi}),8,3);
            auto value=arg(3,0),low=arg(3,1);branch(3,emit(3,{Op::less,value,low}),4,5);ret(4,arg(4,1));
            auto input=arg(5,0),high=arg(5,2);branch(5,emit(5,{Op::greater,input,high}),6,7);ret(6,arg(6,2));ret(7,arg(7,0));
            ir::Instruction fail{Op::panic};fail.callee="Clamp bounds are reversed or NaN";emit(8,fail);f.blocks[8].end={End::unreachable};
        }
        module.functions.push_back(std::move(f));
    }
}
}
