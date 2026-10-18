#include "void/runtime_abi.hpp"
#include <new>
namespace {
template<class Work> void checked(vd::RuntimeFailure* failure,Work work) noexcept {
    // An absent failure context is a broken internal call contract.
    if(!failure) std::terminate();
    if(failure->message) return;
    try {
        work();
    } catch(const vd::ListModified&) {failure->message="List structurally modified during selected operation";}
      catch(const std::bad_alloc&) {failure->message="Fixed value allocation failed";}
      catch(const vd::BufferBusy&) {failure->message="Fixed buffer is busy";}
      catch(const std::out_of_range&) {failure->message="Fixed index outside extent";}
      catch(const std::length_error&) {failure->message="Fixed layout is not representable";}
      catch(const std::invalid_argument&) {failure->message="Invalid fixed value contract";}
      catch(const std::logic_error&) {failure->message="Invalid fixed root or runtime state";}
      catch(...) {std::terminate();}
}
template<class Work> void checked(vd::RuntimeFailure* failure,const vd::BufferLayoutRef* layout,Work work) noexcept {
    checked(failure,[&]{
        if(!layout || !*layout) throw std::invalid_argument("missing fixed layout");
        work(vd::Runtime::current(),*layout);
    });
}
template<class Work> void array_checked(unsigned encoding,std::uint64_t extent,vd::RuntimeFailure* failure,Work work) noexcept {
    checked(failure,[&]{
        auto& runtime=vd::Runtime::current();
        auto layout=runtime.fixed_scalar_layout(static_cast<vd::ScalarEncoding>(encoding),extent);
        work(runtime,layout);
    });
}
template<class Work> void array_source_checked(void** source,unsigned encoding,std::uint64_t extent,vd::RuntimeFailure* failure,Work work) noexcept {
    checked(failure,[&]{
        auto& runtime=vd::Runtime::current();
        auto root=runtime.root_word(source);
        auto layout=runtime.fixed_scalar_layout(root,static_cast<vd::ScalarEncoding>(encoding),extent);
        work(runtime,root,layout);
    });
}
constexpr vd::BufferWorkQuota array_quota{16384,256,4096};

}
extern "C" {
void vd_fixed_validate_layout(const vd::BufferLayoutRef* layout,std::uint64_t extent,unsigned encoding,vd::RuntimeFailure* failure) noexcept {
    checked(failure,layout,[&](auto& r,const auto& l){
        r.validate_fixed_scalar_layout(l,static_cast<vd::ScalarEncoding>(encoding),extent);
    });
}
void vd_fixed_default(void** out,const vd::BufferLayoutRef* layout,const vd::BufferWorkQuota* quota,vd::RuntimeFailure* failure) noexcept {
    checked(failure,layout,[&](auto& r,const auto& l){if(!quota) throw std::invalid_argument("missing work quota");r.fixed_default_into(out,l,*quota);});
}
void vd_fixed_literal(void** out,const vd::BufferLayoutRef* layout,const std::uint64_t* words,std::size_t count,const vd::BufferWorkQuota* quota,vd::RuntimeFailure* failure) noexcept {
    checked(failure,layout,[&](auto& r,const auto& l){
        if(!quota) throw std::invalid_argument("missing work quota");
        if(count && !words) throw std::invalid_argument("missing literal words");
        r.fixed_literal_into(out,l,{words,count},*quota);
    });
}
void vd_fixed_copy(void** out,void** source,const vd::BufferLayoutRef* layout,const vd::BufferWorkQuota* quota,vd::RuntimeFailure* failure) noexcept {
    checked(failure,layout,[&](auto& r,const auto& l){if(!quota) throw std::invalid_argument("missing work quota");r.fixed_copy_into(out,r.root_word(source),l,*quota);});
}
std::uint64_t vd_fixed_read(void** source,const vd::BufferLayoutRef* layout,std::uint64_t index,vd::RuntimeFailure* failure) noexcept {
    std::uint64_t result=0;
    checked(failure,layout,[&](auto& r,const auto& l){result=r.fixed_scalar(r.root_word(source),l,index);});
    return result;
}
void vd_fixed_write(void** source,const vd::BufferLayoutRef* layout,std::uint64_t index,std::uint64_t bits,vd::RuntimeFailure* failure) noexcept {
    checked(failure,layout,[&](auto& r,const auto& l){r.fixed_store_scalar(r.root_word(source),l,index,bits);});
}
}

extern "C" {
void vd_array_default(void** out,unsigned encoding,std::uint64_t extent,std::uint64_t bits,vd::RuntimeFailure* failure) noexcept {
    array_checked(encoding,extent,failure,[&](auto& r,const auto& layout){
        if(!bits) r.fixed_default_into(out,layout,array_quota);
        else {
            vd::Runtime::BufferBuilder builder(r,layout,extent,array_quota);
            for(std::uint64_t i=0;i<extent;++i) builder.write_scalar(bits);
            builder.finish_into(out);
        }
    });
}
void vd_array_copy(void** out,void** source,unsigned encoding,std::uint64_t extent,vd::RuntimeFailure* failure) noexcept {
    array_source_checked(source,encoding,extent,failure,[&](auto& r,auto root,const auto& layout){r.fixed_copy_into(out,root,layout,array_quota);});
}
std::uint64_t vd_array_read(void** source,std::uint64_t index,unsigned encoding,std::uint64_t extent,vd::RuntimeFailure* failure) noexcept {
    std::uint64_t bits=0;
    array_source_checked(source,encoding,extent,failure,[&](auto& r,auto root,const auto& layout){bits=r.fixed_scalar(root,layout,index);});
    return bits;
}
void vd_array_write(void** source,std::uint64_t index,std::uint64_t bits,unsigned encoding,std::uint64_t extent,vd::RuntimeFailure* failure) noexcept {
    array_source_checked(source,encoding,extent,failure,[&](auto& r,auto root,const auto& layout){r.fixed_store_scalar(root,layout,index,bits);});
}
}

extern "C" {
void vd_list_new(void** out,unsigned encoding,std::uint64_t count,vd::RuntimeFailure* failure) noexcept {
    checked(failure,[&]{vd::Runtime::current().list_into(out,static_cast<vd::ScalarEncoding>(encoding),count);});
}
#define VD_LIST_QUERY(name) \
std::uint64_t vd_list_##name(void** source,unsigned encoding,vd::RuntimeFailure* failure) noexcept { \
    std::uint64_t result=0;checked(failure,[&]{auto& r=vd::Runtime::current();result=r.list_##name(r.root_word(source),static_cast<vd::ScalarEncoding>(encoding));});return result;}
VD_LIST_QUERY(count)
VD_LIST_QUERY(version)
#undef VD_LIST_QUERY
std::uint64_t vd_list_read(void** source,unsigned encoding,std::uint64_t index,vd::RuntimeFailure* failure) noexcept {
    std::uint64_t result=0;checked(failure,[&]{auto& r=vd::Runtime::current();result=r.list_read(r.root_word(source),static_cast<vd::ScalarEncoding>(encoding),index);});return result;
}
void vd_list_write(void** source,unsigned encoding,std::uint64_t index,std::uint64_t bits,std::uint64_t version,unsigned selected,vd::RuntimeFailure* failure) noexcept {
    checked(failure,[&]{auto& r=vd::Runtime::current();r.list_write(r.root_word(source),static_cast<vd::ScalarEncoding>(encoding),index,bits,selected ? std::optional(version) : std::nullopt);});
}
void vd_list_push(void** source,unsigned encoding,std::uint64_t bits,vd::RuntimeFailure* failure) noexcept {
    checked(failure,[&]{auto& r=vd::Runtime::current();r.list_push(r.root_word(source),static_cast<vd::ScalarEncoding>(encoding),bits);});
}
std::uint64_t vd_list_remove(void** source,unsigned encoding,std::uint64_t index,vd::RuntimeFailure* failure) noexcept {
    std::uint64_t result=0;checked(failure,[&]{auto& r=vd::Runtime::current();result=r.list_remove(r.root_word(source),static_cast<vd::ScalarEncoding>(encoding),index);});return result;
}
}
namespace {
template<class Work> void native_array(const vd::NativeArrayElement* element,std::uint64_t extent,vd::RuntimeFailure* failure,Work work) noexcept {
    checked(failure,[&]{if(!element)throw std::invalid_argument("missing array element descriptor");auto& runtime=vd::Runtime::current();auto layout=runtime.native_array_layout(*element,extent);work(runtime,layout);});
}
}
extern "C" {
void vd_dict_new(void** out,const vd::NativeDictionary* d,vd::RuntimeFailure* f) noexcept {checked(f,[&]{if(!d)throw std::invalid_argument("missing dictionary metadata");vd::Runtime::current().dictionary_new(out,*d);});}
std::uint64_t vd_dict_count(void** source,const vd::NativeDictionary* d,unsigned version,vd::RuntimeFailure* f) noexcept {std::uint64_t result=0;checked(f,[&]{if(!d)throw std::invalid_argument("missing dictionary metadata");auto& r=vd::Runtime::current();result=r.dictionary_count(r.root_word(source),*d,version!=0);});return result;}
unsigned vd_dict_read(void* out,void** source,const vd::NativeDictionary* d,const void* key,unsigned contains,vd::RuntimeFailure* f) noexcept {unsigned result=0;checked(f,[&]{if(!d)throw std::invalid_argument("missing dictionary metadata");auto& r=vd::Runtime::current();result=r.dictionary_read(out,r.root_word(source),*d,key,contains!=0);});return result;}
void vd_dict_entry(void* out,void** source,const vd::NativeDictionary* d,std::uint64_t index,vd::RuntimeFailure* f) noexcept {checked(f,[&]{if(!d)throw std::invalid_argument("missing dictionary metadata");auto& r=vd::Runtime::current();r.dictionary_entry(out,r.root_word(source),*d,index);});}
void vd_dict_write(void** source,const vd::NativeDictionary* d,const void* entry,std::uint64_t version,vd::RuntimeFailure* f) noexcept {checked(f,[&]{if(!d)throw std::invalid_argument("missing dictionary metadata");auto& r=vd::Runtime::current();r.dictionary_write(r.root_word(source),*d,entry,version);});}
unsigned vd_dict_remove(void** source,const vd::NativeDictionary* d,const void* key,vd::RuntimeFailure* f) noexcept {unsigned result=0;checked(f,[&]{if(!d)throw std::invalid_argument("missing dictionary metadata");auto& r=vd::Runtime::current();result=r.dictionary_remove(r.root_word(source),*d,key);});return result;}
void vd_dict_clear(void** source,const vd::NativeDictionary* d,vd::RuntimeFailure* f) noexcept {checked(f,[&]{if(!d)throw std::invalid_argument("missing dictionary metadata");auto& r=vd::Runtime::current();r.dictionary_clear(r.root_word(source),*d);});}
void vd_value_array_new(void** out,const vd::NativeArrayElement* e,std::uint64_t count,unsigned list,vd::RuntimeFailure* f) noexcept {native_array(e,count,f,[&](auto& r,const auto& l){r.array_new(out,l,list!=0);});}
std::uint64_t vd_value_array_count(void** source,const vd::NativeArrayElement* e,vd::RuntimeFailure* f) noexcept {std::uint64_t result=0;native_array(e,0,f,[&](auto& r,const auto& l){result=r.array_count(r.root_word(source),l,true);});return result;}
std::uint64_t vd_value_array_version(void** source,const vd::NativeArrayElement* e,vd::RuntimeFailure* f) noexcept {std::uint64_t result=0;native_array(e,0,f,[&](auto& r,const auto& l){result=r.array_version(r.root_word(source),l);});return result;}
void vd_value_array_read(void* out,void** source,const vd::NativeArrayElement* e,std::uint64_t extent,unsigned list,std::uint64_t index,vd::RuntimeFailure* f) noexcept {native_array(e,extent,f,[&](auto& r,const auto& l){r.array_read(out,r.root_word(source),l,list!=0,index);});}
void vd_value_array_write(void** source,const vd::NativeArrayElement* e,std::uint64_t extent,unsigned list,std::uint64_t index,const void* value,std::uint64_t version,unsigned selected,unsigned initialize,vd::RuntimeFailure* f) noexcept {native_array(e,extent,f,[&](auto& r,const auto& l){r.array_write(r.root_word(source),l,list!=0,index,value,initialize!=0,selected?std::optional(version):std::nullopt);});}
void vd_value_array_insert(void** source,const vd::NativeArrayElement* e,std::uint64_t index,const void* value,std::uint64_t version,vd::RuntimeFailure* f) noexcept {native_array(e,0,f,[&](auto& r,const auto& l){r.array_insert(r.root_word(source),l->schema(),index,value,version);});}
void vd_value_array_clear(void** source,const vd::NativeArrayElement* e,vd::RuntimeFailure* f) noexcept {native_array(e,0,f,[&](auto& r,const auto& l){r.array_clear(r.root_word(source),l->schema());});}
void vd_value_array_slice(void** out,void** source,const vd::NativeArrayElement* e,std::uint64_t extent,unsigned list,std::uint64_t first,std::uint64_t count,unsigned reverse,vd::RuntimeFailure* f) noexcept {native_array(e,extent,f,[&](auto& r,const auto& l){r.array_slice(out,r.root_word(source),l,list!=0,first,count,reverse!=0);});}
void vd_value_array_push(void** source,const vd::NativeArrayElement* e,const void* value,vd::RuntimeFailure* f) noexcept {native_array(e,0,f,[&](auto& r,const auto& l){r.array_push(r.root_word(source),l->schema(),value);});}
void vd_value_array_remove(void* out,void** source,const vd::NativeArrayElement* e,std::uint64_t index,vd::RuntimeFailure* f) noexcept {native_array(e,0,f,[&](auto& r,const auto& l){r.array_remove(out,r.root_word(source),l->schema(),index);});}
void vd_value_array_copy(void** out,void** source,const vd::NativeArrayElement* e,std::uint64_t extent,vd::RuntimeFailure* f) noexcept {native_array(e,extent,f,[&](auto& r,const auto& l){r.array_copy(out,r.root_word(source),l);});}
}
