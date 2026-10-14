#pragma once
#include "../managed/probe.hpp"
#include "void/native.hpp"
#include "../integration/native_fixture.h"
#include <cstring>
#include <limits>
#include <stdexcept>
namespace string_test {
struct Calls {
    unsigned count=0,fail_at=0;
    int fail_after=0;
    unsigned nested_roots=0;
    bool protected_output=true;
    std::unique_ptr<vd::Runtime::Handle> captured;
    bool capture=false;
};
extern thread_local Calls* active;
struct Observe {
    explicit Observe(Calls& c) { active=&c; }
    ~Observe() {active=nullptr;}
};
struct Inner {void* text;std::int32_t count;};
struct Parcel {std::int64_t tag;Inner inner;};
inline void inspect() {
    if(!active || !managed_test::active) return;
    for(const auto& frame:managed_test::active->frames) for(std::size_t n=0;n<frame.count;++n) {
        const auto& slot=frame.slots[n];
        if(slot.size!=sizeof(Parcel)) continue;
        for(std::size_t leaf=0;leaf<slot.count;++leaf) if(slot.offsets[leaf]==offsetof(Parcel,inner)+offsetof(Inner,text)) {
            void* value=nullptr;std::memcpy(&value,static_cast<const char*>(slot.storage)+slot.offsets[leaf],sizeof(value));
            if(value && vd::Runtime::current().string_bytes(value)==std::string_view("rooted\0\xf0\x9f\x8e\xae",11)) ++active->nested_roots;
        }
    }
}
inline void runtime_checks() {
    using managed_test::require;
    vd::Runtime runtime(1);vd::Runtime::Attachment attachment(runtime);vd::Runtime::Managed managed(runtime);
    void* slots[3]={};const std::size_t offset=0;
    vd::Runtime::Slot roots[]={{&slots[0],sizeof(void*),&offset,1},{&slots[1],sizeof(void*),&offset,1},{&slots[2],sizeof(void*),&offset,1}};
    vd::Runtime::Frame frame(runtime,roots,3);
    runtime.string_into(&slots[0],std::string_view("a\0b",3));
    runtime.string_into(&slots[1],"\xc3\xa9");
    runtime.concat_into(&slots[2],slots[0],slots[1]);
    require(runtime.string_bytes(slots[2])==std::string_view("a\0b\xc3\xa9",5),"NUL/UTF-8 content lost");
    for(int budget=0;budget<7;++budget) {
        void* previous=slots[2];bool failed=false;
        {managed_test::AllocationFailure fail(budget);try {runtime.string_into(&slots[2],std::string(80,'z'));}catch(const std::bad_alloc&) {failed=true;}}
        if(failed) require(slots[2]==previous,"failed string replaced output");
        runtime.collect();require(runtime.contains(slots[0]) && runtime.contains(slots[1]),"string roots lost after failure");
    }
    bool rejected=false;try {runtime.string_into(&slots[2],"\xc0\xaf");}catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"invalid native string accepted");
    rejected=false;try {vd::native_utf8(runtime.string_bytes(slots[0]));}catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"terminated bridge truncated NUL");
    auto terminated=vd::native_utf8(runtime.string_bytes(slots[1]));
    auto* old=slots[1];slots[1]=nullptr;runtime.collect();require(!runtime.contains(old),"native copy retained managed storage");
    {vd::Runtime::Native native(runtime);require(terminated=="\xc3\xa9" && fixture_text_length(terminated.c_str())==2,"owned native terminated copy invalid");}
    auto retained=std::string(runtime.string_bytes(slots[0]));
    std::unique_ptr<fixture_resource,decltype(&fixture_destroy)> provider(nullptr,fixture_destroy);
    {vd::Runtime::Native native(runtime);provider.reset(fixture_create());require(bool(provider),"native text provider allocation");fixture_retain_text(provider.get(),retained.data(),retained.size());}
    old=slots[0];slots[0]=nullptr;runtime.collect();require(!runtime.contains(old),"retained native copy pinned managed storage");
    {vd::Runtime::Native native(runtime);require(fixture_text_byte(provider.get(),0)=='a' && fixture_text_byte(provider.get(),1)==0 && fixture_text_byte(provider.get(),2)=='b' && fixture_text_byte(provider.get(),3)==-1,"retained length-aware native bytes invalid");fixture_release_text(provider.get());require(fixture_text_byte(provider.get(),0)==-1,"native provider did not release bytes");provider.reset();}
    runtime.string_into(&slots[2],{});require(slots[2]==nullptr,"empty is not canonical");
    for(auto pair:{std::pair{std::numeric_limits<std::size_t>::max(),std::size_t{0}},std::pair{std::size_t{1},std::numeric_limits<std::size_t>::max()}}) {
        bool overflow=false;try {vd::Runtime::string_payload_size(pair.first,pair.second);}catch(const std::length_error&) {overflow=true;}require(overflow,"string extent wrapped");
    }
    void* unrooted=nullptr;rejected=false;try {runtime.string_into(&unrooted,"text");}catch(const std::invalid_argument&) {rejected=true;}require(rejected,"unrooted string output accepted");
    auto ordinary=vd::TypeLayout{8,{}};runtime.allocate_into(&slots[2],ordinary);
    rejected=false;try {runtime.string_bytes(slots[2]);}catch(const std::invalid_argument&) {rejected=true;}require(rejected,"ordinary object accepted as string");
    {vd::Runtime::Native native(runtime);rejected=false;try {runtime.string_bytes(slots[0]);}catch(const std::logic_error&) {rejected=true;}require(rejected,"NativeSafe string dereference accepted");}
}
}
