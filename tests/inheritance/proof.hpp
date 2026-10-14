#pragma once
#include "../managed/probe.hpp"
#include <cstring>
namespace inheritance_test {
struct Base {std::int64_t wide;std::int8_t tag;};
struct Derived {Base base;void* child;void* link;};
struct Carrier {void* root;std::int8_t tail;};
inline void inspect(managed_test::Probe& p,bool& witnessed,std::size_t count,const void* payload,std::size_t size) {
    if(count==2) {
        managed_test::require(size==sizeof(Derived),"derived layout reused base tail padding");
        void* child=nullptr;std::memcpy(&child,static_cast<const char*>(payload)+offsetof(Derived,child),sizeof(child));
        managed_test::require(child && vd::Runtime::current().contains(child),"derived-only child is not at its prefix-preserving offset");
    }
    if(count!=3) return;
    unsigned live=0;
    for(const auto& frame:p.frames) for(std::size_t n=0;n<frame.count;++n) {
        const auto& slot=frame.slots[n];
        for(std::size_t leaf=0;leaf<slot.count;++leaf) {
            void* object=nullptr;std::memcpy(&object,static_cast<const char*>(slot.storage)+slot.offsets[leaf],sizeof(object));
            if(!object) continue;
            managed_test::require(slot.size==sizeof(Carrier) && slot.count==1 && slot.offsets[0]==offsetof(Carrier,root),"direct reference masked sole base-carrier tracing proof");
            ++live;
        }
    }
    managed_test::require(p.frames.size()==2 && live>=2,"base carrier argument/return handoff absent");witnessed=true;
}
}
