#pragma once
#include "void/async.hpp"
#include "void/url.hpp"
#include <atomic>
#include <array>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace vd::filesystem { struct Result; class FileBorrow; }
namespace vd::http {
namespace testing { struct Hooks; }
inline constexpr std::size_t queue_bytes = 65536;
enum class ErrorKind : std::int64_t {
    success, invalid_argument, invalid_url, invalid_header, invalid_encoding, unsupported,
    closed, busy, limit_exceeded, resolve_failed, connect_failed, tls_verification,
    tls_failure, proxy_failed, timeout, protocol_error, redirect_denied, redirect_limit,
    body_not_replayable, upload_stopped, unexpected_eof, io_failure, http_status, transport_failure
};
enum class Phase : std::int64_t { prepare, queue, dns, connect, proxy, tls, headers, upload, download, decode, file, cleanup };
enum class Protocol : std::int64_t { negotiate, http1, http2_required };
enum class Version : std::int64_t { http1_0, http1_1, http2 };
enum class RedirectPolicy : std::int64_t { none, same_origin, safe };
enum class IpFamily : std::int64_t { any, v4, v6 };
enum class Decoding : std::int64_t { decoded, raw };
enum class NativeDomain : std::int64_t { none, curl, curl_multi, zlib, filesystem };
struct Header { std::string name, value; bool sensitive = false; };
struct RedirectInfo { std::int32_t status = 0; std::string from_origin, to_origin; bool method_changed = false; };
struct ResponseInfo {
    std::int32_t status = 0;
    Version version = Version::http1_1;
    std::string url;
    std::vector<Header> headers;
    std::vector<RedirectInfo> history;
    std::int64_t content_length = -1;
    bool content_decoded = false;
};
struct Error {
    ErrorKind kind = ErrorKind::success;
    Phase phase = Phase::prepare;
    std::int64_t native_code = 0;
    bool remote_outcome_unknown = false;
    NativeDomain native_domain = NativeDomain::none;
    url::ErrorKind url_kind = url::ErrorKind::success;
    std::int64_t url_offset = -1;
};
struct Authentication {
    enum Kind : std::int64_t { none, bearer, basic, digest } kind = none;
    std::string user, password; // bearer token is in password
};
struct TlsOptions {
    std::string ca_bundle;
    enum Identity : std::int64_t { none, pem, pkcs12, windows_store } identity = none;
    std::string certificate, key, password;
};
struct ProxyOptions {
    enum Mode : std::int64_t { direct, environment, explicit_proxy } mode = direct;
    std::string endpoint;
    Authentication auth;
    std::vector<std::string> no_proxy;
    // Environment policy is an owned acquisition-time snapshot, never a later
    // getenv from the driver. Populated by the owner-side resource bridge.
    std::string http_proxy, https_proxy, all_proxy, environment_no_proxy;
};
struct ClientOptions {
    TlsOptions tls;
    ProxyOptions proxy;
    bool cookies = false;
    std::int64_t max_active = 4, max_connections = 4, max_per_origin = 2,
        max_http2_streams = 4, max_idle = 4, idle_timeout_ms = 60000,
        connection_lifetime_ms = 300000, dns_cache_ms = 60000;
    std::string user_agent = "VOID";
};
struct RequestOptions {
    Protocol protocol = Protocol::negotiate;
    IpFamily ip_family = IpFamily::any;
    RedirectPolicy redirects = RedirectPolicy::safe;
    std::int64_t max_redirects = 10;
    Decoding decoding = Decoding::decoded;
    std::int64_t max_encoded_bytes = 67108864, max_decoded_bytes = 67108864, max_buffer_bytes = 67108864;
    std::int64_t connect_timeout_ms = 10000, header_timeout_ms = 30000,
        read_timeout_ms = 30000, write_timeout_ms = 30000, total_timeout_ms = 120000;
};
// Pure native producer signal. File borrows/Runtime handles remain exclusively
// in owner/worker adapters; transport only reads this immutable-after-done data.
struct UploadSource {
    std::atomic<bool> cancelled{false}, done{false};
    std::atomic<std::int64_t> file_completed{0};
    Error error;
    bool native_allocation_failure = false;
    std::shared_ptr<const filesystem::Result> io_cause;
};
struct Part {
    std::string name, filename, content_type;
    std::vector<Header> headers;
    std::string bytes;
    bool file = false;
    std::int64_t file_length = -1;
};
struct Request {
    enum Body : std::int64_t { empty, bytes, text, form, multipart, stream } body = empty;
    std::string method = "GET", target, data;
    std::int64_t stream_length = -1;
    std::vector<Header> headers;
    std::vector<Part> parts;
    Authentication auth;
    std::shared_ptr<UploadSource> source;
};
struct Result {
    Error error;
    std::shared_ptr<const ResponseInfo> info;
    std::vector<Header> trailers;
    std::string bytes;
    std::int64_t number = 0, uploaded = 0, downloaded_encoded = 0, downloaded = 0, elapsed_ms = 0;
    bool eof = false, native_allocation_failure = false;
    std::int64_t file_completed = 0;
    std::shared_ptr<const filesystem::Result> io_cause;
    std::shared_ptr<void> reservation; // native pending-publication admission lease
};
// Native-only operation/ticket storage. No Runtime, Task, Scope, source frame or
// generated function pointer crosses into transport state.
struct DriverState;
struct TransferState;
class Operation final {
public:
    static std::shared_ptr<Operation> rejected(Error);
    static std::shared_ptr<Operation> settled(Result);
    bool ready() const noexcept { return ready_.load(std::memory_order_acquire); }
    bool attach(NativeCompletion) noexcept; // false rejects a bounded waiter admission
    const Result& wait(); // caller must enter NativeSafe at the Runtime boundary
    const Result& wait(const std::atomic<bool>* cancellation);
    Result take();
    Result take(const std::atomic<bool>* cancellation);
    void cancel() noexcept;
private:
    friend class Driver;
    friend struct DriverState;
    friend struct TransferState;
    friend class FileDownload;
    std::mutex mutex_;
    std::condition_variable changed_;
    std::atomic<bool> ready_{false}, cancellation_{false};
    std::weak_ptr<DriverState> driver_;
    std::weak_ptr<TransferState> transfer_;
    std::array<std::optional<NativeCompletion>, 8> tickets_;
    Result result_;
    void complete(Result&&) noexcept;
};
struct ClientState;
struct TransferState;
struct DriverState;
class Client final {
    friend class Driver;
    friend struct DriverState;
    std::shared_ptr<ClientState> state_;
    explicit Client(std::shared_ptr<ClientState> state) : state_(std::move(state)) {}
public:
    ~Client();
};
class Transfer final {
    friend class Driver;
    std::shared_ptr<TransferState> state_;
    explicit Transfer(std::shared_ptr<TransferState> state) : state_(std::move(state)) {}
public:
    ~Transfer();
};
class Driver final {
    std::shared_ptr<DriverState> state_;
    std::mutex shutdown_mutex_;
    std::shared_ptr<Transfer> admit(const std::shared_ptr<Client>&, Request, RequestOptions, Error&, bool);
public:
    struct Snapshot {
        std::size_t clients = 0, transfers = 0, reserved_bytes = 0;
        std::size_t encoded_queued = 0, decoded_queued = 0, upload_queued = 0;
    };
    Driver();
    explicit Driver(std::shared_ptr<const testing::Hooks>);
    ~Driver();
    Driver(const Driver&) = delete;
    Driver& operator=(const Driver&) = delete;
    std::shared_ptr<Client> open(ClientOptions, Error&);
    std::shared_ptr<Transfer> start(const std::shared_ptr<Client>&, Request, RequestOptions, Error&);
    std::shared_ptr<Operation> send(const std::shared_ptr<Client>&, Request, RequestOptions);
    // Private native bulk adapter: the file worker feeds bounded upload blocks
    // while the driver independently receives the bounded response.
    std::shared_ptr<Transfer> start_buffered_upload(const std::shared_ptr<Client>&, Request, RequestOptions, Error&);
    std::shared_ptr<Operation> buffered_result(const std::shared_ptr<Transfer>&);
    std::shared_ptr<Operation> file_result(const std::shared_ptr<Transfer>&); // native bulk completion, cancellation targets this transfer
    void attach_file_sink(const std::shared_ptr<Transfer>&, const std::shared_ptr<UploadSource>&);
    std::shared_ptr<Operation> response(const std::shared_ptr<Transfer>&);
    std::shared_ptr<Operation> read(const std::shared_ptr<Transfer>&, std::int64_t max_count);
    std::shared_ptr<Operation> write(const std::shared_ptr<Transfer>&, std::string);
    std::shared_ptr<Operation> write_part(const std::shared_ptr<Transfer>&, std::size_t, std::string);
    Error finish_part(const std::shared_ptr<Transfer>&, std::size_t);
    void notify_source() noexcept;
    Error finish_upload(const std::shared_ptr<Transfer>&);
    std::shared_ptr<Operation> finish(const std::shared_ptr<Transfer>&);
    std::shared_ptr<Operation> close(const std::shared_ptr<Transfer>&);
    std::shared_ptr<Operation> close(const std::shared_ptr<Client>&);
    Error clear_cookies(const std::shared_ptr<Client>&);
    std::shared_ptr<Operation> lifetime(const std::shared_ptr<Transfer>&);
    void request_stop() noexcept;
    void shutdown() noexcept;
    Snapshot snapshot() const;
};
bool valid_header(const Header&) noexcept;
}
