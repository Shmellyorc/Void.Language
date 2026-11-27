#include "void/runtime_abi.hpp"
#include "void/runtime.hpp"
#include <atomic>
#include <limits>

std::uint64_t vd_generator_generation(std::uint64_t* storage) noexcept {
    static std::atomic<std::uint64_t> next{1};
    std::atomic_ref<std::uint64_t> generation(*storage);
    auto previous=generation.load(std::memory_order_acquire);
    if(previous)return previous;
    auto candidate=next.fetch_add(1,std::memory_order_relaxed);
    if(candidate==0 || candidate==std::numeric_limits<std::uint64_t>::max())std::terminate();
    if(generation.compare_exchange_strong(previous,candidate,std::memory_order_acq_rel))return candidate;
    return previous;
}

std::uint64_t vd_generator_thread() noexcept {
    return vd::Runtime::current_thread_identity();
}
