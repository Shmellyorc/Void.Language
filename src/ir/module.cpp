#include "void/ir.hpp"
#include "void/numeric.hpp"
#include "void/reference_capabilities.hpp"
#include <map>
#include <algorithm>
#include <set>
#include <functional>
#include <stdexcept>
namespace vd::ir {
Type::Type(std::shared_ptr<const ValueType> d, Kind k) : kind(k) {
    if (!d) return;
    if (d->self.expired()) d->self = d;
    auto owner = d->arena.lock();
    if (!owner) {
        owner = std::make_shared<TypeArena>();
        std::vector<std::shared_ptr<const ValueType>> pending{d};
        std::set<const ValueType*> seen;
        for (std::size_t n=0; n<pending.size(); ++n) {
            auto current = pending[n];
            if (!seen.insert(current.get()).second) continue;
            if (current->self.expired()) current->self = current;
            current->arena = owner;
            owner->definitions.push_back(current);
            for (const auto& field : current->fields)
                if (auto child = field.type.target.lock()) pending.push_back(std::move(child));
        }
    }
    definition = std::shared_ptr<const ValueType>(std::move(owner),d.get());
}
FieldType::FieldType(Type t) : kind(t.kind), nullable(t.nullable), readonly(t.readonly) {
    if (t.construction) throw std::invalid_argument("construction view cannot be a field type");
    if (!t.readonly_references.empty()) throw std::invalid_argument("qualified reference leaves cannot be nominal field declarations");
    if (t.definition) target = t.definition->self;
}
FieldType::operator Type() const {
    if (kind != Type::aggregate && kind != Type::unresolved && kind != Type::managed_ref) { Type t(kind); t.nullable=nullable; t.readonly=readonly; return t; }
    auto d = target.lock();
    if (!d) throw std::logic_error("nominal type edge outlived its definition table");
    Type t(std::move(d),kind); t.nullable=nullable; t.readonly=readonly; return t;
}

ReferencePaths reference_paths(Type root) {
    ReferencePaths result;
    struct Pending { Type type; ReferencePath path; };
    std::vector<Pending> pending{{root,{}}};
    std::size_t expanded=0;
    while (!pending.empty()) {
        auto [type,path]=std::move(pending.back()); pending.pop_back();
        if (type.construction) continue; // A stage is a place, never a heap root.
        if (type.kind==Type::managed_ref) { result.insert(std::move(path)); continue; }
        if (type.kind!=Type::aggregate) continue;
        if (!type.definition || path.size()>=128 || ++expanded>65536)
            throw std::invalid_argument("VOID IR: recursive or excessive reference shape");
        for (unsigned n=0;n<type.definition->fields.size();++n) {
            auto child=path; child.push_back(n);
            pending.push_back({type.definition->fields[n].type,std::move(child)});
        }
    }
    return result;
}
ReferencePaths readonly_paths(Type type) {
    if (type.readonly) return reference_paths(type);
    return type.readonly_references;
}
Type with_readonly_paths(Type type,const ReferencePaths& paths) {
    // A private construction view is a place capability, not a managed leaf.
    // Its readonly place permission must survive ordinary fact propagation.
    if (type.construction) { type.readonly_references=paths; return type; }
    type.readonly=false; type.readonly_references=paths;
    if (type.kind==Type::managed_ref && !type.construction) {
        type.readonly=paths.contains(ReferencePath{});
        type.readonly_references.clear();
    }
    return type;
}
Type readonly_view(Type type) {
    if (type.construction) { type.readonly=true; return type; }
    return with_readonly_paths(type,reference_paths(type));
}
Type field_type(Type owner,unsigned field) {
    auto result=Type(owner.definition->fields.at(field).type);
    if (owner.kind==Type::managed_ref) return owner.readonly ? readonly_view(result) : result;
    ReferencePaths projected=readonly_paths(result);
    for (const auto& path:readonly_paths(owner)) if (!path.empty() && path[0]==field)
        projected.insert(ReferencePath(path.begin()+1,path.end()));
    return with_readonly_paths(result,projected);
}
Type with_field_type(Type owner,unsigned field,Type value) {
    ReferencePaths paths=readonly_paths(owner);
    for (auto p=paths.begin();p!=paths.end();) {
        if (!p->empty() && (*p)[0]==field) p=paths.erase(p); else ++p;
    }
    for (auto path:readonly_paths(value)) { path.insert(path.begin(),field); paths.insert(std::move(path)); }
    return with_readonly_paths(owner,paths);
}
bool capability_assignable(Type actual,Type expected) {
    if (actual.construction && actual.readonly && !expected.readonly) return false;
    const auto allowed=readonly_paths(expected);
    for (const auto& path:readonly_paths(actual)) if (!allowed.contains(path)) return false;
    return true;
}
std::string capability_key(Type type) {
    std::string result;
    for (const auto& path:readonly_paths(type)) {
        result+='[';
        for (auto field:path) result+=std::to_string(field)+',';
        result+=']';
    }
    return result;
}
std::string reference_path_name(Type type,const ReferencePath& path) {
    std::string result=type.definition ? type.definition->name : type_name(type);
    for (auto field:path) {
        result+='.'+type.definition->fields.at(field).name;
        type=field_type(type,field);
    }
    return result;
}

std::string construction_name(const std::string& symbol,const std::vector<unsigned>& parameters) {
    if (parameters.empty()) return symbol;
    std::string result=symbol+"::construction";
    for (auto p:parameters) result+="::"+std::to_string(p);
    return result;
}
std::string type_name(Type t) {
    if (t.construction) { t.construction=false; return "construction view " + type_name(t); }
    if (t.readonly && t.kind!=Type::managed_ref) { t.readonly=false; return "readonly " + type_name(t); }
    if (t.kind == Type::managed_ref && t.definition) return std::string(t.readonly ? "readonly " : "") + t.definition->identity + (t.nullable ? "?" : "");
    switch (t.kind) { case Type::aggregate: if (t.definition) return t.definition->identity.c_str(); break; case Type::unresolved: if (t.definition) return t.definition->name.c_str(); break; case Type::i8: return "i8"; case Type::i16: return "i16"; case Type::i64: return "i64"; case Type::u8: return "u8"; case Type::u16: return "u16"; case Type::u32: return "u32"; case Type::u64: return "u64"; case Type::f32: return "f32"; case Type::f64: return "f64"; case Type::i32: return "i32"; case Type::boolean: return "bool"; case Type::none: return "no result"; default: break; }
    throw std::invalid_argument("VOID IR: unknown type");
}
#include "construction.inc"
#include "reference_capabilities.inc"
void validate(const Module& m, InstructionTypes* results) {
    if (results) results->clear();
    InstructionTypes qualified_types;
    auto fail = [](const std::string& text) { throw std::invalid_argument("VOID IR: " + text); };
    if (m.identity.empty()) fail("missing module identity");
    std::map<std::string,const ValueType*> definitions;
    std::set<const ValueType*> visiting, checked;
    std::function<void(Type)> inline_layout = [&](Type t) {
        if (t.kind != Type::aggregate) return;
        auto* d=t.definition.get();
        if (visiting.size() >= 128 || !visiting.insert(d).second) fail("recursive or excessive value layout");
        for (const auto& field : d->fields) inline_layout(field.type);
        visiting.erase(d);
    };
    auto valid_type = [&](Type root) {
        std::vector<Type> pending{root};
        for (std::size_t n=0;n<pending.size();++n) {
            auto t=pending[n];
            if (t.construction && (t.kind!=Type::managed_ref || t.nullable)) fail("invalid construction view type");
            if (t.nullable && t.kind!=Type::managed_ref) fail("nullable non-reference type");
            if (t.kind==Type::managed_ref && !m.coordinated) fail("managed references require coordinated runtime");
            if (t.kind!=Type::aggregate && t.kind!=Type::managed_ref) {
                if (t.kind==Type::unresolved || t.kind==Type::invalid || t.kind==Type::null_value) fail("unresolved type");
                type_name(t); continue;
            }
            if (!t.definition || t.definition->identity.empty() || t.definition->name.empty()) fail("missing value type identity");
            auto* d=t.definition.get();
            if (t.construction && !d->managed) fail("construction view requires a managed nominal");
            if (t.kind==Type::aggregate && d->managed) fail("managed nominal cannot be an inline value");
            auto [found,added]=definitions.emplace(d->identity,d);
            if (!added && found->second!=d) fail("conflicting owned value type definitions");
            if (!checked.insert(d).second) continue;
            std::set<std::string> fields;
            for (const auto& field:d->fields) {
                if (field.name.empty() || !fields.insert(field.name).second || Type(field.type)==Type::none) fail("invalid value field");
                if (field.type.readonly) fail("readonly nominal field declarations are not supported");
                pending.push_back(field.type);
            }
            // References break physical layout recursion, not graph validation.
            for (const auto& field:d->fields) inline_layout(field.type);
        }
        const auto shape=reference_paths(root);
        for (const auto& path:root.readonly_references) if (!shape.contains(path)) fail("invalid readonly reference path");
    };
    auto assignable = [](Type actual,Type expected) {
        return actual==expected || (actual.kind==Type::managed_ref && expected.kind==Type::managed_ref && Type::key(actual)==Type::key(expected) && actual.construction==expected.construction && (!actual.nullable || expected.nullable));
    };
    struct Signature { std::vector<Type> parameters; Type result; bool receiver = false, mutates = false, initializer = false; };
    std::map<std::string, Signature> names;
    for (const auto& name : m.imports)
        if (name.empty() || !names.emplace(name, Signature{{Type::i32}, Type::i32}).second) fail("duplicate or empty import");
    for (const auto& f : m.functions) {
        if (f.name.empty() || !names.emplace(f.name, Signature{f.parameters, f.result,f.receiver,f.receiver_mutates,f.initializer}).second) fail("duplicate or empty function");
        std::vector<unsigned> context_parameters;
        for (unsigned n=0;n<f.parameters.size();++n) if (f.parameters[n].construction) context_parameters.push_back(n);
        auto expected_name=f.module_identity+"::"+f.source_name;
        if (!f.initializer) expected_name=construction_name(expected_name,context_parameters);
        if (!f.module_identity.empty() && (f.source_name.empty() || f.name != expected_name))
            fail("inconsistent function ownership");
        if (f.receiver) {
            if (f.parameters.empty() || (f.parameters[0].kind != Type::aggregate && f.parameters[0].kind != Type::managed_ref) || f.locals.empty() || f.locals[0] != f.parameters[0]) fail("invalid receiver signature/storage");
        } else if (f.receiver_mutates) fail("receiver effect without receiver");
        if (f.initializer && (!f.receiver || f.result != Type::none)) fail("invalid initializer signature");
        if (f.result.construction) fail("construction view returned from function");
        if (f.initializer && f.parameters[0].kind==Type::managed_ref && (!f.parameters[0].construction || context_parameters!=std::vector<unsigned>{0})) fail("managed init requires a private construction receiver");
        valid_type(f.result);
        for (auto t : f.parameters) if (t == Type::none) fail("no-result parameter"); else valid_type(t);
        for (auto t : f.locals) if (t == Type::none) fail("no-result slot"); else valid_type(t);
    }
    for (const auto& f : m.functions) {
        if (f.blocks.empty()) fail("function has no blocks");
        qualified_types.emplace_back();
        std::vector<std::vector<Type>> body_types;
        for (const auto& b : f.blocks) {
            std::vector<Type> types;
            auto operand = [&](unsigned id) {
                if (id >= types.size() || types[id] == Type::none) fail("invalid operand in " + f.name);
                return types[id];
            };
            auto expect = [&](unsigned id, Type t) { if (!assignable(operand(id),t)) fail("mistyped operand in " + f.name); };
            auto slot = [&](unsigned id) {
                if (id >= f.locals.size()) fail("invalid slot in " + f.name);
                return f.locals[id];
            };
            for (const auto& i : b.instructions) {
                Type result = Type::i32;
                switch (i.op) {
                case Op::stage_begin:
                    valid_type(i.type);
                    if (!i.type.construction || !i.type.definition->managed || slot(i.a)!=i.type) fail("invalid construction storage");
                    result=i.type; break;
                case Op::gc_construct:
                    valid_type(i.type);
                    if (!m.coordinated || i.type.kind!=Type::managed_ref || i.type.nullable || i.type.construction || i.arguments.size()!=i.type.definition->fields.size()) fail("invalid complete managed construction");
                    for (unsigned n=0;n<i.arguments.size();++n) expect(i.arguments[n],i.type.definition->fields[n].type);
                    if (i.receiver && (!i.receiver->fields.empty() || !slot(i.receiver->slot).construction || Type::key(slot(i.receiver->slot))!=Type::key(i.type))) fail("invalid construction commit place");
                    result=i.type; break;
                case Op::gc_allocate:
                    valid_type(i.type);
                    if (!m.coordinated || i.type.kind != Type::managed_ref || i.type.construction || i.type.definition->fields.empty()) fail("invalid managed allocation");
                    result = i.type; break;
                case Op::gc_read: case Op::gc_write: {
                    auto object = operand(i.a);
                    if (!m.coordinated || object.kind != Type::managed_ref || i.immediate < 0 || static_cast<std::size_t>(i.immediate) >= object.definition->fields.size()) fail("invalid managed field");
                    auto field = object.definition->fields[static_cast<std::size_t>(i.immediate)].type;
                    if (i.op == Op::gc_write) { if (object.readonly) fail("write through readonly reference"); expect(i.b, field); result = Type::none; }
                    else { result = field; if (result.kind==Type::managed_ref) result.readonly=object.readonly; }
                    break;
                }
                case Op::gc_alive:
                    if (!m.coordinated || (operand(i.a).kind != Type::managed_ref || operand(i.a).construction)) fail("invalid managed inspection");
                    result = Type::boolean; break;
                case Op::gc_collect: case Op::gc_poll:
                    if (!m.coordinated) fail("runtime operation outside coordinated module");
                    result = Type::none; break;
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
                    if (i.type.construction || i.type.kind == Type::aggregate || i.type == Type::none || (i.type.kind == Type::managed_ref && i.immediate != 0) || (i.type == Type::boolean && i.immediate != 0 && i.immediate != 1)) fail("invalid literal");
                    if (i.type.kind==Type::managed_ref && !i.type.nullable) fail("null literal requires a nullable reference type");
                    valid_type(i.type); result = i.type; break;
                case Op::add: case Op::subtract: case Op::multiply: case Op::divide: case Op::remainder:
                    result = operand(i.a);
                    if (!numeric::supports(i.op,result)) fail("illegal numeric operation");
                    expect(i.b,result); break;
                case Op::less: case Op::less_equal: case Op::greater: case Op::greater_equal:
                    if (!numeric::supports(i.op,operand(i.a))) fail("non-numeric ordering");
                    expect(i.b,operand(i.a)); result = Type::boolean; break;
                case Op::equal: case Op::not_equal:
                    if (!((operand(i.a).kind==Type::managed_ref && !operand(i.a).construction && operand(i.b).kind==Type::managed_ref && !operand(i.b).construction && Type::key(operand(i.a))==Type::key(operand(i.b))) || (numeric::supports(i.op,operand(i.a)) && operand(i.a)==operand(i.b)))) fail("comparison type mismatch");
                    result = Type::boolean; break;
                case Op::convert:
                    if (!((operand(i.a).kind==Type::managed_ref && !operand(i.a).construction && i.type.kind==Type::managed_ref && !i.type.construction && Type::key(operand(i.a))==Type::key(i.type) && !i.type.nullable) || (numeric::number(operand(i.a)) && numeric::number(i.type)))) fail("invalid conversion");
                    result = i.type; break;
                case Op::negate:
                    result = operand(i.a); if (!numeric::number(result)) fail("non-numeric negation"); break;
                case Op::logical_not: expect(i.a, Type::boolean); result = Type::boolean; break;
                case Op::load: result = slot(i.a); break;
                case Op::store:
                    if (f.receiver && f.parameters[0].kind==Type::aggregate && !f.receiver_mutates && i.a == 0) fail("non-mutating method writes receiver");
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
                            if ((t.kind != Type::aggregate && t.kind!=Type::managed_ref) || field >= t.definition->fields.size()) fail("invalid receiver field path");
                            t = t.definition->fields[field].type;
                        }
                        if (t != signature.parameters[0]) fail("receiver type mismatch");
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
            body_types.push_back(types);
            qualified_types.back().push_back(types);
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
        validate_construction(f,m,body_types);
    }
    validate_reference_capabilities(m,qualified_types);
    if (results) *results=std::move(qualified_types);
}
}
