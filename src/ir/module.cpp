#include "void/ir.hpp"
#include "void/numeric.hpp"
#include <map>
#include <stdexcept>
namespace vd::ir {
const char* type_name(Type t) {
    switch (t) { case Type::i8: return "i8"; case Type::i16: return "i16"; case Type::i64: return "i64"; case Type::u8: return "u8"; case Type::u16: return "u16"; case Type::u32: return "u32"; case Type::u64: return "u64"; case Type::f32: return "f32"; case Type::f64: return "f64"; case Type::i32: return "i32"; case Type::boolean: return "bool"; case Type::none: return "no result"; }
    throw std::invalid_argument("VOID IR: unknown type");
}
void validate(const Module& m) {
    auto fail = [](const std::string& text) { throw std::invalid_argument("VOID IR: " + text); };
    if (m.identity.empty()) fail("missing module identity");
    struct Signature { std::vector<Type> parameters; Type result; };
    std::map<std::string, Signature> names;
    for (const auto& name : m.imports)
        if (name.empty() || !names.emplace(name, Signature{{Type::i32}, Type::i32}).second) fail("duplicate or empty import");
    for (const auto& f : m.functions) {
        if (f.name.empty() || !names.emplace(f.name, Signature{f.parameters, f.result}).second) fail("duplicate or empty function");
        if (!f.module_identity.empty() && (f.source_name.empty() || f.name != f.module_identity + "::" + f.source_name))
            fail("inconsistent function ownership");
        type_name(f.result);
        for (auto t : f.parameters) if (t == Type::none) fail("no-result parameter"); else type_name(t);
        for (auto t : f.locals) if (t == Type::none) fail("no-result slot"); else type_name(t);
    }
    for (const auto& f : m.functions) {
        if (f.blocks.empty()) fail("function has no blocks");
        for (const auto& b : f.blocks) {
            std::vector<Type> types;
            auto operand = [&](unsigned id) {
                if (id >= types.size() || types[id] == Type::none) fail("invalid operand in " + f.name);
                return types[id];
            };
            auto expect = [&](unsigned id, Type t) { if (operand(id) != t) fail("mistyped operand in " + f.name); };
            auto slot = [&](unsigned id) {
                if (id >= f.locals.size()) fail("invalid slot in " + f.name);
                return f.locals[id];
            };
            for (const auto& i : b.instructions) {
                Type result = Type::i32;
                switch (i.op) {
                case Op::argument:
                    if (i.a >= f.parameters.size()) fail("invalid argument index");
                    result = f.parameters[i.a]; break;
                case Op::constant:
                    if (i.type == Type::none || (i.type == Type::boolean && i.immediate != 0 && i.immediate != 1)) fail("invalid literal");
                    type_name(i.type); result = i.type; break;
                case Op::add: case Op::subtract: case Op::multiply: case Op::divide: case Op::remainder:
                    result = operand(i.a);
                    if (!numeric::supports(i.op,result)) fail("illegal numeric operation");
                    expect(i.b,result); break;
                case Op::less: case Op::less_equal: case Op::greater: case Op::greater_equal:
                    if (!numeric::supports(i.op,operand(i.a))) fail("non-numeric ordering");
                    expect(i.b,operand(i.a)); result = Type::boolean; break;
                case Op::equal: case Op::not_equal:
                    if (!numeric::supports(i.op,operand(i.a)) || operand(i.a) != operand(i.b)) fail("comparison type mismatch");
                    result = Type::boolean; break;
                case Op::convert:
                    if (!numeric::number(operand(i.a)) || !numeric::number(i.type)) fail("invalid numeric conversion");
                    result = i.type; break;
                case Op::negate:
                    result = operand(i.a); if (!numeric::number(result)) fail("non-numeric negation"); break;
                case Op::logical_not: expect(i.a, Type::boolean); result = Type::boolean; break;
                case Op::load: result = slot(i.a); break;
                case Op::store: expect(i.b, slot(i.a)); result = Type::none; break;
                case Op::call: {
                    auto target = names.find(i.callee);
                    if (target == names.end()) fail("unknown callee " + i.callee);
                    auto args = i.arguments;
                    // Original proof instructions use a for their single operand.
                    if (args.empty() && target->second.parameters.size() == 1) args.push_back(i.a);
                    if (args.size() != target->second.parameters.size()) fail("call argument count mismatch");
                    for (std::size_t n = 0; n < args.size(); ++n) expect(args[n], target->second.parameters[n]);
                    result = target->second.result; break;
                }
                default: fail("unknown operation");
                }
                types.push_back(result);
            }
            auto destination = [&](unsigned id) { if (id >= f.blocks.size()) fail("invalid branch destination"); };
            switch (b.end.kind) {
            case Terminator::Kind::return_value:
                if (f.result == Type::none) fail("value returned from no-result function");
                expect(b.end.value, f.result); break;
            case Terminator::Kind::return_void:
                if (f.result != Type::none) fail("missing return value"); break;
            case Terminator::Kind::branch:
                expect(b.end.value, Type::boolean); destination(b.end.yes); destination(b.end.no); break;
            case Terminator::Kind::jump: destination(b.end.yes); break;
            case Terminator::Kind::unreachable: break;
            default: fail("unknown terminator");
            }
        }
    }
}
}
