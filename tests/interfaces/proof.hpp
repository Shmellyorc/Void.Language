#pragma once
#include "../managed/probe.hpp"
#include <cstring>
namespace interface_test {
// This is a target-layout expectation for root offsets, not an aggregate C ABI.
struct View {void* object;const void* witness;};
struct Part {std::uint8_t tag;View view;std::int64_t wide;};
struct Parcel {bool prefix;Part nested;};
inline void handoff(managed_test::Probe& p,bool& witnessed,void*& target,std::size_t count) {
    constexpr auto offset=offsetof(Parcel,nested)+offsetof(Part,view);
    if(count!=2 && count!=3) return;
    unsigned live=0;
    for(const auto& frame:p.frames) for(std::size_t n=0;n<frame.count;++n) {
        const auto& slot=frame.slots[n];for(std::size_t leaf=0;leaf<slot.count;++leaf) {
            void* object=nullptr;std::memcpy(&object,static_cast<const char*>(slot.storage)+slot.offsets[leaf],sizeof(object));if(!object) continue;
            if(count==2 && slot.size==sizeof(Parcel) && slot.offsets[leaf]==offset) target=object;
            if(object!=target) continue; // Unrelated loop garbage cannot retain this target.
            managed_test::require(slot.size==sizeof(Parcel) && slot.count==1 && slot.offsets[0]==offset,"direct root masked nested interface aggregate proof");++live;
        }
    }
    managed_test::require(target && live>=2,"nested interface argument/parameter handoff absent");
    if(count==3) witnessed=true;
}
inline void selected(managed_test::Probe& p,bool& witnessed,void*& target,std::size_t count,const void* payload) {
    if(count==2) {std::memcpy(&target,payload,sizeof(target));return;}
    if(count!=4) return;
    unsigned live=0;
    for(const auto& frame:p.frames) for(std::size_t n=0;n<frame.count;++n) {
        const auto& slot=frame.slots[n];for(std::size_t leaf=0;leaf<slot.count;++leaf) {
            void* object=nullptr;std::memcpy(&object,static_cast<const char*>(slot.storage)+slot.offsets[leaf],sizeof(object));if(object!=target) continue;
            managed_test::require(slot.size==sizeof(void*)*3+sizeof(std::uint64_t) && slot.count==2 && slot.offsets[0]==0 && slot.offsets[1]==sizeof(void*) && leaf==1,"selected receiver survived through an unrelated root instead of its prepared snapshot");++live;
        }
    }
    managed_test::require(target && live>0,"selected managed receiver snapshot/root handoff absent");witnessed=true;
}
}
