#pragma once
#include "void/http_resource.hpp"
#include "void/filesystem_abi.hpp"
#include "void/native_work.hpp"

namespace vd::http {
struct UploadFile {
    std::int64_t part = -1; // -1 is a whole stream; otherwise a multipart index
    std::int64_t count = -1;
    std::shared_ptr<filesystem::FileBorrow> borrow;
};
// Pre-admit on the owner before transport registration. start() uses the same
// Runtime worker pool outside or inside an async root. No source callback runs.
class FileUpload final : public NativeWorkerItem, public std::enable_shared_from_this<FileUpload> {
    Runtime& runtime_;
    std::shared_ptr<RuntimeDriver> provider_;
    std::shared_ptr<NativeWorkers> workers_;
    std::shared_ptr<UploadSource> source_ = std::make_shared<UploadSource>();
    std::vector<UploadFile> files_;
    std::shared_ptr<Transfer> transfer_;
    std::shared_ptr<FileUpload> keepalive_;
    std::shared_ptr<Operation> pending_;
    std::optional<NativeCompletion> ticket_;
    std::size_t file_index_ = 0;
    std::int64_t remaining_ = -1;
    bool current_ = false, at_eof_ = false, finished_ = false;
    bool admitted_ = false;
    FileUpload(Runtime&, std::shared_ptr<RuntimeDriver>, std::shared_ptr<NativeWorkers>, std::vector<UploadFile>);
    void retire() noexcept;
public:
    static std::shared_ptr<FileUpload> prepare(Runtime&, std::shared_ptr<RuntimeDriver>, std::vector<UploadFile>);
    ~FileUpload() override;
    const std::shared_ptr<UploadSource>& source() const noexcept { return source_; }
    void start(std::shared_ptr<Transfer>);
    void stop() noexcept;
    void run(Runtime&) noexcept override;
    void request_stop() noexcept override;
    void complete() noexcept override;
    bool queue_ready() const noexcept override { return !ticket_ || ticket_->ready(); }
    bool finished() const noexcept override { return finished_; }
    void cancel_queued(Runtime&) noexcept override;
};
class FileDownload final : public NativeWorkerItem, public std::enable_shared_from_this<FileDownload> {
    Runtime& runtime_;
    std::shared_ptr<RuntimeDriver> provider_;
    std::shared_ptr<NativeWorkers> workers_;
    std::shared_ptr<Transfer> transfer_;
    std::shared_ptr<filesystem::FileBorrow> file_;
    std::shared_ptr<UploadSource> sink_ = std::make_shared<UploadSource>();
    std::shared_ptr<Operation> output_, pending_;
    std::optional<NativeCompletion> ticket_;
    NativeCompletion close_ticket_;
    std::shared_ptr<FileDownload> keepalive_;
    Result result_;
    bool parked_ = false; // owner-side deferred cleanup after client native close
    bool started_ = false;
    std::int64_t written_ = 0;
    std::chrono::steady_clock::time_point admitted_;
    std::int64_t total_;
    enum Stage { headers, reading, finishing, closing, done } stage_ = headers;
    bool admitted_job_ = false;
    FileDownload(Runtime&, std::shared_ptr<RuntimeDriver>, std::shared_ptr<NativeWorkers>, std::shared_ptr<Transfer>,
        std::shared_ptr<filesystem::FileBorrow>, std::int64_t, std::chrono::steady_clock::time_point);
    void next(std::shared_ptr<Operation>);
    void close() noexcept;
public:
    static std::shared_ptr<FileDownload> prepare(Runtime&, std::shared_ptr<RuntimeDriver>, std::shared_ptr<Transfer>,
        std::shared_ptr<filesystem::FileBorrow>, std::int64_t, std::chrono::steady_clock::time_point);
    ~FileDownload() override;
    const std::shared_ptr<Operation>& result() const noexcept { return output_; }
    void start();
    void stop(bool drain = true) noexcept;
    void run(Runtime&) noexcept override;
    void request_stop() noexcept override;
    void complete() noexcept override;
    bool queue_ready() const noexcept override { return !ticket_ || ticket_->ready(); }
    bool finished() const noexcept override { return stage_ == done; }
    void cancel_queued(Runtime&) noexcept override;
    void drain_queued(Runtime&) noexcept override;
};
}
