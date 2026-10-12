#include "void/runtime.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
namespace { thread_local int fail_after = -1; }
void* operator new(std::size_t size) {
    if (fail_after == 0) throw std::bad_alloc();
    if (fail_after > 0) --fail_after;
    if (auto* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
int main() {
    try {
        vd::Runtime runtime;
        vd::Runtime::Attachment attachment(runtime); vd::Runtime::Managed managed(runtime);
        void* root = nullptr; constexpr std::size_t zero = 0;
        vd::Runtime::Slot slot{&root,sizeof(root),&zero,1}; vd::Runtime::Frame frame(runtime,&slot,1);
        vd::TypeLayout parent{20*sizeof(void*),{}};
        for (std::size_t i = 0; i < 20; ++i) parent.references.push_back(i*sizeof(void*));
        runtime.allocate_into(&root,parent);
        void* child = nullptr;
        vd::Runtime::Slot child_slot{&child,sizeof(child),&zero,1};
        {
            vd::Runtime::Frame child_frame(runtime,&child_slot,1);
            for (auto offset : parent.references) { runtime.allocate_into(&child,{8,{}}); runtime.store(root,offset,child); }
        }
        int failures = 0;
        for (int allocation = 0; allocation < 12; ++allocation) {
            fail_after = allocation;
            auto result = runtime.collect();
            fail_after = -1;
            if (result == vd::Runtime::Collection::allocation_failure) ++failures;
            if (runtime.snapshot().objects != 21 || runtime.snapshot().requested) throw std::runtime_error("failed collection swept or remained requested");
            if (runtime.collect() != vd::Runtime::Collection::completed || runtime.snapshot().objects != 21) throw std::runtime_error("mark rollback/recovery failed");
        }
        if (failures < 4) throw std::runtime_error("insufficient traced-worklist allocation failures exercised");
        root = nullptr;
        runtime.collect();
        if (runtime.snapshot().objects) throw std::runtime_error("post-failure dead graph retained");
        std::cout << "deterministic allocation failures: enumeration, partial marking, rollback and recovery passed (" << failures << ")\n";
    } catch (const std::exception& e) { fail_after = -1; std::cerr << e.what() << '\n'; return 1; }
}
