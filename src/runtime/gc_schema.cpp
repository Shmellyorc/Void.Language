#include "void/gc_schema.hpp"
#include <algorithm>
#include <bit>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <type_traits>
namespace vd {
namespace {
[[noreturn]] void bad(const char* message) { throw std::invalid_argument(message); }
std::size_t add(std::size_t a,std::size_t b) {
    if(b>std::numeric_limits<std::size_t>::max()-a) throw std::length_error("GC schema addition overflow");
    return a+b;
}
std::size_t multiply(std::size_t a,std::uint64_t b) {
    if(a && b>std::numeric_limits<std::size_t>::max()/a) throw std::length_error("GC schema repetition overflow");
    return a*static_cast<std::size_t>(b);
}
void valid_alignment(std::size_t a) { if(!a || (a&(a-1))) bad("invalid GC schema alignment"); }
std::size_t aligned(std::size_t n,std::size_t a) { valid_alignment(a);return add(n,a-1)&~(a-1); }
void text(const std::string& s) { if(s.empty() || s.find('\0')!=std::string::npos) bad("invalid GC schema identity"); }
template<class T> ScalarStorage storage() { return {sizeof(T),alignof(T)}; }
// Keys contain local canonical child IDs, never recursively expanded child text.
std::string key(const SchemaNode& node) {
    std::string result=std::to_string(node.index())+":";
    auto number=[&](auto n){result+=std::to_string(n)+":";};
    auto word=[&](const std::string& s){number(s.size());result+=s;};
    std::visit([&](const auto& v) {
        using T=std::decay_t<decltype(v)>;
        if constexpr(std::is_same_v<T,SchemaScalar>) number(static_cast<unsigned>(v.encoding));
        else if constexpr(std::is_same_v<T,SchemaBytes>) {number(v.bytes);number(v.alignment);}
        else if constexpr(std::is_same_v<T,SchemaManaged>) {word(v.target);number(v.nullable);}
        else if constexpr(std::is_same_v<T,SchemaInterface>) {word(v.target);number(v.nullable);number(v.object_offset);number(v.witness_offset);number(v.bytes);number(v.alignment);}
        else if constexpr(std::is_same_v<T,SchemaRecord>) {
            number(v.bytes);number(v.alignment);number(v.fields.size());
            for(const auto& f:v.fields) {number(f.id);number(f.offset);number(f.child);}
        } else if constexpr(std::is_same_v<T,SchemaRepeat>) {number(v.base);number(v.stride);number(v.extent);number(v.child);number(v.bytes);number(v.alignment);}
    },node);
    return result;
}
std::optional<ReferenceEvent> event(const SchemaNode& node,std::size_t base) {
    if(auto* v=std::get_if<SchemaManaged>(&node)) return ReferenceEvent{ReferenceEvent::Kind::managed,base,{},v->nullable,v->target};
    if(std::holds_alternative<SchemaString>(node)) return ReferenceEvent{ReferenceEvent::Kind::string,base,{},true,{}};
    if(auto* v=std::get_if<SchemaInterface>(&node)) return ReferenceEvent{ReferenceEvent::Kind::interface,add(base,v->object_offset),add(base,v->witness_offset),v->nullable,v->target};
    return {};
}
}
StorageDomain StorageDomain::native() {
    StorageDomain d;d.pointer_bytes=sizeof(void*);d.pointer_alignment=alignof(void*);
    if constexpr(std::endian::native==std::endian::little) d.byte_order=ByteOrder::little;
    else if constexpr(std::endian::native==std::endian::big) d.byte_order=ByteOrder::big;
    else throw std::logic_error("unsupported native GC byte order");
    d.scalars={storage<std::int8_t>(),storage<std::int16_t>(),storage<std::int32_t>(),storage<std::int64_t>(),
               storage<std::uint8_t>(),storage<std::uint16_t>(),storage<std::uint32_t>(),storage<std::uint64_t>(),
               storage<float>(),storage<double>(),storage<bool>()};
    return d;
}
ElementSchema::ElementSchema(ElementSchemaSpec spec) : spec_(std::move(spec)) {
    text(spec_.identity);text(spec_.capability_profile);text(spec_.lowering_layout_key);
    if(spec_.storage_domain!=StorageDomain::native()) bad("foreign GC storage domain");
    if(spec_.nodes.empty() || spec_.root>=spec_.nodes.size()) bad("missing GC schema root");
    summaries_.reserve(spec_.nodes.size());
    for(std::size_t index=0;index<spec_.nodes.size();++index) {
        const auto& node=spec_.nodes[index];if(node.valueless_by_exception()) bad("missing GC node variant");
        Summary s;
        auto child=[&](std::size_t id)->const Summary& {if(id>=index) bad("forward or recursive GC shape edge");return summaries_[id];};
        std::visit([&](const auto& v) {
            using T=std::decay_t<decltype(v)>;
            if constexpr(std::is_same_v<T,SchemaScalar>) {
                auto n=static_cast<std::size_t>(v.encoding);if(n>=spec_.storage_domain.scalars.size()) bad("unknown GC scalar encoding");
                auto value=spec_.storage_domain.scalars[n];s.bytes=value.bytes;s.alignment=value.alignment;s.data=true;
            } else if constexpr(std::is_same_v<T,SchemaBytes>) {s.bytes=v.bytes;s.alignment=v.alignment;s.data=v.bytes!=0;valid_alignment(s.alignment);}
            else if constexpr(std::is_same_v<T,SchemaManaged> || std::is_same_v<T,SchemaString>) {
                if constexpr(std::is_same_v<T,SchemaManaged>) text(v.target);
                s.bytes=sizeof(void*);s.alignment=alignof(void*);s.references=true;
                if constexpr(std::is_same_v<T,SchemaManaged>) s.zero_complete=v.nullable;
            } else if constexpr(std::is_same_v<T,SchemaInterface>) {
                text(v.target);valid_alignment(v.alignment);
                if(v.alignment<alignof(void*) || v.object_offset%alignof(void*) || v.witness_offset%alignof(void*) ||
                   (v.object_offset<v.witness_offset ? v.witness_offset-v.object_offset : v.object_offset-v.witness_offset)<sizeof(void*) ||
                   aligned(add(std::max(v.object_offset,v.witness_offset),sizeof(void*)),v.alignment)!=v.bytes)
                    bad("invalid interface GC geometry");
                s.bytes=v.bytes;s.alignment=v.alignment;s.references=true;s.zero_complete=v.nullable;
            } else if constexpr(std::is_same_v<T,SchemaRecord>) {
                valid_alignment(v.alignment);if(v.bytes%v.alignment) bad("unaligned GC record extent");
                s.bytes=v.bytes;s.alignment=v.alignment;
                s.physical.resize(v.fields.size());s.logical.resize(v.fields.size());
                for(std::size_t n=0;n<v.fields.size();++n) {
                    const auto& f=v.fields[n];const auto& c=child(f.child);
                    if(f.offset%c.alignment || v.alignment<c.alignment || f.offset>v.bytes || c.bytes>v.bytes-f.offset) bad("GC field outside aligned record");
                    s.depth=std::max(s.depth,add(c.depth,1));s.references|=c.references;s.data|=c.data;s.zero_complete&=c.zero_complete;s.physical[n]=s.logical[n]=n;
                }
                std::sort(s.logical.begin(),s.logical.end(),[&](auto a,auto b){return v.fields[a].id<v.fields[b].id;});
                for(std::size_t n=1;n<s.logical.size();++n) if(v.fields[s.logical[n-1]].id==v.fields[s.logical[n]].id) bad("duplicate GC logical field");
                std::sort(s.physical.begin(),s.physical.end(),[&](auto a,auto b) {
                    const auto& x=v.fields[a];const auto& y=v.fields[b];
                    if(x.offset!=y.offset) return x.offset<y.offset;
                    return child(x.child).bytes<child(y.child).bytes;
                });
                s.physical.erase(std::remove_if(s.physical.begin(),s.physical.end(),[&](auto n){return child(v.fields[n].child).bytes==0;}),s.physical.end());
                std::size_t end=0;
                for(auto n:s.physical) {const auto& f=v.fields[n];auto bytes=child(f.child).bytes;if(bytes && f.offset<end) bad("overlapping GC fields");if(bytes) end=add(f.offset,bytes);}
            } else if constexpr(std::is_same_v<T,SchemaRepeat>) {
                const auto& c=child(v.child);valid_alignment(v.alignment);
                if(v.extent>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) throw std::length_error("GC extent exceeds i64 count");
                if(v.alignment<c.alignment || v.base%c.alignment || !v.stride || v.stride<c.bytes || v.stride%c.alignment) bad("invalid GC repeat stride/alignment");
                if(!v.extent && (v.base || v.bytes)) bad("noncanonical empty GC repeat");
                if(aligned(add(v.base,multiply(v.stride,v.extent)),v.alignment)!=v.bytes) bad("incorrect GC repeat extent");
                s.bytes=v.bytes;s.alignment=v.alignment;s.depth=add(c.depth,1);s.references=v.extent && c.references;s.data=v.extent && c.data;s.zero_complete=!v.extent || c.zero_complete;
            } else bad("owned/growable GC value role awaits later stage");
        },node);
        summaries_.push_back(std::move(s));
    }
    std::vector<bool> reachable(spec_.nodes.size());reachable[spec_.root]=true;
    for(std::size_t i=spec_.nodes.size();i-->0;) if(reachable[i]) {
        if(auto* r=std::get_if<SchemaRecord>(&spec_.nodes[i])) for(const auto& f:r->fields) reachable[f.child]=true;
        if(auto* r=std::get_if<SchemaRepeat>(&spec_.nodes[i])) reachable[r->child]=true;
    }
    if(std::find(reachable.begin(),reachable.end(),false)!=reachable.end()) bad("unused GC schema node");
    std::map<std::string,std::size_t> canonical;
    std::vector<SchemaNode> nodes;std::vector<Summary> summaries;std::vector<std::size_t> mapping;
    nodes.reserve(spec_.nodes.size());summaries.reserve(spec_.nodes.size());mapping.reserve(spec_.nodes.size());
    for(std::size_t i=0;i<spec_.nodes.size();++i) {
        auto node=std::move(spec_.nodes[i]);
        if(auto* r=std::get_if<SchemaRecord>(&node)) for(auto& f:r->fields) f.child=mapping[f.child];
        if(auto* r=std::get_if<SchemaRepeat>(&node)) r->child=mapping[r->child];
        auto [found,added]=canonical.emplace(key(node),nodes.size());mapping.push_back(found->second);
        if(added) {nodes.push_back(std::move(node));summaries.push_back(std::move(summaries_[i]));}
    }
    spec_.root=mapping[spec_.root];spec_.nodes=std::move(nodes);summaries_=std::move(summaries);
}
std::shared_ptr<ElementSchema> ElementSchema::prepare(const ElementSchemaSpec& s) {return std::shared_ptr<ElementSchema>(new ElementSchema(s));}
std::size_t ElementSchema::bytes() const {return summaries_[spec_.root].bytes;}
std::size_t ElementSchema::alignment() const {return summaries_[spec_.root].alignment;}
std::size_t ElementSchema::depth() const {return summaries_[spec_.root].depth;}
bool ElementSchema::has_references() const {return summaries_[spec_.root].references;}
bool ElementSchema::zero_data_complete() const {return summaries_[spec_.root].zero_complete;}
std::optional<ReferenceEvent> ElementSchema::reference_event(std::size_t node,std::size_t base) const {return event(spec_.nodes[node],base);}
std::size_t ElementSchema::edge_count() const {
    std::size_t count=0;for(const auto& n:spec_.nodes) {if(auto* r=std::get_if<SchemaRecord>(&n)) count=add(count,r->fields.size());if(std::holds_alternative<SchemaRepeat>(n)) count=add(count,1);}return count;
}
bool ElementSchema::equivalent(const ElementSchema& other) const {
    if(spec_.identity!=other.spec_.identity || spec_.capability_profile!=other.spec_.capability_profile ||
       spec_.lowering_layout_key!=other.spec_.lowering_layout_key || spec_.storage_domain!=other.spec_.storage_domain) return false;
    std::vector<std::pair<std::size_t,std::size_t>> work{{spec_.root,other.spec_.root}};std::set<std::pair<std::size_t,std::size_t>> seen;
    while(!work.empty()) {
        auto pair=work.back();work.pop_back();if(!seen.insert(pair).second) continue;
        auto a=spec_.nodes[pair.first], b=other.spec_.nodes[pair.second];
        if(a.index()!=b.index()) return false;
        if(auto* r=std::get_if<SchemaRecord>(&a)) {
            auto& s=std::get<SchemaRecord>(b);if(r->fields.size()!=s.fields.size()) return false;
            for(std::size_t i=0;i<r->fields.size();++i) {work.emplace_back(r->fields[i].child,s.fields[i].child);r->fields[i].child=s.fields[i].child=0;}
        }
        if(auto* r=std::get_if<SchemaRepeat>(&a)) {auto& s=std::get<SchemaRepeat>(b);work.emplace_back(r->child,s.child);r->child=s.child=0;}
        if(a!=b) return false;
    }
    return true;
}
ReferenceShape::ReferenceShape(ElementSchemaRef schema) :schema_(std::move(schema)) {if(!schema_) bad("missing reference schema");}
std::optional<ReferenceEvent> LegacyReferenceMap::at(std::size_t offset) const {
    if(!std::binary_search(offsets_.begin(),offsets_.end(),offset)) return {};
    return ReferenceEvent{ReferenceEvent::Kind::legacy,offset,{},true,{}};
}
ReferenceShape::ReferenceShape(std::shared_ptr<const LegacyReferenceMap> map) :legacy_(std::move(map)) {}
ReferenceShape ReferenceShape::legacy(std::size_t bytes,std::span<const std::size_t> offsets) {
    return ReferenceShape(std::make_shared<const LegacyReferenceMap>(LegacyReferenceMap::validate(bytes,offsets)));
}
bool ReferenceShape::has_references() const {return legacy_ ? !legacy_->offsets().empty() : schema_->has_references();}
std::size_t ReferenceShape::bytes() const {return legacy_ ? legacy_->bytes() : schema_->bytes();}
std::vector<std::size_t> ReferenceShape::legacy_offsets() const {
    if(!legacy_) bad("not a legacy reference layout");
    auto offsets=legacy_->offsets();return {offsets.begin(),offsets.end()};
}
std::optional<ReferenceEvent> ReferenceShape::at(std::size_t offset) const {
    if(legacy_) return legacy_->at(offset);
    if(offset>=schema_->bytes()) return {};
    std::size_t id=schema_->spec_.root,base=0;
    for(;;) {
        const auto& node=schema_->spec_.nodes[id];const auto& summary=schema_->summaries_[id];
        if(auto e=schema_->reference_event(id,base)) return e->offset==offset ? e : std::nullopt;
        if(auto* r=std::get_if<SchemaRecord>(&node)) {
            auto it=std::upper_bound(summary.physical.begin(),summary.physical.end(),offset-base,[&](auto x,auto f){return x<r->fields[f].offset;});
            if(it==summary.physical.begin()) return {};
            const auto& f=r->fields[*--it];if(offset-base-f.offset>=schema_->summaries_[f.child].bytes) return {};
            base=add(base,f.offset);id=f.child;
        } else if(auto* r=std::get_if<SchemaRepeat>(&node)) {
            if(offset-base<r->base) return {};
            auto relative=offset-base-r->base;auto index=relative/r->stride;
            if(index>=r->extent || relative%r->stride>=schema_->summaries_[r->child].bytes) return {};
            base=add(base,add(r->base,multiply(r->stride,index)));id=r->child;
        } else return {};
    }
}
std::optional<ReferenceEvent> ReferenceShape::path(std::span<const SchemaStep> steps) const {
    if(legacy_) {
        if(steps.size()!=1 || steps[0].kind!=SchemaStep::Kind::field || steps[0].value>=legacy_->offsets().size()) return {};
        return legacy_->at(legacy_->offsets()[static_cast<std::size_t>(steps[0].value)]);
    }
    std::size_t id=schema_->spec_.root,base=0;
    for(const auto& step:steps) {
        const auto& node=schema_->spec_.nodes[id];
        if(auto* r=std::get_if<SchemaRecord>(&node)) {
            if(step.kind!=SchemaStep::Kind::field) return {};
            const auto& order=schema_->summaries_[id].logical;
            auto it=std::lower_bound(order.begin(),order.end(),step.value,[&](auto f,auto value){return r->fields[f].id<value;});
            if(it==order.end() || r->fields[*it].id!=step.value) return {};
            const auto& f=r->fields[*it];base=add(base,f.offset);id=f.child;
        } else if(auto* r=std::get_if<SchemaRepeat>(&node)) {
            if(step.kind!=SchemaStep::Kind::element || step.value>=r->extent) return {};
            base=add(base,add(r->base,multiply(r->stride,step.value)));id=r->child;
        } else return {};
    }
    return schema_->reference_event(id,base);
}
ReferenceCursor::ReferenceCursor(ReferenceShape shape) {reset(shape);}
void ReferenceCursor::reset(const ReferenceShape& shape) {
    // Reserve before replacing state; next() never allocates.
    if(shape.schema_ && shape.has_references()) stack_.reserve(shape.schema_->depth());
    stack_.clear();schema_=shape.schema_;legacy_owner_=shape.legacy_;
    borrowed_=nullptr;legacy_index_=0;
    if(schema_ && schema_->has_references()) stack_.push_back({schema_->spec_.root,0,0});
}
std::optional<ReferenceEvent> ReferenceCursor::next() {
    if(legacy_owner_ || borrowed_) {
        if(auto offset=next_offset()) return ReferenceEvent{ReferenceEvent::Kind::legacy,*offset,{},true,{}};
        return {};
    }
    return next_schema();
}
std::optional<ReferenceEvent> ReferenceCursor::next_schema() {
    while(!stack_.empty()) {
        auto& f=stack_.back();const auto& node=schema_->spec_.nodes[f.node];
        if(auto result=schema_->reference_event(f.node,f.base)) {stack_.pop_back();return result;}
        if(auto* r=std::get_if<SchemaRecord>(&node)) {
            const auto& order=schema_->summaries_[f.node].physical;
            if(f.next==order.size()) {stack_.pop_back();continue;}
            const auto& field=r->fields[order[static_cast<std::size_t>(f.next++)]];
            auto base=add(f.base,field.offset);if(schema_->summaries_[field.child].references) stack_.push_back({field.child,base,0});
        } else if(auto* r=std::get_if<SchemaRepeat>(&node)) {
            if(f.next==r->extent) {stack_.pop_back();continue;}
            auto base=add(f.base,add(r->base,multiply(r->stride,f.next++)));stack_.push_back({r->child,base,0});
        } else stack_.pop_back();
    }
    return {};
}
}
