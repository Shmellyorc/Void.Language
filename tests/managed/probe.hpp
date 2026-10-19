#pragma once
#include "void/runtime_abi.hpp"
#include <memory>
#include <atomic>
#include <semaphore>
#include <functional>
#include <vector>
namespace managed_test {
struct Probe {
    struct Frame { void* address; const vd::Runtime::Slot* slots; std::size_t count; };
    std::vector<Frame> frames;
    bool inspect_roots=false, roots_valid=true;
    std::size_t roots_checked=0;
    std::function<void(std::size_t,const void*,std::size_t)> before_construct;
    std::size_t constructions=0, polls=0;
    bool commits_valid=true, collect_before=false, capture=false;
    unsigned capture_at=1;
    unsigned fail_construction=0;
    int fail_allocation=-1;
    unsigned gate_poll=0;
    std::size_t gate_after_constructions=0;
    bool gated=false;
    std::binary_semaphore reached{0}, release{0};
    std::unique_ptr<vd::Runtime::Handle> retained;
};
extern thread_local Probe* active;
extern std::atomic<void(*)()> concurrent_poll; // Native source execution rendezvous, tests only.
struct AllocationFailure {
    explicit AllocationFailure(int budget);
    ~AllocationFailure();
};
struct Observe {
    explicit Observe(Probe& p) { active=&p; }
    ~Observe() { active=nullptr; }
};
void require(bool,const char*);
void allocation_budget(int);
}
