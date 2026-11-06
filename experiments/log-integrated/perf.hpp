#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
namespace integrated_perf {
enum Phase {join,encode,fused,snapshot,reply,managed,gc,write,sync,phases};
inline const char* names[]{"join","encode","fused","snapshot","reply","managed","gc","write","fsync"};
struct Entry {std::uint64_t calls=0,inclusive_ns=0,exclusive_ns=0;};
inline thread_local bool enabled=false;
inline thread_local std::array<Entry,phases> data{};
inline thread_local std::array<std::uint64_t,64> children{};
inline thread_local unsigned depth=0;
inline thread_local std::uint64_t allocation_calls=0,requested_bytes=0,snapshot_bytes=0,scalar_inspections=0,range_checks=0,joined_bytes=0,encoding_owned_bytes=0,final_byte_copies=0,write_bytes=0,identity_misses=0;
inline thread_local std::map<std::string,std::uint64_t,std::less<>> identities;
struct Scope {
 Phase phase;bool active;std::chrono::steady_clock::time_point started;
 explicit Scope(Phase p):phase(p),active(enabled){if(active){if(depth==64)std::terminate();children[depth++]=0;started=std::chrono::steady_clock::now();}}
 ~Scope(){if(active){auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-started).count();auto child=children[--depth];auto& e=data[phase];++e.calls;e.inclusive_ns+=ns;e.exclusive_ns+=ns-child;if(depth)children[depth-1]+=ns;}}
};
inline void allocated(std::size_t n){if(enabled){++allocation_calls;requested_bytes+=n;}}
inline void constructed(const char* key){if(enabled){auto f=identities.find(key);if(f==identities.end())++identity_misses;else++f->second;}else identities.try_emplace(key,0);}
inline void reset(){data={};depth=0;allocation_calls=requested_bytes=snapshot_bytes=scalar_inspections=range_checks=joined_bytes=encoding_owned_bytes=final_byte_copies=write_bytes=identity_misses=0;for(auto& [key,count]:identities)count=0;}
inline void print(std::ostream& o){
 o<<"{\"allocation_calls\":"<<allocation_calls<<",\"requested_bytes\":"<<requested_bytes<<",\"snapshot_bytes\":"<<snapshot_bytes<<",\"scalar_inspections\":"<<scalar_inspections<<",\"range_checks\":"<<range_checks<<",\"joined_bytes\":"<<joined_bytes<<",\"encoding_owned_bytes\":"<<encoding_owned_bytes<<",\"final_byte_copies\":"<<final_byte_copies<<",\"write_bytes\":"<<write_bytes<<",\"identity_misses\":"<<identity_misses<<",\"phases\":{";
 for(unsigned n=0;n<phases;++n){if(n)o<<',';o<<'"'<<names[n]<<"\":{\"calls\":"<<data[n].calls<<",\"inclusive_ns\":"<<data[n].inclusive_ns<<",\"exclusive_ns\":"<<data[n].exclusive_ns<<'}';}
 o<<"},\"identities\":{";bool first=true;for(const auto& [key,count]:identities)if(count){if(!first)o<<',';first=false;o<<'"'<<key<<"\":"<<count;}o<<"}}";
}
}
