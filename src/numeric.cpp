#include "void/numeric.hpp"
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APSInt.h>
#include <llvm/Support/Error.h>
#include <stdexcept>
namespace vd::numeric {
using ir::Type;
bool floating(Type t) { return t == Type::f32 || t == Type::f64; }
bool signed_integer(Type t) { return t == Type::i8 || t == Type::i16 || t == Type::i32 || t == Type::i64; }
bool integer(Type t) { return signed_integer(t) || t == Type::u8 || t == Type::u16 || t == Type::u32 || t == Type::u64; }
bool number(Type t) { return integer(t) || floating(t); }
unsigned width(Type t) {
    switch(t) {
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
