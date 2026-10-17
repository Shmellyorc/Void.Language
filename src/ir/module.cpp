#include "void/ir.hpp"
#include "void/numeric.hpp"
#include "void/reference_capabilities.hpp"
#include <map>
#include <algorithm>
#include <set>
#include <functional>
#include <bit>
#include <limits>
#include <stdexcept>
namespace vd::ir {
bool array_type(Type t) { return t.kind==Type::fixed_array || t.kind==Type::growable_array; }
Type array_element(Type t) {
    if (!array_type(t) || !t.definition || !t.definition->element) throw std::invalid_argument("VOID IR: missing array element");
    return *t.definition->element;
}
std::string array_identity(Type element, std::optional<std::uint64_t> extent) {
    const auto key=std::to_string(static_cast<unsigned>(element.kind))+":"+Type::key(element)+(element.nullable ? "?" : "");
    return "$array:"+std::to_string(key.size())+":"+key+":"+(extent ? std::to_string(*extent) : "growable");
}
bool scalar_fixed_array(Type t) {
    return t.kind==Type::fixed_array && t.definition && t.definition->element &&
        (numeric::number(array_element(t)) || array_element(t)==Type::boolean);
}
CompactValueShape compact_value_shape(Type root) {
    std::map<const ValueType*,CompactValueShape> memo;
    std::set<const ValueType*> active;
    std::size_t work=0;
    auto add=[](std::uint64_t a,std::uint64_t b) {
        if(b>std::numeric_limits<std::uint64_t>::max()-a)
            throw std::length_error("Array logical scalar extent is not representable in 64 bits");
        return a+b;
    };
    std::function<CompactValueShape(Type)> visit=[&](Type t)->CompactValueShape {
        if(t.nullable || t.construction || !t.preparation.empty()) throw std::invalid_argument("Unsupported array value shape");
        if(numeric::number(t) || t==Type::boolean) return {0,t==Type::boolean ? 1u : numeric::width(t)/8u,1};
        if((t.kind!=Type::fixed_array && t.kind!=Type::aggregate) || !t.definition || t.definition->managed || t.definition->interface_type)
            throw std::invalid_argument("Reference-bearing/growable array shapes await GC support");
        auto* d=t.definition.get();
        if(t.kind==Type::fixed_array) {
            if(!d->element || !d->extent || !d->fields.empty() || !d->base.expired()) throw std::invalid_argument("Malformed repeated array shape");
            if(*d->extent>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
                throw std::length_error("Array extent is not representable as an i64 count");
        } else if(d->element || d->extent || !d->base.expired()) throw std::invalid_argument("Malformed array value shape");
        if(auto found=memo.find(d);found!=memo.end()) return found->second;
        if(active.size()>=128 || !active.insert(d).second) throw std::invalid_argument("Recursive or excessive array value shape");
        CompactValueShape result{1,0,1};
        auto child=[&](Type element,std::uint64_t count) {
            if(++work>65536) throw std::length_error("Array distinct-shape analysis work exhausted");
            auto shape=visit(element);result.depth=std::max(result.depth,shape.depth+1);
            if(shape.scalar_bytes && count>std::numeric_limits<std::uint64_t>::max()/shape.scalar_bytes)
                throw std::length_error("Array logical scalar extent is not representable in 64 bits");
            result.scalar_bytes=add(result.scalar_bytes,count*shape.scalar_bytes);
            // Defaults still contain explicit struct fields. Bound that real recipe
            // expansion, independently of repeated array multiplicity.
            if(count) {
                if(shape.template_nodes>65536-result.template_nodes) throw std::length_error("Array default-template analysis work exhausted");
                result.template_nodes+=shape.template_nodes;
            }
        };
        if(t.kind==Type::fixed_array) {
            child(array_element(t),*d->extent); // Validate finite element even at zero extent.
        } else {
            for(const auto& field:d->fields) child(field.type,1);
        }
        if(result.depth>128) throw std::invalid_argument("Excessive array value depth");
        active.erase(d);memo.emplace(d,result);return result;
    };
    return visit(root);
}
std::uint64_t checked_array_index(std::uint64_t bits,Type index,std::uint64_t extent) {
    if (!numeric::integer(index)) throw std::invalid_argument("Array index requires an integer");
    const auto width=numeric::width(index);
    if(width<64) bits &= (std::uint64_t{1}<<width)-1;
    if(numeric::signed_integer(index) && (bits & (std::uint64_t{1}<<(width-1)))) {
        const auto mask=width==64 ? ~std::uint64_t{0} : (std::uint64_t{1}<<width)-1;
        const auto magnitude=((~bits)&mask)+1;
        if(magnitude>extent) throw std::out_of_range("Array index out of bounds");
        return extent-magnitude;
    }
    if(bits>=extent) throw std::out_of_range("Array index out of bounds");
    return bits;
}
bool reference_value(Type t) { return t.kind==Type::managed_ref || t.kind==Type::interface_ref; }
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
            if (auto base=current->base.lock()) pending.push_back(std::move(base));
            if (current->element) if (auto child=current->element->target.lock()) pending.push_back(std::move(child));
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
    if (kind != Type::aggregate && kind != Type::unresolved && kind != Type::managed_ref && kind != Type::interface_ref && kind!=Type::fixed_array && kind!=Type::growable_array) { Type t(kind); t.nullable=nullable; t.readonly=readonly; return t; }
    auto d = target.lock();
    if (!d) throw std::logic_error("nominal type edge outlived its definition table");
    Type t(std::move(d),kind); t.nullable=nullable; t.readonly=readonly; return t;
}

ReferencePaths reference_paths(Type root, bool include_strings) {
    ReferencePaths result;
    struct Pending { Type type; ReferencePath path; };
    std::vector<Pending> pending{{root,{}}};
    std::size_t expanded=0;
    while (!pending.empty()) {
        auto [type,path]=std::move(pending.back()); pending.pop_back();
        if (type.construction) continue; // A stage is a place, never a heap root.
        if (reference_value(type) || type.kind==Type::prepared_receiver || (include_strings && type.kind==Type::string)) { result.insert(std::move(path)); continue; }
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
    if(type.construction) return type.readonly ? ReferencePaths{ReferencePath{}} : type.readonly_references;
    if (type.readonly) return reference_paths(type);
    return type.readonly_references;
}
Type with_readonly_paths(Type type,const ReferencePaths& paths) {
    // A private construction view is a place capability, not a managed leaf.
    // Its readonly place permission must survive ordinary fact propagation.
    if (type.construction) {
        if(!paths.empty() && paths!=ReferencePaths{ReferencePath{}}) throw std::invalid_argument("VOID IR: invalid construction authority path");
        type.readonly|=paths.contains(ReferencePath{});type.readonly_references.clear();return type;
    }
    type.readonly=false; type.readonly_references=paths;
    if ((reference_value(type) || type.kind==Type::prepared_receiver) && !type.construction) {
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
bool subtype(Type actual,Type expected) {
    if (actual.kind!=Type::managed_ref || expected.kind!=Type::managed_ref ||
        actual.construction || expected.construction || !actual.definition || !expected.definition || !actual.definition->managed || !expected.definition->managed) return false;
    auto current=actual.definition;
    std::set<const ValueType*> seen;
    while (current && seen.size()<128 && seen.insert(current.get()).second) {
        if (current->identity==expected.definition->identity) return true;
        current=current->base.lock();
    }
    return false;
}
std::vector<unsigned> physical_field_path(Type owner,unsigned field) {
    if (!owner.definition || field>=owner.definition->fields.size()) throw std::invalid_argument("VOID IR: invalid logical field");
    std::vector<unsigned> result;
    auto current=owner.definition;
    std::set<const ValueType*> seen;
    while (auto base=current->base.lock()) {
        if (seen.size()>=128 || !seen.insert(current.get()).second) throw std::invalid_argument("VOID IR: recursive base layout");
        if (field>=base->fields.size()) { result.push_back(1+field-static_cast<unsigned>(base->fields.size())); return result; }
        result.push_back(0); current=std::move(base);
    }
    result.push_back(field); return result;
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
    if(t.kind==Type::interface_ref && t.definition) return std::string(t.readonly ? "readonly " : "")+t.definition->identity+(t.nullable ? "?" : "");
    if(t.kind==Type::prepared_receiver && t.definition) return "prepared receiver "+t.definition->identity;
    if (t.readonly && t.kind!=Type::managed_ref) { t.readonly=false; return "readonly " + type_name(t); }
    if(array_type(t)) return "["+type_name(array_element(t))+(t.definition->extent ? ", "+std::to_string(*t.definition->extent) : "")+"]";
    if (t.kind == Type::managed_ref && t.definition) return std::string(t.readonly ? "readonly " : "") + t.definition->identity + (t.nullable ? "?" : "");
    switch (t.kind) { case Type::string: return "string"; case Type::aggregate: if (t.definition) return t.definition->identity.c_str(); break; case Type::unresolved: if (t.definition) return t.definition->name.c_str(); break; case Type::i8: return "i8"; case Type::i16: return "i16"; case Type::i64: return "i64"; case Type::u8: return "u8"; case Type::u16: return "u16"; case Type::u32: return "u32"; case Type::u64: return "u64"; case Type::f32: return "f32"; case Type::f64: return "f64"; case Type::i32: return "i32"; case Type::boolean: return "bool"; case Type::none: return "no result"; default: break; }
    throw std::invalid_argument("VOID IR: unknown type");
}
std::vector<VirtualTarget> virtual_targets(const Module& module,const Function& root) {
    std::vector<VirtualTarget> result;
    if(root.virtual_slot.empty()) throw std::invalid_argument("VOID IR: missing virtual family");
    std::set<std::string> identities;
    for(auto t:module.nominal_types) {
        if(t.kind!=Type::managed_ref) continue;
        if(!identities.insert(Type::key(t)).second) throw std::invalid_argument("VOID IR: duplicate nominal catalog identity");
        if(!subtype(t,root.parameters.at(0))) continue;
        const Function* selected=nullptr;
        for(auto ancestor=t.definition;ancestor && !selected;ancestor=ancestor->base.lock())
            for(const auto& candidate:module.functions) if(candidate.virtual_slot==root.virtual_slot && candidate.parameters.at(0).definition==ancestor) {selected=&candidate;break;}
        if(!selected) throw std::invalid_argument("VOID IR: missing virtual dispatch implementation");
        result.push_back({t,selected->name});
    }
    if(result.empty()) throw std::invalid_argument("VOID IR: virtual family absent from nominal catalog");
    return result;
}
#include "interfaces.inc"
#include "construction.inc"
#include "reference_capabilities.inc"
#include "fixed_ownership.inc"
void validate(const Module& m, InstructionTypes* results,std::map<std::string,ReferenceEffects>* verified_effects) {
    if (results) results->clear();
    InstructionTypes qualified_types;
    auto fail = [](const std::string& text) { throw std::invalid_argument("VOID IR: " + text); };
    if (m.identity.empty()) fail("missing module identity");
    if(!m.interface_requirements.empty() && (m.root_identity!=m.identity || !m.source_imports.contains(m.root_identity))) fail("invalid interface source scope graph");
    std::map<std::string,const ValueType*> definitions;
    std::set<const ValueType*> visiting, checked;
    using LayoutExtent=std::pair<unsigned,std::size_t>;
    std::map<const ValueType*,LayoutExtent> extents;
    std::function<LayoutExtent(Type)> inline_layout = [&](Type t)->LayoutExtent {
        if(t.kind!=Type::aggregate && t.kind!=Type::fixed_array) return {0,0};
        auto* d=t.definition.get();
        if(!d) fail("missing inline type definition");
        if(auto found=extents.find(d);found!=extents.end()) return found->second;
        if(visiting.size()>=128 || !visiting.insert(d).second) fail("recursive or excessive value layout");
        unsigned depth=1;std::size_t expanded=0;auto base=d->base.lock();
        if(t.kind==Type::fixed_array) {
            if(!d->extent || !d->element) fail("missing fixed-array layout");
            const auto child=inline_layout(array_element(t));
            if(child.second>=65536) fail("excessive array template work");
            expanded=*d->extent ? child.second+1 : 0;depth=child.first+1;
        }
        if(base) {auto parent=inline_layout(Type(base));depth=parent.first+1;expanded=parent.second;}
        for(unsigned n=base ? static_cast<unsigned>(base->fields.size()) : 0;n<d->fields.size();++n) {
            auto child=inline_layout(d->fields[n].type);depth=std::max(depth,child.first+1);
            if(child.second>=65536 || expanded>=65536-child.second) fail("excessive expanded value layout");
            expanded+=child.second+1;
        }
        visiting.erase(d);
        if(depth>128) fail("excessive physical layout depth");
        extents.emplace(d,LayoutExtent{depth,expanded});return {depth,expanded};
    };
    auto valid_type = [&](Type root) {
        std::vector<Type> pending{root};
        for (std::size_t n=0;n<pending.size();++n) {
            auto t=pending[n];
            if(array_type(t)) {
                if(t.kind!=Type::fixed_array) fail("growable array IR storage awaits later checkpoints");
                if(!t.definition || !t.definition->element || t.nullable || t.construction || !t.preparation.empty() ||
                   !t.definition->fields.empty() || t.definition->managed || t.definition->interface_type || !t.definition->base.expired() ||
                   (t.kind==Type::fixed_array)!=t.definition->extent.has_value()) fail("invalid structural array type");
                const auto element=array_element(t);
                if(element==Type::none || element.kind==Type::prepared_receiver || element.kind==Type::invalid || element.kind==Type::unresolved ||
                   element.construction || element.readonly || !element.readonly_references.empty() ||
                   t.definition->identity!=array_identity(element,t.definition->extent)) fail("invalid array element or identity");
                if(checked.insert(t.definition.get()).second) {
                    try {compact_value_shape(t);}
                    catch(const std::length_error& failure) {fail(failure.what());}
                    pending.push_back(element);inline_layout(t);
                }
                continue;
            }
            if (t.kind==Type::string && (t.definition || t.nullable || t.construction || !t.preparation.empty() || !t.readonly_references.empty())) fail("invalid intrinsic string type");
            if (t.construction && (t.kind!=Type::managed_ref || t.nullable)) fail("invalid construction view type");
            if (t.nullable && !reference_value(t)) fail("nullable non-reference type");
            if ((reference_value(t) || t.kind==Type::prepared_receiver || t.kind==Type::string) && !m.coordinated) fail("managed references require coordinated runtime");
            if(t.kind==Type::interface_ref || t.kind==Type::prepared_receiver) {
                if(!t.definition || !t.definition->interface_type || t.definition->managed || t.definition->identity.empty() || !t.definition->fields.empty() || !t.definition->base.expired()) fail("invalid interface definition");
                if(t.definition->visibility!="public" && t.definition->visibility!="internal" && t.definition->visibility!="private") fail("invalid interface visibility");
                if(t.definition->module_identity.empty() || t.definition->identity!=t.definition->module_identity+"::"+t.definition->name || !m.source_imports.contains(t.definition->module_identity)) fail("invalid nominal interface ownership");
                if(t.kind==Type::prepared_receiver) {
                    auto requirement=std::find_if(m.interface_requirements.begin(),m.interface_requirements.end(),[&](const auto& r){return r.name==t.preparation;});
                    if(t.preparation.empty() || requirement==m.interface_requirements.end() || requirement->parameters.empty() || requirement->parameters[0].definition!=t.definition) fail("invalid prepared requirement identity");
                }
                auto [found,added]=definitions.emplace(t.definition->identity,t.definition.get());
                if(!added && found->second!=t.definition.get()) fail("duplicate nominal interface identity");
                continue;
            }
            if(t.definition && t.definition->interface_type) fail("interface used as an ordinary concrete nominal");
            if (t.kind!=Type::aggregate && t.kind!=Type::managed_ref) {
                if (t.kind==Type::unresolved || t.kind==Type::invalid || t.kind==Type::null_value) fail("unresolved type");
                type_name(t); continue;
            }
            if (!t.definition || t.definition->identity.empty() || t.definition->identity.find('\0')!=std::string::npos || t.definition->name.empty()) fail("missing value type identity");
            auto* d=t.definition.get();
            if(d->element || d->extent) fail("structural array metadata on nominal type");
            if(!m.interface_requirements.empty() && d->visibility!="public" && d->visibility!="internal" && d->visibility!="private") fail("invalid concrete nominal visibility");
            if(!m.interface_requirements.empty() && (d->identity!=d->module_identity+"::"+d->name || !m.source_imports.contains(d->module_identity))) fail("invalid concrete nominal ownership for interface binding");
            if (t.construction && !d->managed) fail("construction view requires a managed nominal");
            if (t.kind==Type::aggregate && d->managed) fail("managed nominal cannot be an inline value");
            auto [found,added]=definitions.emplace(d->identity,d);
            if (!added && found->second!=d) fail("conflicting owned value type definitions");
            if (!checked.insert(d).second) continue;
            if (auto base=d->base.lock()) {
                if (!d->managed || !base->managed) fail("inheritance requires managed nominal types");
                std::set<const ValueType*> ancestry{d};
                for(auto parent=base;parent;parent=parent->base.lock()) {
                    if (ancestry.size()>=128 || !ancestry.insert(parent.get()).second) fail("recursive or excessive inheritance");
                }
                if (d->fields.size()<base->fields.size()) fail("missing inherited field prefix");
                for(unsigned n=0;n<base->fields.size();++n) {
                    const auto& a=d->fields[n];const auto& b=base->fields[n];
                    if(a.name!=b.name || Type(a.type)!=Type(b.type) || a.owner!=b.owner || a.using_field!=b.using_field) fail("inconsistent inherited field ownership/type");
                }
                pending.emplace_back(base,Type::managed_ref);
            }
            std::set<std::string> fields;
            for (const auto& field:d->fields) {
                if (field.name.empty() || field.name.find('\0')!=std::string::npos || !fields.insert(field.name).second || Type(field.type)==Type::none) fail("invalid value field");
                if(field.using_field && ((Type(field.type).kind!=Type::aggregate && Type(field.type).kind!=Type::managed_ref) || field.type.nullable)) fail("invalid using component type");
                if(Type(field.type).kind==Type::prepared_receiver) fail("prepared receiver cannot be stored in a field");
                if (field.type.readonly) fail("readonly nominal field declarations are not supported");
                pending.push_back(field.type);
            }
            // References break physical layout recursion, not graph validation.
            inline_layout(Type(t.definition));
        }
        const auto shape=reference_paths(root);
        for (const auto& path:root.readonly_references) if (!shape.contains(path)) fail("invalid readonly reference path");
    };
    auto assignable = [](Type actual,Type expected) {
        return actual==expected || (reference_value(actual) && actual.kind==expected.kind && Type::key(actual)==Type::key(expected) && actual.construction==expected.construction && (!actual.nullable || expected.nullable));
    };
    std::set<const ValueType*> catalog_definitions;
    for(auto t:m.nominal_types) {
        if((t.kind!=Type::aggregate && t.kind!=Type::managed_ref && t.kind!=Type::interface_ref) || !t.definition || !catalog_definitions.insert(t.definition.get()).second) fail("invalid or duplicate nominal catalog entry");
        valid_type(t);
        if(t.kind==Type::managed_ref && !t.definition->managed) fail("nominal catalog requires a managed class");
        auto base=t.definition->base.lock();
        for(unsigned n=base ? static_cast<unsigned>(base->fields.size()) : 0;n<t.definition->fields.size();++n)
            if(t.definition->fields[n].owner!=t.definition->identity) fail("nominal catalog field belongs to another owner");
    }
    struct Signature { std::vector<Type> parameters; Type result; bool receiver = false, mutates = false, initializer = false; };
    std::size_t string_bytes=0; std::set<std::string_view> unique_strings;
    for (const auto& text:m.strings) {
        if (!m.coordinated || text.size()>string_constant_limit || !valid_utf8(text) || !unique_strings.insert(text).second || text.size()>string_pool_limit-string_bytes) fail("invalid or excessive string constant pool");
        string_bytes+=text.size();
    }
    std::map<std::string, Signature> names;
    for (const auto& name : m.imports)
        if (name.empty() || !names.emplace(name, Signature{{Type::i32}, Type::i32}).second) fail("duplicate or empty import");
    std::map<std::string,const Function*> requirements;
    for(const auto& requirement:m.interface_requirements) {
        if(requirement.parameters.empty() || requirement.parameters[0].kind!=Type::interface_ref || requirement.parameters[0].nullable || requirement.parameters[0].construction ||
            !requirement.receiver || requirement.initializer || requirement.overriding || !requirement.virtual_slot.empty() || !requirement.blocks.empty() || !requirement.locals.empty() ||
            requirement.method_name.empty() || requirement.method_name=="init" || requirement.method_name=="deinit") fail("invalid interface requirement header");
        auto owner=requirement.parameters[0].definition;
        if(!owner || requirement.name!=owner->identity+"::method::"+requirement.method_name || requirement.module_identity!=owner->module_identity || requirement.visibility!=owner->visibility)
            fail("invalid interface requirement ownership");
        if(!requirements.emplace(requirement.name,&requirement).second || !names.emplace(requirement.name,Signature{requirement.parameters,requirement.result,true}).second) fail("duplicate interface requirement");
        for(auto t:requirement.parameters) {valid_type(t);if(t==Type::none || t.construction || t.kind==Type::prepared_receiver) fail("invalid interface requirement parameter");}
        for(auto t:requirement.parameters) if(t.definition && ((requirement.visibility=="public" && t.definition->visibility!="public") || (requirement.visibility=="internal" && t.definition->visibility=="private"))) fail("interface requirement exposes a less-visible parameter type");
        if(requirement.result.definition && ((requirement.visibility=="public" && requirement.result.definition->visibility!="public") || (requirement.visibility=="internal" && requirement.result.definition->visibility=="private"))) fail("interface requirement exposes a less-visible result type");
        valid_type(requirement.result);if(requirement.result.construction || requirement.result.kind==Type::prepared_receiver) fail("invalid interface requirement result");
    }
    for(auto t:m.nominal_types) if(t.kind==Type::interface_ref) {
        bool found=false;for(const auto& [name,r]:requirements) found|=r->parameters[0].definition==t.definition;
        if(!found) fail("interface has no requirements");
    }
    for (const auto& f : m.functions) {
        if(!m.interface_requirements.empty() && !m.source_imports.contains(f.module_identity)) fail("function has no source access scope");
        if (f.name.empty() || !names.emplace(f.name, Signature{f.parameters, f.result,f.receiver,f.receiver_mutates,f.initializer}).second) fail("duplicate or empty function");
        std::vector<unsigned> context_parameters;
        for (unsigned n=0;n<f.parameters.size();++n) if (f.parameters[n].construction) context_parameters.push_back(n);
        auto expected_name=f.module_identity+"::"+f.source_name;
        if (!f.initializer) expected_name=construction_name(expected_name,context_parameters);
        if (!f.module_identity.empty() && (f.source_name.empty() || f.name != expected_name))
            fail("inconsistent function ownership");
        if (f.receiver) {
            if (f.parameters.empty() || (f.parameters[0].kind != Type::aggregate && f.parameters[0].kind != Type::managed_ref) || f.locals.empty() || f.locals[0] != f.parameters[0] || readonly_paths(f.locals[0])!=readonly_paths(f.parameters[0])) fail("invalid receiver signature/storage");
        } else if (f.receiver_mutates) fail("receiver effect without receiver");
        if (f.initializer && (!f.receiver || f.result != Type::none)) fail("invalid initializer signature");
        if(f.initializer && f.parameters[0].construction && f.parameters[0].readonly) fail("initializer requires mutable construction storage");
        if(f.result.kind==Type::prepared_receiver) fail("prepared receiver cannot be returned");
        for(auto t:f.parameters) if(t.kind==Type::prepared_receiver) fail("prepared receiver cannot be a formal parameter");
        if (f.result.construction) fail("construction view returned from function");
        if (f.initializer && f.parameters[0].kind==Type::managed_ref && (!f.parameters[0].construction || context_parameters!=std::vector<unsigned>{0})) fail("managed init requires a private construction receiver");
        valid_type(f.result);
        for (auto t : f.parameters) if (t == Type::none) fail("no-result parameter"); else valid_type(t);
        for (auto t : f.locals) if (t == Type::none) fail("no-result slot"); else valid_type(t);
    }
    std::map<std::string,const Function*> functions;
    std::set<std::string> initializer_types;
    for(const auto& f:m.functions) {
        functions.emplace(f.name,&f);
        if(f.initializer && !initializer_types.insert(Type::key(f.parameters[0])).second) fail("duplicate nominal initializer");
    }
    for(const auto& f:m.functions) {
        if(!f.method_name.empty()) {
            if(f.visibility!="public" && f.visibility!="private" && f.visibility!="internal") fail("invalid method visibility");
            if(f.initializer && f.method_name!="init") fail("invalid initializer lifecycle name");
            if(!f.receiver) fail("method ownership requires a receiver");
            auto expected=Type::key(f.parameters[0])+"::method::"+f.method_name;
            std::vector<unsigned> context;
            for(unsigned n=0;n<f.parameters.size();++n) if(f.parameters[n].construction) context.push_back(n);
            if(!f.initializer) expected=construction_name(expected,context);
            if(f.name!=expected) fail("method belongs to another nominal owner");
            for(const auto& field:f.parameters[0].definition->fields) if(field.name==f.method_name) fail("method conflicts with stored field");
            if(!f.initializer) for(auto base=f.parameters[0].definition->base.lock();base;base=base->base.lock()) {
                for(const auto& inherited:m.functions) if(inherited.receiver && !inherited.initializer && inherited.method_name==f.method_name && inherited.parameters[0].definition==base) {
                    if(!f.overriding || inherited.virtual_slot.empty() || inherited.virtual_slot!=f.virtual_slot) fail("inherited method hiding or wrong override family");
                }
            }
        }
        if (f.virtual_slot.empty()) { if(f.overriding) fail("override without virtual slot"); continue; }
        if (f.method_name.empty() || f.visibility=="private" || !f.receiver || f.initializer || f.parameters[0].kind!=Type::managed_ref || !f.parameters[0].definition->managed || f.parameters[0].construction) fail("invalid virtual receiver");
        auto root=functions.find(f.virtual_slot);
        if(root==functions.end() || root->second->virtual_slot!=f.virtual_slot || root->second->overriding) fail("invalid virtual family root");
        const auto& original=*root->second;
        if (f.method_name!=original.method_name || f.visibility!=original.visibility || f.overriding!=(f.name!=f.virtual_slot) || f.parameters.size()!=original.parameters.size() || f.result!=original.result || !subtype(f.parameters[0],original.parameters[0])) fail("invalid override signature");
        for(unsigned n=1;n<f.parameters.size();++n) if(f.parameters[n]!=original.parameters[n] || (f.parameters[n].readonly!=original.parameters[n].readonly || readonly_paths(f.parameters[n])!=readonly_paths(original.parameters[n]))) fail("inexact override parameter");
        if(std::none_of(m.nominal_types.begin(),m.nominal_types.end(),[&](auto t){return t.kind==Type::managed_ref && Type::key(t)==Type::key(f.parameters[0]);})) fail("virtual receiver absent from nominal catalog");
        if(f.name==f.virtual_slot) virtual_targets(m,f);
        if(f.overriding && Type::key(f.parameters[0])==Type::key(original.parameters[0])) fail("override requires a descendant receiver");
        for(const auto& other:m.functions) if(other.name!=f.name && other.virtual_slot==f.virtual_slot && Type::key(other.parameters[0])==Type::key(f.parameters[0])) fail("duplicate virtual implementation");
    }
    for(auto t:m.nominal_types) if(t.kind==Type::managed_ref) {
        bool own=false,ancestor=false;
        for(const auto& f:m.functions) if(f.initializer && Type::key(f.parameters[0])==Type::key(t)) own=true;
        for(auto base=t.definition->base.lock();base;base=base->base.lock()) for(const auto& f:m.functions)
            if(f.initializer && Type::key(f.parameters[0])==base->identity) ancestor=true;
        if(ancestor && !own) fail("derived type omitted required initializer");
    }
    std::map<std::string,Module::Binding> expected_bindings,bindings;
    try {for(auto binding:interface_universe(m)) expected_bindings.emplace(binding.key,std::move(binding));}
    catch(const std::length_error& failure) {fail(failure.what());}
    for(const auto& binding:m.interface_bindings) {
        valid_type(binding.view);valid_type(binding.interface);
        if(binding.key!=binding_key(binding) || !bindings.emplace(binding.key,binding).second) fail("invalid or duplicate interface binding key");
        auto expected=expected_bindings.find(binding.key);
        if(expected==expected_bindings.end() || expected->second.methods!=binding.methods || expected->second.view!=binding.view || expected->second.interface!=binding.interface)
            fail("interface binding is not independently accessible and structurally valid");
    }
    if(bindings.size()!=expected_bindings.size()) fail("incomplete interface binding universe");
    for (const auto& f : m.functions) {
        if (f.blocks.empty()) fail("function has no blocks");
        qualified_types.emplace_back();
        std::vector<std::vector<Type>> body_types;
        for (const auto& b : f.blocks) {
            std::vector<Type> types;
            std::vector<bool> pure_templates;
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
                if(i.array_repeat && i.op!=Op::fixed_array) fail("repeat template on non-array instruction");
                switch (i.op) {
                case Op::interface_pack: {
                    valid_type(i.type);auto from=operand(i.a);auto concrete=from;concrete.nullable=false;
                    if(from.construction || i.type.kind!=Type::interface_ref || (from.nullable && !i.type.nullable) || !capability_assignable(from,i.type)) fail("invalid interface erasure");
                    auto proof=structural_binding(m,concrete,i.type,f.module_identity);
                    if(i.callee!=proof.key || !bindings.contains(proof.key)) fail("interface pack bypasses scoped binding/access proof");
                    result=i.type;break;
                }
                case Op::interface_prepare: {
                    auto from=operand(i.a);auto requirement=requirements.find(i.callee);
                    if(from.kind!=Type::interface_ref || from.nullable || from.construction || requirement==requirements.end() || !method_accessible(m,*requirement->second,f.module_identity) || requirement->second->parameters[0].definition!=from.definition ||
                        i.type.kind!=Type::prepared_receiver || i.type.nullable || i.type.definition!=from.definition || i.type.preparation!=i.callee || !capability_assignable(from,i.type)) fail("invalid interface receiver preparation");
                    valid_type(i.type);result=i.type;break;
                }
                case Op::interface_call: {
                    auto found=requirements.find(i.callee);
                    if(found==requirements.end() || !i.receiver || !i.receiver->fields.empty() || i.initialization) fail("invalid interface invocation");
                    auto prepared=slot(i.receiver->slot);const auto& requirement=*found->second;
                    if(prepared.kind!=Type::prepared_receiver || prepared.preparation!=i.callee || prepared.definition!=requirement.parameters[0].definition || i.arguments.size()+1!=requirement.parameters.size()) fail("interface call does not match prepared receiver");
                    for(unsigned n=0;n<i.arguments.size();++n) expect(i.arguments[n],requirement.parameters[n+1]);
                    result=requirement.result;break;
                }
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
                    if(std::any_of(m.nominal_types.begin(),m.nominal_types.end(),[&](auto t){return t.kind==Type::managed_ref && Type::key(t)==Type::key(i.type);})) fail("nominal source objects require complete construction");
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
                case Op::fixed_array:
                    valid_type(i.type);
                    if(i.type.kind!=Type::fixed_array || i.arguments.size()!=(i.array_repeat ? (*i.type.definition->extent ? 1u : 0u) : *i.type.definition->extent) || i.receiver || i.initialization || i.immediate || !i.callee.empty())
                        fail("invalid fixed-array construction/template");
                    if(i.array_repeat) for(auto argument:i.arguments)
                        if(argument>=pure_templates.size() || !pure_templates[argument]) fail("effectful repeated array prototype");
                    for(auto argument:i.arguments) expect(argument,array_element(i.type));
                    result=i.type;break;
                case Op::fixed_snapshot:
                    if(!f.fixed_ownership || !scalar_fixed_array(operand(i.a)) || i.receiver || !i.arguments.empty() || i.b || i.immediate || i.initialization || !i.callee.empty())
                        fail("invalid fixed snapshot");
                    result=operand(i.a);break;
                case Op::fixed_select:
                    if(!f.fixed_ownership || !i.receiver || !i.receiver->fields.empty() || !scalar_fixed_array(slot(i.receiver->slot)) ||
                       slot(i.receiver->slot).readonly || !numeric::integer(operand(i.a)) || i.b || i.immediate || i.initialization || !i.arguments.empty() || !i.callee.empty())
                        fail("invalid fixed selection");
                    result=Type::none;break;
                case Op::fixed_commit: {
                    if(!f.fixed_ownership || i.a>=types.size() || b.instructions[i.a].op!=Op::fixed_select || i.receiver ||
                       i.immediate || i.initialization || !i.arguments.empty() || !i.callee.empty()) fail("invalid selected-place commit");
                    expect(i.b,array_element(slot(b.instructions[i.a].receiver->slot)));
                    result=Type::none;break;
                }
                case Op::array_get: case Op::array_set: {
                    const auto array=operand(i.a), index=operand(i.b);
                    if(!scalar_fixed_array(array) || !numeric::integer(index) || i.receiver || i.initialization || i.immediate || !i.callee.empty())
                        fail("invalid checked scalar array access");
                    if(i.op==Op::array_set) {
                        if(i.arguments.size()!=1) fail("array update requires exactly one value");
                        expect(i.arguments[0],array_element(array));result=array;
                    } else {
                        if(!i.arguments.empty()) fail("array read has unexpected operands");
                        result=array_element(array);
                    }
                    break;
                }
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
                case Op::string_literal:
                    if (!m.coordinated || i.type!=Type::string || i.immediate<0 || static_cast<std::size_t>(i.immediate)>=m.strings.size()) fail("invalid string literal");
                    valid_type(i.type); result=Type::string; break;
                case Op::string_concat: case Op::string_equal:
                    if (!m.coordinated || operand(i.a)!=Type::string || operand(i.b)!=Type::string) fail("invalid string operands");
                    result=i.op==Op::string_concat ? Type::string : Type::boolean; break;
                case Op::constant:
                    if (array_type(i.type) || i.type.construction || i.type.kind == Type::aggregate || i.type.kind==Type::prepared_receiver || i.type == Type::none || (reference_value(i.type) && i.immediate != 0) || (i.type == Type::boolean && i.immediate != 0 && i.immediate != 1) || (i.type==Type::string && i.immediate!=0)) fail("invalid literal");
                    if (reference_value(i.type) && !i.type.nullable) fail("null literal requires a nullable reference type");
                    valid_type(i.type); result = i.type; break;
                case Op::add: case Op::subtract: case Op::multiply: case Op::divide: case Op::remainder:
                    result = operand(i.a);
                    if (!numeric::supports(i.op,result)) fail("illegal numeric operation");
                    expect(i.b,result); break;
                case Op::less: case Op::less_equal: case Op::greater: case Op::greater_equal:
                    if (!numeric::supports(i.op,operand(i.a))) fail("non-numeric ordering");
                    expect(i.b,operand(i.a)); result = Type::boolean; break;
                case Op::equal: case Op::not_equal:
                    if (!((reference_value(operand(i.a)) && !operand(i.a).construction && operand(i.b).kind==operand(i.a).kind && !operand(i.b).construction && Type::key(operand(i.a))==Type::key(operand(i.b))) || (numeric::supports(i.op,operand(i.a)) && operand(i.a)==operand(i.b)))) fail("comparison type mismatch");
                    result = Type::boolean; break;
                case Op::stage_prefix: {
                    auto from=operand(i.a),to=i.type;
                    if(!from.construction || !to.construction) fail("base projection requires construction views");
                    from.construction=to.construction=false;
                    if(!subtype(from,to) || from.nullable || to.nullable || !capability_assignable(operand(i.a),i.type)) fail("invalid construction base projection");
                    result=i.type; break;
                }
                case Op::upcast:
                    valid_type(i.type);
                    if(!(subtype(operand(i.a),i.type) || (operand(i.a).kind==Type::interface_ref && i.type.kind==Type::interface_ref && Type::key(operand(i.a))==Type::key(i.type))) || (operand(i.a).nullable && !i.type.nullable) || !capability_assignable(operand(i.a),i.type)) fail("invalid managed upcast");
                    result=i.type; break;
                case Op::base_init: {
                    valid_type(i.type);
                    if(!f.initializer || !f.parameters[0].construction || !i.receiver || i.receiver->slot!=0 || !i.receiver->fields.empty()) fail("base initialization requires its derived initializer receiver");
                    auto base=f.parameters[0].definition->base.lock();
                    if(!base || i.type.kind!=Type::managed_ref || i.type.nullable || i.type.construction || i.type.definition!=base) fail("base initializer type mismatch");
                    if(i.callee.empty()) {
                        if(i.arguments.size()!=base->fields.size()) fail("base fields argument count mismatch");
                        for(unsigned n=0;n<i.arguments.size();++n) expect(i.arguments[n],base->fields[n].type);
                        for(const auto& target:m.functions) if(target.initializer && Type::key(target.parameters[0])==base->identity) fail("base field construction bypasses initializer");
                    } else {
                        auto found=functions.find(i.callee);
                        if(found==functions.end() || !found->second->initializer || Type::key(found->second->parameters[0])!=base->identity || i.arguments.size()+1!=found->second->parameters.size()) fail("invalid base initializer target");
                        for(unsigned n=0;n<i.arguments.size();++n) expect(i.arguments[n],found->second->parameters[n+1]);
                    }
                    result=Type::none; break;
                }
                case Op::convert:
                    valid_type(i.type);
                    if (!((reference_value(operand(i.a)) && !operand(i.a).construction && i.type.kind==operand(i.a).kind && !i.type.construction && Type::key(operand(i.a))==Type::key(i.type) && !i.type.nullable) || (numeric::number(operand(i.a)) && numeric::number(i.type)))) fail("invalid conversion");
                    result = i.type; break;
                case Op::negate:
                    result = operand(i.a); if (!numeric::number(result)) fail("non-numeric negation"); break;
                case Op::logical_not: expect(i.a, Type::boolean); result = Type::boolean; break;
                case Op::load: result = slot(i.a); break;
                case Op::store:
                    if (f.receiver && f.parameters[0].kind==Type::aggregate && !f.receiver_mutates && i.a == 0) fail("non-mutating method writes receiver");
                    expect(i.b, slot(i.a)); result = Type::none; break;
                case Op::call: case Op::virtual_call: {
                    auto declaration=functions.find(i.callee);
                    if(requirements.contains(i.callee)) fail("ordinary call cannot bypass interface receiver preparation");
                    if(i.op==Op::virtual_call && (declaration==functions.end() || declaration->second->virtual_slot.empty() || i.initialization)) fail("invalid virtual call target");
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
                bool pure=i.op==Op::constant;
                if(i.op==Op::aggregate || i.op==Op::fixed_array) {
                    pure=true;
                    for(auto argument:i.arguments) pure=pure && argument<pure_templates.size() && pure_templates[argument];
                }
                pure_templates.push_back(pure);
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
        validate_fixed_ownership(f,m,body_types);
    }
    if(!m.interface_requirements.empty()) for(const auto& [key,definition]:definitions) if(!catalog_definitions.contains(definition)) fail("incomplete nominal universe for structural interfaces");
    validate_reference_capabilities(m,qualified_types,verified_effects);
    if (results) *results=std::move(qualified_types);
}
}
