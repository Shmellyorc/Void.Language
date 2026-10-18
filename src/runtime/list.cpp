#include "void/runtime.hpp"
#include <bit>
#include <cstring>
#include <limits>
namespace vd {
namespace {
std::size_t scalar_width(ScalarEncoding e) {
    switch(e) {
    case ScalarEncoding::i8:case ScalarEncoding::u8:case ScalarEncoding::boolean:return 1;
    case ScalarEncoding::i16:case ScalarEncoding::u16:return 2;
    case ScalarEncoding::i32:case ScalarEncoding::u32:case ScalarEncoding::f32:return 4;
    default:return 8;
    }
}
void valid_bits(ScalarEncoding e,std::uint64_t bits) {
    auto width=scalar_width(e);
    if((width<8 && bits>>(8*width)) || (e==ScalarEncoding::boolean && bits>1))throw std::invalid_argument("invalid list scalar bits");
}
std::uint64_t load_bits(const void* p,std::size_t width) {
    std::uint64_t bits=0;auto* out=reinterpret_cast<char*>(&bits);
    if constexpr(std::endian::native==std::endian::big)out+=sizeof(bits)-width;
    std::memcpy(out,p,width);return bits;
}
void store_bits(void* p,std::uint64_t bits,std::size_t width) {
    auto* in=reinterpret_cast<const char*>(&bits);
    if constexpr(std::endian::native==std::endian::big)in+=sizeof(bits)-width;
    std::memcpy(p,in,width);
}

}
Runtime::ListHeader& Runtime::list_header(RegisteredRootWord source,ScalarEncoding encoding) const {
    auto key="void.fixed.scalar.v1/"+std::to_string(static_cast<unsigned>(encoding));
    if(!types_)throw std::invalid_argument("missing list metadata");
    auto found=types_->schemas.find(key);if(found==types_->schemas.end())throw std::invalid_argument("missing list scalar schema");
    return list_header(source,found->second);
}
void Runtime::list_into(void** out,ScalarEncoding encoding,std::uint64_t count) {array_new(out,fixed_scalar_layout(encoding,count),true,true);}
std::uint64_t Runtime::list_count(RegisteredRootWord source,ScalarEncoding encoding) const {require_managed();std::lock_guard lock(mutex_);return list_header(source,encoding).count;}
std::uint64_t Runtime::list_version(RegisteredRootWord source,ScalarEncoding encoding) const {require_managed();std::lock_guard lock(mutex_);return list_header(source,encoding).version;}
std::uint64_t Runtime::list_read(RegisteredRootWord source,ScalarEncoding encoding,std::uint64_t index) const {
    require_managed();std::lock_guard lock(mutex_);const auto& h=list_header(source,encoding);
    if(index>=h.count)throw std::out_of_range("list index outside extent");
    const auto& state=heap_.buffer_state(h.backing);return load_bits(static_cast<const char*>(h.backing)+state.layout->element_offset(index),scalar_width(encoding));
}
void Runtime::list_write(RegisteredRootWord source,ScalarEncoding encoding,std::uint64_t index,std::uint64_t bits,std::optional<std::uint64_t> version) {
    valid_bits(encoding,bits);auto schema=fixed_scalar_schema(encoding);char data[8]{};store_bits(data,bits,scalar_width(encoding));array_write_value(source,schema,nullptr,index,data,false,version);
}
void Runtime::list_push(RegisteredRootWord source,ScalarEncoding encoding,std::uint64_t bits) {
    valid_bits(encoding,bits);auto schema=fixed_scalar_schema(encoding);char data[8]{};store_bits(data,bits,scalar_width(encoding));array_push(source,schema,data);
}
std::uint64_t Runtime::list_remove(RegisteredRootWord source,ScalarEncoding encoding,std::uint64_t index) {
    auto schema=fixed_scalar_schema(encoding);char data[8]{};array_remove(data,source,schema,index);return load_bits(data,scalar_width(encoding));
}
}
