#include "void/http.hpp"
#include "void/detail/http_hooks.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#if __has_feature(address_sanitizer)
#include <sanitizer/allocator_interface.h>
#endif

namespace {
using namespace vd::http;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::size_t live_bytes() {
#if __has_feature(address_sanitizer)
    return __sanitizer_get_current_allocated_bytes();
#else
    return 0;
#endif
}
}
int main(int argc, char** argv) {
    try {
        require(argc == 4, "HTTP port, DNS endpoint and IPv6 HTTP port required");
        auto hooks = std::make_shared<testing::Hooks>(); hooks->dns_servers = argv[2];
        Driver driver(hooks); Error error; ClientOptions config;
        config.max_connections = config.max_per_origin = config.max_idle = 2;
        auto client = driver.open(config, error); require(bool(client), "DNS client acquisition");
        RequestOptions v4; v4.ip_family = IpFamily::v4;
        auto get = [&](std::string host, RequestOptions options = {}) {
            Request request; request.target = "http://" + host + ":" + argv[1] + "/plain";
            return driver.send(client, std::move(request), options)->take();
        };
        auto check_body = [&](const Result& value) {
            require(value.error.kind == ErrorKind::success && value.bytes == std::string("hello\0world", 11), "actual DNS-routed HTTP body");
        };
        check_body(get("warm.void.test", v4)); check_body(get("warm.void.test", v4));
        for (unsigned n = 0; n < 6; ++n) check_body(get("cold" + std::to_string(n) + ".void.test", v4));
        check_body(get("warm.void.test", v4));
        for (unsigned n = 0; n < 2; ++n) {
            auto failed = get("nx.void.test", v4);
            require(failed.error.kind == ErrorKind::resolve_failed && !failed.error.remote_outcome_unknown, "negative DNS failure has known unsent outcome");
        }
        check_body(get("tcp.void.test", v4));
        check_body(get("b\xc3\xbc" "cher.void.test", v4));
        if (std::string_view(argv[3]) != "-") {
            Request request; request.target = "http://v6.void.test:" + std::string(argv[3]) + "/plain";
            RequestOptions v6; v6.ip_family = IpFamily::v6;
            check_body(driver.send(client, request, v6)->take());
        }
        Request stalled; stalled.target = "http://stall.void.test:" + std::string(argv[1]) + "/plain";
        auto transfer = driver.start(client, stalled, v4, error); require(bool(transfer), "pending DNS admission");
        auto waiting = driver.response(transfer);
        std::this_thread::sleep_for(std::chrono::milliseconds(30)); require(!waiting->ready(), "DNS genuinely pending");
        check_body(get("127.0.0.1", v4));
        require(driver.close(transfer)->take().error.kind == ErrorKind::success, "DNS cancellation drains native lookup");
        auto cancelled = waiting->take();
        require(cancelled.error.kind == ErrorKind::closed && !cancelled.error.remote_outcome_unknown, "DNS cancellation preserves known unsent outcome");
        RequestOptions timeout = v4; timeout.connect_timeout_ms = 20;
        auto timed = driver.send(client, stalled, timeout)->take();
        require(timed.error.kind == ErrorKind::timeout && timed.error.phase == Phase::dns && !timed.error.remote_outcome_unknown,
            "DNS is covered by connect timeout in the DNS phase without request bytes");
        require(driver.close(client)->take().error.kind == ErrorKind::success, "DNS client close");

        client = driver.open(config, error); require(bool(client), "DNS pressure client");
        const auto baseline = live_bytes(); std::size_t low = static_cast<std::size_t>(-1), high = 0;
        for (unsigned n = 0; n < 64; ++n) {
            check_body(get("fanout.n" + std::to_string(n) + ".void.test", v4));
            if (n >= 16) { const auto live = live_bytes(); low = std::min(low, live); high = std::max(high, live); }
        }
        if (baseline) require(high - baseline < 1048576 && high - low < 1048576, "persistent DNS cache exceeds bounded native envelope");
        std::cout << "DNS retained live allocations: " << baseline << "; steady " << low << ".." << high << " bytes\n";
        require(driver.close(client)->take().error.kind == ErrorKind::success, "DNS pressure close");
        std::cout << "Native c-ares UDP/TCP, IPv4/IPv6, IDN, cache pressure, negative cache, cancellation and timeout passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
