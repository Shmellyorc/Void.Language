#include "void/ir.hpp"
#include "void/native_binding.hpp"
#include "void/numeric.hpp"
#include "void/temporal_ops.hpp"
#include "void/reference_capabilities.hpp"
#include <map>
#include <algorithm>
#include <set>
#include <functional>
#include <iterator>
#include <bit>
#include <limits>
#include <stdexcept>
namespace vd::ir {
bool array_type(Type t) { return t.kind==Type::fixed_array || t.kind==Type::growable_array; }
bool collection_type(Type t) {return array_type(t) || t.kind==Type::dictionary;}
Type dictionary_type(Type key,Type value) {
    auto identity=[](Type t){return std::to_string(t.kind)+":"+Type::key(t)+(t.nullable?"?":"");};
    const auto k=identity(key),v=identity(value);auto name="$dict:"+std::to_string(k.size())+":"+k+":"+v;
    auto entry=std::make_shared<ValueType>();entry->identity=entry->name=name+"::entry";
    auto plain=[](Type t){t.readonly=false;t.readonly_references.clear();return t;};
    entry->fields={{"key",FieldType(plain(key))},{"value",FieldType(plain(value))},{"hash",FieldType(Type::u64)}};
    Type pair(entry);auto definition=std::make_shared<ValueType>();definition->identity=definition->name=name;definition->element=FieldType(pair);
    Type result(definition,Type::dictionary);ReferencePaths paths;
    for(unsigned field=0;field<2;++field)for(auto path:readonly_paths(field?value:key)){path.insert(path.begin(),field);path.insert(path.begin(),array_element_path);paths.insert(std::move(path));}
    return with_readonly_paths(result,paths);
}
Type dictionary_key(Type t) {return field_type(array_element(t),0);}
Type dictionary_value(Type t) {return field_type(array_element(t),1);}
bool dictionary_key_supported(Type root) {
    std::set<const ValueType*> active;std::map<const ValueType*,bool> memo;std::size_t work=0;
    std::function<bool(Type)> visit=[&](Type t){
        if(numeric::integer(t) || t==Type::boolean || t==Type::string || t.kind==Type::managed_ref)return true;
        if(t.kind!=Type::aggregate || !t.definition || (!t.definition->cases.empty() || t.definition->callable) || active.size()>=128 || ++work>65536)return false;
        if(auto found=memo.find(t.definition.get());found!=memo.end())return found->second;
        if(!active.insert(t.definition.get()).second)return false;
        for(const auto& field:t.definition->fields)if(!visit(field.type))return false;active.erase(t.definition.get());memo.emplace(t.definition.get(),true);return true;
    };return visit(root);
}

bool pattern_comparable(Type root) {
    std::vector<std::pair<Type,unsigned>> pending{{root,0}};std::set<const ValueType*> seen;
    while(!pending.empty()) {
        auto [type,depth]=pending.back();pending.pop_back();
        if(depth>128 || type.construction || type.kind==Type::prepared_receiver || collection_type(type) || type==Type::none || type.kind==Type::invalid || type.kind==Type::unresolved || type.kind==Type::null_value)return false;
        if(type.kind==Type::aggregate){if(!type.definition)return false;if(seen.insert(type.definition.get()).second)for(const auto& field:type.definition->fields)pending.push_back({field.type,depth+1});}
    }
    return true;
}

Type array_element(Type t) {
    if (!collection_type(t) || !t.definition || !t.definition->element) throw std::invalid_argument("VOID IR: missing array element");
    auto element=Type(*t.definition->element);ReferencePaths paths=readonly_paths(element);
    for(const auto& path:readonly_paths(t))if(!path.empty() && path[0]==array_element_path)paths.insert(ReferencePath(path.begin()+1,path.end()));
    if(t.readonly)element=readonly_view(element);else element=with_readonly_paths(element,paths);
    return element;
}
std::string array_identity(Type element, std::optional<std::uint64_t> extent) {
    const auto key=std::to_string(static_cast<unsigned>(element.kind))+":"+Type::key(element)+(element.nullable ? "?" : "");
    return "$array:"+std::to_string(key.size())+":"+key+":"+(extent ? std::to_string(*extent) : "growable");
}
bool scalar_array(Type t) {
    return array_type(t) && t.definition && t.definition->element &&
        (numeric::number(array_element(t)) || array_element(t)==Type::boolean);
}
bool scalar_fixed_array(Type t) {return t.kind==Type::fixed_array && scalar_array(t);}
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
        if((t.nullable && !reference_value(t) && !(t.definition && t.definition->callable)) || t.construction || !t.preparation.empty()) throw std::invalid_argument("Unsupported array value shape");
        if(numeric::number(t) || t==Type::boolean) return {0,t==Type::boolean ? 1u : numeric::width(t)/8u,1};
        if(reference_value(t) || t.kind==Type::string || (t.kind==Type::growable_array || t.kind==Type::dictionary))return {0,sizeof(void*),1};
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
bool worker_transfer_safe(Type root) {
    std::vector<Type> pending{root};std::set<const ValueType*> seen;
    while(!pending.empty()){auto t=pending.back();pending.pop_back();
        if(t.construction || t.nullable)return false;
        if(t.kind==Type::none || t.kind==Type::string || (t.kind>=Type::i32 && t.kind<=Type::f64))continue;
        if(t.kind!=Type::aggregate && t.kind!=Type::fixed_array)return false;
        if(!t.definition || t.definition->managed || t.definition->callable || t.definition->interface_type || t.definition->task_result)return false;
        if(!seen.insert(t.definition.get()).second)continue;
        if(t.kind==Type::fixed_array)pending.push_back(array_element(t));else for(const auto& field:t.definition->fields)pending.push_back(field.type);
    }return true;
}
bool async_boundary_safe(Type root) {
    std::vector<Type> pending{root};std::set<const ValueType*> seen;
    while(!pending.empty()){auto t=pending.back();pending.pop_back();if(t.construction || t.kind==Type::prepared_receiver)return false;if(!t.definition || !seen.insert(t.definition.get()).second)continue;if(t.definition->callable || t.definition->native_resource)return false;if(t.definition->element)pending.push_back(*t.definition->element);for(const auto& field:t.definition->fields)pending.push_back(field.type);}
    return true;
}
bool requires_initialization(Type t){return !t.nullable && (reference_value(t) || (t.definition && (t.definition->callable || !t.definition->cases.empty())));}
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
            if(current->task_result)if(auto child=current->task_result->target.lock())pending.push_back(std::move(child));
            if(current->call_result)if(auto child=current->call_result->target.lock())pending.push_back(std::move(child));
            for(const auto& parameter:current->call_parameters)if(auto child=parameter.target.lock())pending.push_back(std::move(child));
            if (current->element) if (auto child=current->element->target.lock()) pending.push_back(std::move(child));
            for (const auto& field : current->fields)
                if (auto child = field.type.target.lock()) pending.push_back(std::move(child));
        }
    }
    definition = std::shared_ptr<const ValueType>(std::move(owner),d.get());
}
FieldType::FieldType(Type t) : kind(t.kind), nullable(t.nullable), readonly(t.readonly), readonly_references(t.readonly_references) {
    if (t.construction) throw std::invalid_argument("construction view cannot be a field type");
    if (t.definition) target = t.definition->self;
}
FieldType::operator Type() const {
    if (kind != Type::aggregate && kind != Type::unresolved && kind != Type::managed_ref && kind != Type::interface_ref && kind!=Type::fixed_array && kind!=Type::growable_array && kind!=Type::dictionary) { Type t(kind); t.nullable=nullable; t.readonly=readonly;t.readonly_references=readonly_references; return t; }
    auto d = target.lock();
    if (!d) throw std::logic_error("nominal type edge outlived its definition table");
    Type t(std::move(d),kind); t.nullable=nullable; t.readonly=readonly;t.readonly_references=readonly_references; return t;
}

ReferencePaths reference_paths(Type root, bool include_strings) {
    ReferencePaths result;
    struct Pending {Type type;ReferencePath path;std::set<const ValueType*> lists;};
    std::vector<Pending> pending{{root,{}, {}}};
    std::size_t expanded=0;
    while (!pending.empty()) {
        auto [type,path,lists]=std::move(pending.back()); pending.pop_back();
        if(type.definition && type.definition->callable)continue; // Callable environments are opaque; invocation uses the checked signature.
        if (type.construction) continue; // A stage is a place, never a heap root.
        // A private immutable task carries logical result authority/provenance,
        // like a collection handle. The LLVM root mapper traces its pointer only.
        if(type.definition && type.definition->task_result) {
            result.insert(path);
            if(!lists.insert(type.definition.get()).second || path.size()>=128 || ++expanded>65536)throw std::invalid_argument("VOID IR: recursive task result shape");
            if(Type(*type.definition->task_result)!=Type::none){path.push_back(1);pending.push_back({type.definition->fields[1].type,std::move(path),std::move(lists)});}
            continue;
        }
        if (reference_value(type) || type.kind==Type::prepared_receiver || (include_strings && type.kind==Type::string)) { result.insert(std::move(path)); continue; }
        if(collection_type(type)) {
            if(!type.definition || !type.definition->element)throw std::invalid_argument("VOID IR: missing array reference shape");
            if((type.kind==Type::growable_array || type.kind==Type::dictionary)) {result.insert(path);if(!lists.insert(type.definition.get()).second)continue;}
            if(path.size()>=128 || ++expanded>65536)throw std::invalid_argument("VOID IR: recursive or excessive array reference shape");
            path.push_back(array_element_path);pending.push_back({Type(*type.definition->element),std::move(path),std::move(lists)});continue;
        }
        if (type.kind!=Type::aggregate) continue;
        if (!type.definition || path.size()>=128 || ++expanded>65536)
            throw std::invalid_argument("VOID IR: recursive or excessive reference shape");
        for (unsigned n=0;n<type.definition->fields.size();++n) {
            auto child=path; child.push_back(n);
            pending.push_back({type.definition->fields[n].type,std::move(child),lists});
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
    ReferencePaths normalized=paths;
    if(!paths.empty() && !type.construction) {
        auto declared=reference_paths(type);
        for(const auto& path:paths)if(!declared.contains(path)) {
            auto raw=type;raw.readonly=false;raw.readonly_references.clear();ReferencePath prefix;std::optional<ReferencePath> container;
            if((raw.kind==Type::growable_array || raw.kind==Type::dictionary) && declared.contains(prefix))container=prefix;
            for(auto field:path) {
                raw=field_type(raw,field);raw.readonly=false;raw.readonly_references.clear();prefix.push_back(field);
                if((raw.kind==Type::growable_array || raw.kind==Type::dictionary) && declared.contains(prefix))container=prefix;
            }
            if(!container)throw std::invalid_argument("VOID IR: readonly path outside reference shape");
            normalized.erase(path);normalized.insert(*container);
        }
    }
    // A private construction view is a place capability, not a managed leaf.
    // Its readonly place permission must survive ordinary fact propagation.
    if (type.construction) {
        if(!normalized.empty() && normalized!=ReferencePaths{ReferencePath{}}) throw std::invalid_argument("VOID IR: invalid construction authority path");
        type.readonly|=normalized.contains(ReferencePath{});type.readonly_references.clear();return type;
    }
    type.readonly=false; type.readonly_references=normalized;
    if((type.kind==Type::growable_array || type.kind==Type::dictionary)) {type.readonly=normalized.contains(ReferencePath{});type.readonly_references.erase(ReferencePath{});return type;}
    if ((reference_value(type) || type.kind==Type::prepared_receiver) && !type.construction) {
        type.readonly=normalized.contains(ReferencePath{});
        if(type.definition && type.definition->task_result)type.readonly_references.erase(ReferencePath{});else type.readonly_references.clear();
    }
    return type;
}
Type readonly_view(Type type) {
    if (type.construction) { type.readonly=true; return type; }
    return with_readonly_paths(type,reference_paths(type));
}
Type field_type(Type owner,unsigned field) {
    if(collection_type(owner) && field==array_element_path)return array_element(owner);
    const auto& declaration=owner.definition->fields.at(field);
    auto result=Type(declaration.type);
    if(declaration.readonly)result=readonly_view(result);
    if (owner.kind==Type::managed_ref && !owner.definition->task_result) return owner.readonly ? readonly_view(result) : result;
    if(owner.readonly)return readonly_view(result);
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
    std::set<std::string> seen;
    std::function<bool(Type,Type)> visit=[&](Type a,Type e) {
        if(a.construction && a.readonly && !e.readonly)return false;
        const auto allowed=readonly_paths(e);for(const auto& path:readonly_paths(a))if(!allowed.contains(path))return false;
        auto key=std::to_string(a.kind)+":"+Type::key(a)+"|"+capability_key(a)+"|"+capability_key(e);
        if(!seen.insert(key).second)return true;
        if(collection_type(a) && collection_type(e)) {
            if((a.kind==Type::growable_array || a.kind==Type::dictionary) && !e.readonly && readonly_paths(array_element(a))!=readonly_paths(array_element(e)))return false;
            return visit(array_element(a),array_element(e));
        }
        if(a.kind==Type::aggregate && e.kind==Type::aggregate)for(unsigned n=0;n<a.definition->fields.size();++n)if(!visit(field_type(a,n),field_type(e,n)))return false;
        return true;
    };return visit(actual,expected);
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
    std::string result=array_type(type) ? type_name(type) : type.definition ? (type.definition->display_name.empty()?type.definition->name:type.definition->display_name) : type_name(type);
    for (auto field:path) {
        if(collection_type(type) && field==array_element_path)result+="[]";else result+='.'+type.definition->fields.at(field).name;
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
    if(t.definition && !t.definition->display_name.empty())return std::string(t.readonly?"readonly ":"")+t.definition->display_name+(t.nullable?"?":"");
    if(t.kind==Type::interface_ref && t.definition) return std::string(t.readonly ? "readonly " : "")+t.definition->identity+(t.nullable ? "?" : "");
    if(t.kind==Type::prepared_receiver && t.definition) return "prepared receiver "+t.definition->identity;
    if (t.readonly && t.kind!=Type::managed_ref) { t.readonly=false; return "readonly " + type_name(t); }
    if(t.kind==Type::dictionary)return "{"+type_name(dictionary_key(t))+": "+type_name(dictionary_value(t))+"}";
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
    for(const auto& symbol:m.imports)if(symbol.starts_with("vd_native_fixture_call"))throw std::invalid_argument("VOID IR: native bridge requires its verified typed descriptor");
    if (results) results->clear();
    InstructionTypes qualified_types;
    auto fail = [](const std::string& text) { throw std::invalid_argument("VOID IR: " + text); };
    if (m.identity.empty()) fail("missing module identity");
    if(!m.interface_requirements.empty() && (m.root_identity!=m.identity || !m.source_imports.contains(m.root_identity))) fail("invalid interface source scope graph");
    std::map<std::string,const ValueType*> definitions;
    std::set<const ValueType*> visiting, checked,dictionary_entries;
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
            if(t.kind==Type::dictionary) {
                if(!m.coordinated || !t.definition || !t.definition->element || t.definition->extent || !t.definition->fields.empty() || t.nullable || t.construction || !t.preparation.empty())fail("invalid dictionary type");
                auto entry=array_element(t);if(entry.kind!=Type::aggregate || entry.definition->fields.size()!=3 || field_type(entry,2)!=Type::u64 || !dictionary_key_supported(dictionary_key(t)) || dictionary_type(dictionary_key(t),dictionary_value(t))!=t)fail("invalid dictionary key/value shape");
                auto expected=dictionary_type(dictionary_key(t),dictionary_value(t));if(entry.definition->identity!=array_element(expected).definition->identity || entry.definition->managed || entry.definition->interface_type || !entry.definition->base.expired() || entry.definition->element || entry.definition->extent || entry.definition->fields[0].name!="key" || entry.definition->fields[1].name!="value" || entry.definition->fields[2].name!="hash")fail("invalid dictionary entry type");
                dictionary_entries.insert(entry.definition.get());if(checked.insert(t.definition.get()).second)pending.push_back(entry);continue;
            }
            if(array_type(t)) {
                if(t.kind==Type::growable_array && !m.coordinated)fail("growable arrays require coordinated storage");
                if(!t.definition || !t.definition->element || t.nullable || t.construction || !t.preparation.empty() ||
                   !t.definition->fields.empty() || t.definition->managed || t.definition->interface_type || !t.definition->base.expired() ||
                   (t.kind==Type::fixed_array)!=t.definition->extent.has_value()) fail("invalid structural array type");
                const auto element=Type(*t.definition->element);
                if(element==Type::none || element.kind==Type::prepared_receiver || element.kind==Type::invalid || element.kind==Type::unresolved ||
                   element.construction || element.readonly || !element.readonly_references.empty() ||
                   t.definition->identity!=array_identity(element,t.definition->extent)) fail("invalid array element or identity");
                if(checked.insert(t.definition.get()).second) {
                    try {if(t.kind==Type::fixed_array)compact_value_shape(t);}
                    catch(const std::length_error& failure) {fail(failure.what());}
                    pending.push_back(element);inline_layout(t);
                }
                continue;
            }
            if (t.kind==Type::string && (t.definition || t.nullable || t.construction || !t.preparation.empty() || !t.readonly_references.empty())) fail("invalid intrinsic string type");
            if (t.construction && (t.kind!=Type::managed_ref || t.nullable)) fail("invalid construction view type");
            if (t.nullable && !reference_value(t) && !(t.definition && t.definition->callable)) fail("nullable non-reference type");
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
            if(auto base=d->base.lock();base && base->native_resource)fail("native resource cannot be extended");
            if(d->native_resource && ((m.native_library!="void.fixture.v1" && m.native_library!="void.sink.v1") || t.kind!=Type::managed_ref || d->native_provider!=m.native_library || !d->managed || d->fields.size()!=1 || Type(d->fields[0].type)!=Type::u64 || d->callable || !d->base.expired() || !d->cases.empty() || d->interface_type))fail("invalid native resource identity");
            if(d->task_result){
                auto r=Type(*d->task_result);if(t.kind!=Type::managed_ref || !d->managed || d->callable || d->interface_type || !d->base.expired() || !d->cases.empty() || d->result_type || d->element || d->extent || d->fields.size()!=2 || Type(d->fields[0].type)!=Type::i32 || (Type(d->fields[1].type)!=(r==Type::none?Type::boolean:r) || capability_key(d->fields[1].type)!=capability_key(r==Type::none?Type::boolean:r)) || d->identity!="$task:"+std::to_string(r.kind)+":"+Type::key(r)+(r.nullable?"?":"")+":"+capability_key(r))fail("invalid task handle shape");
            }
            if(d->callable){
                if(t.kind!=Type::aggregate || d->managed || d->interface_type || !d->cases.empty() || d->result_type || d->fields.size()!=2 || Type(d->fields[0].type)!=Type::u64 || Type(d->fields[1].type).kind!=Type::managed_ref || !d->fields[1].type.nullable || !d->call_result || !d->base.expired())fail("invalid function value shape");
                pending.push_back(*d->call_result);for(const auto& parameter:d->call_parameters){if(Type(parameter)==Type::none)fail("no-result function parameter");pending.push_back(parameter);}
            }
            if(d->result_type && d->cases.empty())fail("result type has no variants");
            if(!d->cases.empty()) {
                if(t.kind!=Type::aggregate || d->fields.empty() || Type(d->fields[0].type)!=Type::i32 || d->managed || d->interface_type || !d->base.expired())fail("invalid sum layout");
                unsigned field=1;std::set<std::string> names;
                for(const auto& variant:d->cases){if(variant.name.empty() || variant.name.find('\0')!=std::string::npos || variant.begin!=field || !names.insert(variant.name).second || variant.count>d->fields.size()-field)fail("invalid sum variant layout");field+=variant.count;}
                if(field!=d->fields.size() || (d->result_type && (d->cases.size()!=2 || d->cases[0].name!="Ok" || d->cases[1].name!="Err" || d->cases[0].count!=1 || d->cases[1].count!=1)))fail("invalid sum variant coverage");
            }
            if(d->element || d->extent) fail("structural array metadata on nominal type");
            if(!m.interface_requirements.empty() && d->visibility!="public" && d->visibility!="internal" && d->visibility!="private") fail("invalid concrete nominal visibility");
            const bool callable_environment=d->identity=="$void::$environment";
            if(callable_environment && (!d->managed || !d->fields.empty() || !d->base.expired() || !d->module_identity.empty() || d->name!="$environment" || d->visibility!="private"))fail("invalid shared callable environment base");
            if(!m.interface_requirements.empty() && !callable_environment && !dictionary_entries.contains(d) && !d->result_type && !d->callable && !d->task_result && (d->identity!=d->module_identity+"::"+d->name || !m.source_imports.contains(d->module_identity))) fail("invalid concrete nominal ownership for interface binding");
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
                    if(a.name!=b.name || Type(a.type)!=Type(b.type) || capability_key(a.type)!=capability_key(b.type) || a.owner!=b.owner || a.using_field!=b.using_field || a.event!=b.event || a.visibility!=b.visibility || a.readonly!=b.readonly) fail("inconsistent inherited field ownership/type");
                }
                pending.emplace_back(base,Type::managed_ref);
            }
            std::set<std::string> fields;
            unsigned field_index=0;auto inherited=d->base.lock();
            for (const auto& field:d->fields) {
                if (field.name.empty() || field.name.find('\0')!=std::string::npos || !fields.insert(field.name).second || Type(field.type)==Type::none) fail("invalid value field");
                if(field.using_field && ((Type(field.type).kind!=Type::aggregate && Type(field.type).kind!=Type::managed_ref) || field.type.nullable || (Type(field.type).definition && (Type(field.type).definition->callable || !Type(field.type).definition->cases.empty())))) fail("invalid using component type");
                if(field.visibility!="public" && field.visibility!="internal" && field.visibility!="private")fail("invalid stored-field visibility");
                if(field.event && field.readonly)fail("const event field");
                if(!d->module_identity.empty() && (!inherited || field_index>=inherited->fields.size()) && field.owner!=d->identity)fail("forged stored-field declaring owner");
                if(field.readonly && readonly_paths(field.type)!=reference_paths(field.type))fail("const field omits declared readonly reference profile");
                if(Type(field.type).kind==Type::prepared_receiver) fail("prepared receiver cannot be stored in a field");
                ++field_index;
                pending.push_back(field.type);
            }
            // References break physical layout recursion, not graph validation.
            inline_layout(Type(t.definition));
        }
        const auto shape=reference_paths(root);
        for (const auto& path:root.readonly_references) if (!shape.contains(path)) fail("invalid readonly reference path");
    };
    auto assignable = [](Type actual,Type expected) {
        return actual==expected || (actual.definition && expected.definition && actual.definition->callable && expected.definition->callable && Type::key(actual)==Type::key(expected) && (!actual.nullable || expected.nullable)) || (reference_value(actual) && actual.kind==expected.kind && Type::key(actual)==Type::key(expected) && actual.construction==expected.construction && (!actual.nullable || expected.nullable));
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
    struct Signature { std::vector<Type> parameters; Type result; bool receiver = false, mutates = false, initializer = false, closure = false; };
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
        if(f.asynchronous){
            if(!m.coordinated || f.task.kind!=Type::managed_ref || f.task.nullable || f.task.construction || !f.task.definition || !f.task.definition->task_result || f.initializer || f.accessor || (f.receiver && f.method_name=="deinit") || !f.virtual_slot.empty() || f.closure || (f.receiver && (f.parameters.empty() || f.parameters[0].kind!=Type::managed_ref)))fail("invalid async declaration or task result");
            if(!async_boundary_safe(f.result))fail("callable escape through async result");for(auto t:f.parameters)if(!async_boundary_safe(t))fail("callable or borrowed place crosses async argument boundary");
            valid_type(f.task);
            if(std::none_of(m.nominal_types.begin(),m.nominal_types.end(),[&](auto t){return t.kind==Type::managed_ref && t.definition==f.task.definition;}))fail("async task absent from generation catalog");
            if(Type(*f.task.definition->task_result)!=f.result || capability_key(field_type(f.task,1))!=capability_key(f.result))fail("invalid async declaration or task result");
        }else if(f.task!=Type::none)fail("sync declaration carries async task shape");
        if (f.name.empty() || !names.emplace(f.name, Signature{f.parameters, f.result,f.receiver,f.receiver_mutates,f.initializer,f.closure}).second) fail("duplicate or empty function");
        std::vector<unsigned> context_parameters;
        for (unsigned n=0;n<f.parameters.size();++n) if (f.parameters[n].construction) context_parameters.push_back(n);
        auto expected_name=f.module_identity+"::"+f.source_name;
        if (!f.initializer) expected_name=construction_name(expected_name,context_parameters);
        if (!f.module_identity.empty() && (f.source_name.empty() || f.name != expected_name))
            fail("inconsistent function ownership");
        if(f.closure && (f.receiver || f.initializer || f.parameters.empty() || f.parameters[0].kind!=Type::managed_ref || f.parameters[0].nullable || f.parameters[0].construction))fail("invalid closure environment signature");
        if(f.bound_receiver && (!f.closure || f.parameters.empty() || f.parameters[0].kind!=Type::managed_ref || f.parameters[0].definition->fields.size()!=1 || (field_type(f.parameters[0],0).kind!=Type::managed_ref && field_type(f.parameters[0],0).kind!=Type::interface_ref)))fail("invalid bound callback receiver");
        if (f.receiver) {
            if (f.parameters.empty() || (f.parameters[0].kind != Type::aggregate && f.parameters[0].kind != Type::managed_ref) || f.locals.empty() || f.locals[0] != f.parameters[0] || readonly_paths(f.locals[0])!=readonly_paths(f.parameters[0])) fail("invalid receiver signature/storage");
            if(!f.module_identity.empty() && !f.parameters[0].definition->module_identity.empty() && f.module_identity!=f.parameters[0].definition->module_identity)fail("foreign receiver declaration");
        } else if (f.receiver_mutates) fail("receiver effect without receiver");
        if(f.accessor>2 || (f.accessor && (!f.receiver || f.initializer)) ||
           (f.accessor==1 && (f.parameters.size()!=1 || f.result==Type::none || f.receiver_mutates)) ||
           (f.accessor==2 && (f.parameters.size()!=2 || f.result!=Type::none)))fail("invalid property accessor");
        if (f.initializer && (!f.receiver || f.result != Type::none)) fail("invalid initializer signature");
        if(f.initializer && !f.module_identity.empty() && f.method_name!="init")fail("initializer grant on an ordinary source method");
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
        if(!f.lexical_owner.empty()) {
            if(f.receiver) {
                if(f.parameters.empty() || Type::key(f.parameters[0])!=f.lexical_owner)fail("forged lexical field owner");
                if(!f.module_identity.empty() && f.parameters[0].definition->module_identity!=f.module_identity)fail("foreign lexical field owner");
            } else {
                auto parent=functions.find(f.lexical_parent);
                const bool adapter=!f.receiver && !f.asynchronous && f.visibility=="private" && f.name.starts_with(f.lexical_parent+"::$display$");
                if((!f.closure && !adapter) || parent==functions.end() || parent->second==&f || parent->second->lexical_owner!=f.lexical_owner || parent->second->module_identity!=f.module_identity)fail("forged closure field owner");
                bool selected=false;for(const auto& block:parent->second->blocks)for(const auto& i:block.instructions)selected|=(i.op==Op::function_reference || (adapter && i.op==Op::call)) && i.callee==f.name;
                if(!selected)fail("closure field owner has no lexical creation");
                std::set<std::string> ancestry{f.name};auto enclosing=parent->second;
                while(!enclosing->receiver) {
                    if(!ancestry.insert(enclosing->name).second || !functions.contains(enclosing->lexical_parent))fail("cyclic or unowned closure field context");
                    enclosing=functions.at(enclosing->lexical_parent);
                }
                if(enclosing->parameters.empty() || Type::key(enclosing->parameters[0])!=f.lexical_owner)fail("closure field context has no declaring receiver");
            }
        }
        if(f.cleanup_entry && (!m.coordinated || *f.cleanup_entry>=f.blocks.size()))fail("invalid function cleanup entry");
        qualified_types.emplace_back();
        std::set<unsigned> group_slots;
        for(const auto& block:f.blocks)for(unsigned n=0;n<block.instructions.size();++n){const auto& i=block.instructions[n];if(i.op==Op::store && i.b<n && block.instructions[i.b].op==Op::async_group_enter)group_slots.insert(i.a);}
        std::vector<std::vector<Type>> body_types;
        for (const auto& b : f.blocks) {
            std::vector<Type> types;
            std::vector<bool> pure_templates;
            std::vector<bool> group_values;const Instruction* validating=nullptr;
            for(const auto& instruction:b.instructions)if(instruction.op==Op::function_reference && functions.contains(instruction.callee) && functions.at(instruction.callee)->asynchronous)fail("async function value requires an async callable contract");
            auto operand = [&](unsigned id) {
                if (id >= types.size() || types[id] == Type::none) fail("invalid operand in " + f.name);
                if(group_values[id] && (!validating || (validating->op!=Op::async_group_join && !(validating->op==Op::store && group_slots.contains(validating->a)))))fail("async group token escapes lifecycle operations");
                return types[id];
            };
            auto expect = [&](unsigned id, Type t) { if (!assignable(operand(id),t)) fail("mistyped operand in " + f.name+": expected "+type_name(t)+", received "+type_name(operand(id))+" at value "+std::to_string(id)); };
            auto slot = [&](unsigned id) {
                if (id >= f.locals.size()) fail("invalid slot in " + f.name);
                return f.locals[id];
            };
            for (const auto& i : b.instructions) {
                validating=&i;
                Type result = Type::i32;
                if(i.array_repeat && i.op!=Op::fixed_array) fail("repeat template on non-array instruction");
                switch (i.op) {
                case Op::interface_pack: {
                    valid_type(i.type);auto from=operand(i.a);auto concrete=from;concrete.nullable=false;
                    if(from.construction || i.type.kind!=Type::interface_ref || (from.nullable && !i.type.nullable) || !capability_assignable(from,i.type)) fail("invalid interface erasure");
                    auto proof=structural_binding(m,concrete,i.type,f.module_identity);
                    if(binding_requires_readonly(proof) && !i.type.readonly)fail("interface erasure discards const component authority");
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
                    if(i.type.definition && i.type.definition->task_result)fail("task construction requires async call");
                    valid_type(i.type);
                    if (!m.coordinated || i.type.kind!=Type::managed_ref || i.type.nullable || i.type.construction || i.arguments.size()!=i.type.definition->fields.size()) fail("invalid complete managed construction");
                    for (unsigned n=0;n<i.arguments.size();++n) expect(i.arguments[n],i.type.definition->fields[n].type);
                    if (i.receiver && (!i.receiver->fields.empty() || !slot(i.receiver->slot).construction || Type::key(slot(i.receiver->slot))!=Type::key(i.type))) fail("invalid construction commit place");
                    result=i.type; break;
                case Op::gc_allocate:
                    if(i.type.definition && i.type.definition->task_result)fail("task construction requires async call");
                    valid_type(i.type);
                    if(std::any_of(m.nominal_types.begin(),m.nominal_types.end(),[&](auto t){return t.kind==Type::managed_ref && Type::key(t)==Type::key(i.type);})) fail("nominal source objects require complete construction");
                    if (!m.coordinated || i.type.kind != Type::managed_ref || i.type.construction || i.type.definition->fields.empty()) fail("invalid managed allocation");
                    result = i.type; break;
                case Op::gc_read: case Op::gc_write: {
                    if(operand(i.a).definition && operand(i.a).definition->task_result)fail("task payload access requires await");
                    auto object = operand(i.a);
                    if (!m.coordinated || object.kind != Type::managed_ref || i.immediate < 0 || static_cast<std::size_t>(i.immediate) >= object.definition->fields.size()) fail("invalid managed field");
                    const auto index=static_cast<unsigned>(i.immediate);
                    const auto& declaration=object.definition->fields[index];
                    bool commit_read=false;
                    if(i.op==Op::gc_read && object.construction) {
                        // A final complete-object commit may copy hidden storage,
                        // but its private read cannot supply any other operation.
                        unsigned uses=0;const auto id=types.size();
                        for(const auto& user:b.instructions) {
                            for(auto argument:user.arguments)if(argument==id){++uses;commit_read|=user.op==Op::gc_construct && user.receiver.has_value();}
                            if((&user-b.instructions.data())<=static_cast<std::ptrdiff_t>(id))continue;
                            // Slot numbers and formal indexes are not SSA uses.
                            switch(user.op) {
                            case Op::load:case Op::argument:case Op::constant:case Op::string_literal:case Op::stage_begin:break;
                            case Op::store:if(user.b==id)++uses;break;
                            default:if(user.a==id)++uses;if(user.b==id)++uses;break;
                            }
                        }
                        if((b.end.kind==Terminator::Kind::return_value || b.end.kind==Terminator::Kind::branch) && b.end.value==id)++uses;
                        commit_read&=uses==1;
                    }
                    if(!commit_read && !field_accessible(m,object,index,f.module_identity,f.lexical_owner))fail("inaccessible stored field '"+declaration.name+"' in "+f.name);
                    auto field=field_type(object,index);
                    if (i.op == Op::gc_write) {
                        bool direct=f.initializer && f.receiver && declaration.owner==Type::key(f.parameters[0]) && b.instructions[i.a].op==Op::load && b.instructions[i.a].a==0;
                        if(declaration.readonly && !direct)fail("write to const stored field outside declaring initializer");
                        if (object.readonly) fail("write through readonly reference"); expect(i.b, field); result = Type::none;
                    } else result=field;
                    break;
                }
                case Op::gc_release:
                    if(!f.receiver || f.method_name!="deinit" || operand(i.a).kind!=Type::managed_ref || operand(i.a).nullable || operand(i.a).construction)fail("invalid deterministic release");result=Type::boolean;break;
                case Op::native_call: {
                    valid_type(i.type);
                    std::vector<Type> parameters;for(auto value:i.arguments)parameters.push_back(operand(value));
                    if(!m.coordinated || m.native_library!=native_provider(i.callee) || i.receiver || !native_signature(i.callee,parameters,i.type))fail("invalid native ABI/call/publication");
                    if(native_binding(i.callee).name=="register" && b.instructions[i.arguments[1]].op!=Op::function_reference)fail("native callback lacks generation-local provenance");
                    result=i.type;break;
                }
                case Op::gc_alive:
                    if (!m.coordinated || (operand(i.a).kind != Type::managed_ref || operand(i.a).construction)) fail("invalid managed inspection");
                    result = Type::boolean; break;
                case Op::gc_collect: case Op::gc_poll:
                    if (!m.coordinated) fail("runtime operation outside coordinated module");
                    result = Type::none; break;
                case Op::function_null:
                    valid_type(i.type);if(!i.type.definition || !i.type.definition->callable || !i.type.nullable || i.receiver || !i.arguments.empty() || !i.callee.empty())fail("invalid null function value");result=i.type;break;
                case Op::function_reference: {
                    valid_type(i.type);auto target=names.find(i.callee);
                    if(!i.type.definition || !i.type.definition->callable || target==names.end() || target->second.receiver || target->second.initializer || target->second.parameters.size()!=i.type.definition->call_parameters.size()+(target->second.closure?1:0) || target->second.result!=Type(*i.type.definition->call_result) || i.receiver || i.arguments.size()!=(target->second.closure?1:0))fail("invalid function reference");
                    if(target->second.closure){expect(i.arguments[0],target->second.parameters[0]);auto environment=Type(i.type.definition->fields[1].type);environment.nullable=false;if(!subtype(operand(i.arguments[0]),environment))fail("function environment does not match callable storage");}
                    for(unsigned n=0;n<i.type.definition->call_parameters.size();++n)if(target->second.parameters[n+(target->second.closure?1:0)]!=Type(i.type.definition->call_parameters[n]) || capability_key(target->second.parameters[n+(target->second.closure?1:0)])!=capability_key(i.type.definition->call_parameters[n]))fail("function reference parameter authority mismatch");
                    if(!capability_assignable(target->second.result,*i.type.definition->call_result))fail("function reference discards result authority");result=i.type;break;
                }
                case Op::cleanup_call:
                    if(!operand(i.a).definition || !operand(i.a).definition->callable || !operand(i.a).definition->call_parameters.empty() || Type(*operand(i.a).definition->call_result)!=Type::none || !f.cleanup_entry || static_cast<unsigned>(&b-f.blocks.data())<*f.cleanup_entry || !i.arguments.empty() || i.receiver || !i.callee.empty())fail("invalid cleanup call");result=Type::none;break;
                case Op::function_call: {
                    auto type=operand(i.a);if(!type.definition || !type.definition->callable || i.arguments.size()!=type.definition->call_parameters.size() || i.receiver || !i.callee.empty())fail("invalid function invocation");for(unsigned n=0;n<i.arguments.size();++n)expect(i.arguments[n],type.definition->call_parameters[n]);result=*type.definition->call_result;break;
                }
                case Op::panic:
                    if(i.callee.empty() || i.a || i.b || i.receiver || !i.arguments.empty())fail("invalid source failure");result=Type::none;break;
                case Op::type_test: case Op::type_cast:
                    valid_type(i.type);
                    if(!reference_value(operand(i.a)) || operand(i.a).construction || !reference_value(i.type) || i.type.nullable || i.receiver || !i.arguments.empty() || !i.callee.empty())fail("invalid dynamic type pattern");
                    result=i.op==Op::type_test?Type(Type::boolean):i.type;break;
                case Op::pattern_equal:
                    if(operand(i.a)!=operand(i.b) || !pattern_comparable(operand(i.a)))fail("invalid pattern comparison");result=Type::boolean;break;
                case Op::sum: {
                    valid_type(i.type);
                    if(i.type.kind!=Type::aggregate || !i.type.definition || i.immediate<0 || static_cast<std::size_t>(i.immediate)>=i.type.definition->cases.size() || i.receiver || i.initialization || !i.callee.empty())fail("invalid sum construction");
                    const auto& variant=i.type.definition->cases[i.immediate];
                    if(i.arguments.size()!=variant.count)fail("invalid sum payload count");
                    for(unsigned n=0;n<i.arguments.size();++n)expect(i.arguments[n],field_type(i.type,variant.begin+n));
                    result=i.type;break;
                }
                case Op::sum_get: {
                    auto type=operand(i.a);
                    if(type.kind!=Type::aggregate || !type.definition || type.definition->cases.empty() || i.immediate<=0 || static_cast<std::size_t>(i.immediate)>=type.definition->fields.size() || i.receiver || !i.arguments.empty() || !i.callee.empty())fail("invalid sum payload access");
                    result=field_type(type,i.immediate);break;
                }
                case Op::aggregate:
                    valid_type(i.type);
                    if (i.type.kind != Type::aggregate || !i.type.definition || (!i.type.definition->cases.empty() || i.type.definition->callable) || i.arguments.size() != i.type.definition->fields.size()) fail("invalid aggregate construction");
                    for (unsigned n = 0; n < i.arguments.size(); ++n) expect(i.arguments[n], i.type.definition->fields[n].type);
                    result = i.type; break;
                case Op::fixed_array:
                    valid_type(i.type);
                    if(!array_type(i.type) || (i.type.kind==Type::fixed_array ? i.arguments.size()!=(i.array_repeat ? (*i.type.definition->extent ? 1u : 0u) : *i.type.definition->extent) : i.array_repeat) || i.receiver || i.initialization || i.immediate || !i.callee.empty())
                        fail("invalid fixed-array construction/template");
                    if(i.array_repeat) for(auto argument:i.arguments)
                        if(argument>=pure_templates.size() || !pure_templates[argument]) fail("effectful repeated array prototype");
                    for(auto argument:i.arguments) expect(argument,array_element(i.type));
                    result=i.type;break;
                case Op::array_allocate:
                    valid_type(i.type);if(i.type.kind!=Type::fixed_array || !i.arguments.empty() || i.receiver || i.immediate || !i.callee.empty())fail("invalid array construction storage");result=i.type;break;
                case Op::array_initialize:
                    if(operand(i.a).kind!=Type::fixed_array || operand(i.b)!=Type::u64 || i.arguments.size()!=1 || i.receiver || i.immediate || !i.callee.empty())fail("invalid array element initialization");expect(i.arguments[0],array_element(operand(i.a)));result=Type::none;break;
                case Op::array_capture:
                    if(i.immediate<0 || static_cast<std::size_t>(i.immediate)>=f.locals.size() || !array_type(operand(i.a)) || operand(i.a)!=f.locals[i.immediate] || i.b || i.receiver || !i.arguments.empty() || !i.callee.empty())fail("invalid array iteration capture");
                    result=Type::none;break;
                case Op::list_check:
                    if(operand(i.a).kind!=Type::growable_array || operand(i.b)!=Type::u64 || i.receiver || !i.arguments.empty() || i.immediate || !i.callee.empty())fail("invalid iterator version check");
                    result=Type::none;break;
                case Op::array_count: case Op::list_version:
                    if(!array_type(operand(i.a)) || (i.op==Op::list_version && operand(i.a).kind!=Type::growable_array) || i.b || i.receiver || !i.arguments.empty() || i.immediate || i.initialization || !i.callee.empty())fail("invalid array property");
                    result=i.op==Op::array_count ? Type::i64 : Type::u64;break;
                case Op::dict_new:
                    valid_type(i.type);if(i.type.kind!=Type::dictionary || i.a || i.b || i.receiver || i.immediate || i.initialization || !i.callee.empty())fail("invalid dictionary construction");for(auto argument:i.arguments)expect(argument,array_element(i.type));result=i.type;break;
                case Op::dict_get: case Op::dict_contains: case Op::dict_remove: case Op::dict_set: case Op::dict_entry: case Op::dict_count: case Op::dict_version: case Op::dict_check: case Op::dict_clear: {
                    auto owner=operand(i.a);if(owner.kind!=Type::dictionary || i.receiver || i.immediate || i.initialization || !i.callee.empty())fail("invalid dictionary operation");
                    if(i.op==Op::dict_set){expect(i.b,array_element(owner));if(i.arguments.size()!=1)fail("invalid dictionary write version");expect(i.arguments[0],Type::u64);result=Type::none;}
                    else {if(!i.arguments.empty())fail("unexpected dictionary arguments");
                        if(i.op==Op::dict_get || i.op==Op::dict_contains || i.op==Op::dict_remove){expect(i.b,dictionary_key(owner));result=i.op==Op::dict_get?dictionary_value(owner):Type(Type::boolean);}
                        else if(i.op==Op::dict_entry){if(!numeric::integer(operand(i.b)))fail("invalid dictionary cursor");result=array_element(owner);}
                        else if(i.op==Op::dict_check){expect(i.b,Type::u64);result=Type::none;}
                        else {if(i.b)fail("unexpected dictionary operand");result=i.op==Op::dict_clear?Type(Type::none):i.op==Op::dict_count?Type(Type::i64):Type(Type::u64);}
                    }break;
                }
                case Op::array_slice:
                    valid_type(i.type);
                    if(!array_type(operand(i.a)) || i.type.kind!=Type::growable_array || array_element(i.type)!=array_element(operand(i.a)) || !numeric::integer(operand(i.b)) || i.arguments.size()!=1 || !numeric::integer(operand(i.arguments[0])) || i.immediate<0 || i.immediate>1 || i.receiver || i.initialization || !i.callee.empty())fail("invalid array slice");
                    result=i.type;break;
                case Op::list_clear:
                    if(operand(i.a).kind!=Type::growable_array || i.b || i.immediate || i.receiver || !i.arguments.empty() || i.initialization || !i.callee.empty())fail("invalid list clear");result=Type::none;break;
                case Op::list_index: case Op::list_push: case Op::list_remove: case Op::list_commit: case Op::list_insert: {
                    auto array=operand(i.a);
                    if(!array_type(array) || array.kind!=Type::growable_array || i.receiver || (i.immediate && !(i.op==Op::list_index && i.immediate==1)) || i.initialization || !i.callee.empty())fail("invalid list operation");
                    if(i.op==Op::list_push) {expect(i.b,array_element(array));if(!i.arguments.empty())fail("unexpected list push operands");result=Type::none;}
                    else {
                        if(!numeric::integer(operand(i.b)))fail("list index requires integer");
                        if(i.op==Op::list_commit || i.op==Op::list_insert) {
                            if(operand(i.b)!=Type::u64 || i.arguments.size()!=2)fail("invalid selected list commit");
                            expect(i.arguments[0],array_element(array));expect(i.arguments[1],Type::u64);result=Type::none;
                        } else {if(!i.arguments.empty())fail("unexpected list index operands");result=i.op==Op::list_index ? Type::u64 : array_element(array);}
                    }
                    break;
                }
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
                    if(!array_type(array) || (i.op==Op::array_set && array.kind!=Type::fixed_array) || !numeric::integer(index) || i.receiver || i.initialization || i.immediate || !i.callee.empty())
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
                    auto owner=operand(i.a);if(owner.definition && ((owner.definition->callable) || (!owner.definition->cases.empty() && (i.op==Op::insert || i.immediate!=0))))fail("sum payload requires checked access");
                    auto t = operand(i.a);
                    if (t.kind != Type::aggregate || i.immediate < 0 || static_cast<std::size_t>(i.immediate) >= t.definition->fields.size()) fail("invalid aggregate field");
                    auto index=static_cast<unsigned>(i.immediate);
                    if(!field_accessible(m,t,index,f.module_identity,f.lexical_owner))fail("inaccessible stored field '"+t.definition->fields[index].name+"' in "+f.name);
                    auto field=field_type(t,index);
                    if (i.op == Op::insert) {
                        bool direct=f.initializer && f.receiver && t.definition->fields[index].owner==Type::key(f.parameters[0]) && b.instructions[i.a].op==Op::load && b.instructions[i.a].a==0;
                        if(t.definition->fields[index].readonly && !direct)fail("write to const stored field outside declaring initializer");
                        expect(i.b, field); result = t;
                    }
                    else result = field;
                    break;
                }
                case Op::async_cancel_children:
                    if(!f.asynchronous || i.a || i.b || i.immediate || i.receiver || !i.arguments.empty() || !i.callee.empty() || (f.cleanup_entry && &b-f.blocks.data()>=*f.cleanup_entry))fail("invalid early async exit");result=Type::none;break;
                case Op::async_cancel: {
                    auto task=operand(i.a);if(!m.coordinated || !task.definition || !task.definition->task_result || task.kind!=Type::managed_ref || task.nullable || task.construction || i.b || i.immediate || i.receiver || !i.arguments.empty() || !i.callee.empty())fail("invalid task cancellation");result=Type::none;break;
                }
                case Op::async_group_enter: case Op::async_group_join: case Op::async_drain: {
                    if(!f.asynchronous || i.b || i.receiver || !i.arguments.empty() || !i.callee.empty())fail("invalid async lifecycle operation");
                    auto cleanup=f.cleanup_entry && &b-f.blocks.data()>=*f.cleanup_entry;
                    if(i.op==Op::async_drain){if(!cleanup || &b-f.blocks.data()!=*f.cleanup_entry || &i!=b.instructions.data() || i.a || i.immediate)fail("illegal async drain transition");}
                    else {if(cleanup)fail("async group operation during cleanup");if(i.op==Op::async_group_join){expect(i.a,Type::u64);if(i.immediate!=0 && i.immediate!=1)fail("invalid group exit mode");}else if(i.a || i.immediate)fail("invalid group entry");}
                    result=i.op==Op::async_group_enter?Type::u64:Type::none;break;
                }
                case Op::await: {
                    auto task=operand(i.a);if(!f.asynchronous || !task.definition || !task.definition->task_result || task.kind!=Type::managed_ref || task.nullable || task.construction)fail("illegal await or task shape");
                    if(f.cleanup_entry && static_cast<std::size_t>(&b-f.blocks.data())>=*f.cleanup_entry)fail("await during deferred cleanup");
                    result=Type(*task.definition->task_result)==Type::none?Type::none:field_type(task,1);break;
                }
                case Op::argument:
                    if (i.a >= f.parameters.size()) fail("invalid argument index");
                    result = f.parameters[i.a]; break;
                case Op::string_join:
                    if(!m.coordinated || i.a || i.b || i.type!=Type::string || i.receiver || !i.callee.empty() || i.initialization || i.array_repeat || i.immediate)fail("invalid interpolation join");
                    for(auto value:i.arguments)if(operand(value)!=Type::string)fail("invalid interpolation text");result=Type::string;break;
                case Op::format_unwrap: {
                    auto type=operand(i.a);
                    if(!m.coordinated || type.kind!=Type::aggregate || !type.definition || !type.definition->result_type || type.definition->fields.size()!=3 || field_type(type,1)!=Type::string || field_type(type,2)!=Type::string || i.b || i.immediate || i.receiver || !i.arguments.empty() || !i.callee.empty() || i.initialization || i.array_repeat || i.type!=Type::string)fail("invalid format unwrap");result=Type::string;break;
                }
                case Op::temporal_native: {
                    if(!m.coordinated || i.a || i.b || i.immediate<0 || static_cast<std::size_t>(i.immediate)>=temporal::operations.size() || i.receiver || !i.callee.empty() || i.initialization || i.array_repeat)fail("invalid temporal operation");
                    const auto& contract=temporal::operations.at(i.immediate);
                    if(i.arguments.size()!=contract.arguments.size())fail("invalid temporal argument count");
                    for(unsigned n=0;n<i.arguments.size();++n)expect(i.arguments[n],contract.arguments[n]=='t'?Type(Type::string):Type(Type::i64));
                    valid_type(i.type);if(i.type.kind!=Type::aggregate || !i.type.definition->result_type || i.type.definition->fields.size()!=3 || field_type(i.type,2)!=Type::string)fail("invalid temporal result");
                    auto value=field_type(i.type,1);
                    if(contract.result_words<=1){if(value!=(contract.result_words?Type(Type::i64):Type(Type::string)))fail("invalid temporal scalar result");}
                    else {if(value.kind!=Type::aggregate || Type::key(value)!="/@stdlib/time.void::_clock_sample" || value.definition->module_identity!="/@stdlib/time.void" || value.definition->fields.size()!=3)fail("invalid temporal clock sample");for(unsigned n=0;n<3;++n)if(field_type(value,n)!=Type::i64 || !value.definition->fields[n].readonly || value.definition->fields[n].visibility!="private")fail("invalid temporal clock sample fields");}
                    result=i.type;break;
                }
                case Op::format_layout:
                    if(!m.coordinated || operand(i.a)!=Type::string || i.b || i.immediate<0 || i.immediate>1 || i.receiver || !i.arguments.empty() || !i.callee.empty() || i.initialization || i.array_repeat || i.type.kind!=Type::aggregate || !i.type.definition || !i.type.definition->result_type || i.type.definition->fields.size()!=3 || field_type(i.type,1)!=Type::string || field_type(i.type,2)!=Type::string)fail("invalid format layout");
                    valid_type(i.type);result=i.type;break;
                case Op::scalar_display: case Op::scalar_format: {
                    auto value=operand(i.a);
                    if(!m.coordinated || (!numeric::number(value) && value!=Type::boolean && value!=Type::string) || i.receiver || !i.arguments.empty() || !i.callee.empty() || i.initialization || i.array_repeat || i.immediate)fail("invalid scalar display");
                    if(i.op==Op::scalar_display){if(i.b || i.type!=Type::string)fail("invalid scalar display result");result=Type::string;}
                    else {valid_type(i.type);if(operand(i.b)!=Type::string || i.type.kind!=Type::aggregate || !i.type.definition->result_type || i.type.definition->fields.size()!=3 || field_type(i.type,1)!=Type::string || field_type(i.type,2)!=Type::string)fail("invalid scalar format result");result=i.type;}
                    break;
                }
                case Op::string_count: case Op::string_index: case Op::string_encode: case Op::string_decode: {
                    if(!m.coordinated || i.receiver || !i.arguments.empty() || !i.callee.empty() || i.initialization || i.array_repeat)fail("invalid text operation flags");
                    if(i.op==Op::string_decode){valid_type(i.type);if(operand(i.a).kind!=Type::growable_array || array_element(operand(i.a))!=Type::u8 || i.b || i.immediate || i.type.kind!=Type::aggregate || !i.type.definition->result_type || i.type.definition->fields.size()!=3 || field_type(i.type,1)!=Type::string || field_type(i.type,2)!=Type::string)fail("invalid UTF-8 decode");result=i.type;}
                    else {if(operand(i.a)!=Type::string)fail("invalid text receiver");
                        if(i.op==Op::string_count){if(i.b || i.immediate<0 || i.immediate>1)fail("invalid text count");result=Type::i64;}
                        else if(i.op==Op::string_index){if(!numeric::integer(operand(i.b)) || i.immediate)fail("invalid text index");result=Type::string;}
                        else {valid_type(i.type);if(i.b || i.immediate || i.type.kind!=Type::growable_array || array_element(i.type)!=Type::u8 || i.type.readonly || !readonly_paths(i.type).empty())fail("invalid UTF-8 encode");result=i.type;}
                    }break;
                }
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
                    if (!((operand(i.a).definition && operand(i.b).definition && operand(i.a).definition->callable && operand(i.b).definition->callable && Type::key(operand(i.a))==Type::key(operand(i.b))) || (reference_value(operand(i.a)) && !operand(i.a).construction && operand(i.b).kind==operand(i.a).kind && !operand(i.b).construction && Type::key(operand(i.a))==Type::key(operand(i.b))) || (numeric::supports(i.op,operand(i.a)) && operand(i.a)==operand(i.b)))) fail("comparison type mismatch");
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
                case Op::bitcast:
                    valid_type(i.type);
                    if(!numeric::number(operand(i.a)) || !numeric::number(i.type) || numeric::width(operand(i.a))!=numeric::width(i.type) || i.b || i.receiver || !i.arguments.empty() || !i.callee.empty())fail("invalid numeric bitcast");
                    result=i.type;break;
                case Op::convert:
                    valid_type(i.type);
                    if (!((operand(i.a).definition && i.type.definition && operand(i.a).definition->callable && i.type.definition->callable && Type::key(operand(i.a))==Type::key(i.type)) || (reference_value(operand(i.a)) && !operand(i.a).construction && i.type.kind==operand(i.a).kind && !i.type.construction && Type::key(operand(i.a))==Type::key(i.type) && !i.type.nullable) || (numeric::number(operand(i.a)) && numeric::number(i.type)))) fail("invalid conversion");
                    result = i.type; break;
                case Op::negate:
                    result = operand(i.a); if (!numeric::number(result)) fail("non-numeric negation"); break;
                case Op::logical_not: expect(i.a, Type::boolean); result = Type::boolean; break;
                case Op::load: result = slot(i.a); break;
                case Op::store:
                    if (f.receiver && f.parameters[0].kind==Type::aggregate && !f.receiver_mutates && i.a == 0) fail("non-mutating method writes receiver");
                    expect(i.b, slot(i.a)); result = Type::none; break;
                case Op::async_call: case Op::call: case Op::virtual_call: {
                    auto declaration=functions.find(i.callee);
                    if(requirements.contains(i.callee)) fail("ordinary call cannot bypass interface receiver preparation");
                    if(i.op==Op::virtual_call && (declaration==functions.end() || declaration->second->virtual_slot.empty() || i.initialization)) fail("invalid virtual call target");
                    auto target = names.find(i.callee);
                    if (target == names.end()) fail("unknown callee " + i.callee);
                    const auto& signature = target->second;
                    if((i.op==Op::async_call)!=(declaration!=functions.end() && declaration->second->asynchronous))fail("async call effect mismatch");
                    if (i.initialization != signature.initializer) fail("initializer invocation kind mismatch");
                    if (i.initialization && (!i.receiver || !i.receiver->fields.empty() || (f.receiver && i.receiver->slot==0))) fail("initializer requires fresh construction storage");
                    if (i.receiver.has_value() != signature.receiver) fail("call receiver mismatch");
                    std::size_t first = 0;
                    if (i.receiver) {
                        auto t = slot(i.receiver->slot);
                        for (auto field : i.receiver->fields) {
                            if ((t.kind != Type::aggregate && t.kind!=Type::managed_ref) || field >= t.definition->fields.size()) fail("invalid receiver field path");
                            if(!field_accessible(m,t,field,f.module_identity,f.lexical_owner))fail("inaccessible receiver field");
                            if(signature.mutates && t.definition->fields[field].readonly)fail("mutating receiver through const field");
                            t = field_type(t,field);
                        }
                        if (t != signature.parameters[0]) fail("receiver type mismatch");
                        first = 1;
                    }
                    auto args = i.arguments;
                    // Original proof instructions use a for their single operand.
                    if (!signature.receiver && args.empty() && signature.parameters.size() == 1) args.push_back(i.a);
                    if (args.size()+first != signature.parameters.size()) fail("call argument count mismatch");
                    for (std::size_t n = 0; n < args.size(); ++n) expect(args[n], signature.parameters[n+first]);
                    result = i.op==Op::async_call ? declaration->second->task : target->second.result; break;
                }
                default: fail("unknown operation");
                }
                if((i.op==Op::gc_construct || i.op==Op::gc_allocate || i.op==Op::stage_begin) && i.type.definition && i.type.definition->native_resource)fail("forged native resource");
                if((i.op==Op::gc_read || i.op==Op::gc_write || i.op==Op::extract || i.op==Op::insert) && operand(i.a).definition && operand(i.a).definition->native_resource)fail("native resource payload access");
                bool pure=i.op==Op::constant || i.op==Op::string_literal || i.op==Op::function_null || (i.op==Op::function_reference && i.arguments.empty());
                if(i.op==Op::sum || i.op==Op::aggregate || i.op==Op::fixed_array || i.op==Op::dict_new) {
                    pure=true;
                    for(auto argument:i.arguments) pure=pure && argument<pure_templates.size() && pure_templates[argument];
                }
                pure_templates.push_back(pure);
                group_values.push_back(i.op==Op::async_group_enter || (i.op==Op::load && group_slots.contains(i.a)));
                types.push_back(result);
            }
            body_types.push_back(types);
            qualified_types.back().push_back(types);
            auto destination = [&](unsigned id) { if (id >= f.blocks.size()) fail("invalid branch destination"); };
            validating=nullptr;
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
        if(f.asynchronous){
#include "async_validation.inc"
        }
        validate_construction(f,m,body_types);
        validate_fixed_ownership(f,m,body_types);
    }
    if(m.async_root){auto entry=functions.find(m.entry_symbol);if(entry!=functions.end() && entry->second->worker)fail("worker cannot be a root adapter");if(entry==functions.end() || !entry->second->asynchronous || entry->second->parameters!=std::vector<Type>{Type::i32} || entry->second->result!=Type::i32)fail("invalid explicit async root adapter signature");}
    if(!m.interface_requirements.empty()) for(const auto& [key,definition]:definitions) if(!catalog_definitions.contains(definition) && !dictionary_entries.contains(definition) && !definition->result_type && !definition->callable) fail("incomplete nominal universe for structural interfaces");
    std::map<std::string,ReferenceEffects> worker_effects;
    auto* inferred=verified_effects?verified_effects:&worker_effects;
    validate_reference_capabilities(m,qualified_types,inferred);
    for(const auto& hook:m.functions)if(hook.receiver && (hook.method_name=="string" || hook.method_name=="format")) {
        bool signature=hook.method_name=="string"?hook.parameters.size()==1 && hook.result==Type::string:hook.parameters.size()==2 && hook.parameters[1]==Type::string && hook.result.definition && hook.result.definition->result_type && field_type(hook.result,1)==Type::string && field_type(hook.result,2)==Type::string;
        if(!m.coordinated || !signature || hook.asynchronous || hook.receiver_mutates || hook.parameters[0].construction)fail("invalid display hook contract");
        const auto& effects=inferred->at(hook.name);
        for(const auto* set:{&effects.mutations,&effects.required_mutable})for(const auto& origin:*set)if(origin.parameter==0)fail("display hook requires mutable receiver authority");
    }
    for(const auto& worker:m.functions)if(worker.worker){
        const auto& effects=inferred->at(worker.name);
        if(!effects.mutations.empty() || !effects.retained.empty() || !effects.required_mutable.empty())fail("worker has shared reference effects");
        if(!worker.asynchronous || worker.receiver || worker.closure || worker.bound_receiver || !worker_transfer_safe(worker.result))fail("invalid worker entry/result");
        std::vector<Type> transfers=worker.parameters;transfers.push_back(worker.result);
        while(!transfers.empty()){auto t=transfers.back();transfers.pop_back();if(!worker_transfer_safe(t))fail("unsafe worker transfer");
            if(t.kind==Type::aggregate){for(const auto& method:m.functions)if(method.receiver && method.method_name=="deinit" && method.parameters[0]==t)fail("worker resource transfer");for(const auto& field:t.definition->fields)transfers.push_back(field.type);}
            else if(t.kind==Type::fixed_array)transfers.push_back(array_element(t));
        }
        std::set<std::string> visited;std::vector<const Function*> graph{&worker};
        while(!graph.empty()){auto* f=graph.back();graph.pop_back();if(!visited.insert(f->name).second)continue;
            if(f!=&worker && f->asynchronous)fail("worker cannot create/await tasks");
            for(const auto& block:f->blocks)for(const auto& i:block.instructions){
                switch(i.op){
                case Op::async_drain:if(f!=&worker)fail("worker task operation");break;
                case Op::temporal_native:if(temporal::operations.at(i.immediate).clock)fail("worker cannot read Runtime clocks");break;
                case Op::native_call:case Op::async_call:case Op::async_cancel:case Op::async_cancel_children:case Op::async_group_enter:case Op::async_group_join:case Op::await:case Op::function_call:case Op::interface_call:case Op::virtual_call:fail("unsupported worker task/indirect effect");break;
                case Op::call:case Op::base_init:case Op::function_reference:{auto target=functions.find(i.callee);if(target==functions.end())fail("worker has unknown native effect");graph.push_back(target->second);break;}
                default:break;
                }
            }
        }
    }
    for(const auto& f:m.functions)for(const auto& block:f.blocks)for(const auto& i:block.instructions)if(i.op==Op::async_call && functions.at(i.callee)->worker && (!f.asynchronous || f.worker))fail("spawn requires an owning async activation");
    if (results) *results=std::move(qualified_types);
}
}
