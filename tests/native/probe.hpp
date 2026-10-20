#pragma once
#include <atomic>
#include <cstdint>
#include <semaphore>
#include <thread>
namespace native_test {
struct Gate {std::thread::id owner;std::atomic<bool> armed{true};std::binary_semaphore reached{0},release{0},unregistered{0};bool fail_callback=false;};
extern std::atomic<Gate*> gate;
extern std::atomic<void(*)(std::int32_t)> scalar_poll;
}
