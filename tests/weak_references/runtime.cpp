#include "void/runtime.hpp"
#include <array>
#include <chrono>
#include <future>
#include <iostream>
#include <latch>
#include <stdexcept>

namespace {
using vd::Runtime;
using namespace std::chrono_literals;

void require(bool condition, const char* message) {
    if(!condition)throw std::runtime_error(message);
}

void exercise(Runtime& runtime) {
    Runtime::Attachment attachment(runtime);
    Runtime::Managed managed(runtime);
    constexpr std::size_t zero=0;
    void* target=nullptr;
    void* first=nullptr;
    void* second=nullptr;
    void* result=nullptr;
    const std::array roots{
        Runtime::Slot{&target,sizeof(target),&zero,1},
        Runtime::Slot{&first,sizeof(first),&zero,1},
        Runtime::Slot{&second,sizeof(second),&zero,1},
        Runtime::Slot{&result,sizeof(result),&zero,1}
    };
    Runtime::Frame frame(runtime,roots.data(),roots.size());
    vd::TypeLayout target_layout{sizeof(void*),{},runtime.descriptor("target")};
    runtime.allocate_into(&target,target_layout);
    auto* address=target;
    runtime.weak_create(&first,target,"$weak:target");
    runtime.weak_create(&second,target,"$weak:target");
    require(runtime.collect()==Runtime::Collection::completed,"collection with target rooted");
    runtime.weak_lock(&result,first);
    require(result==target,"failed to promote the live target");
    target=nullptr;
    require(runtime.collect()==Runtime::Collection::completed,"collection with promoted target");
    runtime.weak_lock(&result,second);
    require(result==address,"promotion did not keep the shared target live");
    result=nullptr;
    require(runtime.collect()==Runtime::Collection::completed,"target retirement");
    require(runtime.snapshot().objects==2,"weak cells retained their target");
    runtime.allocate_into(&target,target_layout);
    require(target==address,"fixture did not reuse the retired address");
    runtime.weak_lock(&result,first);
    require(!result,"first weak cell resurrected a recycled address");
    runtime.weak_lock(&result,second);
    require(!result,"second weak cell resurrected a recycled address");

    runtime.weak_create(&second,target,"$weak:target");
    std::latch entered(1);
    auto collection=std::async(std::launch::async,[&] {
        Runtime::Attachment collector_attachment(runtime);
        Runtime::Managed collector(runtime);
        entered.count_down();
        return runtime.collect(5s);
    });
    entered.wait();
    const auto until=std::chrono::steady_clock::now()+3s;
    while(!runtime.snapshot().requested && std::chrono::steady_clock::now()<until)
        std::this_thread::yield();
    require(runtime.snapshot().requested,"concurrent collection did not request rendezvous");
    target=nullptr;
    runtime.weak_lock(&result,second);
    runtime.poll();
    require(collection.get()==Runtime::Collection::completed,"concurrent collection failed");
    require(result==address && runtime.contains(result),"promotion was not published before marking");
    require(runtime.begin_release(result),"deterministic release failed");
    runtime.weak_lock(&target,second);
    require(!target && runtime.contains(result),"released target was still promotable");
    result=nullptr;

    // Repeated batches leave live cells, dead cells and dead targets intermixed.
    std::vector<std::unique_ptr<Runtime::Handle>> cells;
    for(unsigned round=0;round<4;++round) {
        runtime.allocate_into(&target,target_layout);
        for(unsigned i=0;i<4096;++i) {
            runtime.weak_create(&result,target,"$weak:target");
            if(i%4==0)cells.push_back(std::make_unique<Runtime::Handle>(runtime,result));
        }
        target=first=second=result=nullptr;
        require(runtime.collect()==Runtime::Collection::completed,"weak population collection");
        require(runtime.snapshot().objects==cells.size(),"weak population retained extra objects");
        for(const auto& cell:cells) {
            runtime.weak_lock(&result,cell->get());
            require(!result,"a weak cell retained a retired target");
        }
        cells.clear();
        require(runtime.collect()==Runtime::Collection::completed && !runtime.snapshot().objects,
                "weak bookkeeping retained collected cells");
    }
}
}

int main() {
    try {
        Runtime runtime;
        const std::array descriptors{
            vd::NominalDescriptor{"target","","target-v1",{sizeof(void*),{}}},
            vd::NominalDescriptor{"$weak:target","","weak-target-v1",{2*sizeof(void*),{}}}
        };
        runtime.admit_types(descriptors);
        exercise(runtime);
        runtime.close();
        require(!runtime.snapshot().threads && !runtime.memory_snapshot().handles,"weak ownership survived shutdown");
    } catch(const std::exception& failure) {
        std::cerr<<failure.what()<<'\n';
        return 1;
    }
}
