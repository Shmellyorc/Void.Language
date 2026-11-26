#include "void/ir.hpp"
#include "void/console_ops.hpp"
#include "void/numeric.hpp"
#include <algorithm>
#include <functional>
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
std::optional<Type> dictionary_equality_owner(Type root) {
    std::vector<std::pair<Type,unsigned>> pending{{root,0}};std::set<const ValueType*> seen;std::size_t work=0;
    while(!pending.empty()) {
        auto [type,depth]=pending.back();pending.pop_back();
        if(!type.definition)continue;
        if(type.definition->custom_equality)return type;
        if(depth>=128 || ++work>65536 || !seen.insert(type.definition.get()).second)continue;
        if(type.kind==Type::aggregate)for(const auto& field:type.definition->fields)pending.emplace_back(field.type,depth+1);
        else if(type.kind==Type::fixed_array && type.definition->element)pending.emplace_back(array_element(type),depth+1);
        // Managed identity keys do not hash their payload fields or ancestry.
    }
    return {};
}
bool dictionary_key_supported(Type root) {
    if(dictionary_equality_owner(root))return false;
    std::set<const ValueType*> active;std::map<const ValueType*,bool> memo;std::size_t work=0;
    std::function<bool(Type)> visit=[&](Type t){
        if(numeric::integer(t) || t.kind==Type::numeric_enum || t==Type::boolean || t==Type::string || t.kind==Type::managed_ref)return true;
        if(t.kind!=Type::aggregate || !t.definition || t.definition->callable || t.definition->result_type || t.definition->option_type || active.size()>=128 || ++work>65536)return false;
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
bool nullable_value(Type t) {return t.kind==Type::aggregate && t.definition && t.definition->nullable_value;}
bool nullable_type(Type t) {return t.nullable || nullable_value(t);}
Type nullable_payload(Type t) {if(nullable_value(t))return field_type(t,1);t.nullable=false;return t;}
std::string nullable_identity(Type t) {return "$nullable:"+array_identity(t,{});}
bool multiple_return_type(const Type& type) {
    return type.kind==Type::aggregate && type.definition && type.definition->identity.starts_with("$returns:");
}
std::string multiple_return_identity(const std::vector<ValueField>& components) {
    std::string identity="$returns:";
    for(const auto& component:components)identity+=array_identity(component.type,{});
    return identity;
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
        if(t.kind==Type::numeric_enum) {
            if(!t.definition || !t.definition->enum_backing || !numeric::integer(*t.definition->enum_backing))
                throw std::invalid_argument("Invalid numeric enum array element");
            return {0,numeric::width(*t.definition->enum_backing)/8u,1};
        }
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
        if(!t.definition || t.definition->managed || t.definition->callable || t.definition->interface_type || t.definition->task_result || console::endpoint_type(t))return false;
        if(!seen.insert(t.definition.get()).second)continue;
        if(t.kind==Type::fixed_array)pending.push_back(array_element(t));else for(const auto& field:t.definition->fields)pending.push_back(field.type);
    }return true;
}
bool async_boundary_safe(Type root) {
    std::vector<Type> pending{root};std::set<const ValueType*> seen;
    while(!pending.empty()){auto t=pending.back();pending.pop_back();if(t.construction || t.kind==Type::prepared_receiver)return false;if(!t.definition || !seen.insert(t.definition.get()).second)continue;if(t.definition->callable || t.definition->native_resource)return false;if(t.definition->element)pending.push_back(*t.definition->element);for(const auto& field:t.definition->fields)pending.push_back(field.type);}
    return true;
}
bool requires_initialization(Type t){return !nullable_type(t) && (t.kind==Type::numeric_enum || console::endpoint_type(t) || reference_value(t) || (t.definition && (t.definition->callable || !t.definition->cases.empty())));}
bool fallible_initializer_result(Type t) {
    return t.kind==Type::aggregate && !t.nullable && !t.construction && t.definition &&
        t.definition->result_type && t.definition->fields.size()==3 && field_type(t,1)==Type::boolean;
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
Type Type::nominal(std::shared_ptr<const ValueType> d) {
    const auto kind=d->enum_backing?numeric_enum:d->interface_type?interface_ref:d->managed?managed_ref:aggregate;
    return Type(std::move(d),kind);
}
std::string option_identity(Type payload) {return "$option:"+array_identity(payload,{});}
bool numeric_enum_member(Type t,std::uint64_t bits) {
    if(t.kind==Type::numeric_enum && t.definition && t.definition->enum_flags) {const auto width=numeric::width(*t.definition->enum_backing);return width==64 || !(bits>>width);}
    return t.kind==Type::numeric_enum && t.definition &&
        std::any_of(t.definition->numeric_members.begin(),t.definition->numeric_members.end(),
                    [&](const auto& member){return member.bits==bits;});
}
bool bitwise_type(Type t) {return numeric::integer(t) || (t.kind==Type::numeric_enum && t.definition && t.definition->enum_flags);}
std::string numeric_enum_layout(Type t) {
    if(t.kind!=Type::numeric_enum)return {};
    std::string layout=(t.definition->enum_flags?"flags:":std::string{})+type_name(*t.definition->enum_backing)+":";
    for(const auto& member:t.definition->numeric_members)
        layout+=std::to_string(member.name.size())+":"+member.name+"="+std::to_string(member.bits)+";";
    return layout;
}
FieldType::FieldType(Type t) : kind(t.kind), nullable(t.nullable), readonly(t.readonly), readonly_references(t.readonly_references) {
    if (t.construction) throw std::invalid_argument("construction view cannot be a field type");
    if (t.definition) target = t.definition->self;
}
FieldType::operator Type() const {
    if (kind != Type::aggregate && kind != Type::unresolved && kind != Type::managed_ref && kind != Type::interface_ref && kind!=Type::fixed_array && kind!=Type::growable_array && kind!=Type::dictionary && kind!=Type::numeric_enum) { Type t(kind); t.nullable=nullable; t.readonly=readonly;t.readonly_references=readonly_references; return t; }
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
        // Identical immutable descriptors and authority are reflexively
        // assignable. Avoid re-projecting every descendant of deep containers.
        if(a.kind==e.kind && a.definition==e.definition && a.nullable==e.nullable && a.construction==e.construction && a.readonly==e.readonly && a.readonly_references==e.readonly_references && a.preparation==e.preparation)return true;
        if(a.construction && a.readonly && !e.readonly)return false;
        const auto allowed=readonly_paths(e);
        for(const auto& path:readonly_paths(a))if(!allowed.contains(path)){
            // A stored const field already grants readonly projection in the
            // declared nominal type, even without an explicit outer path fact.
            auto projected=e;
            for(auto field:path){if(!projected.definition || (collection_type(projected)?field!=array_element_path:field>=projected.definition->fields.size()))return false;projected=field_type(projected,field);}
            if(!readonly_paths(projected).contains(ReferencePath{}))return false;
        }
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
    const bool nullable=nullable_type(type);
    auto logical=nullable?nullable_payload(type):type;
    std::string result=collection_type(logical) ? type_name(logical) : logical.definition ? (logical.definition->display_name.empty()?logical.definition->name:logical.definition->display_name) : type_name(logical);
    if(nullable)result+='?';
    for (auto field:path) {
        // The private presence wrapper is not a source field selection.
        if(nullable_value(type) && field==1){type=field_type(type,field);continue;}
        if(collection_type(type) && field==array_element_path)result+="[]";else result+='.'+type.definition->fields.at(field).name;
        type=field_type(type,field);
    }
    return result;
}

std::string type_name(Type t) {
    if (t.construction) { t.construction=false; return "construction view " + type_name(t); }
    if(nullable_value(t))return type_name(nullable_payload(t))+"?";
    if(t.nullable && !reference_value(t) && !(t.definition && t.definition->callable)){t.nullable=false;return type_name(t)+"?";}
    if(t.definition && t.definition->task_result)return std::string(t.readonly?"readonly ":"")+"task<"+
        type_name(Type(*t.definition->task_result)==Type::none?Type(Type::none):field_type(t,1))+">"+(t.nullable?"?":"");
    if(t.definition && !t.definition->display_name.empty())return std::string(t.readonly?"readonly ":"")+t.definition->display_name+(t.nullable?"?":"");
    if(t.kind==Type::interface_ref && t.definition) return std::string(t.readonly ? "readonly " : "")+t.definition->identity+(t.nullable ? "?" : "");
    if(t.kind==Type::prepared_receiver && t.definition) return "prepared receiver "+t.definition->identity;
    if (t.readonly && t.kind!=Type::managed_ref) { t.readonly=false; return "readonly " + type_name(t); }
    if(t.kind==Type::dictionary)return "{"+type_name(dictionary_key(t))+": "+type_name(dictionary_value(t))+"}";
    if(array_type(t)) return "["+type_name(array_element(t))+(t.definition->extent ? ", "+std::to_string(*t.definition->extent) : "")+"]";
    if (t.kind == Type::managed_ref && t.definition) return std::string(t.readonly ? "readonly " : "") + t.definition->identity + (t.nullable ? "?" : "");
    switch (t.kind) { case Type::string: return "string"; case Type::numeric_enum: case Type::aggregate: if (t.definition) return t.definition->identity.c_str(); break; case Type::unresolved: if (t.definition) return t.definition->name.c_str(); break; case Type::i8: return "i8"; case Type::i16: return "i16"; case Type::i64: return "i64"; case Type::u8: return "u8"; case Type::u16: return "u16"; case Type::u32: return "u32"; case Type::u64: return "u64"; case Type::f32: return "f32"; case Type::f64: return "f64"; case Type::i32: return "i32"; case Type::boolean: return "bool"; case Type::none: return "no result"; default: break; }
    throw std::invalid_argument("VOID IR: unknown type");
}

// Aggregate fields and fixed-array elements carry value storage. Managed and
// growable handles have separate initialization/publication contracts.
bool contains_numeric_enum(Type root) {
    if(root.kind!=Type::aggregate && root.kind!=Type::fixed_array)return root.kind==Type::numeric_enum;
    std::vector<Type> pending{root};
    std::set<const ValueType*> seen;
    for(std::size_t n=0;n<pending.size();++n) {
        const auto type=pending[n];
        if(type.kind==Type::numeric_enum)return true;
        if(!type.definition || !seen.insert(type.definition.get()).second)continue;
        if(type.kind==Type::aggregate)
            for(const auto& field:type.definition->fields)pending.emplace_back(field.type);
        else if(type.kind==Type::fixed_array && type.definition->element)
            pending.emplace_back(*type.definition->element);
    }
    return false;
}
}
