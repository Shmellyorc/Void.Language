#pragma once
#include "void/runtime_abi.hpp"
#include <memory>
#include <semaphore>
namespace managed_test {
struct Probe {
    std::size_t constructions=0, polls=0;
    bool commits_valid=true, collect_before=false, capture=false;
    unsigned fail_construction=0;
    int fail_allocation=-1;
    unsigned gate_poll=0;
    std::binary_semaphore reached{0}, release{0};
    std::unique_ptr<vd::Runtime::Handle> retained;
};
extern thread_local Probe* active;
struct AllocationFailure {
    explicit AllocationFailure(int budget);
    ~AllocationFailure();
};
struct Observe {
    explicit Observe(Probe& p) { active=&p; }
    ~Observe() { active=nullptr; }
};
void require(bool,const char*);
}
