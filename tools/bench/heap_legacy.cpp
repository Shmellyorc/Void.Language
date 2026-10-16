// Ordinary Heap microbenchmark, deliberately compatible with pre-Stage-2B headers.
// Timing builds use the normal allocator; a separate instrumented build counts
// requested bytes/new calls and retained bytes without contaminating throughput.
#include "void/heap.hpp"
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef VD_BENCH_ALLOCATIONS
namespace {
struct Stats { std::size_t calls=0, requested=0, live=0, peak=0; };
Stats stats[32]; int recording=-1;
struct alignas(std::max_align_t) Header { std::size_t bytes; int slot; };
}
void* operator new(std::size_t size) {
    if(size>std::numeric_limits<std::size_t>::max()-sizeof(Header)) throw std::bad_alloc();
    auto* h=static_cast<Header*>(std::malloc(sizeof(Header)+(size ? size : 1)));
    if(!h) throw std::bad_alloc();
    h->bytes=size;h->slot=recording;
    if(recording>=0) {auto& s=stats[recording];++s.calls;s.requested+=size;s.live+=size;if(s.live>s.peak)s.peak=s.live;}
    return h+1;
}
void operator delete(void* p) noexcept {
    if(!p)return;
    auto* h=static_cast<Header*>(p)-1;
    if(h->slot>=0)stats[h->slot].live-=h->bytes;
    std::free(h);
}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}
void* operator new[](std::size_t size) {return ::operator new(size);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
#endif
int main(int argc,char** argv) {
    try {
        const std::size_t count=argc>1 ? std::stoull(argv[1]) : 50000;
        if(!count || count>1000000)throw std::invalid_argument("benchmark object count must be 1..1000000 (fixture work bound)");
        using Clock=std::chrono::steady_clock;
        auto ns=[](auto start,auto stop){return std::chrono::duration<double,std::nano>(stop-start).count();};
        std::cout<<"[";bool comma=false;
        for(std::size_t refs: {0,1,4,16}) {
            vd::TypeLayout layout{32+refs*sizeof(void*),{}};
            // Deliberately unsorted and unaligned, as accepted by both revisions.
            for(std::size_t n=refs;n>0;--n)layout.references.push_back(1+(n-1)*sizeof(void*));
            vd::Heap heap;std::vector<void*> objects(count); // Fixture vector is outside counters/timer.
#ifdef VD_BENCH_ALLOCATIONS
            int allocation_slot=static_cast<int>(refs==16 ? 6 : refs==4 ? 4 : refs==1 ? 2 : 0);
            recording=allocation_slot;
#endif
            auto start=Clock::now();for(auto& p:objects)p=heap.allocate(layout);auto end=Clock::now();
#ifdef VD_BENCH_ALLOCATIONS
            recording=-1;auto allocation=stats[allocation_slot];
#endif
            // Real graph edges and roots, not just null word scanning. Every
            // object points backwards through one field; remaining words stay null.
            if(refs)for(std::size_t n=1;n<count;++n)heap.store(objects[n],1,objects[n-1]);
#ifdef VD_BENCH_ALLOCATIONS
            recording=allocation_slot+1;
#endif
            auto gc_start=Clock::now();
            for(int pass=0;pass<3;++pass) {heap.collect(objects);if(heap.size()!=count)throw std::runtime_error("live graph swept");}
            auto gc_end=Clock::now();
#ifdef VD_BENCH_ALLOCATIONS
            recording=-1;auto collection=stats[allocation_slot+1];
#endif
            auto sweep_start=Clock::now();heap.collect();auto sweep_end=Clock::now();
            if(heap.size())throw std::runtime_error("dead graph retained");
            if(comma)std::cout<<",";comma=true;
            std::cout<<"{\"references\":"<<refs<<",\"objects\":"<<count
                     <<",\"allocate_ns_per_object\":"<<ns(start,end)/count
                     <<",\"live_gc_ns_per_object\":"<<ns(gc_start,gc_end)/(3*count)
                     <<",\"dead_gc_ns_per_object\":"<<ns(sweep_start,sweep_end)/count;
#ifdef VD_BENCH_ALLOCATIONS
            auto payload=count*layout.size;
            std::cout<<",\"new_calls\":"<<allocation.calls
                     <<",\"requested_metadata_bytes\":"<<allocation.requested-payload
                     <<",\"retained_metadata_bytes\":"<<allocation.live-payload
                     <<",\"peak_requested_live_bytes\":"<<allocation.peak
                     <<",\"gc_new_calls_three_passes\":"<<collection.calls
                     <<",\"gc_requested_bytes_three_passes\":"<<collection.requested;
#endif
            std::cout<<"}";
        }
        std::cout<<"]\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
