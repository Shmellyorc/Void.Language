// Focused manual Order 4 measurement; not part of timing-sensitive CI.
// Build -O2 with this revision's headers and libvoid_runtime.a, then run.
#include "void/runtime_abi.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <new>
#include <vector>
using namespace vd;
namespace {
thread_local bool counting=false;
thread_local std::size_t allocations=0,bytes=0;
void account(std::size_t n){if(counting){++allocations;bytes+=n;}}
std::uint64_t checksum=0;
template<class F> void measure(const char* name,unsigned size,unsigned iterations,F work) {
    for(unsigned i=0;i<100;++i)checksum+=work();
    std::array<double,5> samples;
    for(auto& ns:samples){auto start=std::chrono::steady_clock::now();for(unsigned i=0;i<iterations;++i)checksum+=work();auto end=std::chrono::steady_clock::now();ns=std::chrono::duration<double,std::nano>(end-start).count()/iterations;}
    allocations=bytes=0;counting=true;for(unsigned i=0;i<100;++i)checksum+=work();counting=false;
    std::sort(samples.begin(),samples.end());
    std::cout<<name<<','<<size<<','<<iterations<<','<<samples[2]<<','<<samples.front()<<','<<samples.back()<<','<<double(allocations)/100<<','<<double(bytes)/100<<'\n';
}
}
void* operator new(std::size_t n){account(n);if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
void* operator new(std::size_t n,std::align_val_t a){account(n);void* p=nullptr;if(!posix_memalign(&p,static_cast<std::size_t>(a),n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p,std::align_val_t) noexcept {std::free(p);}
void operator delete(void* p,std::size_t,std::align_val_t) noexcept {std::free(p);}
int main(){
    std::cout<<std::setprecision(12)<<"case,size,iterations,median_ns,min_ns,max_ns,allocations_per_op,requested_bytes_per_op\n";
    constexpr std::size_t zero=0;constexpr BufferWorkQuota quota{16384,256,4096};
    for(unsigned count:{8u,128u,1024u}){
        Runtime r;Runtime::Attachment a(r);Runtime::Managed m(r);
        std::vector<void*> homes(count);std::vector<Runtime::Slot> slots(count);
        for(unsigned i=0;i<count;++i)slots[i]={&homes[i],sizeof(void*),&zero,1};
        measure("frame_enter_leave",count,10000,[&]{Runtime::Frame f(r,slots.data(),count);return 1u;});
        Runtime::Frame f(r,slots.data(),count);
        measure("root_word_last",count,20000,[&]{auto token=r.root_word(&homes.back());(void)token;return 1u;});
    }
    {
        Runtime r;Runtime::Attachment a(r);Runtime::Managed m(r);auto l=r.fixed_scalar_layout(ScalarEncoding::i32,16);
        measure("scalar_layout",16,50000,[&]{return r.fixed_scalar_layout(ScalarEncoding::i32,16)->capacity();});
    }
    for(unsigned count:{1u,128u,1024u}){
        Runtime r;Runtime::Attachment a(r);Runtime::Managed m(r);auto l=r.fixed_scalar_layout(ScalarEncoding::i32,16);
        std::vector<void*> homes(count+1);std::vector<Runtime::Slot> slots(count+1);
        for(unsigned i=0;i<slots.size();++i)slots[i]={&homes[i],sizeof(void*),&zero,1};
        Runtime::Frame f(r,slots.data(),slots.size());
        for(unsigned i=0;i<count;++i)r.fixed_default_into(&homes[i],l,quota);
        RuntimeFailure failure;
        measure("array_read_buffer_heap",count,20000,[&]{auto x=vd_array_read(&homes[0],0,static_cast<unsigned>(ScalarEncoding::i32),16,&failure);if(failure.message)std::abort();return x;});
        measure("fixed_read_buffer_heap",count,20000,[&]{return vd_fixed_read(&homes[0],&l,0,&failure);});
        measure("copy_buffer_heap",count,100,[&]{vd_array_copy(&homes.back(),&homes[0],static_cast<unsigned>(ScalarEncoding::i32),16,&failure);if(failure.message)std::abort();
            // Keep heap size fixed, avoiding retention drift between samples.
            if(r.collect()!=Runtime::Collection::completed)std::abort();return 1u;});
    }
    for(unsigned count:{128u,1024u}){
        Runtime r;Runtime::Attachment a(r);Runtime::Managed m(r);auto l=r.fixed_scalar_layout(ScalarEncoding::i32,16);
        std::vector<void*> homes(count+1);std::vector<Runtime::Slot> slots(count+1);
        for(unsigned i=0;i<slots.size();++i)slots[i]={&homes[i],sizeof(void*),&zero,1};Runtime::Frame f(r,slots.data(),slots.size());
        r.fixed_default_into(&homes[0],l,quota);for(unsigned i=1;i<=count;++i)r.allocate_into(&homes[i],{8,{}});
        RuntimeFailure failure;
        measure("array_read_mixed_heap",count,20000,[&]{auto x=vd_array_read(&homes[0],0,static_cast<unsigned>(ScalarEncoding::i32),16,&failure);if(failure.message)std::abort();return x;});
    }
    // Empty-string publication performs the existing heap alias guard without
    // allocating payloads. Two native stack roots isolate heap-size effects.
    for(unsigned count:{1u,128u,1024u}) {
        Runtime r;Runtime::Attachment a(r);Runtime::Managed m(r);
        void* source=nullptr;void* output=nullptr;
        Runtime::Slot slots[]={{&source,sizeof(void*),&zero,1},{&output,sizeof(void*),&zero,1}};
        Runtime::Frame f(r,slots,2);auto l=r.fixed_scalar_layout(ScalarEncoding::u8,16);
        std::vector<std::unique_ptr<Runtime::Handle>> retained;
        for(unsigned i=0;i<count;++i){r.fixed_default_into(&source,l,quota);retained.push_back(std::make_unique<Runtime::Handle>(r,source));}
        measure("empty_publish_buffer_heap",count,20000,[&]{r.string_into(&output,{});return 1u;});
    }
    for(unsigned extent:{1048576u,2097152u}) {
        Runtime r;Runtime::Attachment a(r);Runtime::Managed m(r);
        void* source=nullptr;void* output=nullptr;
        Runtime::Slot slots[]={{&source,sizeof(void*),&zero,1},{&output,sizeof(void*),&zero,1}};
        Runtime::Frame f(r,slots,2);auto l=r.fixed_scalar_layout(ScalarEncoding::u8,extent);
        r.fixed_default_into(&source,l,quota);RuntimeFailure failure;
        measure("copy_payload",extent,20,[&]{vd_array_copy(&output,&source,static_cast<unsigned>(ScalarEncoding::u8),extent,&failure);if(failure.message || output==source)std::abort();
            if(r.collect()!=Runtime::Collection::completed)std::abort();return 1u;});
    }
    std::cerr<<"checksum="<<checksum<<'\n';
}
