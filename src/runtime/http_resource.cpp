#include "void/http_resource.hpp"
#include "void/filesystem_abi.hpp"
#include "void/http_file.hpp"
#include <algorithm>
#include <stdexcept>

namespace vd::http {
namespace {
char driver_kind, client_kind, transfer_kind;
template<class T> std::shared_ptr<T> find(Runtime& runtime, void* identity, const void* kind) {
    auto ledger = NativeLibrary::existing(runtime);
    if (!ledger) return {};
    auto resource = ledger->find(identity, true);
    if (!resource) return {};
    if (resource->kind() != kind) throw std::logic_error("Invalid HTTP resource identity");
    resource->own_thread();
    return std::static_pointer_cast<T>(resource);
}
}
RuntimeDriver::RuntimeDriver(Runtime& owner) : NativeResource(owner, nullptr) {
    Runtime::Native safe(owner);
    driver_ = std::make_unique<Driver>();
}
RuntimeDriver::~RuntimeDriver() {
    // Final process-provider cleanup can run when the last client/job adapter
    // releases this lease. Keep destruction, not just the thread join, NativeSafe.
    if (runtime.is_managed()) { Runtime::Native safe(runtime); driver_.reset(); }
    else driver_.reset();
}
const void* RuntimeDriver::kind() const noexcept { return &driver_kind; }
std::error_code RuntimeDriver::close() {
    if (!driver_) return {};
    if (runtime.is_managed()) { Runtime::Native safe(runtime); return close(); }
    driver_->shutdown();
    return {};
}
void RuntimeDriver::request_stop() noexcept { if (driver_) driver_->request_stop(); }
ClientResource::ClientResource(Runtime& owner, void* object, std::shared_ptr<RuntimeDriver> driver, std::shared_ptr<Client> handle)
    : NativeResource(owner, object), identity_(object), provider(std::move(driver)), client(std::move(handle)) {}
const void* ClientResource::kind() const noexcept { return &client_kind; }
std::error_code ClientResource::close() {
    if (runtime.is_managed()) { Runtime::Native safe(runtime); return close(); }
    stop_producers(); prepare_sinks(); provider->driver().close(client)->wait(); finish_sinks();
    {
        std::lock_guard lock(children_mutex_);
        for (const auto& weak : children_) if (auto child = weak.lock()) child->close();
        children_.clear();
    }
    if (auto ledger = NativeLibrary::existing(runtime)) ledger->erase(identity_, this);
    wrapper.reset(nullptr); return {};
}
void ClientResource::child(const std::shared_ptr<TransferResource>& child) {
    own_thread();
    std::lock_guard lock(children_mutex_);
    std::erase_if(children_, [](const auto& weak) { auto value = weak.lock(); return !value || value->closed.load(); });
    children_.push_back(child);
}
void ClientResource::producer(const std::shared_ptr<FileUpload>& value) {
    own_thread(); std::lock_guard lock(producers_mutex_);
    std::erase_if(producers_, [](const auto& weak) { auto value = weak.lock(); return !value || value->source()->done.load(std::memory_order_acquire); });
    producers_.push_back(value);
}
void ClientResource::stop_producers() noexcept {
    std::lock_guard lock(producers_mutex_);
    for (const auto& weak : producers_) if (auto value = weak.lock()) value->stop();
}
void ClientResource::sink(const std::shared_ptr<FileDownload>& value) {
    own_thread(); std::lock_guard lock(sinks_mutex_);
    std::erase_if(sinks_, [](const auto& weak) { auto value = weak.lock(); return !value || value->result()->ready(); });
    sinks_.push_back(value);
}
void ClientResource::prepare_sinks() noexcept {
    std::lock_guard lock(sinks_mutex_);
    for (const auto& weak : sinks_) if (auto value = weak.lock()) value->stop(false);
}
void ClientResource::finish_sinks() noexcept {
    if (runtime.is_managed()) { Runtime::Native safe(runtime); finish_sinks(); return; }
    std::lock_guard lock(sinks_mutex_);
    for (const auto& weak : sinks_) if (auto value = weak.lock()) { value->stop(); value->result()->wait(); }
    sinks_.clear();
}
void ClientResource::request_stop() noexcept { provider->request_stop(); }
TransferResource::TransferResource(Runtime& owner, void* object, std::shared_ptr<ClientResource> client, std::shared_ptr<Transfer> handle,
    std::vector<std::shared_ptr<filesystem::FileBorrow>> files, std::shared_ptr<FileUpload> producer)
    : NativeResource(owner, object), identity_(object), files_(std::move(files)), producer_(std::move(producer)), parent(std::move(client)), transfer(std::move(handle)) {}
const void* TransferResource::kind() const noexcept { return &transfer_kind; }
std::error_code TransferResource::close() {
    if (runtime.is_managed()) { Runtime::Native safe(runtime); return close(); }
    if (producer_) producer_->stop();
    parent->provider->driver().close(transfer)->wait();
    release_files(); closed = true;
    if (auto ledger = NativeLibrary::existing(runtime)) ledger->erase(identity_, this);
    wrapper.reset(nullptr); return {};
}
void TransferResource::release_files() noexcept { std::lock_guard lock(files_mutex_); files_.clear(); }
void TransferResource::stop_file_source() noexcept { if (producer_) producer_->stop(); }
void TransferResource::request_stop() noexcept { parent->provider->request_stop(); }
std::shared_ptr<RuntimeDriver> runtime_driver(Runtime& runtime) {
    auto ledger = NativeLibrary::get(runtime);
    if (auto existing = ledger->find(&driver_kind, true)) {
        existing->own_thread(); return std::static_pointer_cast<RuntimeDriver>(existing);
    }
    auto provider = std::make_shared<RuntimeDriver>(runtime);
    ledger->publish(&driver_kind, provider);
    return provider;
}
std::shared_ptr<ClientResource> open_client_resource(Runtime& runtime, void* identity, ClientOptions options, Error& error) {
    auto provider = runtime_driver(runtime);
    std::shared_ptr<Client> client;
    { Runtime::Native safe(runtime); client = provider->driver().open(std::move(options), error); }
    if (!client) return {};
    try {
        auto resource = std::make_shared<ClientResource>(runtime, identity, provider, client);
        NativeLibrary::get(runtime)->publish(identity, resource);
        return resource;
    } catch (...) {
        Runtime::Native safe(runtime); provider->driver().close(client)->wait(); throw;
    }
}
std::shared_ptr<TransferResource> start_transfer_resource(Runtime& runtime, void* identity,
    const std::shared_ptr<ClientResource>& client, Request request, RequestOptions options, Error& error,
    std::vector<std::shared_ptr<filesystem::FileBorrow>> files, std::shared_ptr<FileUpload> producer) {
    if (!client) { error = {ErrorKind::closed, Phase::prepare}; return {}; }
    client->own_thread();
    std::shared_ptr<Transfer> transfer;
    { Runtime::Native safe(runtime); transfer = client->provider->driver().start(client->client, std::move(request), options, error); }
    if (!transfer) return {};
    try {
        // Start before publication: rollback can always cancel and drain the
        // admitted producer, including allocation/publication failure below.
        if (producer) { client->producer(producer); producer->start(transfer); }
        auto resource = std::make_shared<TransferResource>(runtime, identity, client, transfer, std::move(files), producer);
        client->child(resource);
        NativeLibrary::get(runtime)->publish(identity, resource);
        return resource;
    } catch (...) {
        if (producer) producer->stop();
        Runtime::Native safe(runtime); client->provider->driver().close(transfer)->wait(); throw;
    }
}
std::shared_ptr<ClientResource> find_client_resource(Runtime& runtime, void* identity) { return find<ClientResource>(runtime, identity, &client_kind); }
std::shared_ptr<TransferResource> find_transfer_resource(Runtime& runtime, void* identity) { return find<TransferResource>(runtime, identity, &transfer_kind); }
namespace {
class TransferLifetime final : public ExternalNativeJob {
    Runtime& runtime_;
    std::shared_ptr<TransferResource> resource_;
    std::shared_ptr<NativeLibrary> ledger_;
    std::shared_ptr<Operation> operation_;
    void* identity_;
public:
    TransferLifetime(Runtime& runtime, std::shared_ptr<TransferResource> resource, void* identity)
        : runtime_(runtime), resource_(std::move(resource)), ledger_(NativeLibrary::get(runtime)),
          operation_(resource_->parent->provider->driver().lifetime(resource_->transfer)), identity_(identity) {
        runtime_.begin_native_job();
    }
    ~TransferLifetime() override {
        resource_->close(); // NativeSafe, shielded, preallocated native close operation.
        ledger_->erase(identity_, resource_.get());
        runtime_.end_native_job();
    }
    void start(NativeCompletion ticket) noexcept override {
        if (!operation_->attach(ticket)) {
            RuntimeFailure failure; failure.message = "HTTP lifetime waiter admission failed"; ticket.complete(failure);
        }
    }
    void cancel() noexcept override { resource_->stop_file_source(); operation_->cancel(); }
    void cancel_queued() noexcept override { resource_->stop_file_source(); operation_->cancel(); }
    void publish(Runtime&, void*) override {}
    void abandon_external(NativeCompletion ticket) noexcept override { ticket.complete(); }
};
}
void track_transfer_lifetime(Runtime& runtime, const std::shared_ptr<TransferResource>& resource,
    const TypeLayout& layout, const void* generation) {
    resource->own_thread();
    // No public source handle or generated callback crosses into the ticket.
    // The existing ledger is the sole identity/ownership authority.
    void* identity = resource->identity();
    if (!identity) throw std::logic_error("Cannot track a closed HTTP transfer");
    void* hidden = nullptr;
    constexpr std::size_t zero = 0;
    Runtime::Slot slot{&hidden, sizeof(hidden), &zero, 1};
    Runtime::Frame root(runtime, &slot, 1);
    async_submit_external(&hidden, layout, generation,
        std::make_shared<TransferLifetime>(runtime, resource, identity), true);
}
CompletionJob::CompletionJob(Runtime& runtime, std::shared_ptr<RuntimeDriver> provider, std::shared_ptr<Operation> operation)
    : runtime_(runtime), provider_(std::move(provider)), operation_(std::move(operation)) { runtime_.begin_native_job(); }
CompletionJob::~CompletionJob() {
    if (!subscribed_) operation_->cancel(); // failed/cancelled task admission
    runtime_.end_native_job();
}
void CompletionJob::start(NativeCompletion ticket) noexcept {
    subscribed_ = operation_->attach(ticket);
    if (!subscribed_) {
        result_.error = {ErrorKind::limit_exceeded, Phase::queue}; ticket.complete();
    }
}
void CompletionJob::cancel() noexcept { cancel_native_sources(); if (subscribed_) operation_->cancel(); }
void CompletionJob::cancel_queued() noexcept { cancel_native_sources(); operation_->cancel(); }
void CompletionJob::abandon_external(NativeCompletion ticket) noexcept {
    // A completed network operation has no newly acquired source resource.
    // Streaming acquisition is owned by its separate activation-lifetime job.
    ticket.complete();
}
Result CompletionJob::take_result() {
    if (!subscribed_) return std::move(result_);
    auto result = operation_->take();
    if (result.native_allocation_failure) throw std::bad_alloc();
    return result;
}
}
