#include "void/ir.hpp"
#include "void/numeric.hpp"
#include <map>
#include <set>
#include <functional>
#include <stdexcept>
namespace vd::ir {
const char* type_name(Type t) {
    switch (t.kind) { case Type::aggregate: if (t.definition) return t.definition->identity.c_str(); break; case Type::unresolved: if (t.definition) return t.definition->name.c_str(); break; case Type::i8: return "i8"; case Type::i16: return "i16"; case Type::i64: return "i64"; case Type::u8: return "u8"; case Type::u16: return "u16"; case Type::u32: return "u32"; case Type::u64: return "u64"; case Type::f32: return "f32"; case Type::f64: return "f64"; case Type::i32: return "i32"; case Type::boolean: return "bool"; case Type::none: return "no result"; default: break; }
    throw std::invalid_argument("VOID IR: unknown type");
}
void validate(const Module& m) {
    auto fail = [](const std::string& text) { throw std::invalid_argument("VOID IR: " + text); };
    if (m.identity.empty()) fail("missing module identity");
    std::map<std::string,const ValueType*> definitions;
    std::set<const ValueType*> visiting, checked;
    std::function<void(Type)> valid_type = [&](Type t) {
        if (t.kind != Type::aggregate) { if (t.kind == Type::unresolved || t.kind == Type::invalid) fail("unresolved type"); type_name(t); return; }
        if (!t.definition || t.definition->identity.empty() || t.definition->name.empty()) fail("missing value type identity");
        auto* d = t.definition.get();
        if (checked.contains(d)) return;
        if (visiting.size() >= 128 || !visiting.insert(d).second) fail("recursive or excessive value layout");
        auto [found,added] = definitions.emplace(d->identity,d);
        if (!added && found->second != d) fail("conflicting owned value type definitions");
        std::set<std::string> fields;
        for (const auto& field : d->fields) {
            if (field.name.empty() || !fields.insert(field.name).second || field.type == Type::none) fail("invalid value field");
            valid_type(field.type);
        }
        visiting.erase(d); checked.insert(d);
    };
    struct Signature { std::vector<Type> parameters; Type result; bool receiver = false, mutates = false, initializer = false; };
    std::map<std::string, Signature> names;
    for (const auto& name : m.imports)
        if (name.empty() || !names.emplace(name, Signature{{Type::i32}, Type::i32}).second) fail("duplicate or empty import");
    for (const auto& f : m.functions) {
        if (f.name.empty() || !names.emplace(f.name, Signature{f.parameters, f.result,f.receiver,f.receiver_mutates,f.initializer}).second) fail("duplicate or empty function");
        if (!f.module_identity.empty() && (f.source_name.empty() || f.name != f.module_identity + "::" + f.source_name))
            fail("inconsistent function ownership");
        if (f.receiver) {
            if (f.parameters.empty() || f.parameters[0].kind != Type::aggregate || f.locals.empty() || f.locals[0] != f.parameters[0]) fail("invalid receiver signature/storage");
        } else if (f.receiver_mutates) fail("receiver effect without receiver");
        if (f.initializer && (!f.receiver || f.result != Type::none)) fail("invalid initializer signature");
        valid_type(f.result);
        for (auto t : f.parameters) if (t == Type::none) fail("no-result parameter"); else valid_type(t);
        for (auto t : f.locals) if (t == Type::none) fail("no-result slot"); else valid_type(t);
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
                case Op::aggregate:
                    valid_type(i.type);
                    if (i.type.kind != Type::aggregate || !i.type.definition || i.arguments.size() != i.type.definition->fields.size()) fail("invalid aggregate construction");
                    for (unsigned n = 0; n < i.arguments.size(); ++n) expect(i.arguments[n], i.type.definition->fields[n].type);
                    result = i.type; break;
                case Op::extract: case Op::insert: {
                    auto t = operand(i.a);
                    if (t.kind != Type::aggregate || i.immediate < 0 || static_cast<std::size_t>(i.immediate) >= t.definition->fields.size()) fail("invalid aggregate field");
                    auto field = t.definition->fields[static_cast<std::size_t>(i.immediate)].type;
                    if (i.op == Op::insert) { expect(i.b, field); result = t; }
                    else result = field;
                    break;
                }
                case Op::argument:
                    if (i.a >= f.parameters.size()) fail("invalid argument index");
                    result = f.parameters[i.a]; break;
                case Op::constant:
                    if (i.type.kind == Type::aggregate || i.type == Type::none || (i.type == Type::boolean && i.immediate != 0 && i.immediate != 1)) fail("invalid literal");
                    valid_type(i.type); result = i.type; break;
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
                case Op::store:
                    if (f.receiver && !f.receiver_mutates && i.a == 0) fail("non-mutating method writes receiver");
                    expect(i.b, slot(i.a)); result = Type::none; break;
                case Op::call: {
                    auto target = names.find(i.callee);
                    if (target == names.end()) fail("unknown callee " + i.callee);
                    const auto& signature = target->second;
                    if (i.initialization != signature.initializer) fail("initializer invocation kind mismatch");
                    if (i.initialization && (!i.receiver || !i.receiver->fields.empty() || (f.receiver && i.receiver->slot==0))) fail("initializer requires fresh construction storage");
                    if (i.receiver.has_value() != signature.receiver) fail("call receiver mismatch");
                    std::size_t first = 0;
                    if (i.receiver) {
                        auto t = slot(i.receiver->slot);
                        for (auto field : i.receiver->fields) {
                            if (t.kind != Type::aggregate || field >= t.definition->fields.size()) fail("invalid receiver field path");
                            t = t.definition->fields[field].type;
                        }
                        if (t != signature.parameters[0]) fail("receiver type mismatch");
                        if (f.receiver && !f.receiver_mutates && i.receiver->slot == 0 && signature.mutates) fail("non-mutating method calls mutating receiver");
                        first = 1;
                    }
                    auto args = i.arguments;
                    // Original proof instructions use a for their single operand.
                    if (!signature.receiver && args.empty() && signature.parameters.size() == 1) args.push_back(i.a);
                    if (args.size()+first != signature.parameters.size()) fail("call argument count mismatch");
                    for (std::size_t n = 0; n < args.size(); ++n) expect(args[n], signature.parameters[n+first]);
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
