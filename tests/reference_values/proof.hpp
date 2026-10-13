#pragma once
#include "../managed/probe.hpp"
#include <cstring>
#include <cstddef>
namespace reference_value_test {
// Private host-target layout expectation, not a public aggregate C ABI.
struct LinksLayout {std::uint8_t prefix;void* next;bool suffix;};
struct CarrierLayout {std::int8_t tag;void* required;LinksLayout nested;std::int64_t wide;std::int32_t count;};
struct NodeLayout {LinksLayout edges;std::int32_t value;};
// At allocation three the factory has returned. No host Handle and no live
// direct-reference source binding can hide a missing aggregate registration.
inline void inspect_carriers(managed_test::Probe& p,bool& witnessed,std::size_t count) {
    if(count!=3) return;
    unsigned live=0;
    for(const auto& frame:p.frames) for(std::size_t n=0;n<frame.count;++n) {
        const auto& slot=frame.slots[n];
        for(std::size_t leaf=0;leaf<slot.count;++leaf) {
            void* object=nullptr;
            std::memcpy(&object,static_cast<const char*>(slot.storage)+slot.offsets[leaf],sizeof(object));
            if(!object) continue;
            managed_test::require(slot.size>sizeof(void*) && slot.count==2,"direct root masked aggregate carrier proof");
            managed_test::require(slot.size==sizeof(CarrierLayout) && slot.offsets[0]==offsetof(CarrierLayout,required) && slot.offsets[1]==offsetof(CarrierLayout,nested)+offsetof(LinksLayout,next),"nested target root offsets/padding mismatch");
            managed_test::require(vd::Runtime::current().contains(object),"aggregate root points outside the heap");
            ++live;
        }
    }
    managed_test::require(p.frames.size()==2 && live>=4,"factory/argument/parameter aggregate handoff absent");
    witnessed=true;
}
}
