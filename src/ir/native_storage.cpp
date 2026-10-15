#include "void/native_storage.hpp"
#include <limits>
#include <stdexcept>
#include <utility>
namespace vd {
namespace {
bool alignment(std::uint64_t value) { return value && !(value & (value-1)); }
std::uint64_t aligned(std::uint64_t bytes,std::uint64_t align,std::uint64_t maximum) {
    if(!alignment(align) || bytes>maximum || align-1>maximum-bytes)
        throw std::length_error("Native storage alignment/size exceeds target bounds");
    return (bytes+align-1)&~(align-1);
}
}
NativeStoragePlanner::NativeStoragePlanner(StorageTarget target):target_(std::move(target)) {
    if(target_.triple.empty() || target_.data_layout.empty() || !target_.value_layout ||
       !target_.maximum_object_bytes || !alignment(target_.stack_alignment))
        throw std::invalid_argument("Native storage planning requires an explicit complete target layout");
}
void NativeStoragePlanner::storage(StorageRole role,std::string identity,StorageExtent extent) {
    if(identity.empty() || !alignment(extent.alignment) || extent.bytes>target_.maximum_object_bytes)
        throw std::invalid_argument("Invalid target storage home");
    for(const auto& home:homes_) if(home.identity==identity) throw std::invalid_argument("Duplicate native storage home");
    homes_.push_back({role,std::move(identity),extent});
}
void NativeStoragePlanner::value(StorageRole role,std::string identity,ir::Type type,std::uint64_t copies) {
    auto extent=target_.value_layout(type);
    const auto stride=aligned(extent.bytes,extent.alignment,target_.maximum_object_bytes);
    if(stride && copies>target_.maximum_object_bytes/stride) throw std::length_error("Native storage copies exceed target bounds");
    extent.bytes=stride*copies;storage(role,std::move(identity),extent);
}
NativeStoragePlan NativeStoragePlanner::finish(const std::set<StorageRole>& accounted,bool array_carrying) const {
    const std::set<StorageRole> required{StorageRole::parameter,StorageRole::result,StorageRole::local,
        StorageRole::instruction,StorageRole::copy,StorageRole::roots,StorageRole::staging,StorageRole::known_abi};
    if(accounted!=required) throw std::invalid_argument("Incomplete native storage/ABI inventory");
    std::uint64_t bytes=0;
    for(const auto& home:homes_) {
        bytes=aligned(bytes,home.extent.alignment,target_.maximum_object_bytes);
        if(home.extent.bytes>target_.maximum_object_bytes-bytes) throw std::length_error("Native storage sum exceeds target bounds");
        bytes+=home.extent.bytes;
    }
    const auto unrounded=bytes;
    bytes=aligned(bytes,target_.stack_alignment,target_.maximum_object_bytes);
    if(array_carrying && bytes>array_function_storage_budget)
        throw std::length_error("Array-carrying function exceeds the approved 65536-byte native static-storage budget");
    return {target_.triple,target_.data_layout,homes_,bytes,unrounded};
}
}
