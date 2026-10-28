#pragma once
#include <atomic>
#include <chrono>
#include <cstring>
#include <ostream>
namespace perf {
enum Metric { allocation, allocation_bytes, copied_bytes, moved_bytes,
    list_count, list_read, list_push, list_header, root_word, reference_source,
    collect, buffer_copy, bulk_write, array_slice, string_encode, poll,
    native_execute, async_find, async_receive, async_pending, async_collect,
    async_children, async_failure, async_scan, async_tasks, async_work,
    gc_handles, gc_slots, count };
inline constexpr const char* names[]={"allocations","allocation_bytes","copied_bytes","moved_bytes",
    "list_count","list_read","list_push","list_header","root_word","reference_source",
    "gc_collect","buffer_copy","bulk_write","array_slice","string_encode","runtime_poll",
    "native_execute","async_find_visits","async_receive_visits","async_pending_visits","async_collect_visits",
    "async_children_visits","async_failure_visits","async_scan_visits","async_tasks","async_work_visits",
    "gc_handle_visits","gc_slot_visits"};
struct Counter {std::atomic<std::uint64_t> calls{0},ns{0};};
inline Counter counters[count];
inline std::atomic<bool> enabled{false};
inline thread_local std::uint64_t nested_ns=0;
inline void add(Metric metric,std::uint64_t amount=1) noexcept {if(enabled.load(std::memory_order_relaxed))counters[metric].calls.fetch_add(amount,std::memory_order_relaxed);}
struct Timer {
    Metric metric;bool active;std::uint64_t children;std::chrono::steady_clock::time_point start;
    Timer(Metric value):metric(value),active(enabled.load(std::memory_order_relaxed)),children(nested_ns){if(active){counters[metric].calls.fetch_add(1,std::memory_order_relaxed);start=std::chrono::steady_clock::now();}}
    ~Timer(){if(active){auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();auto exclusive=ns-(nested_ns-children);counters[metric].ns.fetch_add(exclusive,std::memory_order_relaxed);nested_ns+=exclusive;}}
};
inline void* copy(void* to,const void* from,std::size_t size){add(copied_bytes,size);return std::memcpy(to,from,size);}
inline void* move(void* to,const void* from,std::size_t size){add(moved_bytes,size);return std::memmove(to,from,size);}
inline void reset(){for(auto& c:counters){c.calls=0;c.ns=0;}nested_ns=0;}
inline void print(std::ostream& out){out<<'{';for(unsigned i=0;i<count;++i){if(i)out<<',';out<<'"'<<names[i]<<"\":{\"count\":"<<counters[i].calls<<",\"exclusive_ns\":"<<counters[i].ns<<'}';}out<<'}';}
}
