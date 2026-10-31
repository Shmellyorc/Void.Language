#include "void/http_resource.hpp"
#include "void/filesystem.hpp"
#include "void/http_file.hpp"
#include "void/detail/filesystem_hooks.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <fstream>
#include <charconv>
#include <stdexcept>
#if __has_feature(address_sanitizer)
#include <sanitizer/allocator_interface.h>
#endif

namespace {
using namespace vd;
using namespace vd::http;
char generation;
struct Context {
    Runtime& runtime;
    std::string base;
    unsigned mode;
    std::uint64_t count;
    std::shared_ptr<ClientResource> client;
    std::shared_ptr<TransferResource> transfer;
    std::shared_ptr<filesystem::File> file;
    std::size_t minimum = static_cast<std::size_t>(-1), maximum = 0, handles = 0;
    std::size_t tasks = 0, groups = 0, work = 0;
};
Context* current;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::size_t rss_kib() {
#ifdef __linux__
    std::ifstream input("/proc/self/status"); std::string line;
    while (std::getline(input, line)) if (line.starts_with("VmRSS:")) {
        auto first = line.find_first_of("0123456789"); std::size_t value = 0;
        if (first != std::string::npos) std::from_chars(line.data() + first, line.data() + line.size(), value);
        return value;
    }
#endif
    return 0;
}
std::size_t live_native_bytes() {
#if __has_feature(address_sanitizer)
    // Counts live requested allocations, excluding ASan's shadow, redzones and
    // freed-object quarantine. Keep ordinary ASan quarantine/leak checks active.
    return __sanitizer_get_current_allocated_bytes();
#else
    return 0;
#endif
}
TypeLayout task_layout(Runtime& runtime, bool read) {
    TypeLayout layout = read ? TypeLayout{24, {8}} : TypeLayout{8, {}};
    layout.descriptor = runtime.descriptor(read ? "http-test-read" : "http-test-task");
    return layout;
}
class NetworkJob final : public CompletionJob {
    bool read_;
public:
    NetworkJob(Context& context, std::shared_ptr<Operation> operation, bool read)
        : CompletionJob(context.runtime, context.client->provider, std::move(operation)), read_(read) {}
    void publish(Runtime& runtime, void* task) override {
        auto result = take_result();
        require(result.error.kind == ErrorKind::success, "actual HTTP external operation failed");
        if (read_) {
            void* text = nullptr;
            constexpr std::size_t zero = 0;
            Runtime::Slot slot{&text, sizeof(text), &zero, 1}; Runtime::Frame roots(runtime, &slot, 1);
            runtime.string_into(&text, result.bytes); runtime.store(task, 8, text);
            std::memcpy(static_cast<char*>(task) + 16, &result.eof, sizeof(result.eof));
        } else {
            const std::int32_t answer = 42;
            std::memcpy(static_cast<char*>(task) + 4, &answer, sizeof(answer));
        }
    }
};
class FileProducer final : public NativeJob {
    Runtime& runtime_;
    std::shared_ptr<RuntimeDriver> provider_;
    std::shared_ptr<Transfer> transfer_;
    std::shared_ptr<filesystem::File> file_;
public:
    explicit FileProducer(Context& context) : runtime_(context.runtime), provider_(context.client->provider),
        transfer_(context.transfer->transfer), file_(context.file) { runtime_.begin_native_job(); }
    ~FileProducer() override { runtime_.end_native_job(); }
    void execute(const std::atomic<bool>& cancellation, RuntimeFailure* failure) noexcept override {
        try {
            for (;;) {
                filesystem::Request request(filesystem::Operation::read_bytes); request.file = file_; request.first = queue_bytes;
                auto input = filesystem::run(request, &cancellation);
                require(input.error == filesystem::ErrorKind::success, "file producer read");
                if (!input.bytes.empty()) {
                    auto sent = provider_->driver().write(transfer_, std::move(input.bytes))->take(&cancellation);
                    require(sent.error.kind == ErrorKind::success, "file producer write");
                }
                if (input.eof) break;
            }
            require(provider_->driver().finish_upload(transfer_).kind == ErrorKind::success, "file producer EOF");
        } catch (...) { failure->message = "Actual HTTP single-worker file producer failed"; }
    }
    void publish(Runtime&, void* task) override {
        const std::int32_t answer = 42; std::memcpy(static_cast<char*>(task) + 4, &answer, sizeof(answer));
    }
};
struct Frame {
    void* self = nullptr;
    void* pending = nullptr;
    void* held = nullptr;
    void* producer = nullptr;
    unsigned stage = 0, iteration = 0;
    std::uint64_t received = 0;
};
constexpr std::size_t offsets[] = {offsetof(Frame, self), offsetof(Frame, pending), offsetof(Frame, held), offsetof(Frame, producer)};
std::string_view text_result(Runtime& runtime, void* task) {
    void* text = nullptr; std::memcpy(&text, static_cast<char*>(task) + 8, sizeof(text));
    return runtime.string_bytes(text);
}
void resume(void* address, RuntimeFailure* failure) {
    auto& context = *current;
    auto& runtime = context.runtime;
    auto& frame = *static_cast<Frame*>(address);
    auto& driver = context.client->provider->driver();
    if (!frame.stage) {
        void* identity = nullptr; constexpr std::size_t zero = 0;
        Runtime::Slot slot{&identity, sizeof(identity), &zero, 1}; Runtime::Frame roots(runtime, &slot, 1);
        runtime.allocate_into(&identity, {1, {}});
        Request request; request.target = context.base + (context.mode == 1 ? "/echo" : "/large?" + std::to_string(context.count));
        if (context.mode == 1) { request.method = "POST"; request.body = Request::stream; }
        RequestOptions options; options.max_encoded_bytes = options.max_decoded_bytes = context.count;
        options.total_timeout_ms = context.count > 134217728 ? 300000 : 120000;
        Error error;
        context.transfer = start_transfer_resource(runtime, identity, context.client, request, options, error);
        require(bool(context.transfer), "actual HTTP Runtime transfer acquisition");
        track_transfer_lifetime(runtime, context.transfer, task_layout(runtime, false), &generation);
        if (context.mode == 1) {
            void* producer = nullptr;
            Runtime::Slot producer_slot{&producer, sizeof(producer), &zero, 1}; Runtime::Frame producer_root(runtime, &producer_slot, 1);
            async_submit_native(&producer, task_layout(runtime, false), &generation, std::make_shared<FileProducer>(context));
            runtime.store(address, offsetof(Frame, producer), producer);
        }
        frame.stage = context.mode == 2 ? 4 : 1;
    }
    while (frame.stage == 1) {
        if (!frame.pending) {
            void* pending = nullptr; constexpr std::size_t zero = 0;
            Runtime::Slot slot{&pending, sizeof(pending), &zero, 1}; Runtime::Frame roots(runtime, &slot, 1);
            async_submit_external(&pending, task_layout(runtime, true), &generation,
                std::make_shared<NetworkJob>(context, driver.read(context.transfer->transfer, queue_bytes), true));
            runtime.store(address, offsetof(Frame, pending), pending);
        }
        if (!vd_async_ready(frame.pending, failure)) { vd_async_suspend(address, frame.pending, failure); return; }
        if (failure->message) break;
        bool eof = false; std::memcpy(&eof, static_cast<char*>(frame.pending) + 16, sizeof(eof));
        auto bytes = text_result(runtime, frame.pending);
        if (context.mode == 0) require(std::all_of(bytes.begin(), bytes.end(), [](char c) { return c == 'x'; }), "async pull bytes changed");
        frame.received += bytes.size();
        if (!frame.held && !bytes.empty()) runtime.store(address, offsetof(Frame, held), frame.pending);
        runtime.store(address, offsetof(Frame, pending), nullptr);
        require(runtime.collect() == Runtime::Collection::completed, "actual HTTP pull GC failed");
        require(frame.held && vd_async_ready(frame.held, failure) && !text_result(runtime, frame.held).empty(), "retained HTTP task lost repeated await");
        const auto snapshot = runtime.memory_snapshot();
        const auto native = async_snapshot();
        context.tasks = std::max(context.tasks, native.tasks); context.groups = std::max(context.groups, native.groups);
        context.work = std::max(context.work, native.work);
        context.handles = std::max(context.handles, snapshot.handles);
        if (++frame.iteration >= 32) {
            context.minimum = std::min(context.minimum, snapshot.heap.payload_bytes);
            context.maximum = std::max(context.maximum, snapshot.heap.payload_bytes);
        }
        if (!context.mode && (frame.iteration % 256 == 0 || eof)) {
            std::size_t rss;
            { Runtime::Native safe(runtime); rss = rss_kib(); }
            std::cout << "{\"http_async_sample\":{\"received\":" << frame.received
                << ",\"heap_payload\":" << snapshot.heap.payload_bytes << ",\"handles\":" << snapshot.handles
                << ",\"tasks\":" << native.tasks << ",\"groups\":" << native.groups << ",\"work\":" << native.work
                << ",\"rss_kib\":" << rss << ",\"network_reserved\":" << driver.snapshot().reserved_bytes << "}}\n";
        }
        if (eof) frame.stage = 2;
    }
    if (frame.stage == 2) {
        require(frame.received == (context.mode == 1 ? 150005u : context.count), "actual HTTP async byte count");
        void* pending = nullptr; constexpr std::size_t zero = 0;
        Runtime::Slot slot{&pending, sizeof(pending), &zero, 1}; Runtime::Frame roots(runtime, &slot, 1);
        async_submit_external(&pending, task_layout(runtime, false), &generation,
            std::make_shared<NetworkJob>(context, driver.finish(context.transfer->transfer), false));
        runtime.store(address, offsetof(Frame, pending), pending); frame.stage = 3;
    }
    if (frame.stage == 3) {
        if (!vd_async_ready(frame.pending, failure)) { vd_async_suspend(address, frame.pending, failure); return; }
        frame.stage = 4;
    }
    if (!vd_async_drain(address, failure)) return;
    if (!failure->message) {
        const std::int32_t answer = 42; std::memcpy(static_cast<char*>(frame.self) + 4, &answer, sizeof(answer));
        vd_async_complete(frame.self);
    }
}
void* start(std::int32_t, RuntimeFailure* failure) {
    Frame temporary;
    auto& runtime = Runtime::current();
    Runtime::Slot slot{&temporary, sizeof(temporary), offsets, 4}; Runtime::Frame roots(runtime, &slot, 1);
    vd_async_new(&temporary.self, &temporary.pending, sizeof(Frame), offsets, 4, 8, nullptr, 0,
        "http-test-task", &generation, resume, failure);
    if (failure->message) return nullptr;
    static_cast<Frame*>(temporary.pending)->self = temporary.self;
    vd_async_start(temporary.self, failure); return temporary.self;
}
}
namespace {
// Native fixture authority only. Source integration uses borrow_file(), which
// repeats the actual IO ledger/owner/claim checks; no source FFI is introduced.
class FixtureBorrow final : public filesystem::FileBorrow {
    std::shared_ptr<filesystem::File> file_;
public:
    explicit FixtureBorrow(std::shared_ptr<filesystem::File> file) : file_(std::move(file)) {}
    std::shared_ptr<filesystem::File> file() const noexcept override { return file_; }
};
void native_identity_cases(const std::string& base) {
    Runtime runtime; Runtime::Attachment attached(runtime); Runtime::Managed managed(runtime);
    std::array<void*, 2> identities{}; constexpr std::size_t offsets[] = {0, sizeof(void*)};
    Runtime::Slot slot{identities.data(), sizeof(identities), offsets, 2}; Runtime::Frame roots(runtime, &slot, 1);
    for (auto& identity : identities) runtime.allocate_into(&identity, {1, {}});
    Error error;
    auto old_client = open_client_resource(runtime, identities[0], {}, error);
    require(bool(old_client), "old client identity admission"); old_client->close();
    auto new_client = open_client_resource(runtime, identities[0], {}, error);
    require(bool(new_client), "reused client identity admission"); old_client->close();
    require(find_client_resource(runtime, identities[0]) == new_client, "old cleanup erased a newer client at the same managed key");
    Request request; request.target = base + "/plain";
    auto old_transfer = start_transfer_resource(runtime, identities[1], new_client, request, {}, error);
    require(bool(old_transfer), "old transfer identity admission"); old_transfer->close();
    auto new_transfer = start_transfer_resource(runtime, identities[1], new_client, request, {}, error);
    require(bool(new_transfer), "reused transfer identity admission"); old_transfer->close();
    require(find_transfer_resource(runtime, identities[1]) == new_transfer, "old cleanup erased a newer transfer at the same managed key");
    new_transfer->close(); new_client->close(); identities = {};
    require(runtime.collect() == Runtime::Collection::completed && !runtime.snapshot().objects, "identity cleanup retained managed roots");
    { Runtime::Native safe(runtime); require(!runtime.close(), "identity Runtime shutdown"); }
}
void native_file_cases(const std::string& base, const std::string& directory) {
    Runtime runtime; runtime.async_worker_limit(1);
    Runtime::Attachment attached(runtime); Runtime::Managed managed(runtime);
    auto provider = runtime_driver(runtime);
    auto& driver = provider->driver(); Error error;
    std::shared_ptr<Client> client;
    { Runtime::Native safe(runtime); client = driver.open({}, error); }
    require(bool(client), "native file client acquisition");
    auto make_file = [&](const std::string& name, const std::string& bytes, std::int64_t cursor) {
        Runtime::Native safe(runtime);
        filesystem::Result status;
        auto file = filesystem::open(directory + "/" + name, filesystem::OpenMode::create, status);
        require(bool(file), "native producer file acquisition");
        filesystem::Request write(filesystem::Operation::write_bytes); write.file = file; write.bytes = bytes;
        require(filesystem::run(write).error == filesystem::ErrorKind::success, "native producer file setup");
        filesystem::Request seek(filesystem::Operation::seek); seek.file = file; seek.first = cursor;
        require(filesystem::run(seek).error == filesystem::ErrorKind::success, "native producer initial cursor");
        return file;
    };
    auto file = make_file("multipart.bin", "prefix" + std::string(100000, 'F') + std::string(100000, 'G'), 6);
    auto borrow = std::make_shared<FixtureBorrow>(file);
    auto producer = FileUpload::prepare(runtime, provider, {{1, 0, borrow}, {2, 100000, borrow}, {4, -1, borrow}});
    Request request; request.method = "POST"; request.target = base + "/multipart"; request.body = Request::multipart; request.source = producer->source();
    Part before; before.name = "before"; before.bytes = "B";
    Part empty; empty.name = "empty"; empty.file = true; empty.file_length = 0;
    Part first; first.name = "first"; first.filename = "first.bin"; first.file = true; first.file_length = 100000;
    Part middle; middle.name = "middle"; middle.bytes = "M";
    Part last; last.name = "last"; last.filename = "last.bin"; last.file = true;
    request.parts = {before, empty, first, middle, last};
    std::shared_ptr<Transfer> transfer;
    { Runtime::Native safe(runtime); transfer = driver.start_buffered_upload(client, request, {}, error); }
    require(bool(transfer), "native multipart admission"); producer->start(transfer);
    {
        Runtime::Native safe(runtime);
        auto result = driver.buffered_result(transfer)->take();
        require(result.error.kind == ErrorKind::success && !result.native_allocation_failure, "native multipart file transport");
        require(result.bytes == "before:1:B|empty:0:-|first:100000:F|middle:1:M|last:100000:G", "multipart mixed file/byte ordering and framing");
        require(result.file_completed == 200000, "multipart repeated file cursor count");
        filesystem::Request position(filesystem::Operation::position); position.file = file;
        require(filesystem::run(position).number == 200006, "multipart exact current cursor");
        require(filesystem::close(*file).error == filesystem::ErrorKind::success, "multipart caller closes own file");
    }
    auto one = make_file("producer-first.bin", std::string(100000, '1'), 0);
    auto two = make_file("producer-second.bin", std::string(100000, '2'), 0);
    auto first_producer = FileUpload::prepare(runtime, provider, {{-1, 100000, std::make_shared<FixtureBorrow>(one)}});
    auto second_producer = FileUpload::prepare(runtime, provider, {{-1, 100000, std::make_shared<FixtureBorrow>(two)}});
    request = {}; request.method = "POST"; request.body = Request::stream; request.stream_length = 100000;
    request.target = base + "/upload-gate"; request.source = first_producer->source();
    std::shared_ptr<Transfer> first_transfer, second_transfer;
    { Runtime::Native safe(runtime); first_transfer = driver.start_buffered_upload(client, request, {}, error); }
    require(bool(first_transfer), "first independent producer admission"); first_producer->start(first_transfer);
    request.target = base + "/upload-release"; request.source = second_producer->source();
    { Runtime::Native safe(runtime); second_transfer = driver.start_buffered_upload(client, request, {}, error); }
    require(bool(second_transfer), "second independent producer admission"); second_producer->start(second_transfer);
    {
        Runtime::Native safe(runtime);
        auto first_result = driver.buffered_result(first_transfer)->take(); auto second_result = driver.buffered_result(second_transfer)->take();
        require(first_result.error.kind == ErrorKind::success && second_result.error.kind == ErrorKind::success &&
            first_result.bytes == "first" && second_result.bytes == "second", "single worker remained occupied waiting for a network response");
        require(filesystem::close(*one).error == filesystem::ErrorKind::success && filesystem::close(*two).error == filesystem::ErrorKind::success,
            "independent producer files remain caller owned");
    }
    // The peer refuses to consume the first 32 MiB until a second file
    // request arrives. A producer blocked on network backpressure must yield
    // the sole worker, rather than relying on the peer buffering its upload.
    auto large_file = make_file("producer-dependent.bin", std::string(33554432, 'd'), 0);
    auto release_file = make_file("producer-dependency-release.bin", "release", 0);
    auto dependent = FileUpload::prepare(runtime, provider, {{-1, 33554432, std::make_shared<FixtureBorrow>(large_file)}});
    auto releasing = FileUpload::prepare(runtime, provider, {{-1, 7, std::make_shared<FixtureBorrow>(release_file)}});
    request.target = base + "/upload-dependency"; request.stream_length = 33554432; request.source = dependent->source();
    { Runtime::Native safe(runtime); first_transfer = driver.start_buffered_upload(client, request, {}, error); }
    require(bool(first_transfer), "backpressured producer admission"); dependent->start(first_transfer);
    request.target = base + "/upload-dependency-release"; request.stream_length = 7; request.source = releasing->source();
    { Runtime::Native safe(runtime); second_transfer = driver.start_buffered_upload(client, request, {}, error); }
    require(bool(second_transfer), "backpressure dependency release admission"); releasing->start(second_transfer);
    {
        Runtime::Native safe(runtime);
        auto first_result = driver.buffered_result(first_transfer)->take(); auto second_result = driver.buffered_result(second_transfer)->take();
        require(first_result.error.kind == ErrorKind::success && first_result.bytes == "independent" &&
            second_result.error.kind == ErrorKind::success && second_result.bytes == "released", "network backpressure occupied the only IO worker");
        filesystem::close(*large_file); filesystem::close(*release_file);
    }
    auto download_file = make_file("native-download.bin", "prefix", 6);
    auto source_file = make_file("native-duplex-file.bin", std::string(100000, 'f'), 0);
    auto duplex = FileUpload::prepare(runtime, provider, {{0, 100000, std::make_shared<FixtureBorrow>(source_file)}});
    Request download_request; download_request.method = "POST"; download_request.target = base + "/multipart";
    download_request.body = Request::multipart; download_request.source = duplex->source();
    Part file_part; file_part.name = "file"; file_part.file = true; file_part.file_length = 100000; download_request.parts = {file_part};
    std::shared_ptr<Transfer> download_transfer;
    { Runtime::Native safe(runtime); download_transfer = driver.start(client, download_request, {}, error); }
    require(bool(download_transfer), "duplex bulk file admission");
    auto downloader = FileDownload::prepare(runtime, provider, download_transfer, std::make_shared<FixtureBorrow>(download_file),
        120000, std::chrono::steady_clock::now());
    duplex->start(download_transfer); downloader->start();
    {
        Runtime::Native safe(runtime); auto result = downloader->result()->take();
        require(result.error.kind == ErrorKind::success && result.file_completed == 13 && result.downloaded == 13,
            "single worker multipart input and bulk output");
        filesystem::Request position(filesystem::Operation::position); position.file = download_file;
        require(filesystem::run(position).number == 19, "bulk output retained the current cursor");
        filesystem::close(*download_file); filesystem::close(*source_file);
    }
    auto short_file = make_file("producer-short.bin", "short", 0);
    auto short_producer = FileUpload::prepare(runtime, provider, {{-1, 100, std::make_shared<FixtureBorrow>(short_file)}});
    request.target = base + "/echo"; request.stream_length = 100; request.source = short_producer->source();
    std::shared_ptr<Transfer> short_transfer;
    { Runtime::Native safe(runtime); short_transfer = driver.start_buffered_upload(client, request, {}, error); }
    require(bool(short_transfer), "short file producer admission"); short_producer->start(short_transfer);
    {
        Runtime::Native safe(runtime); auto result = driver.buffered_result(short_transfer)->take();
        require(result.error.kind == ErrorKind::unexpected_eof && result.file_completed == 5, "native producer known short input");
        require(filesystem::close(*short_file).error == filesystem::ErrorKind::success, "short input file remains caller owned");
    }
    struct FileGate { std::mutex mutex; std::condition_variable changed; bool entered = false, released = false; };
    auto gate = std::make_shared<FileGate>(); auto hooks = std::make_shared<filesystem::testing::Hooks>();
    hooks->before = [gate](filesystem::Operation operation) {
        if (operation != filesystem::Operation::read_bytes) return;
        std::unique_lock lock(gate->mutex); gate->entered = true; gate->changed.notify_all();
        gate->changed.wait(lock, [&] { return gate->released; });
    };
    std::shared_ptr<filesystem::File> blocked_file;
    {
        Runtime::Native safe(runtime); filesystem::Result status;
        blocked_file = filesystem::open(directory + "/producer-blocked.bin", filesystem::OpenMode::create, status, hooks);
        require(bool(blocked_file), "blocked producer file acquisition");
        filesystem::Request write(filesystem::Operation::write_bytes); write.file = blocked_file; write.bytes.assign(100000, 'b');
        require(filesystem::run(write).error == filesystem::ErrorKind::success, "blocked producer file setup");
        filesystem::Request seek(filesystem::Operation::seek); seek.file = blocked_file;
        require(filesystem::run(seek).error == filesystem::ErrorKind::success, "blocked producer file rewind");
    }
    auto blocked_producer = FileUpload::prepare(runtime, provider, {{-1, 100000, std::make_shared<FixtureBorrow>(blocked_file)}});
    request.stream_length = 100000; request.source = blocked_producer->source();
    std::shared_ptr<Transfer> blocked_transfer;
    { Runtime::Native safe(runtime); blocked_transfer = driver.start(client, request, {}, error); }
    require(bool(blocked_transfer), "blocked producer admission"); blocked_producer->start(blocked_transfer);
    auto queued_file = make_file("producer-unentered.bin", std::string(100000, 'q'), 0);
    auto queued_producer = FileUpload::prepare(runtime, provider, {{-1, 100000, std::make_shared<FixtureBorrow>(queued_file)}});
    request.source = queued_producer->source(); std::shared_ptr<Transfer> queued_transfer;
    { Runtime::Native safe(runtime); queued_transfer = driver.start(client, request, {}, error); }
    require(bool(queued_transfer), "unentered producer admission"); queued_producer->start(queued_transfer);
    void* sink_identity = nullptr; constexpr std::size_t zero = 0;
    Runtime::Slot sink_slot{&sink_identity, sizeof(sink_identity), &zero, 1}; Runtime::Frame sink_roots(runtime, &sink_slot, 1);
    runtime.allocate_into(&sink_identity, {1, {}});
    auto sink_client = open_client_resource(runtime, sink_identity, {}, error);
    require(bool(sink_client), "queued sink client admission");
    auto queued_destination = make_file("sink-unentered.bin", "", 0);
    Request sink_request; sink_request.target = base + "/plain"; std::shared_ptr<Transfer> sink_transfer;
    { Runtime::Native safe(runtime); sink_transfer = driver.start(sink_client->client, sink_request, {}, error); }
    require(bool(sink_transfer), "unentered sink transfer admission");
    auto queued_sink = FileDownload::prepare(runtime, provider, sink_transfer, std::make_shared<FixtureBorrow>(queued_destination),
        120000, std::chrono::steady_clock::now());
    sink_client->sink(queued_sink); queued_sink->start();
    {
        Runtime::Native safe(runtime);
        std::unique_lock lock(gate->mutex);
        const bool entered = gate->changed.wait_for(lock, std::chrono::seconds(5), [&] { return gate->entered; });
        if (!entered) { gate->released = true; gate->changed.notify_all(); }
        require(entered, "file syscall barrier not reached"); lock.unlock();
        queued_producer->stop(); auto queued_close = driver.close(queued_transfer);
        for (unsigned attempt = 0; attempt < 100 && !queued_close->ready(); ++attempt) std::this_thread::sleep_for(std::chrono::milliseconds(5));
        const bool unentered_closed = queued_close->ready() && queued_producer->source()->done;
        filesystem::Request queued_position(filesystem::Operation::position); queued_position.file = queued_file;
        const bool unentered_untouched = filesystem::run(queued_position).number == 0;
        sink_client->prepare_sinks(); auto sink_closed = driver.close(sink_client->client);
        for (unsigned attempt = 0; attempt < 100 && !sink_closed->ready(); ++attempt) std::this_thread::sleep_for(std::chrono::milliseconds(5));
        const bool unentered_sink_closed = sink_closed->ready();
        if (unentered_sink_closed) sink_client->close();
        auto closing = driver.close(blocked_transfer);
        Request independent; independent.target = base + "/plain";
        auto result = driver.send(client, independent, {})->take();
        const bool independent_ok = result.error.kind == ErrorKind::success && result.bytes.size() == 11;
        const bool close_waited = !closing->ready() && !blocked_producer->source()->done;
        { std::lock_guard release(gate->mutex); gate->released = true; gate->changed.notify_all(); }
        closing->wait(); queued_close->wait(); sink_closed->wait(); sink_client->close();
        require(unentered_sink_closed && queued_sink->result()->ready(), "queued sink close needed the occupied worker");
        auto sink_result = queued_sink->result()->take();
        require(sink_result.error.kind == ErrorKind::closed && sink_result.file_completed == 0, "queued sink cancellation wrote data or lost its closed result");
        queued_position.file = queued_destination;
        require(filesystem::run(queued_position).number == 0, "queued sink wrote after client close");
        filesystem::close(*queued_destination);
        require(unentered_closed && unentered_untouched, "unentered producer needed the occupied worker to close or consumed file data");
        filesystem::close(*queued_file);
        require(independent_ok && close_waited && blocked_producer->source()->done, "transport blocked on file IO or close published before quiescence");
        require(filesystem::close(*blocked_file).error == filesystem::ErrorKind::success, "cancelled producer file remains caller owned");
    }
    auto write_gate = std::make_shared<FileGate>(); auto write_hooks = std::make_shared<filesystem::testing::Hooks>();
    write_hooks->before = [write_gate](filesystem::Operation operation) {
        if (operation != filesystem::Operation::write_bytes) return;
        std::unique_lock lock(write_gate->mutex); write_gate->entered = true; write_gate->changed.notify_all();
        write_gate->changed.wait(lock, [&] { return write_gate->released; });
    };
    std::shared_ptr<filesystem::File> entered_destination;
    { Runtime::Native safe(runtime); filesystem::Result status;
      entered_destination = filesystem::open(directory + "/sink-entered.bin", filesystem::OpenMode::create, status, write_hooks); }
    require(bool(entered_destination), "entered sink file admission");
    sink_client = open_client_resource(runtime, sink_identity, {}, error);
    require(bool(sink_client), "entered sink client admission");
    { Runtime::Native safe(runtime); sink_transfer = driver.start(sink_client->client, sink_request, {}, error); }
    auto entered_sink = FileDownload::prepare(runtime, provider, sink_transfer, std::make_shared<FixtureBorrow>(entered_destination),
        120000, std::chrono::steady_clock::now());
    sink_client->sink(entered_sink); entered_sink->start();
    {
        Runtime::Native safe(runtime); std::unique_lock lock(write_gate->mutex);
        const bool entered = write_gate->changed.wait_for(lock, std::chrono::seconds(5), [&] { return write_gate->entered; });
        if (!entered) { write_gate->released = true; write_gate->changed.notify_all(); }
        require(entered, "file sink syscall barrier was not reached"); lock.unlock();
        sink_client->prepare_sinks(); auto closing = driver.close(sink_client->client);
        Request independent; independent.target = base + "/plain"; auto result = driver.send(client, independent, {})->take();
        const bool close_waited = !closing->ready();
        { std::lock_guard release(write_gate->mutex); write_gate->released = true; write_gate->changed.notify_all(); }
        closing->wait(); sink_client->close();
        require(close_waited && result.error.kind == ErrorKind::success && entered_sink->result()->ready(), "client close skipped entered file IO or blocked the driver");
        filesystem::Request position(filesystem::Operation::position); position.file = entered_destination;
        const auto cursor = filesystem::run(position).number;
        auto stopped = entered_sink->result()->take();
        require(stopped.error.kind == ErrorKind::closed && stopped.file_completed == cursor && cursor >= 0 && cursor <= 11,
            "entered sink cancellation lost actual file progress");
        auto settled = driver.send(client, independent, {})->take();
        require(settled.error.kind == ErrorKind::success && filesystem::run(position).number == cursor,
            "sink performed file IO after client close returned");
        filesystem::close(*entered_destination); driver.close(client)->wait();
    }
    std::cout << "Native file producers outside async roots: mixed multipart, zero/known/unknown counts, repeated cursor and single-worker response independence passed\n";
}
}
void http_runtime_cases(const std::string& base, const std::string& directory, std::uint64_t count) {
    native_file_cases(base, directory);
    native_identity_cases(base);
    for (unsigned mode = 0; mode < 3; ++mode) {
        Runtime runtime(256); runtime.async_worker_limit(1);
        const NominalDescriptor descriptors[] = {{"http-test-task", "", "two-i32", {8, {}}},
            {"http-test-read", "", "read-string-eof", {24, {8}}}};
        runtime.admit_types(descriptors);
        Runtime::Attachment attached(runtime); Runtime::Managed managed(runtime);
        Context context{runtime, base, mode, count}; current = &context;
        void* identity = nullptr; constexpr std::size_t zero = 0;
        Runtime::Slot slot{&identity, sizeof(identity), &zero, 1}; Runtime::Frame roots(runtime, &slot, 1);
        runtime.allocate_into(&identity, {1, {}});
        Error error; context.client = open_client_resource(runtime, identity, {}, error);
        require(bool(context.client), "Runtime HTTP client acquisition");
        require(runtime_driver(runtime) == context.client->provider, "Runtime created multiple HTTP drivers");
        if (mode == 1) {
            Runtime::Native safe(runtime);
            filesystem::Result status; context.file = filesystem::open(directory + "/async-producer.bin", filesystem::OpenMode::create, status);
            require(bool(context.file), "single-worker file acquisition");
            filesystem::Request write(filesystem::Operation::write_bytes); write.file = context.file; write.bytes.assign(150000, 'p');
            require(filesystem::run(write).error == filesystem::ErrorKind::success, "single-worker file setup");
            filesystem::Request seek(filesystem::Operation::seek); seek.file = context.file;
            require(filesystem::run(seek).error == filesystem::ErrorKind::success, "single-worker file rewind");
        }
        RuntimeFailure failure;
        const auto answer = vd_async_root(start, 0, 4, &generation, &failure);
        if (failure.message) std::cerr << failure.message << '\n';
        require(!failure.message && answer == 42, "actual HTTP async root failed");
        require(context.client->provider->driver().snapshot().transfers == 0, "activation leaked native HTTP transfer");
        if (!mode) {
            require(context.maximum - context.minimum <= 65536, "actual HTTP completed managed results accumulated");
            require(context.handles <= 16, "actual HTTP completion Handle registrations accumulated");
            require(context.tasks <= 4 && context.groups <= 1 && context.work <= 1, "actual HTTP native Task/group/Work records accumulated");
            std::cout << count << " bytes actual HTTP async pulls: live payload " << context.minimum << ".." << context.maximum
                      << "; handles <= " << context.handles << "; tasks/groups/work <= " << context.tasks << '/' << context.groups << '/' << context.work
                      << "; retained first task repeatedly awaited\n";
        }
        if (context.file) { Runtime::Native safe(runtime); require(filesystem::close(*context.file).error == filesystem::ErrorKind::success, "producer explicit file close"); }
        context.transfer.reset(); context.client->close(); NativeLibrary::get(runtime)->erase(identity); context.client.reset();
        identity = nullptr;
        require(runtime.collect() == Runtime::Collection::completed && runtime.snapshot().objects == 0, "HTTP owner resource roots leaked");
        { Runtime::Native safe(runtime); require(!runtime.close(), "HTTP Runtime shutdown failed"); }
        current = nullptr;
    }
    std::cout << "Actual HTTP Runtime driver, single-worker file producer and activation cleanup passed\n";
}

void http_h2_backpressure_cases(const std::string& base, const std::string& certificate) {
    Driver driver; Error error; ClientOptions config;
    config.tls.ca_bundle = certificate;
    config.max_connections = config.max_per_origin = config.max_idle = 1;
    auto client = driver.open(config, error); require(bool(client), "h2 paused client");
    RequestOptions options; options.protocol = Protocol::http2_required;
    options.total_timeout_ms = 15000;
    const auto baseline = rss_kib();
    const auto baseline_live = live_native_bytes();
    std::array<std::shared_ptr<Transfer>, 3> paused;
    for (auto& transfer : paused) {
        Request request; request.target = base + "/paused";
        transfer = driver.start(client, request, options, error); require(bool(transfer), "h2 paused admission");
        auto headers = driver.response(transfer)->take();
        require(headers.error.kind == ErrorKind::success && headers.info && headers.info->version == Version::http2,
            "h2 paused stream headers");
    }
    std::size_t peak = baseline, peak_live = baseline_live;
    for (unsigned sample = 0; sample < 8; ++sample) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const auto state = driver.snapshot(); peak = std::max(peak, rss_kib());
        peak_live = std::max(peak_live, live_native_bytes());
        require(state.decoded_queued <= 3 * queue_bytes && state.encoded_queued <= 3 * queue_bytes,
            "h2 paused application queues stay bounded");
        std::cout << "{\"h2_paused_sample\":{\"sample\":" << sample << ",\"rss_kib\":" << rss_kib()
            << ",\"live_native_bytes\":" << live_native_bytes() << ",\"network_reserved\":" << state.reserved_bytes
            << ",\"decoded_queued\":" << state.decoded_queued << "}}\n";
    }
    Request fast; fast.target = base + "/plain";
    for (unsigned round = 0; round < 5; ++round) {
        if (round) {
            auto consumed = driver.read(paused[(round - 1) % paused.size()], queue_bytes)->take();
            require(consumed.error.kind == ErrorKind::success && !consumed.eof && !consumed.bytes.empty(), "h2 bounded pause/resume pull");
        }
        auto sibling = driver.send(client, fast, options)->take();
        require(sibling.error.kind == ErrorKind::success && sibling.info && sibling.info->version == Version::http2 && sibling.bytes == "real-h2",
            "three paused h2 streams must not stall a fast sibling on the sole connection");
        peak = std::max(peak, rss_kib()); peak_live = std::max(peak_live, live_native_bytes());
        std::cout << "{\"h2_resume_sample\":{\"round\":" << round << ",\"rss_kib\":" << rss_kib()
            << ",\"live_native_bytes\":" << live_native_bytes() << "}}\n";
    }
    std::cout << "Three paused 512 MiB h2 responses and fast siblings: RSS " << baseline << ".." << peak
        << " KiB; live requested native allocations " << baseline_live << ".." << peak_live
        << " bytes; one connection, actual HTTP/2\n";
    if (baseline_live) require(peak_live - baseline_live <= driver.snapshot().reserved_bytes,
        "h2 paused live allocations exceed admitted native reservation");
    else if (baseline) require(peak <= baseline + 80 * 1024, "h2 paused provider RSS exceeds measured regression envelope");
    for (auto& transfer : paused) require(driver.close(transfer)->take().error.kind == ErrorKind::success, "h2 paused close");
    require(driver.close(client)->take().error.kind == ErrorKind::success, "h2 paused client close");
}
