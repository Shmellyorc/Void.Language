// Compile/link with the actual compiler, standard library and target SDK.
// Also runnable directly to verify ordinary cross-thread cancellation callbacks.
#include <version>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <stop_token>
#include <thread>

#if !defined(__cpp_lib_jthread) || __cpp_lib_jthread < 201911L
#error "The standard library must provide C++20 stop tokens and callbacks"
#endif

int main() {
    // Bound a broken cancellation/wait implementation even in CMake's try_run.
    std::mutex watchdog_mutex;
    std::condition_variable watchdog_condition;
    bool done=false;
    std::thread watchdog([&]{
        std::unique_lock lock(watchdog_mutex);
        if(!watchdog_condition.wait_for(lock,std::chrono::seconds(10),[&]{return done;}))
            std::_Exit(2);
    });
    std::stop_source source;
    auto token=source.get_token();
    std::atomic<unsigned> calls=0;
    std::stop_callback callback(token,[&]{++calls;});
    std::atomic<bool> waiting=false;
    std::mutex mutex;
    std::condition_variable_any condition;
    bool cancelled=false;
    std::thread worker([&]{
        std::unique_lock lock(mutex);
        waiting.store(true);
        waiting.notify_all();
        cancelled=!condition.wait(lock,token,[]{return false;});
    });
    waiting.wait(false);
    std::thread requester([&]{source.request_stop();});
    requester.join();
    worker.join();
    std::stop_callback late(token,[&]{++calls;});
    const bool passed=cancelled && token.stop_requested() && calls==2 && !source.request_stop();
    {
        std::lock_guard lock(watchdog_mutex);
        done=true;
    }
    watchdog_condition.notify_one();
    watchdog.join();
    return passed?0:1;
}
