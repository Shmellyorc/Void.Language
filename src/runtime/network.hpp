#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#endif
#include <ares.h>

#include "void/async.hpp"
#include "void/host_ops.hpp"
#include "void/http_provider.hpp"
#include <array>
#include <chrono>
#include <condition_variable>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace vd::net {
using Clock = std::chrono::steady_clock;
constexpr std::size_t max_sockets = 1024, max_operations = 2048, max_lookups = 32;
constexpr std::size_t max_payload = 64 * 1024 * 1024, max_tcp = 4 * 1024 * 1024;
constexpr std::size_t datagram_buffer = 65536, max_v4 = 65507, max_v6 = 65527;
constexpr std::int64_t max_timeout = 86400000;
#ifdef _WIN32
using Descriptor = SOCKET;
using AddressSize = int;
using Poll = WSAPOLLFD;
constexpr auto invalid_descriptor = INVALID_SOCKET;
#else
using Descriptor = int;
using AddressSize = socklen_t;
using Poll = pollfd;
constexpr Descriptor invalid_descriptor = -1;
#endif

enum class Kind { ok, invalid_argument, invalid_address, name_not_found, resolve_failed,
    address_in_use, address_not_available, permission_denied, connection_refused, connection_reset,
    network_unreachable, host_unreachable, timed_out, closed, write_closed, busy, message_too_large,
    limit_exceeded, unsupported, io_failure };
enum class Phase { prepare, resolve, open, bind, listen, connect, accept, read, write, shutdown, close };
enum class Domain { none, posix, winsock, cares };

struct Endpoint {
    sockaddr_storage storage{};
    AddressSize size = 0;
    const sockaddr* data() const { return reinterpret_cast<const sockaddr*>(&storage); }
    sockaddr* data() { return reinterpret_cast<sockaddr*>(&storage); }
    int family() const { return storage.ss_family; }
    int public_family() const { return family()==AF_INET6?6:4; }
    std::uint16_t port() const;
    std::uint32_t scope() const;
    std::string ip() const;
    std::string format() const;
    bool multicast() const;
    bool unspecified() const;
    bool link_without_scope() const;
};
std::optional<Endpoint> parse_ip(std::string_view, std::uint16_t port=0);
bool dns_name(std::string_view);

struct Error {
    Kind kind = Kind::ok;
    Phase phase = Phase::prepare;
    Domain domain = Domain::none;
    int code = 0, cleanup = 0;
};
Error os_error(Phase, int);
Error resolver_error(int);
struct Reply {
    Error error;
    std::int64_t number = 0, detail = -1, port = 0, family = 0;
    bool flag = false;
    std::string text, bytes;
};
Reply address_reply(const Endpoint&);
bool close_operation(unsigned);
bool data_operation(unsigned);
bool factory_operation(unsigned);
bool supported();

struct Quota {
    std::atomic<std::size_t> sockets{0}, operations{0}, closes{0}, lookups{0}, bytes{0};
};
class Credit {
public:
    ~Credit();
    bool take(std::atomic<std::size_t>&, std::size_t count, std::size_t limit);
    void release() noexcept;
private:
    std::atomic<std::size_t>* account_ = nullptr;
    std::size_t count_ = 0;
};

struct Socket {
    explicit Socket(std::shared_ptr<Quota> quota) : quota(std::move(quota)) {}
    std::shared_ptr<Quota> quota;
    Credit slot;
    std::atomic<unsigned> directions{0};
    std::atomic<bool> closing{false}, closed{false};
    Descriptor descriptor = invalid_descriptor;
    Endpoint local, remote;
    int close_error = 0;
    bool no_delay = true, write_closed = false, eof = false;
};

struct Lookup {
    struct Watch { Descriptor fd = invalid_descriptor; short events = 0; };
    ares_channel_t* channel = nullptr;
    std::array<Watch, 16> watches{};
    std::array<ares_fd_events_t, 16> ready{};
    std::size_t ready_count = 0;
    bool finished = false, overflow = false;
    int status = ARES_SUCCESS;
    std::array<Endpoint,32> results{};
    std::size_t count = 0;
    void stop() noexcept;
};
struct Request {
    explicit Request(unsigned operation) : operation(operation) {}
    ~Request();
    void retire() noexcept;
    const unsigned operation;
    Runtime* admission = nullptr; // Native accounting only.
    std::shared_ptr<Quota> quota;
    Credit operations, payload, lookup_slot;
    unsigned reservation = 0;
    std::shared_ptr<Socket> socket, created;
    std::optional<NativeCompletion> completion;
    std::atomic<bool> cancelled{false};
    bool abandon = false, registered = false;
    short events = 0;
    bool ready = false, connecting = false;
    Clock::time_point deadline = Clock::time_point::max(), attempt_deadline = Clock::time_point::max();
    std::chrono::milliseconds attempt{0};
    int family = 0, backlog = 0;
    std::uint16_t port = 0;
    bool no_delay = true, hostname = false;
    std::size_t maximum = 0, progress = 0, candidate = 0;
    std::string name, input;
    std::optional<Endpoint> endpoint;
    std::unique_ptr<Lookup> lookup;
    Error last_attempt;
    Reply reply;
};
class Service {
public:
    explicit Service(std::string test_resolver = {});
    ~Service();
    Service(const Service&) = delete;
    Service& operator=(const Service&) = delete;
    bool prepare(Request&, std::size_t payload_bytes);
    void submit(const std::shared_ptr<Request>&, NativeCompletion) noexcept;
    void cancel(Request&) noexcept;
    int close(const std::shared_ptr<Socket>&) noexcept;
    void request_stop() noexcept;
    void shutdown();
private:
    struct PollTarget { Request* request; bool dns; };
    void run() noexcept;
    bool step(Request&);
    bool resolve(Request&);
    bool connect(Request&);
    bool transfer(Request&);
    void complete(Request&) noexcept;
    void collect_ready();
    void close_socket(Socket&) noexcept;
    const std::shared_ptr<const http::ProviderLease> lease_;
    const std::shared_ptr<Quota> quota_ = std::make_shared<Quota>();
    const std::string test_resolver_;
    std::mutex mutex_, join_;
    std::condition_variable changed_;
    bool stopping_ = false, stopped_ = false;
    std::vector<std::shared_ptr<Request>> incoming_, active_;
    std::vector<std::shared_ptr<Socket>> sockets_;
    std::vector<Poll> polls_;
    std::vector<PollTarget> targets_;
    std::thread worker_;
};
}
