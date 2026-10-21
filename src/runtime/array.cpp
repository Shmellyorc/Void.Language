#include "void/runtime.hpp"
#include "void/utf8.hpp"
#include <cstring>
#include <limits>
namespace vd {
namespace {
constexpr BufferWorkQuota quota{16384,256,4096};
constexpr auto maximum=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
void structural_room(std::uint64_t version) {if(version==std::numeric_limits<std::uint64_t>::max())throw std::length_error("array structural version exhausted");}
}
std::string Runtime::list_identity(const ElementSchemaRef& schema) {
    constexpr std::string_view scalar="void.fixed.scalar.v1/";
    return schema->identity().starts_with(scalar) ? "void.list.v1/"+schema->identity().substr(scalar.size()) : "void.list.v2/"+schema->identity();
}
Runtime::ListHeader& Runtime::list_header(RegisteredRootWord source,const ElementSchemaRef& schema) const {
    auto* object=reference_source(source);const auto key=list_identity(schema);
    if(!types_)throw std::invalid_argument("missing list catalog");
    auto admitted=types_->schemas.find(schema->identity());if(admitted==types_->schemas.end() || admitted->second!=schema)throw std::invalid_argument("foreign list schema");
    auto type=types_->types.find(key);
    if(type==types_->types.end() || type->second->layout.size!=sizeof(ListHeader) || (type->second->layout.references.size()!=1 || type->second->layout.references[0]!=offsetof(ListHeader,backing)) || heap_.nominal_identity(object)!=key)throw std::invalid_argument("invalid list header");
    auto& h=*static_cast<ListHeader*>(object);
    if(h.count>h.capacity || h.capacity>maximum)throw std::invalid_argument("invalid list extent");
    if(!h.capacity) {if(h.count || h.backing)throw std::invalid_argument("invalid empty list");}
    else {
        const auto& state=heap_.buffer_state(h.backing);BufferLayout expected(schema,h.capacity,0,0);
        if(!state.complete || state.valid_prefix!=h.count || !state.layout->equivalent(expected))throw std::invalid_argument("invalid list backing");
    }
    return h;
}
BufferLayoutRef Runtime::native_array_layout(const NativeArrayElement& element,std::uint64_t capacity) {
    require_managed();if(!element.key || (element.count && !element.fields))throw std::invalid_argument("missing native element metadata");
    if(element.count==1 && element.fields[0].offset==0 && element.fields[0].kind<=10 && std::string_view(element.key)=="void.fixed.scalar.v1/"+std::to_string(element.fields[0].kind)) {
        auto result=fixed_scalar_layout(static_cast<ScalarEncoding>(element.fields[0].kind),capacity);if(result->schema()->bytes()!=element.bytes || result->alignment()!=element.alignment)throw std::invalid_argument("invalid native scalar geometry");return result;
    }
    ElementSchemaRef schema;
    {std::lock_guard lock(mutex_);if(types_) {auto found=types_->schemas.find(element.key);if(found!=types_->schemas.end())schema=found->second;}}
    if(schema) {
        if(!schema->mutable_data() || schema->spec_.lowering_layout_key!=element.key)throw std::invalid_argument("incompatible native array element profile");
        const auto* record=std::get_if<SchemaRecord>(&schema->spec_.nodes[schema->spec_.root]);
        if(!record || record->bytes!=element.bytes || record->alignment!=element.alignment || record->fields.size()!=element.count)throw std::invalid_argument("incompatible native array element metadata");
        for(std::size_t n=0;n<element.count;++n) {
            const auto& input=element.fields[n];const auto& field=record->fields[n];NativeArrayField expected{"",field.offset,0,~0u,0};
            const auto& node=schema->spec_.nodes[field.child];
            std::visit([&](const auto& value) {
                using T=std::decay_t<decltype(value)>;
                if constexpr(std::is_same_v<T,SchemaScalar>)expected.kind=static_cast<unsigned>(value.encoding);
                else if constexpr(std::is_same_v<T,SchemaString>)expected.kind=12;
                else if constexpr(std::is_same_v<T,SchemaManaged>){expected.kind=std::string_view(value.target).starts_with("void.dict.v1/")?16:11;expected.target=value.target.c_str();expected.nullable=value.nullable;}
                else if constexpr(std::is_same_v<T,SchemaInterface>){expected.kind=13;expected.target=value.target.c_str();expected.nullable=value.nullable;}
                else if constexpr(std::is_same_v<T,SchemaOwnedFixed>){expected.kind=14;expected.target=value.element.c_str();expected.extent=value.extent;expected.nullable=value.nullable;}
                else if constexpr(std::is_same_v<T,SchemaGrowable>){expected.kind=15;expected.target=value.header.c_str();expected.nullable=value.nullable;}
            },node);
            if(expected.kind==~0u || input.kind!=expected.kind || input.offset!=expected.offset || input.extent!=expected.extent || input.nullable!=expected.nullable || !input.target || std::string_view(input.target)!=expected.target)throw std::invalid_argument("incompatible native array field metadata");
        }
    } else {
        Native native(*this);ElementSchemaSpec spec{element.key,"mutable",element.key,StorageDomain::native(),0,{}};SchemaRecord record{element.bytes,element.alignment,{}};
        std::vector<NominalDescriptor> headers;
        for(std::size_t n=0;n<element.count;++n) {
            const auto& f=element.fields[n];SchemaNode node;
            if(f.nullable>1 || (f.kind!=14 && f.extent) || ((f.kind<=10 || f.kind==12) && f.nullable))throw std::invalid_argument("invalid native element field flags");
            if(f.kind<=10)node=SchemaScalar{static_cast<ScalarEncoding>(f.kind)};
            else if(f.kind==11) {if(!f.target)throw std::invalid_argument("missing managed element type");node=SchemaManaged{f.target,f.nullable!=0};}
            else if(f.kind==12)node=SchemaString{};
            else if(f.kind==13) {if(!f.target)throw std::invalid_argument("missing interface element type");node=SchemaInterface{f.target,f.nullable!=0,0,sizeof(void*),2*sizeof(void*),alignof(void*)};}
            else if(f.kind==14) {if(!f.target)throw std::invalid_argument("missing nested fixed type");node=SchemaOwnedFixed{f.target,f.extent,f.nullable!=0};}
            else if(f.kind==15) {if(!f.target)throw std::invalid_argument("missing nested list type");node=SchemaGrowable{f.target,f.nullable!=0};headers.push_back({f.target,"","array-list-v2",{sizeof(ListHeader),{offsetof(ListHeader,backing)}}});}
            else if(f.kind==16) {if(!f.target)throw std::invalid_argument("missing dictionary type");node=SchemaManaged{f.target,f.nullable!=0};headers.push_back({f.target,"","dictionary-v1",{sizeof(DictionaryHeader),{offsetof(DictionaryHeader,entries),offsetof(DictionaryHeader,buckets)}}});}
            else throw std::invalid_argument("unknown native element field");
            record.fields.push_back({n,f.offset,spec.nodes.size()});spec.nodes.push_back(std::move(node));
        }
        std::sort(headers.begin(),headers.end(),[](const auto& a,const auto& b){return a.identity<b.identity;});headers.erase(std::unique(headers.begin(),headers.end(),[](const auto& a,const auto& b){return a.identity==b.identity;}),headers.end());
        spec.root=spec.nodes.size();spec.nodes.push_back(std::move(record));admit_metadata(headers,{},std::span(&spec,1));schema=buffer_schema(element.key);
    }
    if(schema->bytes()!=element.bytes || schema->alignment()!=element.alignment)throw std::invalid_argument("native element layout mismatch");
    return buffer_layout(schema,capacity);
}
void Runtime::array_new(void** out,const BufferLayoutRef& layout,bool list,bool zero_live) {
    require_managed();require_buffer_output(out);{std::lock_guard lock(mutex_);validate_buffer_layout(layout);heap_.validate_output_address(out);}
    std::shared_ptr<const NominalDescriptor> type;
    if(list) {
        const auto key=list_identity(layout->schema());
        {std::lock_guard lock(mutex_);if(types_) {auto found=types_->types.find(key);if(found!=types_->types.end())type=found->second;}}
        if(!type) {Native native(*this);NominalDescriptor spec{key,"","array-list-v2",{sizeof(ListHeader),{offsetof(ListHeader,backing)}}};admit_types({&spec,1});type=descriptor(key);}
        if(type->layout.size!=sizeof(ListHeader) || type->layout.references!=std::vector<std::size_t>{offsetof(ListHeader,backing)})throw std::invalid_argument("invalid list descriptor");
    }
    void* backing=nullptr;constexpr std::size_t zero=0;Slot slot{&backing,sizeof(backing),&zero,1};Frame roots(*this,&slot,1);
    auto live=zero_live ? layout->capacity() : 0;
    if(!list || layout->capacity()) {BufferBuilder builder(*this,layout,live,quota);if(live)builder.zero_elements(live);builder.finish_into(&backing);}
    if(!list) {*out=backing;return;}
    ListHeader payload{backing,live,layout->capacity(),0};auto header_layout=type->layout;header_layout.descriptor=type;construct_into(out,header_layout,&payload);
}
std::uint64_t Runtime::array_count(RegisteredRootWord source,const BufferLayoutRef& layout,bool list) const {
    require_managed();std::lock_guard lock(mutex_);validate_buffer_layout(layout);
    if(list)return list_header(source,layout->schema()).count;
    const auto& state=heap_.buffer_state(reference_source(source));if(!state.complete || !state.layout->equivalent(*layout) || state.valid_prefix!=layout->capacity())throw std::invalid_argument("incomplete fixed array");return state.valid_prefix;
}
std::uint64_t Runtime::array_version(RegisteredRootWord source,const BufferLayoutRef& layout) const {require_managed();std::lock_guard lock(mutex_);validate_buffer_layout(layout);return list_header(source,layout->schema()).version;}
void Runtime::array_read(void* out,RegisteredRootWord source,const BufferLayoutRef& layout,bool list,std::uint64_t index) const {
    require_managed();if(!out)throw std::invalid_argument("missing array result");std::lock_guard lock(mutex_);validate_buffer_layout(layout);heap_.validate_output_address(reinterpret_cast<void**>(out));
    auto* object=reference_source(source);auto count=layout->capacity();
    if(list) {const auto& h=list_header(source,layout->schema());object=h.backing;count=h.count;}
    if(index>=count)throw std::out_of_range("array index outside extent");
    const auto& state=heap_.buffer_state(object);
    if(!state.complete || state.layout->schema()!=layout->schema() || (!list && !state.layout->equivalent(*layout)) || state.valid_prefix!=count)throw std::invalid_argument("invalid array read");
    std::memcpy(out,static_cast<char*>(object)+state.layout->element_offset(index),layout->schema()->bytes());
}
void Runtime::validate_native_element(const void* value,DataCursor& cursor,const ElementSchemaRef& schema) const {
    if(auto* scalar=std::get_if<SchemaScalar>(&schema->spec_.nodes[schema->spec_.root])) {
        if(!value || (scalar->encoding==ScalarEncoding::boolean && *static_cast<const unsigned char*>(value)>1))throw std::invalid_argument("invalid scalar array input");return;
    }
    if(!value)throw std::invalid_argument("missing native array value");cursor.reset();std::size_t work=std::numeric_limits<std::size_t>::max();
    while(auto event=cursor.next(work)) {
        auto* bytes=static_cast<const char*>(value)+event->offset;
        if(event->reference) {
            void* object;std::memcpy(&object,bytes,sizeof(object));const void* witness=nullptr;
            if(event->reference->witness_offset)std::memcpy(&witness,static_cast<const char*>(value)+*event->reference->witness_offset,sizeof(witness));
            heap_.validate_reference(*event->reference,object,witness,*types_,false);
        } else if(event->scalar && event->encoding==ScalarEncoding::boolean && static_cast<unsigned char>(*bytes)>1)throw std::invalid_argument("invalid bool element");
    }
}
void Runtime::append_native(BufferBuilder& builder,const BufferLayoutRef& layout,const void* value) {
    DataCursor cursor;cursor.prepare(layout->schema());if(!cursor.has_data()){builder.zero_elements(1);return;}std::size_t work=std::numeric_limits<std::size_t>::max();
    while(auto event=cursor.next(work)) {
        auto* bytes=static_cast<const char*>(value)+event->offset;
        if(event->reference) {
            auto source=root_word(reinterpret_cast<void**>(const_cast<char*>(bytes)));
            if(event->reference->witness_offset) {
                const InterfaceWitness* witness;std::memcpy(&witness,static_cast<const char*>(value)+*event->reference->witness_offset,sizeof(witness));builder.write_interface({source,witness});
            } else builder.write_reference(source);
        } else if(event->scalar) {
            std::uint64_t bits=0;switch(event->bytes) {case 1:{std::uint8_t v;std::memcpy(&v,bytes,1);bits=v;break;}case 2:{std::uint16_t v;std::memcpy(&v,bytes,2);bits=v;break;}case 4:{std::uint32_t v;std::memcpy(&v,bytes,4);bits=v;break;}case 8:std::memcpy(&bits,bytes,8);break;default:throw std::invalid_argument("invalid scalar width");}builder.write_scalar(bits);
        } else builder.write_bytes({reinterpret_cast<const std::byte*>(bytes),event->bytes});
    }
}
void Runtime::array_write(RegisteredRootWord source,const BufferLayoutRef& layout,bool list,std::uint64_t index,const void* value,bool initialize,std::optional<std::uint64_t> version) {if(!layout)throw std::invalid_argument("missing array layout");array_write_value(source,layout->schema(),list?nullptr:layout.get(),index,value,initialize,version);}
void Runtime::array_write_value(RegisteredRootWord source,const ElementSchemaRef& schema,const BufferLayout* fixed,std::uint64_t index,const void* value,bool initialize,std::optional<std::uint64_t> version) {
    require_managed();if(!schema || !schema->mutable_data())throw std::invalid_argument("array profile does not permit mutation");DataCursor cursor;if(!std::holds_alternative<SchemaScalar>(schema->spec_.nodes[schema->spec_.root]))cursor.prepare(schema);poll();std::lock_guard lock(mutex_);validate_native_element(value,cursor,schema);
    auto* object=reference_source(source);ListHeader* h=!fixed ? &list_header(source,schema) : nullptr;
    if(h) {if(version && *version!=h->version)throw ListModified("List structurally modified during selected write");object=h->backing;}
    const auto& state=heap_.buffer_state(object);const auto count=h ? h->count : state.valid_prefix;
    if(!state.complete || state.layout->schema()!=schema || (fixed && !state.layout->equivalent(*fixed)) || state.readers)throw BufferBusy("invalid or leased array write");
    if(initialize ? (index!=count || index>=state.layout->capacity()) : index>=count)throw std::out_of_range("array index outside extent");
    std::memcpy(static_cast<char*>(object)+state.layout->element_offset(index),value,schema->bytes());state.catalog=types_;
    if(initialize) {++state.valid_prefix;if(h)++h->count;}
}
void Runtime::array_push(RegisteredRootWord source,const ElementSchemaRef& schema,const void* value) {array_insert(source,schema,{},value);}
void Runtime::array_insert(RegisteredRootWord source,const ElementSchemaRef& schema,std::optional<std::uint64_t> position,const void* value,std::optional<std::uint64_t> expected) {
    require_managed();if(!schema || !schema->mutable_data())throw std::invalid_argument("array profile does not permit mutation");DataCursor cursor;if(!std::holds_alternative<SchemaScalar>(schema->spec_.nodes[schema->spec_.root]))cursor.prepare(schema);poll();
    void* old=nullptr;void* next=nullptr;constexpr std::size_t zero=0;Slot slots[]={{&old,sizeof(old),&zero,1},{&next,sizeof(next),&zero,1}};Frame roots(*this,slots,2);
    std::uint64_t count,capacity,version;BufferLayoutRef old_layout;
    {std::lock_guard lock(mutex_);validate_native_element(value,cursor,schema);auto& h=list_header(source,schema);if(expected && *expected!=h.version)throw ListModified("List structurally modified during selected insertion");
        if(!position)position=h.count;if(*position>h.count)throw std::out_of_range("list insertion index outside extent");structural_room(h.version);if(h.count==maximum)throw std::length_error("list count overflow");
        if(h.count<h.capacity) {const auto& state=heap_.buffer_state(h.backing);if(state.readers)throw BufferBusy("leased list backing");
            auto* to=static_cast<char*>(h.backing)+state.layout->element_offset(*position);
            std::memmove(to+state.layout->stride(),to,static_cast<std::size_t>(h.count-*position)*state.layout->stride());
            std::memcpy(to,value,schema->bytes());state.catalog=types_;state.valid_prefix=++h.count;++h.version;return;}
        count=h.count;capacity=h.capacity;version=h.version;old=h.backing;if(old)old_layout=heap_.buffer_state(old).layout;
    }
    const auto grown=capacity ? (capacity>maximum/2 ? maximum : capacity*2) : 4;auto next_layout=buffer_layout(schema,grown);
    std::optional<BufferReadLease> lease;if(old)lease.emplace(*this,root_word(&old),old_layout);
    BufferBuilder builder(*this,next_layout,count+1,quota);if(lease)builder.copy_elements(*lease,0,*position);append_native(builder,next_layout,value);if(lease)builder.copy_elements(*lease,*position,count-*position);builder.finish_into(&next);
    std::lock_guard lock(mutex_);auto& h=list_header(source,schema);
    if(h.version!=version || h.backing!=old || h.count!=count)throw ListModified("List structurally modified during resize");
    h.backing=next;h.capacity=grown;h.count=count+1;h.version=version+1;
}
void Runtime::array_remove(void* out,RegisteredRootWord source,const ElementSchemaRef& schema,std::uint64_t index) {
    require_managed();if(!schema || !schema->mutable_data())throw std::invalid_argument("array profile does not permit mutation");if(!out)throw std::invalid_argument("missing removed result");poll();std::lock_guard lock(mutex_);heap_.validate_output_address(reinterpret_cast<void**>(out));auto& h=list_header(source,schema);structural_room(h.version);
    if(index>=h.count)throw std::out_of_range("list index outside extent");const auto& state=heap_.buffer_state(h.backing);if(state.readers)throw BufferBusy("leased list backing");
    auto* from=static_cast<char*>(h.backing)+state.layout->element_offset(index);std::memcpy(out,from,schema->bytes());
    std::memmove(from,from+state.layout->stride(),static_cast<std::size_t>(h.count-index-1)*state.layout->stride());std::memset(static_cast<char*>(h.backing)+state.layout->element_offset(h.count-1),0,state.layout->stride());state.valid_prefix=--h.count;++h.version;
}
void Runtime::array_copy(void** out,RegisteredRootWord source,const BufferLayoutRef& layout) {
    require_buffer_output(out);BufferReadLease lease(*this,source,layout);if(lease.count()!=layout->capacity())throw std::invalid_argument("incomplete fixed snapshot");
    BufferBuilder builder(*this,layout,layout->capacity(),quota);builder.copy_elements(lease,0,layout->capacity());builder.finish_into(out);
}
}

namespace vd {
void Runtime::array_clear(RegisteredRootWord source,const ElementSchemaRef& schema) {
    require_managed();if(!schema || !schema->mutable_data())throw std::invalid_argument("array profile does not permit mutation");poll();std::lock_guard lock(mutex_);auto& h=list_header(source,schema);
    if(!h.count)return;structural_room(h.version);const auto& state=heap_.buffer_state(h.backing);
    if(state.readers)throw BufferBusy("leased list backing");
    std::memset(static_cast<char*>(h.backing)+state.layout->element_offset(0),0,static_cast<std::size_t>(h.count)*state.layout->stride());state.valid_prefix=h.count=0;++h.version;
}
void Runtime::array_slice(void** out,RegisteredRootWord source,const BufferLayoutRef& layout,bool list,std::uint64_t first,std::uint64_t count,bool reverse) {
    require_managed();require_buffer_output(out);poll();
    void* backing=nullptr;void* next=nullptr;void* result=nullptr;constexpr std::size_t zero=0;
    Slot slots[]={{&backing,sizeof(backing),&zero,1},{&next,sizeof(next),&zero,1},{&result,sizeof(result),&zero,1}};Frame roots(*this,slots,3);
    std::uint64_t live,version=0;BufferLayoutRef original;
    {std::lock_guard lock(mutex_);validate_buffer_layout(layout);heap_.validate_output_address(out);
        if(list){auto& h=list_header(source,layout->schema());backing=h.backing;live=h.count;version=h.version;}
        else {backing=reference_source(source);const auto& state=heap_.buffer_state(backing);if(!state.complete || !state.layout->equivalent(*layout) || state.valid_prefix!=layout->capacity())throw std::invalid_argument("invalid fixed slice source");live=layout->capacity();}
        if(count ? (first>=live || (reverse ? count-1>first : count>live-first)) : first>live)throw std::out_of_range("slice outside extent");
        if(backing)original=heap_.buffer_state(backing).layout;
    }
    auto target=buffer_layout(layout->schema(),count);std::optional<BufferReadLease> lease;
    if(count)lease.emplace(*this,root_word(&backing),original);
    if(count){BufferBuilder builder(*this,target,count,quota);if(!reverse)builder.copy_elements(*lease,first,count);else for(std::uint64_t i=0;i<count;++i)builder.copy_elements(*lease,first-i,1);builder.finish_into(&next);}
    array_new(&result,buffer_layout(layout->schema(),0),true);
    auto result_root=root_word(&result);
    {std::lock_guard lock(mutex_);
        if(list){const auto& h=list_header(source,layout->schema());if(h.version!=version || h.backing!=backing || h.count!=live)throw ListModified("List structurally modified during slice");}
        auto& h=list_header(result_root,layout->schema());h.backing=next;h.count=h.capacity=count;*out=result;
    }
}
}

namespace vd {
std::uint64_t Runtime::string_count(void* source,bool bytes) const {
    auto text=string_bytes(source);if(text.size()>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))throw std::length_error("string count is not representable");if(bytes)return text.size();
    std::uint64_t count=0;for(std::size_t i=0;i<text.size();++count)i+=utf8_scalar_bytes(text,i);return count;
}
void Runtime::string_index_into(void** out,void* source,ScalarEncoding encoding,std::uint64_t bits) {
    auto text=string_bytes(source);auto count=string_count(source,false);
    unsigned width;
    switch(encoding){case ScalarEncoding::i8:case ScalarEncoding::u8:width=8;break;case ScalarEncoding::i16:case ScalarEncoding::u16:width=16;break;case ScalarEncoding::i32:case ScalarEncoding::u32:width=32;break;case ScalarEncoding::i64:case ScalarEncoding::u64:width=64;break;default:throw std::invalid_argument("noninteger string index");}
    const bool sign=encoding==ScalarEncoding::i8 || encoding==ScalarEncoding::i16 || encoding==ScalarEncoding::i32 || encoding==ScalarEncoding::i64;
    if(width<64)bits&=(std::uint64_t{1}<<width)-1;
    std::uint64_t index=bits;
    if(sign && (bits&(std::uint64_t{1}<<(width-1)))) {
        auto magnitude=width==64 ? ~bits+1 : (std::uint64_t{1}<<width)-bits;
        if(magnitude>count)throw std::out_of_range("string index outside extent");index=count-magnitude;
    }
    if(index>=count)throw std::out_of_range("string index outside extent");
    std::size_t offset=0;for(std::uint64_t i=0;i<index;++i)offset+=utf8_scalar_bytes(text,offset);
    // string_into stages the complete bytes before its first safepoint.
    string_into(out,text.substr(offset,utf8_scalar_bytes(text,offset)));
}
void Runtime::string_encode_into(void** out,void* source) {
    // Native scratch owns bytes before metadata admission/allocation can poll.
    std::string bytes(string_bytes(source));auto layout=fixed_scalar_layout(ScalarEncoding::u8,bytes.size());
    void* backing=nullptr;void* result=nullptr;constexpr std::size_t zero=0;
    Slot slots[]={{&backing,sizeof(backing),&zero,1},{&result,sizeof(result),&zero,1}};Frame roots(*this,slots,2);
    if(!bytes.empty()){BufferBuilder builder(*this,layout,bytes.size(),quota);for(unsigned char byte:bytes)builder.write_scalar(byte);builder.finish_into(&backing);}
    array_new(&result,fixed_scalar_layout(ScalarEncoding::u8,0),true);
    auto root=root_word(&result);std::lock_guard lock(mutex_);require_buffer_output(out);heap_.validate_output_address(out);
    auto& header=list_header(root,layout->schema());header.backing=backing;header.count=header.capacity=bytes.size();*out=result;
}
bool Runtime::string_decode_into(void** out,RegisteredRootWord source) {
    auto schema=fixed_scalar_layout(ScalarEncoding::u8,0)->schema();
    // Capture the existing backing with a precise root and a read lease. No
    // source mutation is allowed while the owned native snapshot is copied.
    void* backing=nullptr;constexpr std::size_t zero=0;Slot slot{&backing,sizeof(backing),&zero,1};Frame roots(*this,&slot,1);
    std::uint64_t count;BufferLayoutRef layout;
    {std::lock_guard lock(mutex_);const auto& header=list_header(source,schema);count=header.count;backing=header.backing;if(backing)layout=heap_.buffer_state(backing).layout;}
    std::string bytes;
    if(count){BufferReadLease lease(*this,root_word(&backing),layout);bytes.resize(count);
        for(std::uint64_t i=0;i<count;++i)bytes[i]=static_cast<char>(buffer_byte(backing,layout,i,0));}
    if(!valid_utf8(bytes))return false;
    string_into(out,bytes);return true;
}
}
