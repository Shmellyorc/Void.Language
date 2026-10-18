#include "void/runtime.hpp"
#include <cstring>
#include <limits>
namespace vd {
namespace {
constexpr std::uint64_t absent=~std::uint64_t{};
constexpr BufferWorkQuota quota{16384,256,4096};
std::size_t scalar_bytes(unsigned kind) {constexpr unsigned widths[]={1,2,4,8,1,2,4,8,4,8,1};if(kind>10)throw std::invalid_argument("invalid dictionary scalar key");return widths[kind];}
void room(std::uint64_t version) {if(version==absent)throw std::length_error("dictionary structural version exhausted");}
std::uint64_t read_word(const char* p) {std::uint64_t value;std::memcpy(&value,p,8);return value;}
void write_word(char* p,std::uint64_t value) {std::memcpy(p,&value,8);}
}
Runtime::DictionaryLayout Runtime::dictionary_layout(const NativeDictionary& d) {
    require_managed();if(!d.entry || !d.key || d.key->bytes>d.entry->bytes || d.value_offset>d.entry->bytes || d.value_offset>d.hash_offset || (d.key->count && !d.key->fields) || d.hash_offset>d.entry->bytes || d.entry->bytes-d.hash_offset<8)throw std::invalid_argument("invalid dictionary metadata");
    for(std::size_t i=0;i<d.key->count;++i){const auto& f=d.key->fields[i];if((f.kind>7 && f.kind!=10 && f.kind!=11 && f.kind!=12) || f.offset>d.key->bytes || (f.kind<=10?scalar_bytes(f.kind):sizeof(void*))>d.key->bytes-f.offset)throw std::invalid_argument("unsupported dictionary key field");}
    if(!d.entry->count || !d.entry->fields || d.entry->count<=d.key->count || d.value_offset<d.key->bytes)throw std::invalid_argument("invalid dictionary entry geometry");
    const auto& hash=d.entry->fields[d.entry->count-1];if(hash.kind!=7 || hash.offset!=d.hash_offset || hash.nullable || hash.extent)throw std::invalid_argument("invalid dictionary hash field");
    for(std::size_t i=0;i<d.key->count;++i){const auto& a=d.entry->fields[i];const auto& b=d.key->fields[i];if(a.offset!=b.offset || a.kind!=b.kind || a.nullable!=b.nullable || a.extent!=b.extent || !a.target || !b.target || std::string_view(a.target)!=b.target)throw std::invalid_argument("dictionary key does not match entry prefix");}
    for(std::size_t i=d.key->count;i+1<d.entry->count;++i)if(d.entry->fields[i].offset<d.value_offset || d.entry->fields[i].offset>=d.hash_offset)throw std::invalid_argument("invalid dictionary value fields");
    auto entries=native_array_layout(*d.entry,0),key=native_array_layout(*d.key,0);auto identity="void.dict.v1/"+std::string(d.entry->key);std::shared_ptr<const NominalDescriptor> header;{std::lock_guard lock(mutex_);if(types_){auto found=types_->types.find(identity);if(found!=types_->types.end())header=found->second;}}
    if(!header){Native native(*this);NominalDescriptor spec{identity,"","dictionary-v1",{sizeof(DictionaryHeader),{offsetof(DictionaryHeader,entries),offsetof(DictionaryHeader,buckets)}}};admit_types({&spec,1});header=descriptor(identity);}
    if(header->layout.size!=sizeof(DictionaryHeader) || header->layout.references!=std::vector<std::size_t>{offsetof(DictionaryHeader,entries),offsetof(DictionaryHeader,buckets)})throw std::invalid_argument("invalid dictionary header descriptor");
    return {entries,key,header};
}
Runtime::DictionaryHeader& Runtime::dictionary_header(RegisteredRootWord source,const DictionaryLayout& l) const {
    auto* object=reference_source(source);if(heap_.nominal_identity(object)!=l.header->identity)throw std::invalid_argument("invalid dictionary header");auto& h=*static_cast<DictionaryHeader*>(object);
    if((h.capacity && (h.capacity&(h.capacity-1))) || h.count>h.capacity || h.capacity>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())/2)throw std::invalid_argument("invalid dictionary extent");
    if(!h.capacity){if(h.count || h.entries || h.buckets)throw std::invalid_argument("invalid empty dictionary");}
    else {const auto& entries=heap_.buffer_state(h.entries);const auto& buckets=heap_.buffer_state(h.buckets);
        if(!entries.complete || entries.valid_prefix!=h.count || entries.layout->schema()!=l.entries->schema() || entries.layout->capacity()!=h.capacity || !buckets.complete || buckets.valid_prefix!=h.capacity*2 || buckets.layout->capacity()!=h.capacity*2 || buckets.layout->schema()->identity()!="void.fixed.scalar.v1/7")throw std::invalid_argument("invalid dictionary backing");}
    return h;
}
std::uint64_t Runtime::dictionary_hash(const void* key,const NativeArrayElement& d) const {
    std::uint64_t hash=14695981039346656037ull;auto add=[&](const void* data,std::size_t count){auto* bytes=static_cast<const unsigned char*>(data);for(std::size_t n=0;n<count;++n){hash^=bytes[n];hash*=1099511628211ull;}};
    for(std::size_t i=0;i<d.count;++i){auto& f=d.fields[i];auto* p=static_cast<const char*>(key)+f.offset;if(f.kind==12){void* object;std::memcpy(&object,p,sizeof(object));auto text=heap_.string_bytes(object);const auto count=static_cast<std::uint64_t>(text.size());add(&count,sizeof(count));add(text.data(),text.size());}else add(p,f.kind<=10?scalar_bytes(f.kind):sizeof(void*));}return hash;
}
bool Runtime::dictionary_equal(const void* a,const void* b,const NativeArrayElement& d) const {
    for(std::size_t i=0;i<d.count;++i){auto& f=d.fields[i];auto* x=static_cast<const char*>(a)+f.offset;auto* y=static_cast<const char*>(b)+f.offset;
        if(f.kind==12){void* left;void* right;std::memcpy(&left,x,sizeof(left));std::memcpy(&right,y,sizeof(right));if(heap_.string_bytes(left)!=heap_.string_bytes(right))return false;}
        else if(std::memcmp(x,y,f.kind<=10?scalar_bytes(f.kind):sizeof(void*)))return false;}return true;
}
std::uint64_t Runtime::dictionary_find(const DictionaryHeader& h,const NativeDictionary& d,const void* key,std::uint64_t hash) const {
    if(!h.count)return absent;const auto& entries=heap_.buffer_state(h.entries);const auto& buckets=heap_.buffer_state(h.buckets);auto mask=h.capacity*2-1;auto slot=hash&mask;
    for(std::uint64_t probe=0;probe<h.capacity*2;++probe){auto index=read_word(static_cast<const char*>(h.buckets)+buckets.layout->element_offset(slot));if(!index)return absent;if(index>h.count)throw std::invalid_argument("invalid dictionary bucket");auto* entry=static_cast<const char*>(h.entries)+entries.layout->element_offset(index-1);if(read_word(entry+d.hash_offset)==hash && dictionary_equal(entry,key,*d.key))return index-1;slot=(slot+1)&mask;}throw std::invalid_argument("dictionary bucket table full");
}
void Runtime::dictionary_index(DictionaryHeader& h,const NativeDictionary& d) const {
    const auto& entries=heap_.buffer_state(h.entries);const auto& buckets=heap_.buffer_state(h.buckets);auto* data=static_cast<char*>(h.buckets);std::memset(data+buckets.layout->element_offset(0),0,static_cast<std::size_t>(h.capacity*2)*8);auto mask=h.capacity*2-1;
    for(std::uint64_t index=0;index<h.count;++index){auto* entry=static_cast<const char*>(h.entries)+entries.layout->element_offset(index);auto slot=read_word(entry+d.hash_offset)&mask;while(read_word(data+buckets.layout->element_offset(slot)))slot=(slot+1)&mask;write_word(data+buckets.layout->element_offset(slot),index+1);}
}
void Runtime::dictionary_new(void** out,const NativeDictionary& d) {auto l=dictionary_layout(d);DictionaryHeader empty{};auto layout=l.header->layout;layout.descriptor=l.header;construct_into(out,layout,&empty);}
std::uint64_t Runtime::dictionary_count(RegisteredRootWord source,const NativeDictionary& d,bool version) {auto l=dictionary_layout(d);std::lock_guard lock(mutex_);auto& h=dictionary_header(source,l);return version?h.version:h.count;}
bool Runtime::dictionary_read(void* out,RegisteredRootWord source,const NativeDictionary& d,const void* key,bool contains) {
    auto l=dictionary_layout(d);DataCursor cursor;if(!std::holds_alternative<SchemaScalar>(l.key->schema()->spec_.nodes[l.key->schema()->spec_.root]))cursor.prepare(l.key->schema());poll();std::lock_guard lock(mutex_);validate_native_element(key,cursor,l.key->schema());auto& h=dictionary_header(source,l);auto index=dictionary_find(h,d,key,dictionary_hash(key,*d.key));if(contains)return index!=absent;if(index==absent)throw std::out_of_range("dictionary key not found");if(!out)throw std::invalid_argument("missing dictionary output");heap_.validate_output_address(reinterpret_cast<void**>(out));auto& state=heap_.buffer_state(h.entries);std::memcpy(out,static_cast<const char*>(h.entries)+state.layout->element_offset(index),d.entry->bytes);return true;
}
void Runtime::dictionary_entry(void* out,RegisteredRootWord source,const NativeDictionary& d,std::uint64_t index) {auto l=dictionary_layout(d);std::lock_guard lock(mutex_);auto& h=dictionary_header(source,l);if(index>=h.count)throw std::out_of_range("dictionary iterator outside extent");if(!out)throw std::invalid_argument("missing dictionary output");heap_.validate_output_address(reinterpret_cast<void**>(out));auto& state=heap_.buffer_state(h.entries);std::memcpy(out,static_cast<const char*>(h.entries)+state.layout->element_offset(index),d.entry->bytes);}
void Runtime::dictionary_write(RegisteredRootWord source,const NativeDictionary& d,const void* value,std::uint64_t expected) {
    auto l=dictionary_layout(d);DataCursor cursor;cursor.prepare(l.entries->schema());poll();void* old=nullptr;void* next=nullptr;void* buckets=nullptr;constexpr std::size_t zero=0;Slot slots[]={{&old,sizeof(old),&zero,1},{&next,sizeof(next),&zero,1},{&buckets,sizeof(buckets),&zero,1}};Frame roots(*this,slots,3);std::uint64_t count,capacity,hash;
    {std::lock_guard lock(mutex_);validate_native_element(value,cursor,l.entries->schema());auto& h=dictionary_header(source,l);if(h.version!=expected)throw ListModified("dictionary selected write invalidated");hash=dictionary_hash(value,*d.key);auto index=dictionary_find(h,d,value,hash);
        if(index!=absent){auto& state=heap_.buffer_state(h.entries);if(state.readers)throw BufferBusy("leased dictionary backing");auto* entry=static_cast<char*>(h.entries)+state.layout->element_offset(index);std::memcpy(entry+d.value_offset,static_cast<const char*>(value)+d.value_offset,d.hash_offset-d.value_offset);state.catalog=types_;return;}
        room(h.version);if(h.count<h.capacity){auto& state=heap_.buffer_state(h.entries);if(state.readers)throw BufferBusy("leased dictionary backing");auto* entry=static_cast<char*>(h.entries)+state.layout->element_offset(h.count);std::memcpy(entry,value,d.entry->bytes);write_word(entry+d.hash_offset,hash);state.catalog=types_;state.valid_prefix=++h.count;++h.version;auto& index_state=heap_.buffer_state(h.buckets);auto slot=hash&(h.capacity*2-1);auto* data=static_cast<char*>(h.buckets);while(read_word(data+index_state.layout->element_offset(slot)))slot=(slot+1)&(h.capacity*2-1);write_word(data+index_state.layout->element_offset(slot),h.count);return;}
        count=h.count;capacity=h.capacity;old=h.entries;
    }
    if(capacity>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())/4)throw std::length_error("dictionary capacity overflow");auto grown=capacity?capacity*2:4;auto target=buffer_layout(l.entries->schema(),grown);std::optional<BufferReadLease> lease;if(old){auto original=buffer_layout(l.entries->schema(),capacity);lease.emplace(*this,root_word(&old),original);}
    BufferBuilder builder(*this,target,count+1,quota);if(lease)builder.copy_elements(*lease,0,count);append_native(builder,target,value);builder.finish_into(&next);auto bucket_layout=fixed_scalar_layout(ScalarEncoding::u64,grown*2);BufferBuilder index_builder(*this,bucket_layout,grown*2,quota);index_builder.zero_elements(grown*2);index_builder.finish_into(&buckets);
    std::lock_guard lock(mutex_);auto& h=dictionary_header(source,l);if(h.version!=expected || h.entries!=old || h.count!=count)throw ListModified("dictionary modified during growth");auto& state=heap_.buffer_state(next);write_word(static_cast<char*>(next)+state.layout->element_offset(count)+d.hash_offset,hash);DictionaryHeader staged{next,buckets,count+1,grown,expected+1};dictionary_index(staged,d);h=staged;
}
bool Runtime::dictionary_remove(RegisteredRootWord source,const NativeDictionary& d,const void* key) {
    auto l=dictionary_layout(d);DataCursor cursor;if(!std::holds_alternative<SchemaScalar>(l.key->schema()->spec_.nodes[l.key->schema()->spec_.root]))cursor.prepare(l.key->schema());poll();std::lock_guard lock(mutex_);validate_native_element(key,cursor,l.key->schema());auto& h=dictionary_header(source,l);auto index=dictionary_find(h,d,key,dictionary_hash(key,*d.key));if(index==absent)return false;room(h.version);auto& state=heap_.buffer_state(h.entries);if(state.readers)throw BufferBusy("leased dictionary backing");auto* p=static_cast<char*>(h.entries)+state.layout->element_offset(index);std::memmove(p,p+state.layout->stride(),static_cast<std::size_t>(h.count-index-1)*state.layout->stride());--h.count;std::memset(static_cast<char*>(h.entries)+state.layout->element_offset(h.count),0,state.layout->stride());state.valid_prefix=h.count;++h.version;dictionary_index(h,d);return true;
}
void Runtime::dictionary_clear(RegisteredRootWord source,const NativeDictionary& d) {auto l=dictionary_layout(d);poll();std::lock_guard lock(mutex_);auto& h=dictionary_header(source,l);if(!h.count)return;room(h.version);auto& state=heap_.buffer_state(h.entries);if(state.readers)throw BufferBusy("leased dictionary backing");std::memset(static_cast<char*>(h.entries)+state.layout->element_offset(0),0,static_cast<std::size_t>(h.count)*state.layout->stride());state.valid_prefix=h.count=0;++h.version;dictionary_index(h,d);}
}
