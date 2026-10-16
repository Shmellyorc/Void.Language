// Linux-only diagnostic harness. Wrap the REAL production guard at link time;
// calls are timed while its caller holds Runtime's existing mutex. No test index.
#include "void/runtime.hpp"
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
using Clock=std::chrono::steady_clock;
thread_local bool measuring=false;
thread_local std::uint64_t scans=0;
thread_local double scan_ns=0,maximum_ns=0;
void record(Clock::time_point start) {
    auto elapsed=std::chrono::duration<double,std::nano>(Clock::now()-start).count();
    ++scans;scan_ns+=elapsed;if(elapsed>maximum_ns)maximum_ns=elapsed;
}
void check(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
struct Output {
    void* value=nullptr;static constexpr std::size_t zero=0;
    vd::Runtime::Slot slot{&value,sizeof(value),&zero,1};vd::Runtime::Frame frame;
    explicit Output(vd::Runtime& r):frame(r,&slot,1){}
};
}
extern "C" void __real__ZNK2vd4Heap23validate_output_addressEPPv(const vd::Heap*,void**);
extern "C" void __wrap__ZNK2vd4Heap23validate_output_addressEPPv(const vd::Heap* heap,void** slot) {
    if(!measuring) {__real__ZNK2vd4Heap23validate_output_addressEPPv(heap,slot);return;}
    const auto start=Clock::now();
    try {__real__ZNK2vd4Heap23validate_output_addressEPPv(heap,slot);}
    catch(...) {record(start);throw;}
    record(start);
}
int main(int argc,char** argv) {
    try {
        const auto count=argc>1 ? std::stoull(argv[1]) : 100000;
        const auto iterations=argc>2 ? std::stoull(argv[2]) : 64;
        check(iterations>0,"zero measurement iterations");
        std::cout<<"[";bool comma=false;
        for(std::size_t buffer_count:{0,1,16}) {
            vd::Runtime r;
            vd::ElementSchemaSpec spec{"alias-probe","mutable","native-probe-v1",vd::StorageDomain::native(),0,{vd::SchemaBytes{sizeof(void*),alignof(void*)}}};
            r.admit_buffer_schemas({&spec,1});auto layout=r.buffer_layout(r.buffer_schema(spec.identity),1);
            vd::Runtime::Attachment attached(r);vd::Runtime::Managed managed(r);
            Output head(r),fresh(r),out(r);
            // Constant-size precise roots, not count-sized roots/require_output
            // searches: each ordinary node points to the previous chain head.
            for(std::uint64_t i=0;i<count;++i) {
                r.allocate_into(&fresh.value,{sizeof(void*),{0}});
                r.store(fresh.value,0,head.value);head.value=fresh.value;fresh.value=nullptr;
            }
            std::vector<void*> buffers(buffer_count);
            std::vector<std::size_t> offsets(buffer_count);
            for(std::size_t i=0;i<buffer_count;++i)offsets[i]=i*sizeof(void*);
            vd::Runtime::Slot roots{buffers.data(),buffers.size()*sizeof(void*),offsets.data(),offsets.size()};
            vd::Runtime::Frame frame(r,&roots,buffer_count ? 1 : 0);
            for(auto& buffer:buffers) {
                vd::Runtime::BufferBuilder builder(r,layout,1,{65536,1,64});
                builder.zero_elements(1);builder.finish_into(&out.value);buffer=out.value;
            }
            out.value=nullptr;
            check(r.snapshot().objects==count+buffer_count,"fixture object population mismatch");
            // Empty string output performs no payload allocation or collection;
            // its two real guard calls are the complete measured hot operation.
            for(int warm=0;warm<4;++warm)r.string_into(&out.value,"");
            scans=0;scan_ns=0;maximum_ns=0;measuring=true;auto start=Clock::now();
            for(std::uint64_t i=0;i<iterations;++i)r.string_into(&out.value,"");
            auto end=Clock::now();measuring=false;
            check(scans==2*iterations,"production guard wrapper coverage mismatch");
            const auto wall=std::chrono::duration<double,std::nano>(end-start).count();
            if(comma)std::cout<<",";comma=true;
            std::cout<<"{\"ordinary_objects\":"<<count<<",\"buffers\":"<<buffer_count<<",\"iterations\":"<<iterations
                     <<",\"guard_calls\":"<<scans<<",\"wall_ns_per_output\":"<<wall/iterations
                     <<",\"guard_lock_service_ns_per_scan\":"<<scan_ns/scans<<",\"guard_lock_service_max_ns\":"<<maximum_ns;
            // Same guard also rejects a leased alias. This is correctness only;
            // early exit and exception construction do not model a full scan.
            if(buffer_count) {
                out.value=buffers[0];vd::Runtime::BufferReadLease lease(r,r.root_word(&out.value),layout);
                auto** alias=static_cast<void**>(buffers[0]);constexpr std::size_t zero=0;
                vd::Runtime::Slot alias_slot{alias,sizeof(void*),&zero,1};vd::Runtime::Frame alias_frame(r,&alias_slot,1);
                bool rejected=false;try {r.string_into(alias,"");}catch(const vd::BufferBusy&){rejected=true;}
                check(rejected && !*alias,"leased alias changed or did not reject");
            }
            std::cout<<",\"leased_alias_rejected\":"<<(buffer_count ? "true" : "null")<<"}";
            out.value=head.value=nullptr;for(auto& buffer:buffers)buffer=nullptr;
            check(r.collect()==vd::Runtime::Collection::completed && !r.snapshot().objects,"fixture graph not reclaimed");
        }
        std::cout<<"]\n";
    } catch(const std::exception& error) {measuring=false;std::cerr<<error.what()<<'\n';return 1;}
}
