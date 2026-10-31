#pragma once
#include "void/http.hpp"
#include "void/native_resource.hpp"

namespace vd::http {
class TransferResource;
class FileUpload;
class FileDownload;
// Owner-side resource adapters. These objects contain Runtime handles and never
// enter DriverState, TransferState, provider callbacks or completion tickets.
class RuntimeDriver final : public NativeResource {
    std::unique_ptr<Driver> driver_;
public:
    explicit RuntimeDriver(Runtime&);
    ~RuntimeDriver() override;
    const void* kind() const noexcept override;
    std::error_code close() override;
    std::error_code quiesce() override { return close(); }
    void request_stop() noexcept override;
    Driver& driver() const noexcept { return *driver_; }
};
class ClientResource final : public NativeResource {
    void* identity_;
    std::mutex children_mutex_;
    std::vector<std::weak_ptr<TransferResource>> children_;
    std::mutex producers_mutex_;
    std::vector<std::weak_ptr<FileUpload>> producers_;
    std::mutex sinks_mutex_;
    std::vector<std::weak_ptr<FileDownload>> sinks_;
public:
    std::shared_ptr<RuntimeDriver> provider;
    std::shared_ptr<Client> client;
    ClientResource(Runtime&, void*, std::shared_ptr<RuntimeDriver>, std::shared_ptr<Client>);
    const void* kind() const noexcept override;
    std::error_code close() override;
    std::error_code quiesce() override { return close(); }
    void child(const std::shared_ptr<TransferResource>&);
    void producer(const std::shared_ptr<FileUpload>&);
    void stop_producers() noexcept;
    void sink(const std::shared_ptr<FileDownload>&);
    void prepare_sinks() noexcept;
    void finish_sinks() noexcept;
    void request_stop() noexcept override;
};
class TransferResource final : public NativeResource {
    void* identity_;
    std::mutex files_mutex_;
    std::vector<std::shared_ptr<filesystem::FileBorrow>> files_;
    std::shared_ptr<FileUpload> producer_;
public:
    std::atomic<bool> closed{false};
    std::shared_ptr<ClientResource> parent;
    std::shared_ptr<Transfer> transfer;
    TransferResource(Runtime&, void*, std::shared_ptr<ClientResource>, std::shared_ptr<Transfer>,
        std::vector<std::shared_ptr<filesystem::FileBorrow>> = {}, std::shared_ptr<FileUpload> = {});
    void* identity() const { own_thread(); return wrapper.get(); }
    const void* kind() const noexcept override;
    std::error_code close() override;
    std::error_code quiesce() override { return close(); }
    void release_files() noexcept;
    void stop_file_source() noexcept;
    void request_stop() noexcept override;
};
std::shared_ptr<RuntimeDriver> runtime_driver(Runtime&);
std::shared_ptr<ClientResource> open_client_resource(Runtime&, void*, ClientOptions, Error&);
std::shared_ptr<TransferResource> start_transfer_resource(Runtime&, void*,
    const std::shared_ptr<ClientResource>&, Request, RequestOptions, Error&,
    std::vector<std::shared_ptr<filesystem::FileBorrow>> = {}, std::shared_ptr<FileUpload> = {});
std::shared_ptr<ClientResource> find_client_resource(Runtime&, void*);
std::shared_ptr<TransferResource> find_transfer_resource(Runtime&, void*);
void track_transfer_lifetime(Runtime&, const std::shared_ptr<TransferResource>&,
    const TypeLayout&, const void* generation);

// The concrete source ABI job supplies only owner-side rooted publication. Its
// external completion path is shared by all network operations and uses no worker.
class CompletionJob : public ExternalNativeJob {
    Runtime& runtime_;
    std::shared_ptr<RuntimeDriver> provider_;
    std::shared_ptr<Operation> operation_;
    bool subscribed_ = false;
protected:
    virtual void cancel_native_sources() noexcept {}
    Result result_;
    CompletionJob(Runtime&, std::shared_ptr<RuntimeDriver>, std::shared_ptr<Operation>);
public:
    ~CompletionJob() override;
    void start(NativeCompletion) noexcept final;
    void cancel() noexcept final;
    void cancel_queued() noexcept final;
    void abandon_external(NativeCompletion) noexcept final;
    Result take_result(); // owner only, after ticket completion
};
}
