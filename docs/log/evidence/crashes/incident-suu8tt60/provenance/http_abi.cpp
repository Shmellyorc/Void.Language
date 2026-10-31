#include "void/http_abi.hpp"
#include "void/http_resource.hpp"
#include "void/http_file.hpp"
#include "void/http_bulk.hpp"
#include "void/http_provider.hpp"
#include "void/filesystem_abi.hpp"
#include "void/os.hpp"
#include <cstring>
#include <cstdlib>
#include <map>
#include <stdexcept>

namespace vd::http {
namespace {
constexpr auto index(SourceRecord record) { return static_cast<unsigned>(record); }
struct Rejected { ErrorKind kind; filesystem::ErrorKind io_kind = filesystem::ErrorKind::success; };
class Record {
    const NativeArrayElement& geometry_;
    std::vector<std::byte> bytes_;
    std::vector<std::size_t> references_;
    Runtime::Slot slot_;
    Runtime::Frame roots_;
    static std::vector<std::size_t> offsets(const NativeArrayElement& geometry) {
        std::vector<std::size_t> result;
        for (unsigned n = 0; n < geometry.count; ++n) if (geometry.fields[n].kind >= 11) result.push_back(geometry.fields[n].offset);
        return result;
    }
public:
    Record(Runtime& runtime, const NativeArrayElement& geometry, const void* input = nullptr)
        : geometry_(geometry), bytes_(geometry.bytes), references_(offsets(geometry)),
          slot_{bytes_.data(), bytes_.size(), references_.data(), references_.size()}, roots_(runtime, &slot_, 1) {
        if (input) std::memcpy(bytes_.data(), input, bytes_.size());
    }
    void* data() { return bytes_.data(); }
    std::size_t size() const { return bytes_.size(); }
    void** reference(unsigned n) { return reinterpret_cast<void**>(bytes_.data() + geometry_.fields[n].offset); }
    template<class T = std::int64_t> T get(unsigned n) const {
        T result; std::memcpy(&result, bytes_.data() + geometry_.fields[n].offset, sizeof(result)); return result;
    }
    template<class T> void set(unsigned n, T value) { std::memcpy(bytes_.data() + geometry_.fields[n].offset, &value, sizeof(value)); }
    void publish(Runtime& runtime, void* task, std::size_t base) {
        std::memcpy(static_cast<char*>(task) + base, bytes_.data(), bytes_.size());
        for (auto offset : references_) {
            void* value; std::memcpy(&value, bytes_.data() + offset, sizeof(value)); runtime.store(task, base + offset, value);
        }
    }
};
const NativeArrayElement& geometry(const SourceLayout& layout, SourceRecord record) {
    auto* value = layout.records[index(record)];
    if (!value || value->count != source_records[index(record)].fields.size() || value->bytes > 4096)
        throw std::logic_error("Invalid HTTP source geometry");
    for (unsigned n = 0; n < value->count; ++n) {
        const char code = source_records[index(record)].fields[n];
        const unsigned expected = code == 'n' ? 3 : code == 'b' ? 10 : code == 't' ? 12 : code == 'f' ? 11 : 15;
        const auto& field = value->fields[n]; const auto bytes = code == 'b' ? sizeof(bool) : sizeof(std::int64_t);
        if (field.kind != expected || field.offset > value->bytes || bytes > value->bytes - field.offset)
            throw std::logic_error("Invalid HTTP source field geometry");
    }
    return *value;
}
class Snapshot {
    Runtime& runtime_;
    const SourceLayout& layout_;
    std::size_t remaining_;
    void charge(std::size_t size) { if (size > remaining_) throw Rejected{ErrorKind::limit_exceeded}; remaining_ -= size; }
public:
    std::map<void*, std::shared_ptr<filesystem::FileBorrow>> borrows;
    std::vector<UploadFile> files;
    Snapshot(Runtime& runtime, const SourceLayout& layout, std::size_t limit) : runtime_(runtime), layout_(layout), remaining_(limit) {}
    std::string text(Record& record, unsigned field) {
        auto text = runtime_.string_bytes(*record.reference(field)); charge(text.size()); return std::string(text);
    }
    std::string bytes(Record& record, unsigned field) {
        charge(runtime_.list_count(runtime_.root_word(record.reference(field)), ScalarEncoding::u8));
        return runtime_.byte_array_bytes(runtime_.root_word(record.reference(field)));
    }
    std::vector<Header> headers(Record& record, unsigned field) {
        const auto& header = geometry(layout_, SourceRecord::header);
        auto array = runtime_.native_array_layout(header, 0);
        const auto count = runtime_.array_count(runtime_.root_word(record.reference(field)), array, true);
        if (count > 128) throw Rejected{ErrorKind::limit_exceeded};
        std::vector<Header> result; result.reserve(count); Record item(runtime_, header);
        for (std::uint64_t n = 0; n < count; ++n) {
            runtime_.array_read(item.data(), runtime_.root_word(record.reference(field)), array, true, n);
            result.push_back({text(item, 0), bytes(item, 1), item.get<bool>(2)});
        }
        return result;
    }
    std::shared_ptr<filesystem::FileBorrow> borrow(void* identity) {
        if (auto found = borrows.find(identity); found != borrows.end()) return found->second;
        filesystem::ErrorKind error;
        auto value = filesystem::borrow_file(runtime_, identity, error);
        if (!value) throw Rejected{error == filesystem::ErrorKind::busy ? ErrorKind::busy : ErrorKind::closed, error};
        borrows.emplace(identity, value); return value;
    }
    ClientOptions config(const void* input) {
        Record value(runtime_, geometry(layout_, SourceRecord::config), input); ClientOptions out;
        out.tls.ca_bundle = text(value, 0); out.tls.identity = static_cast<TlsOptions::Identity>(value.get(1));
        out.tls.certificate = text(value, 2); out.tls.key = text(value, 3); out.tls.password = text(value, 4);
        out.proxy.mode = static_cast<ProxyOptions::Mode>(value.get(5)); out.proxy.endpoint = text(value, 6);
        out.proxy.auth.kind = static_cast<Authentication::Kind>(value.get(7));
        out.proxy.auth.user = text(value, 8); out.proxy.auth.password = text(value, 9);
        const NativeArrayField string_field{"", 0, 0, 12, 0};
        const NativeArrayElement string_element{"void.array.element.v1/19:", sizeof(void*), alignof(void*), 1, &string_field};
        auto array = runtime_.native_array_layout(string_element, 0);
        const auto count = runtime_.array_count(runtime_.root_word(value.reference(10)), array, true);
        if (count > 1024) throw Rejected{ErrorKind::limit_exceeded};
        Record item(runtime_, string_element);
        for (std::uint64_t n = 0; n < count; ++n) {
            runtime_.array_read(item.data(), runtime_.root_word(value.reference(10)), array, true, n); out.proxy.no_proxy.push_back(text(item, 0));
        }
        out.cookies = value.get<bool>(11); out.max_active = value.get(12); out.max_connections = value.get(13);
        out.max_per_origin = value.get(14); out.max_http2_streams = value.get(15); out.max_idle = value.get(16);
        out.idle_timeout_ms = value.get(17); out.connection_lifetime_ms = value.get(18); out.dns_cache_ms = value.get(19);
        out.user_agent = text(value, 20);
        if (out.proxy.mode == ProxyOptions::environment) {
            // An owned snapshot at acquisition; never defer getenv to transport.
            auto environment = [&](const char* lower, const char* upper) {
#ifdef _WIN32
                // Windows aliases HTTP_PROXY and http_proxy. Ignore both to
                // preserve CGI safety; ALL_PROXY and explicit routing remain.
                if (std::string_view(lower) == "http_proxy") return std::string{};
                os::Result result;
                { Runtime::Native safe(runtime_); result = os::query(os::Operation::environment, lower); }
                if (result.kind == os::ErrorKind::not_found) return std::string{};
                if (result.kind != os::ErrorKind::success) throw Rejected{ErrorKind::invalid_argument};
                charge(result.text.size()); return std::move(result.text);
#else
                const char* value = std::getenv(lower); if (!value && upper) value = std::getenv(upper);
                std::string_view bytes = value ? value : ""; charge(bytes.size()); return std::string(bytes);
#endif
            };
            out.proxy.http_proxy = environment("http_proxy", nullptr); // HTTP_PROXY is unsafe in CGI environments.
            out.proxy.https_proxy = environment("https_proxy", "HTTPS_PROXY");
            out.proxy.all_proxy = environment("all_proxy", "ALL_PROXY"); out.proxy.environment_no_proxy = environment("no_proxy", "NO_PROXY");
        }
        return out;
    }
    Request request(const void* input) {
        Record value(runtime_, geometry(layout_, SourceRecord::request), input); Request out;
        out.method = text(value, 0); out.target = text(value, 1); out.headers = headers(value, 2);
        out.body = static_cast<Request::Body>(value.get(3)); out.data = bytes(value, 4);
        out.stream_length = value.get(6); out.auth.kind = static_cast<Authentication::Kind>(value.get(7));
        out.auth.user = text(value, 8); out.auth.password = text(value, 9);
        const auto& part = geometry(layout_, SourceRecord::part); auto array = runtime_.native_array_layout(part, 0);
        const auto count = runtime_.array_count(runtime_.root_word(value.reference(5)), array, true);
        if (count > 128) throw Rejected{ErrorKind::limit_exceeded};
        out.parts.reserve(count); Record item(runtime_, part);
        for (std::uint64_t n = 0; n < count; ++n) {
            runtime_.array_read(item.data(), runtime_.root_word(value.reference(5)), array, true, n);
            Part packed; packed.name = text(item, 0); packed.filename = text(item, 1); packed.content_type = text(item, 2);
            packed.headers = headers(item, 3); packed.bytes = bytes(item, 4);
            if (void* identity = *item.reference(5)) {
                packed.file = true; packed.file_length = item.get(6);
                if (packed.file_length < -1) throw Rejected{ErrorKind::invalid_argument};
                files.push_back({static_cast<std::int64_t>(n), packed.file_length, borrow(identity)});
            }
            out.parts.push_back(std::move(packed));
        }
        return out;
    }
    std::vector<url::QueryItem> form(void* input) {
        constexpr std::size_t zero = 0;
        Runtime::Slot slot{&input, sizeof(input), &zero, 1}; Runtime::Frame roots(runtime_, &slot, 1);
        const auto& element = geometry(layout_, SourceRecord::form_item);
        auto array = runtime_.native_array_layout(element, 0);
        const auto count = runtime_.array_count(runtime_.root_word(&input), array, true);
        if (count > remaining_ / sizeof(url::QueryItem)) throw Rejected{ErrorKind::limit_exceeded};
        charge(count * sizeof(url::QueryItem));
        std::vector<url::QueryItem> result; result.reserve(count); Record item(runtime_, element);
        for (std::uint64_t n = 0; n < count; ++n) {
            runtime_.array_read(item.data(), runtime_.root_word(&input), array, true, n);
            const bool has_value = item.get<bool>(2);
            result.push_back({text(item, 0), has_value ? text(item, 1) : std::string{}, has_value});
        }
        return result;
    }
    RequestOptions options(const void* input) {
        Record value(runtime_, geometry(layout_, SourceRecord::options), input); RequestOptions out;
        out.protocol = static_cast<Protocol>(value.get(0)); out.ip_family = static_cast<IpFamily>(value.get(1));
        out.redirects = static_cast<RedirectPolicy>(value.get(2)); out.max_redirects = value.get(3);
        out.decoding = static_cast<Decoding>(value.get(4)); out.max_encoded_bytes = value.get(5);
        out.max_decoded_bytes = value.get(6); out.max_buffer_bytes = value.get(7); out.connect_timeout_ms = value.get(8);
        out.header_timeout_ms = value.get(9); out.read_timeout_ms = value.get(10); out.write_timeout_ms = value.get(11); out.total_timeout_ms = value.get(12);
        return out;
    }
    std::vector<std::shared_ptr<filesystem::FileBorrow>> leases() const {
        std::vector<std::shared_ptr<filesystem::FileBorrow>> result; result.reserve(borrows.size());
        for (auto& [identity, borrow] : borrows) { (void)identity; result.push_back(borrow); } return result;
    }
};
void headers_into(Runtime& runtime, const SourceLayout& layout, Record& reply, unsigned field, const std::vector<Header>& fields) {
    const auto& header = geometry(layout, SourceRecord::header); auto array = runtime.native_array_layout(header, 0);
    runtime.array_new(reply.reference(field), array, true); Record item(runtime, header);
    for (const auto& input : fields) {
        runtime.string_into(item.reference(0), input.name); runtime.bytes_into(item.reference(1), input.value); item.set(2, input.sensitive);
        runtime.array_push(runtime.root_word(reply.reference(field)), array->schema(), item.data());
    }
}
void build(Runtime& runtime, const SourceLayout& layout, Record& reply, const Result& result, const ProviderFeatures* features = nullptr) {
    reply.set(0, static_cast<std::int64_t>(result.error.kind)); reply.set(1, static_cast<std::int64_t>(result.error.phase));
    reply.set(2, static_cast<std::int64_t>(result.error.native_domain)); reply.set(3, result.error.native_code); reply.set(4, result.error.remote_outcome_unknown);
    reply.set(10, std::int64_t{-1});
    if (result.info) {
        const auto& info = *result.info; reply.set(5, static_cast<std::int64_t>(info.status)); reply.set(6, static_cast<std::int64_t>(info.version));
        runtime.string_into(reply.reference(7), info.url); headers_into(runtime, layout, reply, 8, info.headers);
        reply.set(10, info.content_length); reply.set(11, info.content_decoded);
        const auto& history = geometry(layout, SourceRecord::history); auto array = runtime.native_array_layout(history, 0);
        runtime.array_new(reply.reference(9), array, true); Record item(runtime, history);
        for (const auto& input : info.history) {
            item.set(0, static_cast<std::int64_t>(input.status)); runtime.string_into(item.reference(1), input.from_origin);
            runtime.string_into(item.reference(2), input.to_origin); item.set(3, input.method_changed);
            runtime.array_push(runtime.root_word(reply.reference(9)), array->schema(), item.data());
        }
    }
    headers_into(runtime, layout, reply, 12, result.trailers); runtime.bytes_into(reply.reference(13), result.bytes);
    reply.set(14, result.number); reply.set(15, result.uploaded); reply.set(16, result.downloaded_encoded);
    reply.set(17, result.downloaded); reply.set(18, result.elapsed_ms); reply.set(19, result.eof); reply.set(20, result.file_completed);
    reply.set(31, static_cast<std::int64_t>(result.error.url_kind)); reply.set(32, result.error.url_offset);
    if (result.io_cause) {
        reply.set(21, static_cast<std::int64_t>(result.io_cause->error)); reply.set(22, result.io_cause->os_code);
        reply.set(23, result.io_cause->completed); reply.set(24, result.io_cause->cleanup_code); runtime.bytes_into(reply.reference(25), result.io_cause->bytes);
    }
    if (features) {
        runtime.string_into(reply.reference(26), features->transport); runtime.string_into(reply.reference(27), features->tls);
        runtime.string_into(reply.reference(28), features->resolver); runtime.string_into(reply.reference(29), features->idn);
        std::int64_t flags = 1 | 2 | 8 | 16 | 32 | 64 | 128 | 512;
#ifdef _WIN32
        flags |= 1024;
#else
        flags |= 256;
#endif
        reply.set(30, flags);
    }
}
TypeLayout task_layout(Runtime& runtime, const SourceLayout& layout) {
    if (!layout.task_size || !layout.task_key || !*layout.task_key) throw std::logic_error("Missing HTTP task geometry");
    TypeLayout task{layout.task_size, {}};
    if (layout.task_reference_count) task.references.assign(layout.task_references, layout.task_references + layout.task_reference_count);
    task.descriptor = runtime.descriptor(layout.task_key); return task;
}
class NetworkJob final : public CompletionJob {
    Runtime& runtime_;
    const SourceLayout& layout_;
    SourceOperation operation_;
    std::shared_ptr<ClientResource> client_;
    std::shared_ptr<TransferResource> transfer_;
    std::shared_ptr<Transfer> private_transfer_;
    std::shared_ptr<FileUpload> producer_;
    std::shared_ptr<FileDownload> downloader_;
    std::vector<std::shared_ptr<filesystem::FileBorrow>> files_;
    void release(const Result& result) {
        if (operation_ == SourceOperation::close_client && client_) client_->close();
        if (operation_ == SourceOperation::close_transfer && transfer_) transfer_->close();
        if (operation_ == SourceOperation::finish && transfer_ && result.error.kind != ErrorKind::busy) transfer_->release_files();
        files_.clear();
    }
public:
    NetworkJob(Runtime& runtime, const SourceLayout& layout, SourceOperation operation, std::shared_ptr<RuntimeDriver> provider,
        std::shared_ptr<Operation> pending, std::shared_ptr<ClientResource> client, std::shared_ptr<TransferResource> transfer,
        std::shared_ptr<Transfer> private_transfer, std::vector<std::shared_ptr<filesystem::FileBorrow>> files,
        std::shared_ptr<FileUpload> producer = {}, std::shared_ptr<FileDownload> downloader = {})
        : CompletionJob(runtime, std::move(provider), std::move(pending)), runtime_(runtime), layout_(layout), operation_(operation),
          client_(std::move(client)), transfer_(std::move(transfer)), private_transfer_(std::move(private_transfer)),
          producer_(std::move(producer)), downloader_(std::move(downloader)), files_(std::move(files)) {}
    void cancel_native_sources() noexcept override {
        if (producer_) producer_->stop();
        if (transfer_) transfer_->stop_file_source();
        if (downloader_) downloader_->request_stop();
    }
    ~NetworkJob() override {
        // Includes failed task admission/publication and cancellation. Private
        // buffered transfers never escape, so drain before releasing file claims.
        if (producer_) producer_->stop();
        if (downloader_) downloader_->stop();
        if (private_transfer_) {
            if (runtime_.is_managed()) { Runtime::Native safe(runtime_); client_->provider->driver().close(private_transfer_)->wait(); }
            else client_->provider->driver().close(private_transfer_)->wait();
        }
        if (operation_ == SourceOperation::close_transfer && transfer_) transfer_->close();
        if (operation_ == SourceOperation::close_client && client_) client_->close();
    }
    void publish(Runtime& runtime, void* task) override {
        auto result = take_result(); release(result);
        Record reply(runtime, geometry(layout_, SourceRecord::reply)); build(runtime, layout_, reply, result);
        reply.publish(runtime, task, layout_.task_result);
    }
};

}
}
extern "C" void vd_http_call(void* output, const vd::http::SourceLayout* layout, unsigned opcode,
    void* const* references, const std::int64_t* numbers, const void* generation, bool asynchronous, vd::RuntimeFailure* failure) noexcept {
    using namespace vd; using namespace vd::http;
    std::shared_ptr<ClientResource> client;
    std::shared_ptr<TransferResource> transfer;
    std::shared_ptr<FileUpload> producer;
    std::shared_ptr<FileDownload> downloader;
    std::shared_ptr<Transfer> private_transfer;
    bool acquisition = false;
    try {
        if (!output || !layout || !references || !numbers || opcode >= static_cast<unsigned>(SourceOperation::cleanup_client) || (asynchronous && !source_operations[opcode].asynchronous))
            throw std::logic_error("Invalid HTTP source operation");
        auto& runtime = Runtime::current();
        for (unsigned n = 0; n < index(SourceRecord::count); ++n) geometry(*layout, static_cast<SourceRecord>(n));
        const auto operation = static_cast<SourceOperation>(opcode);
        const auto& contract = source_operations[opcode];
        std::array<void*, 8> identities{}; std::array<Runtime::Slot, 8> slots{}; constexpr std::size_t zero = 0;
        unsigned reference = 0, count = 0;
        for (char code : contract.arguments) if (code != 's') {
            if (code == 'C' || code == 'T' || code == 'F' || code == 'b' || code == 'q') {
                identities[reference] = references[reference]; slots[count++] = {&identities[reference], sizeof(void*), &zero, 1};
            }
            ++reference;
        }
        Runtime::Frame roots(runtime, slots.data(), count);
        Snapshot snapshot(runtime, *layout, operation == SourceOperation::open ? 1048576 : 67108864);
        Result result; ProviderFeatures features; bool have_features = false;
        std::shared_ptr<Operation> pending;
        if (operation == SourceOperation::open) {
            client = open_client_resource(runtime, identities[0], snapshot.config(references[1]), result.error);
            acquisition = bool(client);
        } else if (operation == SourceOperation::capabilities) {
            { Runtime::Native safe(runtime); features = provider_features(); } have_features = true;
        } else if (operation == SourceOperation::form) {
            auto items = snapshot.form(identities[0]);
            url::QueryResult encoded;
            { Runtime::Native safe(runtime); encoded = url::encode_query(items, true, url::max_form_bytes); }
            result.bytes = std::move(encoded.text);
            if (encoded.error != url::ErrorKind::success) {
                result.error = {encoded.error == url::ErrorKind::limit_exceeded ? ErrorKind::limit_exceeded : ErrorKind::invalid_encoding, Phase::prepare};
                result.error.url_kind = encoded.error; result.error.url_offset = encoded.offset;
            }
        } else {
            const bool client_operation = contract.arguments.front() == 'C';
            if (client_operation) client = find_client_resource(runtime, identities[0]);
            else { transfer = find_transfer_resource(runtime, identities[0]); if (transfer) client = transfer->parent; }
            if (!client || (!client_operation && !transfer)) {
                if (operation != SourceOperation::close_client && operation != SourceOperation::close_transfer) result.error = {ErrorKind::closed, Phase::prepare};
            } else {
                auto& driver = client->provider->driver();
                if (operation == SourceOperation::start || operation == SourceOperation::send || operation == SourceOperation::upload || operation == SourceOperation::download) {
                    const unsigned input = operation == SourceOperation::start ? 2 : 1;
                    auto request = snapshot.request(references[input]); auto options = snapshot.options(references[input + 1]);
                    if (operation == SourceOperation::upload) {
                        if (request.body != Request::empty || numbers[0] < -1) throw Rejected{ErrorKind::invalid_argument};
                        auto borrow = snapshot.borrow(identities[3]); request.body = Request::stream; request.stream_length = numbers[0];
                        snapshot.files.push_back({-1, numbers[0], borrow});
                    }
                    if (!snapshot.files.empty()) {
                        producer = FileUpload::prepare(runtime, client->provider, snapshot.files); request.source = producer->source();
                    }
                    if (operation == SourceOperation::start) {
                        transfer = start_transfer_resource(runtime, identities[1], client, std::move(request), options, result.error, snapshot.leases(), producer);
                        acquisition = bool(transfer);
                        if (transfer && async_execution_context()) track_transfer_lifetime(runtime, transfer, task_layout(runtime, *layout), generation);
                    } else if (operation == SourceOperation::download) {
                        if (request.body == Request::stream || snapshot.borrows.contains(identities[3])) throw Rejected{ErrorKind::invalid_argument};
                        auto file = snapshot.borrow(identities[3]);
                        const auto admitted = std::chrono::steady_clock::now();
                        { Runtime::Native safe(runtime); private_transfer = driver.start(client->client, std::move(request), options, result.error); }
                        if (private_transfer) {
                            downloader = FileDownload::prepare(runtime, client->provider, private_transfer, std::move(file), options.total_timeout_ms, admitted);
                            client->sink(downloader);
                            if (producer) { client->producer(producer); producer->start(private_transfer); }
                            downloader->start(); pending = downloader->result();
                        }
                    } else if (producer) {
                        { Runtime::Native safe(runtime); private_transfer = driver.start_buffered_upload(client->client, std::move(request), options, result.error); }
                        if (private_transfer) { client->producer(producer); producer->start(private_transfer); pending = driver.buffered_result(private_transfer); }
                    } else { Runtime::Native safe(runtime); pending = driver.send(client->client, std::move(request), options); }
                } else if (operation == SourceOperation::response) pending = driver.response(transfer->transfer);
                else if (operation == SourceOperation::read) pending = driver.read(transfer->transfer, numbers[0]);
                else if (operation == SourceOperation::write) {
                    void* input = identities[1]; Runtime::Slot slot{&input, sizeof(input), &zero, 1}; Runtime::Frame bytes_root(runtime, &slot, 1);
                    if (runtime.list_count(runtime.root_word(&input), ScalarEncoding::u8) > queue_bytes) throw Rejected{ErrorKind::limit_exceeded};
                    pending = driver.write(transfer->transfer, runtime.byte_array_bytes(runtime.root_word(&input)));
                } else if (operation == SourceOperation::finish_upload) result.error = driver.finish_upload(transfer->transfer);
                else if (operation == SourceOperation::finish) pending = driver.finish(transfer->transfer);
                else if (operation == SourceOperation::close_transfer) { transfer->stop_file_source(); pending = driver.close(transfer->transfer); }
                else if (operation == SourceOperation::close_client) { client->stop_producers(); client->prepare_sinks(); pending = driver.close(client->client); }
                else if (operation == SourceOperation::clear_cookies) { Runtime::Native safe(runtime); result.error = driver.clear_cookies(client->client); }
            }
        }
        // Immediate validation failures are also completed through the same
        // external task path so the private async return geometry stays exact.
        if (asynchronous) {
            if (!pending) pending = Operation::settled(std::move(result));
            auto provider = client ? client->provider : runtime_driver(runtime);
            auto job = std::make_shared<NetworkJob>(runtime, *layout, operation, std::move(provider), std::move(pending), client, transfer, private_transfer, snapshot.leases(), producer, downloader);
            async_submit_external(static_cast<void**>(output), task_layout(runtime, *layout), generation, std::move(job)); return;
        }
        if (pending) { Runtime::Native safe(runtime); result = pending->take(); }
        if (private_transfer && client) { Runtime::Native safe(runtime); client->provider->driver().close(private_transfer)->wait(); }
        if (result.native_allocation_failure) throw std::bad_alloc();
        if (operation == SourceOperation::close_transfer && transfer) transfer->close();
        if (operation == SourceOperation::close_client && client) client->close();
        if (operation == SourceOperation::finish && transfer && result.error.kind != ErrorKind::busy) transfer->release_files();
        Record reply(runtime, geometry(*layout, SourceRecord::reply)); build(runtime, *layout, reply, result, have_features ? &features : nullptr);
        std::memcpy(output, reply.data(), reply.size()); acquisition = false;
    } catch (const Rejected& error) {
        try {
            auto& runtime = Runtime::current(); Result result; result.error = {error.kind, Phase::prepare};
            if (opcode == static_cast<unsigned>(SourceOperation::form)) result.error.url_kind = url::ErrorKind::limit_exceeded;
            if (error.io_kind != filesystem::ErrorKind::success) {
                auto cause = std::make_shared<filesystem::Result>(); cause->error = error.io_kind;
                result.io_cause = std::move(cause); result.error.native_domain = NativeDomain::filesystem;
            }
            if (asynchronous) {
                auto provider = client ? client->provider : runtime_driver(runtime);
                auto job = std::make_shared<NetworkJob>(runtime, *layout, static_cast<SourceOperation>(opcode), provider,
                    Operation::settled(std::move(result)), client, transfer, nullptr, std::vector<std::shared_ptr<filesystem::FileBorrow>>{});
                async_submit_external(static_cast<void**>(output), task_layout(runtime, *layout), generation, std::move(job)); return;
            }
            Record reply(runtime, geometry(*layout, SourceRecord::reply)); build(runtime, *layout, reply, result); std::memcpy(output, reply.data(), reply.size());
        } catch (...) { failure->message = "HTTP rejection publication failed"; }
    } catch (const std::bad_alloc&) { failure->message = "HTTP boundary allocation failed"; }
    catch (const std::length_error&) { failure->message = "HTTP boundary extent is unrepresentable"; }
    catch (...) { failure->message = "HTTP boundary invariant failed"; }
    if (failure->message || acquisition) {
        // An unstarted producer must publish done before close can await it.
        if (producer) producer->stop();
        if (downloader) downloader->stop();
        producer.reset();
        if (failure->message && opcode == static_cast<unsigned>(SourceOperation::close_client) && client) client->close();
        if (failure->message && opcode == static_cast<unsigned>(SourceOperation::close_transfer) && transfer) transfer->close();
        if (private_transfer && client) { Runtime::Native safe(Runtime::current()); client->provider->driver().close(private_transfer)->wait(); }
        if (acquisition) { if (transfer) transfer->close(); else if (client) client->close(); }
    }
}

extern "C" void vd_http_cleanup(unsigned opcode, void* identity, vd::RuntimeFailure* failure) noexcept {
    using namespace vd; using namespace vd::http;
    try {
        auto& runtime = Runtime::current(); constexpr std::size_t zero = 0;
        Runtime::Slot slot{&identity, sizeof(identity), &zero, 1}; Runtime::Frame root(runtime, &slot, 1);
        if (opcode == static_cast<unsigned>(SourceOperation::cleanup_client)) {
            if (auto resource = find_client_resource(runtime, identity); resource && resource->close()) failure->message = "HTTP client cleanup failed";
        } else if (opcode == static_cast<unsigned>(SourceOperation::cleanup_transfer)) {
            if (auto resource = find_transfer_resource(runtime, identity); resource && resource->close()) failure->message = "HTTP transfer cleanup failed";
        } else throw std::logic_error("Invalid HTTP cleanup control");
    } catch (...) { failure->message = "HTTP resource cleanup invariant failed"; }
}
