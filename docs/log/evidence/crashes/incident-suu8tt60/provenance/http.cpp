#include "void/http.hpp"
#include "void/detail/http_hooks.hpp"
#include "void/filesystem.hpp"
#include "void/http_provider.hpp"
#include "void/url.hpp"
#include "void/utf8.hpp"
#include <curl/curl.h>
#include <zlib.h>
#ifdef _WIN32
#ifndef SECURITY_WIN32
#define SECURITY_WIN32
#endif
#include <security.h>
#include <schannel.h>
#else
#include <openssl/ssl.h>
#endif
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <thread>

namespace vd::http {
namespace {
using Clock = std::chrono::steady_clock;
constexpr std::size_t mib = 1024 * 1024, memory_budget = 512 * mib;
constexpr std::size_t header_block_bytes = 65536, header_total_bytes = 262144;
std::string lower(std::string_view text) {
    std::string result(text);
    for (auto& c : result) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    return result;
}
bool token(std::string_view text) noexcept {
    if (text.empty()) return false;
    for (unsigned char c : text)
        if (!c || !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              std::strchr("!#$%&'*+-.^_`|~", c))) return false;
    return true;
}
bool field_bytes(std::string_view value) noexcept {
    for (unsigned char c : value) if ((c < 32 && c != '\t') || c == 127) return false;
    return true;
}
bool native_string(std::string_view value) noexcept {
    return value.find('\0') == std::string_view::npos && valid_utf8(value);
}
std::string_view trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n')) value.remove_suffix(1);
    return value;
}
std::int64_t elapsed(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
}
bool expired(Clock::time_point start, std::int64_t ms) { return ms && elapsed(start) >= ms; }
std::string origin(const url::Address& address) {
    return address.scheme + "://" + address.host + ":" + std::to_string(address.port);
}
struct Ring {
    std::array<unsigned char, queue_bytes> bytes;
    std::size_t first = 0, size = 0;
    std::size_t free() const noexcept { return queue_bytes - size; }
    std::size_t readable() const noexcept { return std::min(size, queue_bytes - first); }
    std::size_t writable() const noexcept { return std::min(free(), queue_bytes - (first + size) % queue_bytes); }
    unsigned char* read_data() noexcept { return bytes.data() + first; }
    unsigned char* write_data() noexcept { return bytes.data() + (first + size) % queue_bytes; }
    void consume(std::size_t count) noexcept { first = (first + count) % queue_bytes; size -= count; }
    void produce(std::size_t count) noexcept { size += count; }
    void append(const char* data, std::size_t count) noexcept {
        while (count) { auto part = std::min(count, writable()); std::memcpy(write_data(), data, part); produce(part); data += part; count -= part; }
    }
    void copy(char* data, std::size_t count) noexcept {
        while (count) { auto part = std::min(count, readable()); std::memcpy(data, read_data(), part); consume(part); data += part; count -= part; }
    }
};
struct Queues { Ring encoded, decoded, upload; };
struct BufferedBody {
    std::unique_ptr<char[]> data;
    std::size_t used = 0, capacity = 0;
    std::size_t size() const noexcept { return used; }
    void append(Ring& input, std::size_t limit) {
        const auto next_size = used + input.size;
        if (next_size > capacity) {
            const auto next_capacity = std::min(limit, std::max(next_size, std::max<std::size_t>(queue_bytes, capacity * 2)));
            auto next = std::make_unique<char[]>(next_capacity);
            if (used) std::memcpy(next.get(), data.get(), used);
            data = std::move(next); capacity = next_capacity;
        }
        input.copy(data.get() + used, input.size); used = next_size;
    }
    std::string take() const { return used ? std::string(data.get(), used) : std::string{}; }
    void clear() noexcept { data.reset(); used = capacity = 0; }
};
struct CurlError { CURLcode code; };
template<class T> void option(CURL* easy, CURLoption key, T value) {
    const auto code = curl_easy_setopt(easy, key, value);
    if (code != CURLE_OK) throw CurlError{code};
}
Error classify_code(CURLcode code) {
    switch (code) {
    case CURLE_OK: return {};
    case CURLE_COULDNT_RESOLVE_HOST: return {ErrorKind::resolve_failed, Phase::dns, code};
    case CURLE_COULDNT_RESOLVE_PROXY: return {ErrorKind::proxy_failed, Phase::proxy, code};
    case CURLE_PROXY: return {ErrorKind::proxy_failed, Phase::proxy, code};
    case CURLE_COULDNT_CONNECT: return {ErrorKind::connect_failed, Phase::connect, code};
    case CURLE_PEER_FAILED_VERIFICATION: case CURLE_SSL_CACERT_BADFILE:
        return {ErrorKind::tls_verification, Phase::tls, code};
    case CURLE_SSL_CONNECT_ERROR: case CURLE_SSL_CERTPROBLEM: case CURLE_SSL_CIPHER:
        return {ErrorKind::tls_failure, Phase::tls, code};
    case CURLE_OPERATION_TIMEDOUT: return {ErrorKind::timeout, Phase::connect, code};
    case CURLE_PARTIAL_FILE: return {ErrorKind::unexpected_eof, Phase::download, code};
    case CURLE_HTTP2: case CURLE_HTTP2_STREAM: case CURLE_WEIRD_SERVER_REPLY:
        return {ErrorKind::protocol_error, Phase::download, code};
    case CURLE_SEND_FAIL_REWIND: return {ErrorKind::body_not_replayable, Phase::upload, code};
    case CURLE_BAD_CONTENT_ENCODING: return {ErrorKind::invalid_encoding, Phase::decode, code};
    case CURLE_TOO_LARGE: return {ErrorKind::limit_exceeded, Phase::headers, code};
    default: return {ErrorKind::transport_failure, Phase::download, code};
    }
}
Error classify(CURLcode code) {
    auto error = classify_code(code);
    if (code != CURLE_OK) error.native_domain = NativeDomain::curl;
    return error;
}
Error url_failure(const url::Result& parsed, Phase phase) {
    Error result{ErrorKind::invalid_url, phase};
    result.url_kind = parsed.error; result.url_offset = parsed.offset;
    return result;
}
std::shared_ptr<Operation> failed_operation(Error error);
}

bool valid_header(const Header& header) noexcept { return token(header.name) && field_bytes(header.value); }

struct ClientState {
    std::weak_ptr<DriverState> driver;
    ClientOptions options;
    std::atomic<bool> closing{false};
    CURLM* multi = nullptr;
    CURLSH* share = nullptr;
    CURL* cookie_inspector = nullptr;
    std::shared_ptr<Operation> opened = std::make_shared<Operation>();
    std::shared_ptr<Operation> closed = std::make_shared<Operation>();
    std::size_t reservation = 0, admitted = 0;
    std::shared_ptr<Operation> clear_wait;
    // Owned explicit TLS inputs. The provider receives blobs, never caller
    // paths that can grow/change or block the network driver during open/read.
    std::string ca_material, certificate_material, key_material;
    std::size_t material_bytes = 0;
    // Cache age is an upper bound; bounded capacity may evict earlier. Each
    // configured hop can resolve the origin and proxy. Old in-flight lookups
    // and connections retain only their own bounded address references.
    unsigned dns_hops = 0;
};
struct TransferState {
    std::weak_ptr<DriverState> driver;
    std::shared_ptr<ClientState> client;
    Request request;
    RequestOptions options;
    std::shared_ptr<UploadSource> source;
    std::shared_ptr<UploadSource> sink;
    struct MimeReader {
        TransferState* owner = nullptr;
        std::size_t index = 0;
        std::int64_t consumed = 0, accepted = 0;
        bool eof = false;
    };
    std::array<MimeReader, 128> mime_readers;
    std::size_t current_file = 128;
    std::int64_t pending_part = -1;
    url::Address address;
    std::mutex mutex;
    std::atomic<bool> close_requested{false}, upload_finished{false};
    bool buffered = false, configured = false, detached = false, closed = false;
    bool receive_paused = false, send_paused = false, response_ready = false, upload_stopped = false;
    bool redirect_pending = false, decoding = false, gzip = false, member_end = false, decoder_finished = false;
    bool native_failure = false, producer_eof = false, used_proxy = false;
    bool request_permitted = false, request_sent = false;
    CURL* easy = nullptr;
    curl_slist* outgoing = nullptr;
    curl_mime* mime = nullptr;
    z_stream inflater{};
    std::unique_ptr<Queues> queues = std::make_unique<Queues>();
    std::shared_ptr<Operation> reader, writer, response_wait, finish_wait, send_wait;
    std::shared_ptr<Operation> send_operation;
    std::shared_ptr<Operation> closed_wait = std::make_shared<Operation>();
    std::shared_ptr<Operation> lifetime_wait = std::make_shared<Operation>();
    std::int64_t read_limit = 0;
    std::string pending_write;
    BufferedBody buffered_body;
    std::size_t reservation = 0, data_position = 0, write_accepted = 0;
    std::shared_ptr<void> reservation_lease;
    std::int64_t encoded = 0, decoded = 0, uploaded = 0, uploaded_before_hop = 0;
    std::size_t block_bytes = 0, total_headers = 0, total_fields = 0, interim_blocks = 0;
    std::int32_t block_status = 0;
    Version block_version = Version::http1_1;
    std::vector<Header> block, trailers;
    std::vector<RedirectInfo> history;
    std::shared_ptr<const ResponseInfo> info;
    Error error;
    Clock::time_point admitted_at = Clock::now(), header_at = admitted_at, read_at = admitted_at, write_at = admitted_at;
    bool header_timer = false, write_timer = false, upload_transmitted = false;
    curl_off_t posttransfer_before_request = 0;
    ~TransferState() { if (decoding) inflateEnd(&inflater); }
    Result result() const {
        Result value;
        value.error = error; value.info = info; value.uploaded = uploaded;
        if (value.error.kind != ErrorKind::success) value.error.remote_outcome_unknown = request_sent;
        value.downloaded_encoded = encoded; value.downloaded = decoded; value.elapsed_ms = elapsed(admitted_at);
        value.native_allocation_failure = native_failure;
        if (source) {
            value.file_completed = source->file_completed.load(std::memory_order_relaxed);
            if (source->done.load(std::memory_order_acquire)) {
                value.native_allocation_failure |= source->native_allocation_failure;
                value.io_cause = source->io_cause;
            }
        }
        value.reservation = reservation_lease;
        return value;
    }
    void fail(ErrorKind kind, Phase phase, std::int64_t code = 0, NativeDomain domain = NativeDomain::none) noexcept {
        if (error.kind == ErrorKind::success) error = {kind, phase, code, request_sent, domain};
    }
};
struct DriverState : std::enable_shared_from_this<DriverState> {
    std::shared_ptr<const testing::Hooks> hooks;
    std::shared_ptr<const ProviderLease> provider = ProviderLease::acquire();
    mutable std::mutex mutex;
    std::condition_variable changed;
    std::atomic<bool> stopping{false};
    CURLM* polling = nullptr; // lifetime protected by mutex; wakeup is curl's documented cross-thread call
    bool commands = false;
    std::array<std::shared_ptr<ClientState>, 8> clients;
    std::array<std::shared_ptr<TransferState>, 16> transfers;
    std::size_t reserved = 0, connection_reservations = 0;
    // Fixed aggregate wait storage; independent of descriptor numeric values
    // and platform FD_SETSIZE. One driver still owns every client multi.
    std::array<curl_waitfd, 4096> waitfds{};
    std::thread thread;
    void wake() noexcept {
        std::lock_guard lock(mutex);
        commands = true;
        if (polling) curl_multi_wakeup(polling);
        changed.notify_all();
    }
    void run() noexcept;
    void configure(const std::shared_ptr<TransferState>&);
    void service(const std::shared_ptr<TransferState>&);
    void detach(TransferState&, CURLcode, std::unique_lock<std::mutex>&);
    void release(const std::shared_ptr<TransferState>&);
    void cookies(ClientState&, TransferState*);
};
namespace {
Phase timeout_phase(const TransferState& transfer) noexcept {
    if (transfer.response_ready) return Phase::download;
    if (transfer.request_permitted)
        return transfer.upload_transmitted || transfer.header_timer || transfer.request.body == Request::empty ? Phase::headers : Phase::upload;
    if (!transfer.easy) return transfer.detached ? Phase::download : Phase::queue;
    curl_off_t resolved = 0, connected = 0;
    long proxy = 0, tunnel = 0;
    curl_easy_getinfo(transfer.easy, CURLINFO_NAMELOOKUP_TIME_T, &resolved);
    curl_easy_getinfo(transfer.easy, CURLINFO_CONNECT_TIME_T, &connected);
    curl_easy_getinfo(transfer.easy, CURLINFO_USED_PROXY, &proxy);
    curl_easy_getinfo(transfer.easy, CURLINFO_HTTP_CONNECTCODE, &tunnel);
    // Numeric literals have no DNS lookup. The host is already CURLU-validated;
    // this only distinguishes its shape, without parsing a second authority.
    const auto& host = transfer.address.host;
    const bool numeric = host.find(':') != std::string::npos ||
        (!host.empty() && host.find_first_not_of("0123456789.") == std::string::npos);
    if (proxy) {
        if (tunnel == 200 && transfer.address.scheme == "https") return Phase::tls;
        return Phase::proxy;
    }
    if (!resolved && !numeric) return Phase::dns;
    if (connected && transfer.address.scheme == "https") return Phase::tls;
    return Phase::connect;
}
struct Reservation {
    std::weak_ptr<DriverState> driver;
    std::size_t bytes;
    Reservation(std::weak_ptr<DriverState> owner, std::size_t amount) : driver(std::move(owner)), bytes(amount) {}
    ~Reservation() {
        if (auto owner = driver.lock()) {
            std::lock_guard lock(owner->mutex);
            if (owner->reserved < bytes) std::terminate();
            owner->reserved -= bytes;
        }
    }
};
}

bool Operation::attach(NativeCompletion ticket) noexcept {
    bool done;
    {
        std::lock_guard lock(mutex_);
        done = ready();
        if (!done) {
            auto slot = std::find_if(tickets_.begin(), tickets_.end(), [](const auto& value) { return !value; });
            if (slot == tickets_.end()) return false;
            slot->emplace(std::move(ticket));
        }
    }
    if (done) ticket.complete();
    return true;
}
const Result& Operation::wait() {
    std::unique_lock lock(mutex_);
    changed_.wait(lock, [&] { return ready(); });
    return result_;
}
const Result& Operation::wait(const std::atomic<bool>* cancellation) {
    if (!cancellation) return wait();
    bool signalled = false;
    std::unique_lock lock(mutex_);
    while (!ready()) {
        if (!signalled && cancellation->load(std::memory_order_acquire)) {
            signalled = true;
            lock.unlock(); cancel(); lock.lock();
        }
        changed_.wait_for(lock, std::chrono::milliseconds(10), [&] { return ready(); });
    }
    return result_;
}
Result Operation::take() { wait(); std::lock_guard lock(mutex_); return std::move(result_); }
Result Operation::take(const std::atomic<bool>* cancellation) { wait(cancellation); std::lock_guard lock(mutex_); return std::move(result_); }
void Operation::cancel() noexcept {
    cancellation_.store(true, std::memory_order_release);
    if (auto transfer = transfer_.lock()) transfer->close_requested = true;
    if (auto driver = driver_.lock()) driver->wake();
}
std::shared_ptr<Operation> Operation::settled(Result value) {
    auto operation = std::make_shared<Operation>(); operation->complete(std::move(value)); return operation;
}
void Operation::complete(Result&& value) noexcept {
    std::array<std::optional<NativeCompletion>, 8> tickets;
    {
        std::lock_guard lock(mutex_);
        if (ready()) return;
        result_ = std::move(value);
        tickets.swap(tickets_);
        ready_.store(true, std::memory_order_release);
    }
    changed_.notify_all();
    for (auto& ticket : tickets) if (ticket) ticket->complete();
}
std::shared_ptr<Operation> Operation::rejected(Error error) {
    auto operation = std::make_shared<Operation>();
    Result result; result.error = error; operation->complete(std::move(result)); return operation;
}
namespace {
std::shared_ptr<Operation> failed_operation(Error error) {
    return Operation::rejected(error);
}
bool finish_headers(TransferState& transfer, bool auth_final) {
    // Digest's challenge exchange stays provider-owned. No intermediate
    // authentication block is published as the final response.
    if (!auth_final && transfer.block_status == 401 && transfer.request.auth.kind == Authentication::digest) return true;
    if (!auth_final && transfer.block_status == 407 && transfer.client->options.proxy.auth.kind == Authentication::digest) return true;
    if (transfer.block_status == 407) {
        long used_proxy = transfer.used_proxy;
        if (transfer.easy) curl_easy_getinfo(transfer.easy, CURLINFO_USED_PROXY, &used_proxy);
        if (used_proxy) { transfer.fail(ErrorKind::proxy_failed, Phase::proxy, CURLE_PROXY, NativeDomain::curl); return false; }
    }
    const Header* location = nullptr;
    std::string encoding;
    std::int64_t length = -1;
    bool has_transfer_encoding = false;
    for (const auto& field : transfer.block) {
        const auto name = lower(field.name);
        if (name == "location") location = &field;
        if (name == "content-encoding") encoding += (encoding.empty() ? "" : ",") + lower(trim(field.value));
        if (name == "transfer-encoding") has_transfer_encoding = true;
        if (name == "content-length") {
            auto text = trim(field.value); std::int64_t next = -1;
            auto value = std::from_chars(text.data(), text.data() + text.size(), next);
            if (value.ec != std::errc{} || value.ptr != text.data() + text.size() || next < 0 || (length != -1 && length != next)) {
                transfer.fail(ErrorKind::protocol_error, Phase::headers); return false;
            }
            length = next;
        }
    }
    if (length >= 0 && has_transfer_encoding) { transfer.fail(ErrorKind::protocol_error, Phase::headers); return false; }
    auto info = std::make_shared<ResponseInfo>();
    info->status = transfer.block_status; info->version = transfer.block_version;
    info->url = transfer.address.text; info->headers = transfer.block; info->history = transfer.history; info->content_length = length;
    // A denied/limited redirect still has valid response metadata. Do not publish
    // it as the final response while a permitted hop is being followed.
    transfer.info = info;
    if (location && transfer.options.redirects != RedirectPolicy::none &&
        (transfer.block_status == 301 || transfer.block_status == 302 || transfer.block_status == 303 || transfer.block_status == 307 || transfer.block_status == 308)) {
        transfer.redirect_pending = true; return true;
    }
    if (transfer.options.protocol == Protocol::http2_required && info->version != Version::http2) {
        transfer.fail(ErrorKind::unsupported, Phase::headers); return false;
    }
    const bool bodyless = transfer.request.method == "HEAD" || info->status == 204 || info->status == 304;
    if (!bodyless && transfer.options.decoding == Decoding::decoded && !encoding.empty() && encoding != "identity") {
        if (encoding != "gzip" && encoding != "deflate") { transfer.fail(ErrorKind::invalid_encoding, Phase::decode); return false; }
        transfer.gzip = encoding == "gzip";
        auto initialized = inflateInit2(&transfer.inflater, transfer.gzip ? 31 : 15);
        if (initialized == Z_MEM_ERROR) throw std::bad_alloc();
        if (initialized != Z_OK) { transfer.fail(ErrorKind::invalid_encoding, Phase::decode); return false; }
        transfer.decoding = true; info->content_decoded = true;
    }
    transfer.info = std::move(info); transfer.response_ready = true;
    transfer.header_timer = false; transfer.read_at = Clock::now(); transfer.block_bytes = 0;
    if (transfer.block_status >= 400 && (transfer.request.body == Request::stream || transfer.source) && !transfer.producer_eof)
        transfer.upload_stopped = true;
    return true;
}
std::size_t header_callback(char* data, std::size_t size, std::size_t count, void* context) noexcept {
    auto& transfer = *static_cast<TransferState*>(context);
    std::lock_guard lock(transfer.mutex);
    try {
        if (size && count > std::numeric_limits<std::size_t>::max() / size) return 0;
        const auto bytes = size * count;
        std::string_view line(data, bytes);
        if (bytes > header_block_bytes - transfer.block_bytes || bytes > header_total_bytes - transfer.total_headers) {
            transfer.fail(ErrorKind::limit_exceeded, Phase::headers); return 0;
        }
        transfer.block_bytes += bytes; transfer.total_headers += bytes;
        if (line.starts_with("HTTP/")) {
            transfer.block.clear();
            auto space = line.find(' ');
            if (space == std::string_view::npos) { transfer.fail(ErrorKind::protocol_error, Phase::headers); return 0; }
            auto code = line.substr(space + 1, 3);
            auto converted = std::from_chars(code.data(), code.data() + code.size(), transfer.block_status);
            if (converted.ec != std::errc{} || converted.ptr != code.data() + code.size()) { transfer.fail(ErrorKind::protocol_error, Phase::headers); return 0; }
            transfer.block_version = line.starts_with("HTTP/2 ") ? Version::http2 : line.starts_with("HTTP/1.0 ") ? Version::http1_0 : Version::http1_1;
            return bytes;
        }
        if (line == "\r\n" || line == "\n") {
            if (transfer.response_ready) return bytes;
            if (!transfer.block_status) return bytes;
            if (transfer.block_status < 200) {
                if (transfer.block_status == 101 || ++transfer.interim_blocks > 16) { transfer.fail(ErrorKind::protocol_error, Phase::headers); return 0; }
                transfer.block.clear(); transfer.block_bytes = 0; transfer.block_status = 0; return bytes;
            }
            return finish_headers(transfer, false) ? bytes : 0;
        }
        auto colon = line.find(':');
        if (colon == std::string_view::npos || ++transfer.total_fields > 512) { transfer.fail(ErrorKind::protocol_error, Phase::headers); return 0; }
        Header field{std::string(line.substr(0, colon)), std::string(trim(line.substr(colon + 1))), false};
        if (!valid_header(field)) { transfer.fail(ErrorKind::protocol_error, Phase::headers); return 0; }
        if (transfer.client->options.cookies && lower(field.name) == "set-cookie") {
            // The pinned provider has inserted this cookie and released the
            // share lock before invoking HEADERFUNCTION (lib/http.c). Check
            // with the separate, inactive inspector on this same driver thread
            // before curl can process another response or construct a request.
            // Checking only after multi_perform loses the responsible transfer
            // when a batch contains several jar mutations.
            auto driver = transfer.driver.lock();
            if (!driver) { transfer.fail(ErrorKind::closed, Phase::headers); return 0; }
            driver->cookies(*transfer.client, &transfer);
            if (transfer.error.kind != ErrorKind::success || transfer.native_failure) return 0;
        }
        if (transfer.response_ready) {
            const auto name = lower(field.name);
            for (auto forbidden : {"content-length", "transfer-encoding", "host", "connection", "trailer", "content-encoding"})
                if (name == forbidden) { transfer.fail(ErrorKind::protocol_error, Phase::headers); return 0; }
            if (transfer.trailers.size() >= 32 || transfer.block_bytes > 16384) { transfer.fail(ErrorKind::limit_exceeded, Phase::headers); return 0; }
            transfer.trailers.push_back(std::move(field));
        } else {
            if (transfer.block.size() >= 128) { transfer.fail(ErrorKind::limit_exceeded, Phase::headers); return 0; }
            transfer.block.push_back(std::move(field));
        }
        return bytes;
    } catch (...) { transfer.native_failure = true; transfer.fail(ErrorKind::transport_failure, Phase::headers); return 0; }
}
std::size_t write_callback(char* data, std::size_t size, std::size_t count, void* context) noexcept {
    auto& transfer = *static_cast<TransferState*>(context);
    std::lock_guard lock(transfer.mutex);
    try {
        if (!transfer.response_ready && transfer.block_status >= 200 && !finish_headers(transfer, true)) return 0;
        if (transfer.redirect_pending) return 0;
        if (size && count > std::numeric_limits<std::size_t>::max() / size) return 0;
        auto bytes = size * count;
        if (bytes > static_cast<std::uint64_t>(transfer.options.max_encoded_bytes - transfer.encoded)) {
            transfer.fail(ErrorKind::limit_exceeded, Phase::download); return 0;
        }
        auto& queue = transfer.decoding ? transfer.queues->encoded : transfer.queues->decoded;
        if (bytes > queue.free()) { transfer.receive_paused = true; return CURL_WRITEFUNC_PAUSE; }
        if (!transfer.decoding && bytes > static_cast<std::uint64_t>(transfer.options.max_decoded_bytes - transfer.decoded)) {
            transfer.fail(ErrorKind::limit_exceeded, Phase::download); return 0;
        }
        queue.append(data, bytes); transfer.encoded += static_cast<std::int64_t>(bytes);
        if (!transfer.decoding) transfer.decoded += static_cast<std::int64_t>(bytes);
        transfer.read_at = Clock::now();
        return bytes;
    } catch (...) { transfer.native_failure = true; transfer.fail(ErrorKind::transport_failure, Phase::download); return 0; }
}
std::size_t read_callback(char* data, std::size_t size, std::size_t count, void* context) noexcept {
    auto& transfer = *static_cast<TransferState*>(context);
    std::lock_guard lock(transfer.mutex);
    if (size && count > std::numeric_limits<std::size_t>::max() / size) return CURL_READFUNC_ABORT;
    auto capacity = size * count;
    if (transfer.upload_stopped) return 0;
    if (transfer.request.body != Request::stream) {
        auto bytes = std::min(capacity, transfer.request.data.size() - transfer.data_position);
        std::memcpy(data, transfer.request.data.data() + transfer.data_position, bytes); transfer.data_position += bytes;
        if (transfer.data_position == transfer.request.data.size()) { transfer.producer_eof = true; }
        return bytes;
    }
    auto& queue = transfer.queues->upload;
    if (!queue.size) {
        if (transfer.upload_finished.load(std::memory_order_acquire)) {
            transfer.producer_eof = true; return 0;
        }
        transfer.send_paused = true; return CURL_READFUNC_PAUSE;
    }
    auto bytes = std::min(capacity, queue.size); queue.copy(data, bytes); return bytes;
}
int seek_callback(void* context, curl_off_t offset, int origin_value) noexcept {
    auto& transfer = *static_cast<TransferState*>(context);
    std::lock_guard lock(transfer.mutex);
    if (transfer.request.body == Request::stream || origin_value != SEEK_SET || offset != 0) return CURL_SEEKFUNC_CANTSEEK;
    transfer.data_position = 0; transfer.producer_eof = false; return CURL_SEEKFUNC_OK;
}
void advance_file(TransferState& transfer, std::size_t index) noexcept {
    if (transfer.current_file != index) return;
    do { ++transfer.current_file; } while (transfer.current_file < transfer.request.parts.size() && !transfer.request.parts[transfer.current_file].file);
    if (transfer.current_file >= transfer.request.parts.size()) transfer.producer_eof = true;
}
std::size_t mime_read_callback(char* bytes, std::size_t size, std::size_t count, void* context) noexcept {
    auto& reader = *static_cast<TransferState::MimeReader*>(context);
    auto& transfer = *reader.owner;
    std::lock_guard lock(transfer.mutex);
    if (transfer.upload_stopped || transfer.close_requested) return 0;
    if (size && count > std::numeric_limits<std::size_t>::max() / size) return CURL_READFUNC_ABORT;
    const auto length = transfer.request.parts[reader.index].file_length;
    if (length >= 0 && reader.consumed == length) { advance_file(transfer, reader.index); return 0; }
    auto& queue = transfer.queues->upload;
    if (!queue.size) {
        if (reader.eof) {
            if (length >= 0 && reader.consumed != length) { transfer.fail(ErrorKind::unexpected_eof, Phase::upload); return CURL_READFUNC_ABORT; }
            advance_file(transfer, reader.index); return 0;
        }
        transfer.send_paused = true; return CURL_READFUNC_PAUSE;
    }
    auto amount = std::min(queue.size, size * count);
    if (length >= 0) amount = std::min<std::size_t>(amount, static_cast<std::size_t>(length - reader.consumed));
    queue.copy(bytes, amount); reader.consumed += static_cast<std::int64_t>(amount);
    if (length >= 0 && reader.consumed == length) advance_file(transfer, reader.index);
    return amount;
}
int mime_seek_callback(void*, curl_off_t, int) noexcept { return CURL_SEEKFUNC_CANTSEEK; }
int progress_callback(void* context, curl_off_t, curl_off_t, curl_off_t, curl_off_t sent) noexcept {
    auto& transfer = *static_cast<TransferState*>(context);
    std::lock_guard lock(transfer.mutex);
    if (sent > std::numeric_limits<std::int64_t>::max() - transfer.uploaded_before_hop) {
        transfer.fail(ErrorKind::limit_exceeded, Phase::upload); return 1;
    }
    if (transfer.uploaded_before_hop + sent > transfer.uploaded) {
        transfer.uploaded = transfer.uploaded_before_hop + sent; transfer.write_at = Clock::now();
    }
    curl_off_t sent_at = 0;
    if (transfer.easy && transfer.request_permitted && !transfer.upload_transmitted &&
        curl_easy_getinfo(transfer.easy, CURLINFO_POSTTRANSFER_TIME_T, &sent_at) == CURLE_OK &&
        sent_at > transfer.posttransfer_before_request) {
        transfer.upload_transmitted = true;
        if (!transfer.response_ready) { transfer.header_timer = true; transfer.header_at = Clock::now(); }
    }
    return transfer.close_requested.load(std::memory_order_acquire) ? 1 : 0;
}
int prerequisite_callback(void* context, char*, char*, int, int) noexcept {
    auto& transfer = *static_cast<TransferState*>(context);
    std::lock_guard lock(transfer.mutex);
    if (transfer.request_permitted) {
        const bool challenge = (transfer.block_status == 401 && transfer.request.auth.kind == Authentication::digest) ||
            (transfer.block_status == 407 && transfer.client->options.proxy.auth.kind == Authentication::digest);
        if (!challenge) {
            // curl retries stale reused connections and REFUSED_STREAM by
            // default, including a buffered POST already written to the peer.
            // Enforce the approved no-retry policy before another request.
            long bytes = 0;
            curl_easy_getinfo(transfer.easy, CURLINFO_REQUEST_SIZE, &bytes);
            transfer.request_sent |= bytes > 0 || transfer.uploaded > 0;
            transfer.fail(ErrorKind::transport_failure, transfer.request_sent ? Phase::headers : Phase::connect);
            return CURL_PREREQFUNC_ABORT;
        }
        transfer.block_status = 0; transfer.block.clear(); transfer.block_bytes = 0;
    }
    // POSTTRANSFER_TIME accumulates across authentication attempts. A previous
    // challenge's completed request must not start this attempt's header timer.
    curl_easy_getinfo(transfer.easy, CURLINFO_POSTTRANSFER_TIME_T, &transfer.posttransfer_before_request);
    transfer.header_timer = transfer.write_timer = transfer.upload_transmitted = false;
    transfer.write_at = Clock::now();
    if (transfer.options.protocol != Protocol::http2_required) { transfer.request_permitted = true; return CURL_PREREQFUNC_OK; }
    curl_tlssessioninfo* session = nullptr;
    bool h2 = false;
    if (curl_easy_getinfo(transfer.easy, CURLINFO_TLS_SSL_PTR, &session) == CURLE_OK && session && session->internals) {
#ifdef _WIN32
        if (session->backend == CURLSSLBACKEND_SCHANNEL) {
            SecPkgContext_ApplicationProtocol selected{};
            if (QueryContextAttributesW(static_cast<PCtxtHandle>(session->internals), SECPKG_ATTR_APPLICATION_PROTOCOL, &selected) == SEC_E_OK)
                h2 = selected.ProtoNegoStatus == SecApplicationProtocolNegotiationStatus_Success &&
                     selected.ProtocolIdSize == 2 && std::memcmp(selected.ProtocolId, "h2", 2) == 0;
        }
#else
        if (session->backend == CURLSSLBACKEND_OPENSSL) {
            const unsigned char* selected = nullptr; unsigned length = 0;
            SSL_get0_alpn_selected(static_cast<SSL*>(session->internals), &selected, &length);
            h2 = length == 2 && selected && std::memcmp(selected, "h2", 2) == 0;
        }
#endif
    }
    if (h2) { transfer.request_permitted = true; return CURL_PREREQFUNC_OK; }
    // The documented pre-request hook runs after TLS, before GET/HEAD/POST or
    // h2 request bytes. Required h2 cannot rely on a later response-version check.
    transfer.error = {ErrorKind::unsupported, Phase::tls};
    return CURL_PREREQFUNC_ABORT;
}
}

void DriverState::configure(const std::shared_ptr<TransferState>& state) {
    auto& transfer = *state;
    auto& client = *transfer.client;
    // No TTL-only cache can bound retained native memory under an arbitrary
    // sequence of hostnames. The pinned c-ares adapter keeps at most 256
    // addresses per family. Limit retention to eight configured hops using the
    // supported multi-owned DNS cache trigger; preserve connections, live
    // resolutions, negative caching and ordinary TTL checks.
    if (client.dns_hops == 8) {
        const auto cleared = curl_multi_setopt(client.multi, CURLMOPT_NETWORK_CHANGED, CURLMNWC_CLEAR_DNS);
        if (cleared != CURLM_OK) {
            transfer.error = {ErrorKind::transport_failure, Phase::dns, cleared, false, NativeDomain::curl_multi};
            transfer.native_failure = cleared == CURLM_OUT_OF_MEMORY;
            transfer.detached = true; return;
        }
        client.dns_hops = 0;
    }
    ++client.dns_hops;
    transfer.request_permitted = false;
    auto* easy = curl_easy_init();
    if (!easy) throw std::bad_alloc();
    transfer.easy = easy;
    if (hooks && !hooks->dns_servers.empty()) option(easy, CURLOPT_DNS_SERVERS, hooks->dns_servers.c_str());
    option(easy, CURLOPT_PRIVATE, state.get()); option(easy, CURLOPT_NOSIGNAL, 1L);
    option(easy, CURLOPT_PROTOCOLS_STR, "http,https"); option(easy, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
    option(easy, CURLOPT_FOLLOWLOCATION, 0L);
    // The pinned profile compiles the netrc parser out. Its setopt case is
    // consequently absent; passing even IGNORED would return UNKNOWN_OPTION.
    option(easy, CURLOPT_SSL_VERIFYPEER, 1L); option(easy, CURLOPT_SSL_VERIFYHOST, 2L);
    option(easy, CURLOPT_PROXY_SSL_VERIFYPEER, 1L); option(easy, CURLOPT_PROXY_SSL_VERIFYHOST, 2L);
    option(easy, CURLOPT_SSLVERSION, static_cast<long>(CURL_SSLVERSION_TLSv1_2));
    option(easy, CURLOPT_PROXY_SSLVERSION, static_cast<long>(CURL_SSLVERSION_TLSv1_2));
    option(easy, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(transfer.options.connect_timeout_ms));
    option(easy, CURLOPT_HTTP_CONTENT_DECODING, 0L); option(easy, CURLOPT_HTTP_TRANSFER_DECODING, 1L);
    option(easy, CURLOPT_ACCEPT_ENCODING, transfer.options.decoding == Decoding::decoded ? "gzip, deflate" : nullptr);
    option(easy, CURLOPT_HTTP_VERSION, static_cast<long>(transfer.options.protocol == Protocol::http1 ? CURL_HTTP_VERSION_1_1 :
        transfer.options.protocol == Protocol::http2_required ? CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE : CURL_HTTP_VERSION_2TLS));
    option(easy, CURLOPT_IPRESOLVE, static_cast<long>(transfer.options.ip_family == IpFamily::v4 ? CURL_IPRESOLVE_V4 :
        transfer.options.ip_family == IpFamily::v6 ? CURL_IPRESOLVE_V6 : CURL_IPRESOLVE_WHATEVER));
    option(easy, CURLOPT_MAXAGE_CONN, static_cast<long>((client.options.idle_timeout_ms + 999) / 1000));
    option(easy, CURLOPT_MAXLIFETIME_CONN, static_cast<long>((client.options.connection_lifetime_ms + 999) / 1000));
    option(easy, CURLOPT_DNS_CACHE_TIMEOUT, static_cast<long>((client.options.dns_cache_ms + 999) / 1000));
    option(easy, CURLOPT_PIPEWAIT, 1L); option(easy, CURLOPT_SUPPRESS_CONNECT_HEADERS, 1L);
    option(easy, CURLOPT_USERAGENT, client.options.user_agent.c_str());
    option(easy, CURLOPT_URL, transfer.address.text.c_str());
    option(easy, CURLOPT_HEADERFUNCTION, header_callback); option(easy, CURLOPT_HEADERDATA, state.get());
    option(easy, CURLOPT_WRITEFUNCTION, write_callback); option(easy, CURLOPT_WRITEDATA, state.get());
    option(easy, CURLOPT_READFUNCTION, read_callback); option(easy, CURLOPT_READDATA, state.get());
    option(easy, CURLOPT_SEEKFUNCTION, seek_callback); option(easy, CURLOPT_SEEKDATA, state.get());
    option(easy, CURLOPT_NOPROGRESS, 0L); option(easy, CURLOPT_XFERINFOFUNCTION, progress_callback); option(easy, CURLOPT_XFERINFODATA, state.get());
    option(easy, CURLOPT_PREREQFUNCTION, prerequisite_callback); option(easy, CURLOPT_PREREQDATA, state.get());
    if (client.options.proxy.mode == ProxyOptions::direct) option(easy, CURLOPT_PROXY, "");
    else if (client.options.proxy.mode == ProxyOptions::explicit_proxy) {
        option(easy, CURLOPT_PROXY, client.options.proxy.endpoint.c_str());
        std::string bypass;
        for (const auto& entry : client.options.proxy.no_proxy) { if (!bypass.empty()) bypass += ','; bypass += entry; }
        option(easy, CURLOPT_NOPROXY, bypass.c_str());
    } else {
        const auto& proxy = transfer.address.scheme == "https" ? client.options.proxy.https_proxy : client.options.proxy.http_proxy;
        option(easy, CURLOPT_PROXY, proxy.empty() ? client.options.proxy.all_proxy.c_str() : proxy.c_str());
        option(easy, CURLOPT_NOPROXY, client.options.proxy.environment_no_proxy.c_str());
    }
    if (!client.options.tls.ca_bundle.empty()) {
        curl_blob ca{client.ca_material.data(), client.ca_material.size(), CURL_BLOB_NOCOPY};
        option(easy, CURLOPT_CAINFO_BLOB, &ca); option(easy, CURLOPT_CAINFO, nullptr); option(easy, CURLOPT_CAPATH, nullptr);
        option(easy, CURLOPT_SSL_OPTIONS, 0L);
        option(easy, CURLOPT_PROXY_CAINFO_BLOB, &ca); option(easy, CURLOPT_PROXY_CAINFO, nullptr); option(easy, CURLOPT_PROXY_CAPATH, nullptr);
        option(easy, CURLOPT_PROXY_SSL_OPTIONS, 0L);
    }
    if (client.options.tls.identity == TlsOptions::windows_store) {
        option(easy, CURLOPT_SSLCERT, client.options.tls.certificate.c_str());
        // A store lookup ignores this file format. If the provider cannot
        // allocate its store path, its legacy file fallback must never import
        // bytes from a same-named P12 file.
        option(easy, CURLOPT_SSLCERTTYPE, "PEM");
    } else if (client.options.tls.identity != TlsOptions::none) {
        curl_blob certificate{client.certificate_material.data(), client.certificate_material.size(), CURL_BLOB_NOCOPY};
        option(easy, CURLOPT_SSLCERT_BLOB, &certificate);
        option(easy, CURLOPT_SSLCERTTYPE, client.options.tls.identity == TlsOptions::pkcs12 ? "P12" : "PEM");
        if (client.options.tls.identity == TlsOptions::pem) {
            curl_blob key{client.key_material.data(), client.key_material.size(), CURL_BLOB_NOCOPY};
            option(easy, CURLOPT_SSLKEY_BLOB, &key); option(easy, CURLOPT_SSLKEYTYPE, "PEM");
        }
        option(easy, CURLOPT_KEYPASSWD, client.options.tls.password.c_str());
    }
    auto auth = [&](const Authentication& credentials, bool proxy) {
        if (credentials.kind == Authentication::none) return;
        option(easy, proxy ? CURLOPT_PROXYAUTH : CURLOPT_HTTPAUTH,
               static_cast<long>(credentials.kind == Authentication::basic ? CURLAUTH_BASIC : credentials.kind == Authentication::digest ? CURLAUTH_DIGEST : CURLAUTH_BEARER));
        if (credentials.kind == Authentication::bearer) option(easy, CURLOPT_XOAUTH2_BEARER, credentials.password.c_str());
        else {
            option(easy, proxy ? CURLOPT_PROXYUSERNAME : CURLOPT_USERNAME, credentials.user.c_str());
            option(easy, proxy ? CURLOPT_PROXYPASSWORD : CURLOPT_PASSWORD, credentials.password.c_str());
        }
    };
    auth(transfer.request.auth, false); auth(client.options.proxy.auth, true);
    if (client.options.cookies) { option(easy, CURLOPT_SHARE, client.share); option(easy, CURLOPT_COOKIEFILE, ""); }
    bool content_type = false;
    for (const auto& header : transfer.request.headers) {
        content_type |= lower(header.name) == "content-type";
        auto line = header.name + (header.value.empty() ? ";" : ": " + header.value);
        auto* list = curl_slist_append(transfer.outgoing, line.c_str()); if (!list) throw std::bad_alloc(); transfer.outgoing = list;
    }
    auto add_header = [&](const char* value) { auto* list = curl_slist_append(transfer.outgoing, value); if (!list) throw std::bad_alloc(); transfer.outgoing = list; };
    if (!content_type && transfer.request.body == Request::bytes) add_header("Content-Type: application/octet-stream");
    if (!content_type && transfer.request.body == Request::text) add_header("Content-Type: text/plain; charset=utf-8");
    if (transfer.request.body == Request::form) add_header("Content-Type: application/x-www-form-urlencoded");
    if (transfer.request.body == Request::multipart) {
        transfer.mime = curl_mime_init(easy); if (!transfer.mime) throw std::bad_alloc();
        for (std::size_t index = 0; index < transfer.request.parts.size(); ++index) {
            const auto& part = transfer.request.parts[index];
            auto* value = curl_mime_addpart(transfer.mime); if (!value) throw std::bad_alloc();
            if (curl_mime_name(value, part.name.c_str()) || curl_mime_filename(value, part.filename.c_str()) ||
                curl_mime_type(value, part.content_type.empty() ? nullptr : part.content_type.c_str())) throw std::bad_alloc();
            if (part.file) {
                if (curl_mime_data_cb(value, part.file_length, mime_read_callback, mime_seek_callback, nullptr,
                    &transfer.mime_readers[index])) throw std::bad_alloc();
            } else if (curl_mime_data(value, part.bytes.data(), part.bytes.size())) throw std::bad_alloc();
            curl_slist* fields = nullptr;
            for (const auto& field : part.headers) {
                auto* next = curl_slist_append(fields, (field.name + ": " + field.value).c_str());
                if (!next) { curl_slist_free_all(fields); throw std::bad_alloc(); } fields = next;
            }
            if (curl_mime_headers(value, fields, 1)) { curl_slist_free_all(fields); throw std::bad_alloc(); }
        }
        option(easy, CURLOPT_MIMEPOST, transfer.mime);
    } else if (transfer.request.body != Request::empty) {
        option(easy, CURLOPT_UPLOAD, 1L);
        option(easy, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(transfer.request.body == Request::stream ? transfer.request.stream_length :
            static_cast<std::int64_t>(transfer.request.data.size())));
    }
    option(easy, CURLOPT_CUSTOMREQUEST, transfer.request.method.c_str());
    if (transfer.request.method == "HEAD") option(easy, CURLOPT_NOBODY, 1L);
    option(easy, CURLOPT_HTTPHEADER, transfer.outgoing);
    const auto added = curl_multi_add_handle(client.multi, easy);
    if (added != CURLM_OK) throw std::runtime_error("HTTP multi admission failed");
    transfer.configured = true;
    transfer.header_timer = transfer.write_timer = transfer.upload_transmitted = false;
    transfer.posttransfer_before_request = 0;
    transfer.header_at = transfer.read_at = transfer.write_at = Clock::now();
}

void DriverState::detach(TransferState& transfer, CURLcode code, std::unique_lock<std::mutex>& lock) {
    if (transfer.detached) return;
    const auto timed_phase = code == CURLE_OPERATION_TIMEDOUT ? timeout_phase(transfer) : Phase::connect;
    long request_bytes = 0, connect_status = 0, proxy_verification = 0;
    if (transfer.easy) {
        long used_proxy = 0;
        curl_easy_getinfo(transfer.easy, CURLINFO_USED_PROXY, &used_proxy); transfer.used_proxy = used_proxy != 0;
        curl_easy_getinfo(transfer.easy, CURLINFO_REQUEST_SIZE, &request_bytes);
        // CONNECT/authentication with a proxy is not an origin request. The
        // prerequisite hook must have allowed origin bytes before their
        // reported request size can make the origin outcome unknown.
        transfer.request_sent |= transfer.request_permitted && request_bytes > 0;
        curl_easy_getinfo(transfer.easy, CURLINFO_HTTP_CONNECTCODE, &connect_status);
        curl_easy_getinfo(transfer.easy, CURLINFO_PROXY_SSL_VERIFYRESULT, &proxy_verification);
        curl_off_t uploaded = 0;
        curl_easy_getinfo(transfer.easy, CURLINFO_SIZE_UPLOAD_T, &uploaded);
        if (uploaded > std::numeric_limits<std::int64_t>::max() - transfer.uploaded_before_hop)
            transfer.fail(ErrorKind::limit_exceeded, Phase::upload);
        else transfer.uploaded = std::max<std::int64_t>(transfer.uploaded, transfer.uploaded_before_hop + uploaded);
        transfer.request_sent |= transfer.uploaded > 0;
        auto* easy = transfer.easy;
        transfer.easy = nullptr;
        const bool configured = transfer.configured;
        transfer.configured = false;
        // Removal invokes libcurl's final progress callback too. Never hold the
        // callback mutex across provider entry, including remove/cleanup.
        lock.unlock();
        if (configured) curl_multi_remove_handle(transfer.client->multi, easy);
        curl_easy_cleanup(easy);
        lock.lock();
    }
    if (transfer.mime) { curl_mime_free(transfer.mime); transfer.mime = nullptr; }
    curl_slist_free_all(transfer.outgoing); transfer.outgoing = nullptr;
    transfer.configured = false; transfer.detached = true;
    if (code == CURLE_OUT_OF_MEMORY) transfer.native_failure = true;
    if (!transfer.redirect_pending && transfer.error.kind == ErrorKind::success && code != CURLE_OK) {
        if (code == CURLE_ABORTED_BY_CALLBACK && (transfer.close_requested || transfer.client->closing || stopping))
            transfer.error = {ErrorKind::closed, Phase::cleanup, code, false, NativeDomain::curl};
        else if (connect_status >= 300) transfer.error = {ErrorKind::proxy_failed, Phase::proxy, code, false, NativeDomain::curl};
        else if (proxy_verification) transfer.error = {ErrorKind::tls_verification, Phase::proxy, code, false, NativeDomain::curl};
        else {
            transfer.error = classify(code);
            if (code == CURLE_OPERATION_TIMEDOUT) transfer.error.phase = timed_phase;
        }
    }
    transfer.error.remote_outcome_unknown = transfer.error.kind != ErrorKind::success && transfer.request_sent;
    if (!transfer.redirect_pending && !transfer.response_ready && transfer.block_status >= 200)
        finish_headers(transfer, true);
    if (!transfer.redirect_pending && !transfer.response_ready && transfer.error.kind == ErrorKind::success)
        transfer.fail(ErrorKind::protocol_error, Phase::headers);
}

void DriverState::cookies(ClientState& client, TransferState* active) {
    if (!client.options.cookies || !client.cookie_inspector) return;
    curl_slist* list = nullptr;
    auto code = curl_easy_getinfo(client.cookie_inspector, CURLINFO_COOKIELIST, &list);
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> owned(list, curl_slist_free_all);
    std::size_t count = 0, bytes = 0;
    bool exceeded = false;
    for (auto* item = list; item; item = item->next) {
        const auto length = std::strlen(item->data) + 1;
        if (++count > 1024 || length > mib - bytes) { exceeded = true; break; }
        bytes += length;
    }
    if (code != CURLE_OK || exceeded) {
        auto cleared = curl_easy_setopt(client.cookie_inspector, CURLOPT_COOKIELIST, "ALL");
        if (active) {
            active->fail(exceeded ? ErrorKind::limit_exceeded : ErrorKind::transport_failure, Phase::headers, code,
                code == CURLE_OK ? NativeDomain::none : NativeDomain::curl);
            active->native_failure |= code == CURLE_OUT_OF_MEMORY || cleared != CURLE_OK;
        } else if (code != CURLE_OK || cleared != CURLE_OK) throw CurlError{code};
    }
}

void DriverState::release(const std::shared_ptr<TransferState>& transfer) {
    {
        std::lock_guard lock(mutex);
        for (auto& slot : transfers) if (slot == transfer) {
            slot.reset();
            if (!transfer->client->admitted) std::terminate();
            --transfer->client->admitted;
            break;
        }
    }
    transfer->reservation_lease.reset();
}

void DriverState::service(const std::shared_ptr<TransferState>& state) {
    auto& transfer = *state;
    std::unique_lock lock(transfer.mutex);
    auto publish = [&](std::shared_ptr<Operation>& slot, Result&& result) {
        auto operation = std::move(slot);
        lock.unlock(); operation->complete(std::move(result)); lock.lock();
    };
    auto clear_storage = [&] {
        transfer.queues.reset();
        Request empty; std::swap(transfer.request, empty);
        transfer.info.reset();
        std::vector<Header>{}.swap(transfer.block); std::vector<Header>{}.swap(transfer.trailers);
        std::vector<RedirectInfo>{}.swap(transfer.history);
        std::string{}.swap(transfer.pending_write); transfer.buffered_body.clear();
    };
    if (transfer.closed && !transfer.queues) return;
    const bool producer_done = !transfer.source || transfer.source->done.load(std::memory_order_acquire);
    const bool sink_done = !transfer.sink || transfer.sink->done.load(std::memory_order_acquire);
    if (transfer.source && producer_done) {
        if (transfer.error.kind == ErrorKind::success && transfer.source->error.kind != ErrorKind::success)
            transfer.error = transfer.source->error;
        transfer.native_failure |= transfer.source->native_allocation_failure;
        if (transfer.source->native_allocation_failure) transfer.fail(ErrorKind::transport_failure, Phase::file);
    }
    bool cancelled = transfer.close_requested.load(std::memory_order_acquire) || transfer.client->closing || stopping;
    for (auto* slot : {&transfer.reader, &transfer.writer, &transfer.response_wait, &transfer.finish_wait, &transfer.send_wait})
        cancelled |= *slot && (*slot)->cancellation_.load(std::memory_order_acquire);
    cancelled |= transfer.lifetime_wait->cancellation_.load(std::memory_order_acquire);
    if (cancelled) {
        transfer.fail(ErrorKind::closed, Phase::cleanup);
        detach(transfer, CURLE_ABORTED_BY_CALLBACK, lock);
        transfer.closed = true;
    }
    if (transfer.source && (cancelled || transfer.upload_stopped || transfer.error.kind != ErrorKind::success))
        transfer.source->cancelled.store(true, std::memory_order_release);
    if (transfer.sink && (cancelled || transfer.error.kind != ErrorKind::success))
        transfer.sink->cancelled.store(true, std::memory_order_release);
    if (!transfer.detached && !transfer.configured) {
        try { configure(state); }
        catch (const CurlError& error) { transfer.error = classify(error.code); detach(transfer, error.code, lock); }
        catch (...) { transfer.native_failure = true; detach(transfer, CURLE_OUT_OF_MEMORY, lock); }
    }
    if (transfer.redirect_pending && transfer.detached && transfer.error.kind == ErrorKind::success) {
        const Header* location = nullptr;
        for (const auto& header : transfer.block) if (lower(header.name) == "location") location = &header;
        auto target = location ? url::resolve(transfer.address.text, location->value) : url::Result{};
        if (!location || target.error != url::ErrorKind::success) {
            transfer.fail(ErrorKind::invalid_url, Phase::headers);
            transfer.error.url_kind = target.error; transfer.error.url_offset = target.offset;
        }
        else if (transfer.history.size() >= static_cast<std::uint64_t>(transfer.options.max_redirects)) transfer.fail(ErrorKind::redirect_limit, Phase::headers);
        else {
            const auto from = origin(transfer.address), to = origin(target.address);
            const bool change = (transfer.block_status == 303 && transfer.request.method != "HEAD") ||
                ((transfer.block_status == 301 || transfer.block_status == 302) && transfer.request.method == "POST");
            const auto next_method = change ? std::string("GET") : transfer.request.method;
            bool credentials = transfer.request.auth.kind != Authentication::none || transfer.client->options.tls.identity != TlsOptions::none;
            for (const auto& header : transfer.request.headers) credentials |= header.sensitive;
            if ((transfer.address.scheme == "https" && target.address.scheme != "https") ||
                (from != to && (transfer.options.redirects == RedirectPolicy::same_origin || credentials || (next_method != "GET" && next_method != "HEAD"))))
                transfer.fail(ErrorKind::redirect_denied, Phase::headers);
            else if (!change && (transfer.request.body == Request::stream || transfer.source)) transfer.fail(ErrorKind::body_not_replayable, Phase::upload);
            else {
                transfer.history.push_back({transfer.block_status, from, to, change});
                if (from != to) transfer.request.headers.clear();
                if (change) {
                    transfer.upload_stopped |= transfer.request.body == Request::stream || bool(transfer.source);
                    if (transfer.source) transfer.source->cancelled.store(true, std::memory_order_release);
                    transfer.request.method = next_method; transfer.request.body = Request::empty; transfer.request.data.clear(); transfer.request.parts.clear();
                    std::erase_if(transfer.request.headers, [](const auto& header) { return lower(header.name).starts_with("content-"); });
                }
                transfer.address = std::move(target.address); transfer.block.clear(); transfer.block_bytes = 0; transfer.block_status = 0;
                transfer.redirect_pending = false; transfer.detached = false; transfer.data_position = 0; transfer.producer_eof = false;
                transfer.uploaded_before_hop = transfer.uploaded;
                transfer.header_timer = false;
                try { configure(state); }
                catch (const CurlError& error) { transfer.error = classify(error.code); detach(transfer, error.code, lock); }
                catch (...) { transfer.native_failure = true; detach(transfer, CURLE_OUT_OF_MEMORY, lock); }
            }
        }
    }
    // The total deadline includes decoder and consumer pauses after the socket
    // completes. Finishing the network request does not finish the transfer.
    if (expired(transfer.admitted_at, transfer.options.total_timeout_ms) &&
        (!transfer.detached || transfer.queues->encoded.size || transfer.queues->decoded.size || !transfer.decoder_finished))
        transfer.fail(ErrorKind::timeout, timeout_phase(transfer));
    if (!transfer.detached) {
        const bool write_active = transfer.request_permitted && !transfer.upload_transmitted && !transfer.upload_stopped &&
            transfer.request.body != Request::empty && (!transfer.send_paused || transfer.queues->upload.size);
        if (write_active && !transfer.write_timer) transfer.write_at = Clock::now();
        transfer.write_timer = write_active;
        if (transfer.header_timer && expired(transfer.header_at, transfer.options.header_timeout_ms)) transfer.fail(ErrorKind::timeout, Phase::headers);
        else if (transfer.response_ready && !transfer.receive_paused && transfer.queues->decoded.free() && transfer.queues->encoded.free() &&
                 expired(transfer.read_at, transfer.options.read_timeout_ms)) transfer.fail(ErrorKind::timeout, Phase::download);
        else if (transfer.write_timer && expired(transfer.write_at, transfer.options.write_timeout_ms)) transfer.fail(ErrorKind::timeout, Phase::upload);
        if (transfer.error.kind != ErrorKind::success) detach(transfer, CURLE_ABORTED_BY_CALLBACK, lock);
    }
    auto& input = transfer.queues->encoded;
    auto& output = transfer.queues->decoded;
    if (transfer.decoding && transfer.error.kind == ErrorKind::success) {
        while ((input.size || (transfer.detached && !transfer.member_end)) && output.free()) {
            if (transfer.member_end) {
                if (!transfer.gzip) { transfer.fail(ErrorKind::invalid_encoding, Phase::decode); break; }
                const auto reset = inflateReset2(&transfer.inflater, 31);
                if (reset != Z_OK) { transfer.native_failure = reset == Z_MEM_ERROR; transfer.fail(ErrorKind::invalid_encoding, Phase::decode, reset, NativeDomain::zlib); break; }
                transfer.member_end = false;
            }
            const auto readable = input.readable(), writable = output.writable();
            transfer.inflater.next_in = input.read_data(); transfer.inflater.avail_in = static_cast<uInt>(readable);
            transfer.inflater.next_out = output.write_data(); transfer.inflater.avail_out = static_cast<uInt>(writable);
            const auto status = inflate(&transfer.inflater, Z_NO_FLUSH);
            const auto consumed = readable - transfer.inflater.avail_in, produced = writable - transfer.inflater.avail_out;
            if (produced > static_cast<std::uint64_t>(transfer.options.max_decoded_bytes - transfer.decoded)) { transfer.fail(ErrorKind::limit_exceeded, Phase::decode); break; }
            input.consume(consumed); output.produce(produced); transfer.decoded += static_cast<std::int64_t>(produced);
            if (status == Z_STREAM_END) transfer.member_end = true;
            else if (status != Z_OK && status != Z_BUF_ERROR) {
                transfer.native_failure = status == Z_MEM_ERROR; transfer.fail(ErrorKind::invalid_encoding, Phase::decode, status, NativeDomain::zlib); break;
            }
            if (!consumed && !produced) break;
        }
        if (transfer.detached && !input.size) {
            if (transfer.member_end) transfer.decoder_finished = true;
            else if (output.free()) transfer.fail(ErrorKind::unexpected_eof, Phase::decode);
        }
    } else if (!transfer.decoding) transfer.decoder_finished = transfer.detached;
    if (transfer.error.kind != ErrorKind::success && !transfer.detached) detach(transfer, CURLE_ABORTED_BY_CALLBACK, lock);
    if (transfer.writer) {
        if (transfer.upload_stopped || (transfer.source && transfer.source->cancelled) || transfer.detached || transfer.closed || transfer.error.kind != ErrorKind::success) {
            auto result = transfer.result(); if (result.error.kind == ErrorKind::success) result.error = {ErrorKind::upload_stopped, Phase::upload};
            publish(transfer.writer, std::move(result)); transfer.pending_write.clear();
        } else if ((transfer.pending_part < 0 || transfer.current_file == static_cast<std::size_t>(transfer.pending_part)) &&
                   transfer.pending_write.size() <= transfer.queues->upload.free()) {
            transfer.queues->upload.append(transfer.pending_write.data(), transfer.pending_write.size());
            transfer.write_accepted += transfer.pending_write.size();
            if (transfer.pending_part >= 0) transfer.mime_readers[transfer.pending_part].accepted += static_cast<std::int64_t>(transfer.pending_write.size());
            auto result = transfer.result(); result.number = static_cast<std::int64_t>(transfer.pending_write.size());
            transfer.pending_write.clear(); publish(transfer.writer, std::move(result));
        }
    }
    if (transfer.response_wait && (transfer.response_ready || transfer.detached)) publish(transfer.response_wait, transfer.result());
    if (transfer.buffered && output.size) {
        if (output.size > static_cast<std::uint64_t>(transfer.options.max_buffer_bytes) - transfer.buffered_body.size()) {
            transfer.fail(ErrorKind::limit_exceeded, Phase::download); detach(transfer, CURLE_ABORTED_BY_CALLBACK, lock);
        } else {
            transfer.buffered_body.append(output, static_cast<std::size_t>(transfer.options.max_buffer_bytes));
        }
    }
    if (!transfer.closed && transfer.reader && (output.size || (transfer.detached && (!input.size || transfer.error.kind != ErrorKind::success)))) {
        auto result = transfer.result();
        if (output.size) {
            const auto count = std::min<std::size_t>(output.size, transfer.read_limit);
            result.bytes.resize(count); output.copy(result.bytes.data(), count); result.error = {};
        } else result.eof = transfer.error.kind == ErrorKind::success && transfer.decoder_finished;
        publish(transfer.reader, std::move(result));
    }
    if (!transfer.closed && transfer.finish_wait) {
        if (output.size || input.size) {
            auto result = transfer.result(); result.error = {ErrorKind::busy, Phase::download}; publish(transfer.finish_wait, std::move(result));
        } else if (producer_done && transfer.detached && (transfer.decoder_finished || transfer.error.kind != ErrorKind::success)) {
            auto result = transfer.result(); result.trailers = transfer.trailers; publish(transfer.finish_wait, std::move(result));
        }
    }
    if (producer_done && transfer.buffered && transfer.detached && (!input.size || transfer.error.kind != ErrorKind::success) && !output.size) {
        auto result = transfer.result();
        if (!transfer.native_failure) result.bytes = transfer.buffered_body.take();
        result.trailers = std::move(transfer.trailers);
        auto operation = std::move(transfer.send_wait); transfer.closed = true;
        Result clean; // private buffered lifetime has no independent publication
        clear_storage();
        // Socket/decoder/snapshot storage is gone. Keep a reservation for the
        // actual native result plus owner-side publication, rather than charging
        // every retained small result the maximum configured response budget.
        auto reservation = std::static_pointer_cast<Reservation>(transfer.reservation_lease);
        {
            std::lock_guard admission(mutex);
            const auto retained = std::min(reservation->bytes, 2 * mib + 2 * result.bytes.capacity());
            reserved -= reservation->bytes - retained; reservation->bytes = retained;
        }
        lock.unlock(); release(state);
        transfer.closed_wait->complete(Result(clean)); transfer.lifetime_wait->complete(std::move(clean));
        operation->complete(std::move(result));
        return;
    }
    if (!transfer.detached) {
        bool resume_receive = transfer.receive_paused && (transfer.decoding ? input.free() : output.free()) >= CURL_MAX_WRITE_SIZE;
        bool part_eof = transfer.current_file < transfer.request.parts.size() && transfer.mime_readers[transfer.current_file].eof;
        bool resume_send = transfer.send_paused && (transfer.queues->upload.size || part_eof || transfer.upload_finished.load(std::memory_order_acquire) || transfer.upload_stopped);
        if (resume_receive || resume_send) {
            if (resume_receive) { transfer.receive_paused = false; transfer.read_at = Clock::now(); }
            if (resume_send) { transfer.send_paused = false; transfer.write_at = Clock::now(); }
            const int pause = (transfer.receive_paused ? CURLPAUSE_RECV : 0) | (transfer.send_paused ? CURLPAUSE_SEND : 0);
            lock.unlock(); const auto code = curl_easy_pause(transfer.easy, pause); lock.lock();
            if (code != CURLE_OK) { transfer.error = classify(code); detach(transfer, code, lock); }
        }
    }
    if (transfer.closed && producer_done) {
        for (auto* slot : {&transfer.reader, &transfer.writer, &transfer.response_wait, &transfer.finish_wait, &transfer.send_wait})
            if (*slot) publish(*slot, transfer.result());
        if (!sink_done) return; // file IO still owns its full-operation borrow
        auto closed = transfer.closed_wait; auto lifetime = transfer.lifetime_wait;
        Result clean; clean.reservation = transfer.reservation_lease;
        clear_storage();
        lock.unlock(); release(state);
        closed->complete(Result(clean)); lifetime->complete(std::move(clean));
    }
}

void DriverState::run() noexcept {
    for (;;) {
        std::array<std::shared_ptr<ClientState>, 8> local_clients;
        std::array<std::shared_ptr<TransferState>, 16> local_transfers;
        { std::lock_guard lock(mutex); local_clients = clients; local_transfers = transfers; commands = false; }
        bool any = false;
        for (auto& client : local_clients) if (client) {
            any = true;
            if (!client->multi && !client->closing && !stopping) {
                Result opened;
                try {
                    client->multi = curl_multi_init(); if (!client->multi) throw std::bad_alloc();
                    auto set = [&](CURLMoption key, long value) { if (curl_multi_setopt(client->multi, key, value) != CURLM_OK) throw std::runtime_error("HTTP multi configuration failed"); };
                    set(CURLMOPT_MAX_TOTAL_CONNECTIONS, static_cast<long>(client->options.max_connections));
                    set(CURLMOPT_MAX_HOST_CONNECTIONS, static_cast<long>(client->options.max_per_origin));
                    set(CURLMOPT_MAXCONNECTS, static_cast<long>(client->options.max_idle));
                    set(CURLMOPT_MAX_CONCURRENT_STREAMS, static_cast<long>(client->options.max_http2_streams));
                    set(CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
                    if (client->options.cookies) {
                        client->share = curl_share_init(); client->cookie_inspector = curl_easy_init();
                        if (!client->share || !client->cookie_inspector) throw std::bad_alloc();
                        if (curl_share_setopt(client->share, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE) != CURLSHE_OK) throw std::runtime_error("HTTP cookie share initialization failed");
                        option(client->cookie_inspector, CURLOPT_SHARE, client->share); option(client->cookie_inspector, CURLOPT_COOKIEFILE, "");
                    }
                } catch (...) { opened.native_allocation_failure = true; client->closing = true; }
                client->opened->complete(std::move(opened));
            }
        }
        for (auto& transfer : local_transfers) if (transfer) {
            try { service(transfer); }
            catch (...) {
                { std::lock_guard lock(transfer->mutex); transfer->native_failure = true; transfer->close_requested = true; }
                try { service(transfer); } catch (...) { std::terminate(); }
            }
        }
        for (auto& client : local_clients) if (client && client->multi) {
            int running = 0;
            const auto code = curl_multi_perform(client->multi, &running);
            int messages = 0;
            while (auto* message = curl_multi_info_read(client->multi, &messages)) {
                if (message->msg != CURLMSG_DONE) continue;
                TransferState* transfer = nullptr; curl_easy_getinfo(message->easy_handle, CURLINFO_PRIVATE, &transfer);
                if (transfer) {
                    std::unique_lock lock(transfer->mutex);
                    try { detach(*transfer, message->data.result, lock); }
                    catch (...) { transfer->native_failure = true; transfer->close_requested = true; }
                }
            }
            if (code != CURLM_OK) {
                for (auto& transfer : local_transfers) if (transfer && transfer->client == client) {
                    std::lock_guard lock(transfer->mutex);
                    transfer->native_failure |= code == CURLM_OUT_OF_MEMORY;
                    transfer->fail(ErrorKind::transport_failure, Phase::download, code, NativeDomain::curl_multi);
                }
                client->closing = true;
            }
        }
        for (auto& client : local_clients) if (client) {
            std::shared_ptr<Operation> clear;
            { std::lock_guard lock(mutex); clear = std::move(client->clear_wait); }
            if (clear) {
                Result result;
                if (!client->cookie_inspector || client->closing) result.error = {ErrorKind::closed, Phase::cleanup};
                else { auto code = curl_easy_setopt(client->cookie_inspector, CURLOPT_COOKIELIST, "ALL"); if (code != CURLE_OK) result.error = classify(code); }
                clear->complete(std::move(result));
            }
            if (client->closing || stopping) {
                bool pending = false;
                { std::lock_guard lock(mutex);
                  for (const auto& transfer : transfers) if (transfer && transfer->client == client) pending = true; }
                if (pending) continue;
                if (client->cookie_inspector) { curl_easy_cleanup(client->cookie_inspector); client->cookie_inspector = nullptr; }
                if (client->share) { curl_share_cleanup(client->share); client->share = nullptr; }
                if (client->multi) { curl_multi_cleanup(client->multi); client->multi = nullptr; }
                // Closed public handles must not retain configuration blobs
                // after their native admission has been released.
                std::string{}.swap(client->ca_material);
                std::string{}.swap(client->certificate_material);
                std::string{}.swap(client->key_material);
                client->material_bytes = 0;
                { std::lock_guard lock(mutex); for (auto& slot : clients) if (slot == client) slot.reset(); reserved -= client->reservation; connection_reservations -= client->options.max_connections; }
                if (!client->opened->ready()) { Result value; value.error = {ErrorKind::closed, Phase::cleanup}; client->opened->complete(std::move(value)); }
                client->closed->complete(Result{});
            }
        }
        if (stopping) {
            std::lock_guard lock(mutex);
            if (std::none_of(clients.begin(), clients.end(), [](const auto& value) { return bool(value); })) return;
        }
        CURLM* wait = nullptr;
        unsigned wait_count = 0;
        int poll_timeout = 10;
        bool wait_error = false;
        for (auto& client : local_clients) if (client && client->multi && !client->closing) {
            long timeout = -1;
            auto status = curl_multi_timeout(client->multi, &timeout);
            if (status == CURLM_OK && timeout >= 0) poll_timeout = std::min(poll_timeout, static_cast<int>(std::min(timeout, 10L)));
            if (!wait) wait = client->multi;
            else if (status == CURLM_OK) {
                unsigned count = 0;
                status = curl_multi_waitfds(client->multi, nullptr, 0, &count);
                if (status == CURLM_OK && count > waitfds.size() - wait_count) {
                    for (auto& transfer : local_transfers) if (transfer && transfer->client == client) {
                        std::lock_guard lock(transfer->mutex); transfer->fail(ErrorKind::limit_exceeded, Phase::queue);
                    }
                    client->closing = true; wait_error = true; continue;
                }
                if (status == CURLM_OK && count) {
                    status = curl_multi_waitfds(client->multi, waitfds.data() + wait_count,
                        static_cast<unsigned>(waitfds.size() - wait_count), &count);
                    if (status == CURLM_OK) wait_count += count;
                }
            }
            if (status != CURLM_OK) {
                for (auto& transfer : local_transfers) if (transfer && transfer->client == client) {
                    std::lock_guard lock(transfer->mutex);
                    transfer->native_failure |= status == CURLM_OUT_OF_MEMORY;
                    transfer->fail(ErrorKind::transport_failure, Phase::connect, status, NativeDomain::curl_multi);
                }
                client->closing = true; wait_error = true;
            }
        }
        if (wait_error) continue;
        bool immediate = false;
        for (const auto& transfer : local_transfers) if (transfer) {
            std::lock_guard lock(transfer->mutex);
            if (transfer->queues) {
                immediate |= transfer->queues->decoded.size && (transfer->buffered || bool(transfer->reader));
                immediate |= transfer->error.kind == ErrorKind::success && transfer->queues->encoded.size && transfer->queues->decoded.free();
            }
            immediate |= transfer->detached && !transfer->closed && (bool(transfer->response_wait) || bool(transfer->finish_wait) || bool(transfer->reader) || bool(transfer->send_wait));
        }
        if (wait) {
            { std::lock_guard lock(mutex); if (commands || immediate) continue; polling = wait; }
            int activity = 0;
            const auto status = curl_multi_poll(wait, waitfds.data(), wait_count, poll_timeout, &activity);
            { std::lock_guard lock(mutex); polling = nullptr; }
            if (status != CURLM_OK) {
                for (auto& transfer : local_transfers) if (transfer) {
                    std::lock_guard lock(transfer->mutex);
                    transfer->native_failure |= status == CURLM_OUT_OF_MEMORY;
                    transfer->fail(ErrorKind::transport_failure, Phase::download, status, NativeDomain::curl_multi);
                }
                for (auto& client : local_clients) if (client) client->closing = true;
            }
        } else {
            std::unique_lock lock(mutex);
            if (!commands) changed.wait_for(lock, std::chrono::milliseconds(any ? 1 : 50));
        }
    }
}

namespace {
Error client_error(ClientOptions& options) {
    std::size_t snapshot = 0;
    for (const auto* value : {&options.user_agent, &options.tls.ca_bundle, &options.tls.certificate, &options.tls.key,
             &options.tls.password, &options.proxy.endpoint, &options.proxy.auth.user, &options.proxy.auth.password,
             &options.proxy.http_proxy, &options.proxy.https_proxy, &options.proxy.all_proxy, &options.proxy.environment_no_proxy}) {
        if (value->size() > mib - snapshot || !native_string(*value)) return {ErrorKind::limit_exceeded, Phase::prepare};
        snapshot += value->size();
    }
    if (options.proxy.no_proxy.size() > 1024) return {ErrorKind::limit_exceeded, Phase::proxy};
    for (const auto& value : options.proxy.no_proxy) {
        if (value.size() > mib - snapshot) return {ErrorKind::limit_exceeded, Phase::proxy};
        snapshot += value.size();
    }
    for (auto value : {options.max_active, options.max_connections, options.max_per_origin, options.max_http2_streams, options.max_idle})
        if (value < 1 || value > 4) return {ErrorKind::invalid_argument, Phase::prepare};
    if (options.max_per_origin > options.max_connections || options.max_idle > options.max_connections)
        return {ErrorKind::invalid_argument, Phase::prepare};
    for (auto value : {options.idle_timeout_ms, options.connection_lifetime_ms, options.dns_cache_ms})
        if (value < 1 || value > 2147483647) return {ErrorKind::invalid_argument, Phase::prepare};
    if (options.user_agent.size() > 16384 || !native_string(options.user_agent) || !field_bytes(options.user_agent) ||
        !native_string(options.tls.ca_bundle) || !native_string(options.tls.certificate) || !native_string(options.tls.key) || !native_string(options.tls.password))
        return {ErrorKind::invalid_argument, Phase::prepare};
    if (options.tls.identity < TlsOptions::none || options.tls.identity > TlsOptions::windows_store) return {ErrorKind::invalid_argument, Phase::prepare};
#ifdef _WIN32
    if (options.tls.identity == TlsOptions::pem) return {ErrorKind::unsupported, Phase::prepare};
    if (options.tls.identity == TlsOptions::windows_store) {
        const auto& reference = options.tls.certificate;
        const auto first = reference.find('\\');
        const auto second = first == std::string::npos ? first : reference.find('\\', first + 1);
        bool location = false;
        for (const auto name : {"CurrentUser", "LocalMachine", "CurrentService", "Services", "Users",
                               "CurrentUserGroupPolicy", "LocalMachineGroupPolicy", "LocalMachineEnterprise"})
            location = location || reference.substr(0, first) == name;
        if (!location || second == std::string::npos || second <= first + 1 || second - first > 513 ||
            reference.size() - second - 1 != 40 || reference.substr(first + 1, second - first - 1).find_first_of("/:") != std::string::npos ||
            !std::all_of(reference.begin() + static_cast<std::ptrdiff_t>(second + 1), reference.end(), [](unsigned char c) {
                return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
            })) return {ErrorKind::invalid_argument, Phase::prepare};
    }
#else
    if (options.tls.identity == TlsOptions::windows_store) return {ErrorKind::unsupported, Phase::prepare};
#endif
    if (options.tls.identity != TlsOptions::none && options.tls.certificate.empty()) return {ErrorKind::invalid_argument, Phase::prepare};
    if (options.tls.identity == TlsOptions::pem && options.tls.key.empty()) return {ErrorKind::invalid_argument, Phase::prepare};
    if (options.proxy.mode < ProxyOptions::direct || options.proxy.mode > ProxyOptions::explicit_proxy) return {ErrorKind::invalid_argument, Phase::prepare};
    // Proxy structural validation is shared with URL preparation; opaque values
    // are never passed through to ambient provider configuration unchecked.
    if (options.proxy.mode == ProxyOptions::explicit_proxy) {
        auto proxy = url::parse_proxy(options.proxy.endpoint);
        if (proxy.error != url::ErrorKind::success || proxy.address.has_query || proxy.address.has_fragment || proxy.address.path != "/")
            return url_failure(proxy, Phase::proxy);
        if (options.proxy.auth.kind != Authentication::none && proxy.address.scheme != "https") return {ErrorKind::invalid_argument, Phase::proxy};
        options.proxy.endpoint = std::move(proxy.address.text);
    }
    if (options.proxy.mode == ProxyOptions::environment) {
        for (auto* value : {&options.proxy.http_proxy, &options.proxy.https_proxy, &options.proxy.all_proxy}) if (!value->empty()) {
            auto proxy = url::parse_proxy(*value);
            if (proxy.error != url::ErrorKind::success) return url_failure(proxy, Phase::proxy);
            if (options.proxy.auth.kind != Authentication::none && proxy.address.scheme != "https") return {ErrorKind::invalid_argument, Phase::proxy};
            *value = std::move(proxy.address.text);
        }
        if (!field_bytes(options.proxy.environment_no_proxy)) return {ErrorKind::invalid_argument, Phase::proxy};
    }
    if (options.proxy.auth.kind == Authentication::bearer) return {ErrorKind::unsupported, Phase::proxy};
    if (options.proxy.auth.kind < Authentication::none || options.proxy.auth.kind > Authentication::digest)
        return {ErrorKind::invalid_argument, Phase::proxy};
    for (const auto& entry : options.proxy.no_proxy)
        if (!native_string(entry) || !field_bytes(entry) || entry.find(',') != std::string::npos) return {ErrorKind::invalid_argument, Phase::proxy};
    return {};
}
Error prepare_request(const ClientOptions& client, Request& request, const RequestOptions& options, url::Address& address, std::size_t& snapshot) {
    if (!token(request.method) || lower(request.method) == "connect" || lower(request.method) == "trace") return {ErrorKind::invalid_argument, Phase::prepare};
    if (request.body < Request::empty || request.body > Request::stream || request.stream_length < -1) return {ErrorKind::invalid_argument, Phase::prepare};
    if ((request.method == "GET" || request.method == "HEAD") && request.body != Request::empty) return {ErrorKind::invalid_argument, Phase::prepare};
    if (options.protocol < Protocol::negotiate || options.protocol > Protocol::http2_required || options.ip_family < IpFamily::any || options.ip_family > IpFamily::v6 ||
        options.redirects < RedirectPolicy::none || options.redirects > RedirectPolicy::safe || options.decoding < Decoding::decoded || options.decoding > Decoding::raw ||
        options.max_redirects < 0 || options.max_redirects > 64 || options.connect_timeout_ms <= 0) return {ErrorKind::invalid_argument, Phase::prepare};
    for (auto value : {options.connect_timeout_ms, options.header_timeout_ms, options.read_timeout_ms, options.write_timeout_ms, options.total_timeout_ms})
        if (value < 0 || value > 2147483647) return {ErrorKind::invalid_argument, Phase::prepare};
    for (auto value : {options.max_encoded_bytes, options.max_decoded_bytes, options.max_buffer_bytes})
        if (value <= 0) return {ErrorKind::invalid_argument, Phase::prepare};
    auto parsed = url::parse(request.target);
    if (parsed.error != url::ErrorKind::success) return url_failure(parsed, Phase::prepare);
    address = std::move(parsed.address);
    if (options.protocol == Protocol::http2_required && address.scheme != "https") return {ErrorKind::unsupported, Phase::prepare};
    if (request.auth.kind < Authentication::none || request.auth.kind > Authentication::digest ||
        !native_string(request.auth.user) || !native_string(request.auth.password) || request.auth.user.size() + request.auth.password.size() > 16384)
        return {ErrorKind::invalid_argument, Phase::prepare};
    if (request.auth.kind != Authentication::none && address.scheme != "https") return {ErrorKind::invalid_argument, Phase::prepare};
    if (request.auth.kind == Authentication::bearer && !field_bytes(request.auth.password)) return {ErrorKind::invalid_header, Phase::prepare};
    if (request.data.size() > 64 * mib || request.headers.size() > 128 || request.parts.size() > 128) return {ErrorKind::limit_exceeded, Phase::prepare};
    snapshot = request.data.size() + request.target.size() + request.method.size() + request.auth.user.size() + request.auth.password.size();
    std::size_t header_bytes = 0, content_types = 0;
    for (const auto& header : request.headers) {
        const auto name = lower(header.name);
        if (!valid_header(header)) return {ErrorKind::invalid_header, Phase::prepare};
        for (auto forbidden : {"host", "content-length", "transfer-encoding", "connection", "proxy-connection", "upgrade", "trailer", "te", "authorization", "cookie", "proxy-authorization"})
            if (name == forbidden) return {ErrorKind::invalid_header, Phase::prepare};
        if (name == "content-type") ++content_types;
        if (header.name.size() + header.value.size() + 4 > header_block_bytes - header_bytes) return {ErrorKind::limit_exceeded, Phase::prepare};
        header_bytes += header.name.size() + header.value.size() + 4;
    }
    if (content_types > 1 || (content_types && (request.body == Request::form || request.body == Request::multipart))) return {ErrorKind::invalid_header, Phase::prepare};
    snapshot += header_bytes;
    std::int64_t file_lengths = 0;
    for (const auto& part : request.parts) {
        if (part.file) {
            if (request.body != Request::multipart || !request.source || part.file_length < -1 || !part.bytes.empty())
                return {ErrorKind::invalid_argument, Phase::prepare};
            if (part.file_length >= 0) {
                if (part.file_length > std::numeric_limits<std::int64_t>::max() - file_lengths) return {ErrorKind::limit_exceeded, Phase::prepare};
                file_lengths += part.file_length;
            }
        }
        if (!native_string(part.name) || !native_string(part.filename) || !native_string(part.content_type) ||
            !field_bytes(part.name) || !field_bytes(part.filename) || !field_bytes(part.content_type) || part.headers.size() > 128)
            return {ErrorKind::invalid_header, Phase::prepare};
        if (part.bytes.size() > 64 * mib - std::min(snapshot, 64 * mib)) return {ErrorKind::limit_exceeded, Phase::prepare};
        snapshot += part.bytes.size() + part.name.size() + part.filename.size() + part.content_type.size();
        for (const auto& header : part.headers) {
            if (!valid_header(header)) return {ErrorKind::invalid_header, Phase::prepare};
            auto name = lower(header.name);
            if (name == "content-disposition" || name == "content-type" || name == "content-length" || name == "transfer-encoding")
                return {ErrorKind::invalid_header, Phase::prepare};
            if (header.value.size() + header.name.size() > 64 * mib - std::min(snapshot, 64 * mib)) return {ErrorKind::limit_exceeded, Phase::prepare};
            snapshot += header.value.size() + header.name.size();
        }
    }
    if (snapshot > 64 * mib || client.max_active > 4) return {ErrorKind::limit_exceeded, Phase::prepare};
    return {};
}
}

Driver::Driver() : Driver(std::shared_ptr<const testing::Hooks>{}) {}
Driver::Driver(std::shared_ptr<const testing::Hooks> hooks) : state_(std::make_shared<DriverState>()) {
    state_->hooks = std::move(hooks);
    state_->thread = std::thread([state = state_.get()] { state->run(); });
}
Driver::~Driver() { shutdown(); }
void Driver::request_stop() noexcept { state_->stopping.store(true, std::memory_order_release); state_->wake(); }
void Driver::notify_source() noexcept { state_->wake(); }
void Driver::shutdown() noexcept {
    request_stop();
    std::lock_guard lock(shutdown_mutex_);
    if (state_->thread.joinable()) state_->thread.join();
}
Client::~Client() { if (state_) { state_->closing = true; if (auto driver = state_->driver.lock()) driver->wake(); } }
Transfer::~Transfer() { if (state_) { state_->close_requested = true; if (auto driver = state_->driver.lock()) driver->wake(); } }

std::shared_ptr<Client> Driver::open(ClientOptions options, Error& error) {
    error = client_error(options); if (error.kind != ErrorKind::success) return {};
    auto client = std::make_shared<ClientState>(); client->options = std::move(options); client->driver = state_;
    auto handle = std::shared_ptr<Client>(new Client(client));
    auto material = [&](const std::string& path, std::string& target, ErrorKind kind) {
        auto input = filesystem::read_regular(path, static_cast<std::int64_t>(mib - client->material_bytes));
        if (input.error != filesystem::ErrorKind::success) {
            error = {input.error == filesystem::ErrorKind::limit_exceeded ? ErrorKind::limit_exceeded : kind,
                Phase::prepare, input.os_code, false, NativeDomain::filesystem};
            return false;
        }
        client->material_bytes += input.bytes.size(); target = std::move(input.bytes); return true;
    };
    if (!client->options.tls.ca_bundle.empty() && !material(client->options.tls.ca_bundle, client->ca_material, ErrorKind::tls_verification)) return {};
    if (client->options.tls.identity != TlsOptions::none && client->options.tls.identity != TlsOptions::windows_store) {
        if (!material(client->options.tls.certificate, client->certificate_material, ErrorKind::tls_failure)) return {};
        if (client->options.tls.identity == TlsOptions::pem && !material(client->options.tls.key, client->key_material, ErrorKind::tls_failure)) return {};
    }
    client->reservation = static_cast<std::size_t>(client->options.max_connections) * 12 * mib + 8 * mib;
    // A deliberately conservative provisional parsing allowance per connection,
    // in addition to the owned bytes. This is admission, not an RSS ceiling;
    // malformed-ASN.1/system-store envelopes remain part of the provider audit.
    client->reservation += client->material_bytes * (64 * static_cast<std::size_t>(client->options.max_connections) + 1);
    {
        std::lock_guard lock(state_->mutex);
        auto slot = std::find(state_->clients.begin(), state_->clients.end(), nullptr);
        if (state_->stopping || slot == state_->clients.end()) { error = {ErrorKind::busy, Phase::queue}; return {}; }
        if (client->reservation > memory_budget - state_->reserved || static_cast<std::size_t>(client->options.max_connections) > 16 - state_->connection_reservations) {
            error = {ErrorKind::limit_exceeded, Phase::queue}; return {};
        }
        state_->reserved += client->reservation; state_->connection_reservations += client->options.max_connections; *slot = client;
    }
    state_->wake(); const auto& opened = client->opened->wait();
    if (opened.native_allocation_failure) throw std::bad_alloc();
    if (opened.error.kind != ErrorKind::success) { error = opened.error; return {}; }
    return handle;
}
std::shared_ptr<Transfer> Driver::start(const std::shared_ptr<Client>& client, Request request, RequestOptions options, Error& error) {
    return admit(client, std::move(request), options, error, false);
}
std::shared_ptr<Transfer> Driver::start_buffered_upload(const std::shared_ptr<Client>& client, Request request, RequestOptions options, Error& error) {
    if (request.body != Request::stream && !(request.body == Request::multipart && request.source)) { error = {ErrorKind::invalid_argument, Phase::prepare}; return {}; }
    return admit(client, std::move(request), options, error, true);
}
std::shared_ptr<Operation> Driver::buffered_result(const std::shared_ptr<Transfer>& handle) {
    if (!handle || !handle->state_ || handle->state_->driver.lock() != state_) return failed_operation({ErrorKind::closed, Phase::prepare});
    std::lock_guard lock(handle->state_->mutex);
    if (!handle->state_->buffered) return failed_operation({ErrorKind::invalid_argument, Phase::prepare});
    return handle->state_->send_operation;
}
std::shared_ptr<Transfer> Driver::admit(const std::shared_ptr<Client>& client, Request request, RequestOptions options, Error& error, bool buffered) {
    if (!client || client->state_->driver.lock() != state_) { error = {ErrorKind::closed, Phase::prepare}; return {}; }
    url::Address address; std::size_t snapshot = 0;
    error = prepare_request(client->state_->options, request, options, address, snapshot);
    if (error.kind != ErrorKind::success) return {};
    auto transfer = std::make_shared<TransferState>(); transfer->driver = state_; transfer->client = client->state_;
    transfer->request = std::move(request); transfer->options = options; transfer->address = std::move(address);
    transfer->source = transfer->request.source;
    for (std::size_t index = 0; index < transfer->request.parts.size(); ++index) if (transfer->request.parts[index].file) {
        transfer->mime_readers[index].owner = transfer.get(); transfer->mime_readers[index].index = index;
        if (transfer->current_file == 128) transfer->current_file = index;
    }
    transfer->reservation = 16 * mib + snapshot * 2;
    transfer->buffered = buffered;
    if (buffered) {
        if (options.max_buffer_bytes <= 0 || static_cast<std::uint64_t>(options.max_buffer_bytes) > memory_budget / 2) {
            error = {ErrorKind::limit_exceeded, Phase::prepare}; return {};
        }
        transfer->reservation += static_cast<std::size_t>(options.max_buffer_bytes) * 2;
        transfer->send_operation = std::make_shared<Operation>(); transfer->send_operation->driver_ = state_;
        transfer->send_operation->transfer_ = transfer;
        transfer->send_wait = transfer->send_operation;
    }
    transfer->closed_wait->driver_ = state_; transfer->lifetime_wait->driver_ = state_;
    transfer->closed_wait->transfer_ = transfer; transfer->lifetime_wait->transfer_ = transfer;
    auto handle = std::shared_ptr<Transfer>(new Transfer(transfer));
    // Allocate the token before taking the registry lock; destroying a failed
    // admission must never reenter that lock or release an unreserved amount.
    auto reservation = std::make_shared<Reservation>(state_, 0);
    {
        std::lock_guard lock(state_->mutex);
        auto slot = std::find(state_->transfers.begin(), state_->transfers.end(), nullptr);
        if (state_->stopping || client->state_->closing) { error = {ErrorKind::closed, Phase::queue}; return {}; }
        if (slot == state_->transfers.end() || client->state_->admitted >= static_cast<std::size_t>(client->state_->options.max_active)) {
            error = {ErrorKind::busy, Phase::queue}; return {};
        }
        if (transfer->reservation > memory_budget - state_->reserved) { error = {ErrorKind::limit_exceeded, Phase::queue}; return {}; }
        state_->reserved += transfer->reservation; reservation->bytes = transfer->reservation;
        transfer->reservation_lease = reservation; ++client->state_->admitted; *slot = transfer;
    }
    state_->wake(); return handle;
}
std::shared_ptr<Operation> Driver::send(const std::shared_ptr<Client>& client, Request request, RequestOptions options) {
    if (request.body == Request::stream || request.source) return failed_operation({ErrorKind::invalid_argument, Phase::prepare});
    Error error;
    auto handle = admit(client, std::move(request), options, error, true);
    if (!handle) return failed_operation(error);
    auto operation = handle->state_->send_operation;
    // This is a private buffered transfer, not a source resource factory. Only
    // the driver/operation own its native state after successful admission.
    handle->state_.reset(); handle.reset(); state_->wake(); return operation;
}
std::shared_ptr<Operation> Driver::response(const std::shared_ptr<Transfer>& handle) {
    if (!handle || !handle->state_ || handle->state_->driver.lock() != state_) return failed_operation({ErrorKind::closed, Phase::prepare});
    auto transfer = handle->state_; std::lock_guard lock(transfer->mutex);
    if (transfer->closed) return failed_operation({ErrorKind::closed, Phase::prepare});
    if (transfer->response_wait) return failed_operation({ErrorKind::busy, Phase::headers});
    auto operation = std::make_shared<Operation>(); operation->driver_ = state_; operation->transfer_ = transfer; transfer->response_wait = operation; state_->wake(); return operation;
}
std::shared_ptr<Operation> Driver::read(const std::shared_ptr<Transfer>& handle, std::int64_t max_count) {
    if (max_count <= 0 || max_count > static_cast<std::int64_t>(queue_bytes)) return failed_operation({ErrorKind::invalid_argument, Phase::prepare});
    if (!handle || !handle->state_ || handle->state_->driver.lock() != state_) return failed_operation({ErrorKind::closed, Phase::prepare});
    auto transfer = handle->state_; std::lock_guard lock(transfer->mutex);
    if (transfer->closed) return failed_operation({ErrorKind::closed, Phase::prepare});
    if (transfer->reader) return failed_operation({ErrorKind::busy, Phase::download});
    auto operation = std::make_shared<Operation>(); operation->driver_ = state_; operation->transfer_ = transfer; transfer->reader = operation; transfer->read_limit = max_count; state_->wake(); return operation;
}
std::shared_ptr<Operation> Driver::write(const std::shared_ptr<Transfer>& handle, std::string bytes) {
    if (bytes.size() > queue_bytes) return failed_operation({ErrorKind::invalid_argument, Phase::prepare});
    if (!handle || !handle->state_ || handle->state_->driver.lock() != state_) return failed_operation({ErrorKind::closed, Phase::prepare});
    auto transfer = handle->state_; std::lock_guard lock(transfer->mutex);
    if (transfer->closed) return failed_operation({ErrorKind::closed, Phase::prepare});
    if (transfer->upload_stopped || (transfer->source && transfer->source->cancelled)) return failed_operation({ErrorKind::upload_stopped, Phase::upload});
    if (transfer->request.body != Request::stream || transfer->upload_finished) return failed_operation({ErrorKind::invalid_argument, Phase::upload});
    if (transfer->writer) return failed_operation({ErrorKind::busy, Phase::upload});
    if (transfer->request.stream_length >= 0 && bytes.size() > static_cast<std::uint64_t>(transfer->request.stream_length) - transfer->write_accepted)
        return failed_operation({ErrorKind::invalid_argument, Phase::upload});
    auto operation = std::make_shared<Operation>(); operation->driver_ = state_; operation->transfer_ = transfer; transfer->writer = operation; transfer->pending_part = -1; transfer->pending_write = std::move(bytes); state_->wake(); return operation;
}
std::shared_ptr<Operation> Driver::write_part(const std::shared_ptr<Transfer>& handle, std::size_t index, std::string bytes) {
    if (bytes.size() > queue_bytes) return failed_operation({ErrorKind::invalid_argument, Phase::prepare});
    if (!handle || !handle->state_ || handle->state_->driver.lock() != state_) return failed_operation({ErrorKind::closed, Phase::prepare});
    auto transfer = handle->state_; std::lock_guard lock(transfer->mutex);
    if (transfer->closed) return failed_operation({ErrorKind::closed, Phase::prepare});
    if (transfer->upload_stopped || (transfer->source && transfer->source->cancelled)) return failed_operation({ErrorKind::upload_stopped, Phase::upload});
    if (transfer->request.body != Request::multipart || index >= transfer->request.parts.size() || !transfer->request.parts[index].file)
        return failed_operation({ErrorKind::invalid_argument, Phase::upload});
    auto& reader = transfer->mime_readers[index];
    if (reader.eof) return failed_operation({ErrorKind::invalid_argument, Phase::upload});
    if (transfer->writer) return failed_operation({ErrorKind::busy, Phase::upload});
    const auto length = transfer->request.parts[index].file_length;
    if (length >= 0 && bytes.size() > static_cast<std::uint64_t>(length - reader.accepted))
        return failed_operation({ErrorKind::invalid_argument, Phase::upload});
    auto operation = std::make_shared<Operation>(); operation->driver_ = state_; operation->transfer_ = transfer;
    transfer->writer = operation; transfer->pending_part = static_cast<std::int64_t>(index); transfer->pending_write = std::move(bytes);
    state_->wake(); return operation;
}
Error Driver::finish_part(const std::shared_ptr<Transfer>& handle, std::size_t index) {
    if (!handle || !handle->state_ || handle->state_->driver.lock() != state_) return {ErrorKind::closed, Phase::prepare};
    auto transfer = handle->state_; std::lock_guard lock(transfer->mutex);
    if (transfer->closed) return {ErrorKind::closed, Phase::prepare};
    if (transfer->upload_stopped || (transfer->source && transfer->source->cancelled)) return {ErrorKind::upload_stopped, Phase::upload};
    if (transfer->request.body != Request::multipart || index >= transfer->request.parts.size() || !transfer->request.parts[index].file)
        return {ErrorKind::invalid_argument, Phase::upload};
    if (transfer->writer) return {ErrorKind::busy, Phase::upload};
    auto& reader = transfer->mime_readers[index]; const auto length = transfer->request.parts[index].file_length;
    reader.eof = true;
    if (length >= 0 && reader.accepted != length) transfer->fail(ErrorKind::unexpected_eof, Phase::upload);
    if (!length) advance_file(*transfer, index);
    state_->wake(); return transfer->error;
}
Error Driver::finish_upload(const std::shared_ptr<Transfer>& handle) {
    if (!handle || !handle->state_ || handle->state_->driver.lock() != state_) return {ErrorKind::closed, Phase::prepare};
    auto transfer = handle->state_; std::lock_guard lock(transfer->mutex);
    if (transfer->closed) return {ErrorKind::closed, Phase::prepare};
    if (transfer->upload_stopped || (transfer->source && transfer->source->cancelled)) return {ErrorKind::upload_stopped, Phase::upload};
    if (transfer->request.body != Request::stream || transfer->writer) return {ErrorKind::busy, Phase::upload};
    if (transfer->request.stream_length >= 0 && transfer->write_accepted != static_cast<std::uint64_t>(transfer->request.stream_length)) {
        transfer->fail(ErrorKind::unexpected_eof, Phase::upload); state_->wake(); return transfer->error;
    }
    transfer->upload_finished = true; state_->wake(); return {};
}
std::shared_ptr<Operation> Driver::finish(const std::shared_ptr<Transfer>& handle) {
    if (!handle || !handle->state_ || handle->state_->driver.lock() != state_) return failed_operation({ErrorKind::closed, Phase::prepare});
    auto transfer = handle->state_; std::lock_guard lock(transfer->mutex);
    if (transfer->closed) return failed_operation({ErrorKind::closed, Phase::prepare});
    if (transfer->finish_wait) return failed_operation({ErrorKind::busy, Phase::cleanup});
    auto operation = std::make_shared<Operation>(); operation->driver_ = state_; operation->transfer_ = transfer; transfer->finish_wait = operation; state_->wake(); return operation;
}
std::shared_ptr<Operation> Driver::close(const std::shared_ptr<Transfer>& handle) {
    if (!handle || !handle->state_) { auto value = std::make_shared<Operation>(); value->complete(Result{}); return value; }
    if (handle->state_->driver.lock() != state_) return failed_operation({ErrorKind::closed, Phase::prepare});
    handle->state_->close_requested = true; state_->wake(); return handle->state_->closed_wait;
}
std::shared_ptr<Operation> Driver::close(const std::shared_ptr<Client>& client) {
    if (!client) { auto value = std::make_shared<Operation>(); value->complete(Result{}); return value; }
    if (client->state_->driver.lock() != state_) return failed_operation({ErrorKind::closed, Phase::prepare});
    client->state_->closing = true; state_->wake(); return client->state_->closed;
}
std::shared_ptr<Operation> Driver::lifetime(const std::shared_ptr<Transfer>& handle) {
    if (!handle || !handle->state_ || handle->state_->driver.lock() != state_) return failed_operation({ErrorKind::closed, Phase::prepare});
    return handle->state_->lifetime_wait;
}
std::shared_ptr<Operation> Driver::file_result(const std::shared_ptr<Transfer>& handle) {
    if (!handle || !handle->state_ || handle->state_->driver.lock() != state_) return failed_operation({ErrorKind::closed, Phase::prepare});
    auto operation = std::make_shared<Operation>(); operation->driver_ = state_; operation->transfer_ = handle->state_;
    return operation;
}
void Driver::attach_file_sink(const std::shared_ptr<Transfer>& handle, const std::shared_ptr<UploadSource>& sink) {
    if (!handle || !handle->state_ || handle->state_->driver.lock() != state_ || !sink) throw std::invalid_argument("Invalid native file sink");
    auto transfer = handle->state_; std::lock_guard lock(transfer->mutex);
    if (transfer->buffered) throw std::invalid_argument("File output requires a streaming transfer");
    if (transfer->sink) throw std::logic_error("Duplicate native file sink");
    transfer->sink = sink;
    if (transfer->closed) sink->cancelled = true;
}
Error Driver::clear_cookies(const std::shared_ptr<Client>& client) {
    if (!client || client->state_->driver.lock() != state_) return {ErrorKind::closed, Phase::prepare};
    auto operation = std::make_shared<Operation>();
    {
        std::lock_guard lock(state_->mutex);
        if (client->state_->closing) return {ErrorKind::closed, Phase::cleanup};
        if (client->state_->admitted || client->state_->clear_wait) return {ErrorKind::busy, Phase::cleanup};
        if (!client->state_->options.cookies) return {};
        client->state_->clear_wait = operation;
    }
    state_->wake(); return operation->wait().error;
}
Driver::Snapshot Driver::snapshot() const {
    Snapshot snapshot;
    std::array<std::shared_ptr<TransferState>, 16> transfers;
    { std::lock_guard lock(state_->mutex); transfers = state_->transfers; snapshot.reserved_bytes = state_->reserved;
      snapshot.clients = std::count_if(state_->clients.begin(), state_->clients.end(), [](const auto& value) { return bool(value); }); }
    for (const auto& transfer : transfers) if (transfer) {
        std::lock_guard lock(transfer->mutex); ++snapshot.transfers;
        if (transfer->queues) { snapshot.encoded_queued += transfer->queues->encoded.size; snapshot.decoded_queued += transfer->queues->decoded.size; snapshot.upload_queued += transfer->queues->upload.size; }
    }
    return snapshot;
}
}
