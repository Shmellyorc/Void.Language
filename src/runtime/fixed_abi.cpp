#include "void/runtime_abi.hpp"
#include <new>
namespace {
template<class Work> void checked(vd::RuntimeFailure* failure,const vd::BufferLayoutRef* layout,Work work) noexcept {
    // An absent failure context is a broken internal call contract.
    if(!failure) std::terminate();
    if(failure->message) return;
    try {
        if(!layout || !*layout) throw std::invalid_argument("missing fixed layout");
        work(vd::Runtime::current(),*layout);
    } catch(const std::bad_alloc&) {failure->message="Fixed value allocation failed";}
      catch(const vd::BufferBusy&) {failure->message="Fixed buffer is busy";}
      catch(const std::out_of_range&) {failure->message="Fixed index outside extent";}
      catch(const std::length_error&) {failure->message="Fixed layout is not representable";}
      catch(const std::invalid_argument&) {failure->message="Invalid fixed value contract";}
      catch(const std::logic_error&) {failure->message="Invalid fixed root or runtime state";}
      catch(...) {std::terminate();}
}
}
extern "C" {
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
