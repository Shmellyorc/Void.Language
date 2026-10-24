// Compile/link with the actual compiler, standard library and target SDK.
// Also runnable directly to verify ordinary cross-thread cancellation callbacks.
#include <version>
#include <atomic>
#include <stop_token>
#include <thread>

#if !defined(__cpp_lib_jthread) || __cpp_lib_jthread < 201911L
#error "The standard library must provide C++20 stop tokens and callbacks"
#endif

int main() {
    std::stop_source source;
    auto token=source.get_token();
    std::atomic<unsigned> calls=0;
    std::stop_callback callback(token,[&]{++calls;});
    std::thread requester([&]{source.request_stop();});
    requester.join();
    std::stop_callback late(token,[&]{++calls;});
    return token.stop_requested() && calls==2 && !source.request_stop()?0:1;
}
