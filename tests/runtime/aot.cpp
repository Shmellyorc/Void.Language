#include "void/runtime.hpp"
#include "void/execution.hpp"
#include <iostream>
#include <stdexcept>
#include <future>
#include <latch>
extern "C" std::int32_t entry(std::int32_t, vd::RuntimeFailure*);
extern "C" std::int32_t failure(std::int32_t, vd::RuntimeFailure*);
extern "C" std::int32_t entry_receiver(std::int32_t, vd::RuntimeFailure*);
int main() {
    try {
        vd::Runtime runtime;
        for (int i = 0; i < 100; ++i) {
            vd::RuntimeFailure error;
            {
                vd::Runtime::Attachment a(runtime);
                vd::Runtime::Managed m(runtime);
                if (entry(i,&error) != i*2+7 || error.message) throw std::runtime_error("AOT result/roots");
                if (entry_receiver(i,&error) != i*2+1 || error.message) throw std::runtime_error("AOT nested receiver roots/copies");
                failure(i,&error);
                if (!error.message) throw std::runtime_error("AOT failure missing");
                if (runtime.collect() != vd::Runtime::Collection::completed || runtime.snapshot().objects != 0)
                    throw std::runtime_error("AOT roots survived failure/return");
            }
        }
        std::latch ready(4);
        std::vector<std::future<void>> workers;
        for (int n = 0; n < 4; ++n) workers.push_back(std::async(std::launch::async,[&] {
            vd::Runtime::Attachment a(runtime); vd::Runtime::Managed m(runtime);
            ready.count_down();
            for (int i = 0; i < 50; ++i) {
                vd::RuntimeFailure error;
                if (entry(i,&error) != i*2+7 || error.message) throw std::runtime_error("concurrent AOT roots");
            }
        }));
        ready.wait();
        {
            vd::Runtime::Attachment a(runtime); vd::Runtime::Managed m(runtime);
            for (int i = 0; i < 100; ++i)
                if (runtime.collect() != vd::Runtime::Collection::completed) throw std::runtime_error("concurrent AOT rendezvous");
            { vd::Runtime::Native native(runtime); for (auto& worker : workers) worker.get(); }
            runtime.collect();
            if (runtime.snapshot().objects) throw std::runtime_error("concurrent AOT cleanup");
        }
        std::cout << "separately linked AOT precise roots and failure cleanup passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
