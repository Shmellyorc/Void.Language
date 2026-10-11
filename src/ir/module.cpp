#include "void/ir.hpp"
#include <set>
#include <stdexcept>
namespace vd::ir {
void validate(const Module& m) {
    auto fail = [](const std::string& text) { throw std::invalid_argument("VOID IR: " + text); };
    if (m.identity.empty()) fail("missing module identity");
    std::set<std::string> names;
    for (const auto& name : m.imports)
        if (name.empty() || !names.insert(name).second) fail("duplicate or empty import");
    for (const auto& f : m.functions)
        if (f.name.empty() || !names.insert(f.name).second) fail("duplicate or empty function");
    for (const auto& f : m.functions) {
        if (f.blocks.empty()) fail("function has no blocks");
        for (const auto& b : f.blocks) {
            std::vector<bool> booleans;
            auto operand = [&](unsigned id, bool boolean = false) {
                if (id >= booleans.size() || booleans[id] != boolean) fail("invalid or mistyped operand in " + f.name);
            };
            for (const auto& i : b.instructions) {
                switch (i.op) {
                case Op::argument: case Op::constant: break;
                case Op::add: case Op::multiply: case Op::less: operand(i.a); operand(i.b); break;
                case Op::call:
                    operand(i.a);
                    if (!names.contains(i.callee)) fail("unknown callee " + i.callee);
                    break;
                default: fail("unknown operation");
                }
                booleans.push_back(i.op == Op::less);
            }
            if (b.end.kind == Terminator::Kind::return_value) operand(b.end.value);
            else if (b.end.kind == Terminator::Kind::branch) {
                operand(b.end.value, true);
                if (b.end.yes >= f.blocks.size() || b.end.no >= f.blocks.size()) fail("invalid branch destination");
            } else fail("unknown terminator");
        }
    }
}
}
