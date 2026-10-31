#include "void/http_file.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace vd::http {
FileUpload::FileUpload(Runtime& runtime, std::shared_ptr<RuntimeDriver> provider,
    std::shared_ptr<NativeWorkers> workers, std::vector<UploadFile> files)
    : runtime_(runtime), provider_(std::move(provider)), workers_(std::move(workers)), files_(std::move(files)) {
    runtime_.begin_native_job(); admitted_ = true;
}
std::shared_ptr<FileUpload> FileUpload::prepare(Runtime& runtime, std::shared_ptr<RuntimeDriver> provider, std::vector<UploadFile> files) {
    if (!provider || files.empty() || files.size() > 128) throw std::invalid_argument("Invalid native HTTP file producer");
    for (const auto& file : files)
        if (!file.borrow || file.count < -1 || file.part < -1 || file.part >= 128 || (file.part < 0 && files.size() != 1))
            throw std::invalid_argument("Invalid native HTTP file input");
    auto workers = NativeWorkers::get(runtime);
    return std::shared_ptr<FileUpload>(new FileUpload(runtime, std::move(provider), std::move(workers), std::move(files)));
}
void FileUpload::retire() noexcept {
    if (admitted_) { admitted_ = false; runtime_.end_native_job(); }
}
FileUpload::~FileUpload() {
    if (admitted_) {
        source_->cancelled = true; source_->error = {ErrorKind::closed, Phase::prepare}; files_.clear(); retire();
        source_->done.store(true, std::memory_order_release); provider_->driver().notify_source();
    }
}
void FileUpload::start(std::shared_ptr<Transfer> transfer) {
    if (!transfer || transfer_) throw std::logic_error("Invalid HTTP file producer start");
    transfer_ = std::move(transfer); keepalive_ = shared_from_this();
    try { workers_->submit(*this); }
    catch (...) {
        keepalive_.reset(); source_->cancelled = true; source_->error = {ErrorKind::closed, Phase::queue}; files_.clear(); retire();
        source_->done.store(true, std::memory_order_release); provider_->driver().notify_source();
        throw;
    }
}
void FileUpload::run(Runtime& runtime) noexcept {
    Runtime::Native safe(runtime);
    auto& driver = provider_->driver();
    try {
        if (pending_) {
            auto sent = pending_->take(); pending_.reset(); ticket_.reset();
            if (sent.native_allocation_failure) throw std::bad_alloc();
            if (sent.error.kind != ErrorKind::success) {
                if (sent.error.kind != ErrorKind::closed && sent.error.kind != ErrorKind::upload_stopped) source_->error = sent.error;
                finished_ = true; return;
            }
        }
        if (source_->cancelled.load(std::memory_order_acquire) || file_index_ == files_.size()) { finished_ = true; return; }
        const auto& input = files_[file_index_];
        if (!current_) { remaining_ = input.count; current_ = true; at_eof_ = false; }
        if (!remaining_ || at_eof_) {
            const auto error = input.part < 0 ? driver.finish_upload(transfer_) : driver.finish_part(transfer_, static_cast<std::size_t>(input.part));
            if (error.kind != ErrorKind::success) {
                if (error.kind != ErrorKind::closed && error.kind != ErrorKind::upload_stopped) source_->error = error;
                finished_ = true; return;
            }
            ++file_index_; current_ = false; finished_ = file_index_ == files_.size(); return;
        }
        filesystem::Request read(filesystem::Operation::read_bytes);
        read.file = input.borrow->file();
        read.first = remaining_ < 0 ? queue_bytes : std::min<std::int64_t>(queue_bytes, remaining_);
        auto block = filesystem::run(read, &source_->cancelled);
        const auto previous = source_->file_completed.load(std::memory_order_relaxed);
        if (block.completed > std::numeric_limits<std::int64_t>::max() - previous) {
            source_->error = {ErrorKind::limit_exceeded, Phase::file}; finished_ = true; return;
        }
        source_->file_completed.store(previous + block.completed, std::memory_order_relaxed);
        if (block.error != filesystem::ErrorKind::success) {
            // A final response/redirect can stop an entered read. Its ordinary
            // cancellation is not a new file failure that overrides the HTTP
            // response; preserve genuine filesystem errors and actual progress.
            if (block.error == filesystem::ErrorKind::closed && source_->cancelled.load(std::memory_order_acquire)) {
                finished_ = true; return;
            }
            source_->error = {ErrorKind::io_failure, Phase::file, block.os_code, true, NativeDomain::filesystem};
            source_->io_cause = std::make_shared<const filesystem::Result>(std::move(block)); finished_ = true; return;
        }
        at_eof_ = block.eof;
        if (!block.bytes.empty()) {
            if (remaining_ >= 0) remaining_ -= static_cast<std::int64_t>(block.bytes.size());
            ticket_ = workers_->completion();
            pending_ = input.part < 0 ? driver.write(transfer_, std::move(block.bytes)) :
                driver.write_part(transfer_, static_cast<std::size_t>(input.part), std::move(block.bytes));
            if (!pending_->attach(*ticket_)) throw std::logic_error("Native file producer waiter admission failed");
        }
    } catch (const std::bad_alloc&) {
        source_->native_allocation_failure = true; source_->error = {ErrorKind::transport_failure, Phase::file}; finished_ = true;
    } catch (...) { source_->error = {ErrorKind::transport_failure, Phase::file}; finished_ = true; }
}
void FileUpload::request_stop() noexcept {
    source_->cancelled.store(true, std::memory_order_release);
    if (transfer_) provider_->driver().close(transfer_);
}
void FileUpload::stop() noexcept {
    request_stop();
    if (!transfer_) {
        if (admitted_) { cancel_queued(runtime_); complete(); }
    } else workers_->cancel_queued(*this);
}
void FileUpload::cancel_queued(Runtime&) noexcept {
    files_.clear(); pending_.reset(); ticket_.reset(); finished_ = true;
    if (source_->error.kind == ErrorKind::success) source_->error = {ErrorKind::closed, Phase::cleanup};
    source_->done.store(true, std::memory_order_release); provider_->driver().notify_source();
}
void FileUpload::complete() noexcept {
    auto keepalive = std::move(keepalive_);
    auto source = source_; auto provider = provider_;
    files_.clear(); // Release worker-side borrows before the transport handoff.
    retire();
    source->done.store(true, std::memory_order_release);
    provider->driver().notify_source();
}
}

namespace vd::http {
FileDownload::FileDownload(Runtime& runtime, std::shared_ptr<RuntimeDriver> provider, std::shared_ptr<NativeWorkers> workers,
    std::shared_ptr<Transfer> transfer, std::shared_ptr<filesystem::FileBorrow> file, std::int64_t total,
    std::chrono::steady_clock::time_point admitted)
    : runtime_(runtime), provider_(std::move(provider)), workers_(std::move(workers)), transfer_(std::move(transfer)),
      file_(std::move(file)), output_(provider_->driver().file_result(transfer_)), close_ticket_(workers_->completion()), admitted_(admitted), total_(total) {
    runtime_.begin_native_job(); admitted_job_ = true;
    try { provider_->driver().attach_file_sink(transfer_, sink_); }
    catch (...) { admitted_job_ = false; runtime_.end_native_job(); throw; }
}
std::shared_ptr<FileDownload> FileDownload::prepare(Runtime& runtime, std::shared_ptr<RuntimeDriver> provider,
    std::shared_ptr<Transfer> transfer, std::shared_ptr<filesystem::FileBorrow> file, std::int64_t total,
    std::chrono::steady_clock::time_point admitted) {
    if (!provider || !transfer || !file) throw std::invalid_argument("Invalid native HTTP download");
    auto workers = NativeWorkers::get(runtime);
    return std::shared_ptr<FileDownload>(new FileDownload(runtime, std::move(provider), std::move(workers), std::move(transfer), std::move(file), total, admitted));
}
FileDownload::~FileDownload() {
    if (admitted_job_) {
        sink_->cancelled = true; sink_->done.store(true, std::memory_order_release);
        provider_->driver().close(transfer_); file_.reset(); runtime_.end_native_job();
    }
}
void FileDownload::next(std::shared_ptr<Operation> operation) {
    ticket_ = workers_->completion(); pending_ = std::move(operation);
    if (!pending_->attach(*ticket_)) throw std::logic_error("Native file download waiter admission failed");
}
void FileDownload::close() noexcept {
    sink_->done.store(true, std::memory_order_release); provider_->driver().notify_source();
    stage_ = closing; ticket_ = close_ticket_; pending_ = provider_->driver().close(transfer_);
    if (!pending_->attach(*ticket_)) std::terminate(); // one preallocated private cleanup waiter
}
void FileDownload::start() {
    if (started_ || ticket_) throw std::logic_error("Duplicate native file download start");
    next(provider_->driver().response(transfer_)); keepalive_ = shared_from_this(); started_ = true;
    try { workers_->submit(*this); }
    catch (...) { keepalive_.reset(); started_ = false; pending_->cancel(); throw; }
}
void FileDownload::run(Runtime& runtime) noexcept {
    Runtime::Native safe(runtime);
    try {
        auto value = pending_->take(); pending_.reset(); ticket_.reset();
        if (stage_ == closing) { stage_ = done; return; }
        result_ = std::move(value);
        if (result_.native_allocation_failure) throw std::bad_alloc();
        if (result_.error.kind == ErrorKind::success && (sink_->cancelled || output_->cancellation_))
            result_.error = {ErrorKind::closed, Phase::cleanup, 0, true};
        if (result_.error.kind != ErrorKind::success) { close(); return; }
        if (stage_ == headers) {
            if (!result_.info || result_.info->status < 200 || result_.info->status >= 300) {
                result_.error = {ErrorKind::http_status, Phase::headers}; close(); return;
            }
            stage_ = reading;
        } else if (stage_ == reading) {
            if (!result_.bytes.empty()) {
                filesystem::Request write(filesystem::Operation::write_bytes); write.file = file_->file(); write.bytes = std::move(result_.bytes);
                auto written = filesystem::run(write, &sink_->cancelled); written_ += written.completed;
                if (written.error != filesystem::ErrorKind::success) {
                    result_.error = {ErrorKind::io_failure, Phase::file, written.os_code, true, NativeDomain::filesystem};
                    result_.io_cause = std::make_shared<const filesystem::Result>(std::move(written)); close(); return;
                }
                if (sink_->cancelled || output_->cancellation_) { result_.error = {ErrorKind::closed, Phase::cleanup, 0, true}; close(); return; }
            }
            if (result_.eof) stage_ = finishing;
        } else if (stage_ == finishing) { close(); return; }
        result_.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - admitted_).count();
        if (total_ && result_.elapsed_ms >= total_) { result_.error = {ErrorKind::timeout, Phase::file, 0, true}; close(); return; }
        next(stage_ == reading ? provider_->driver().read(transfer_, queue_bytes) : provider_->driver().finish(transfer_));
    } catch (const std::bad_alloc&) {
        result_.native_allocation_failure = true; result_.error = {ErrorKind::transport_failure, Phase::file}; close();
    } catch (...) { result_.error = {ErrorKind::transport_failure, Phase::file}; close(); }
}
void FileDownload::request_stop() noexcept { sink_->cancelled = true; provider_->driver().close(transfer_); }
void FileDownload::cancel_queued(Runtime&) noexcept {
    request_stop(); sink_->done.store(true, std::memory_order_release); provider_->driver().notify_source();
}
void FileDownload::stop(bool drain) noexcept {
    request_stop();
    if (!started_ && admitted_job_) {
        cancel_queued(runtime_); parked_ = true;
    }
    if (drain && parked_) {
        parked_ = false;
        if (runtime_.is_managed()) { Runtime::Native safe(runtime_); drain_queued(runtime_); complete(); }
        else { drain_queued(runtime_); complete(); }
    } else if (workers_->cancel_queued(*this, drain) && !drain) parked_ = true;
}
void FileDownload::drain_queued(Runtime&) noexcept {
    provider_->driver().close(transfer_)->wait();
    if (stage_ != closing) {
        if (pending_ && pending_->ready()) result_ = pending_->take();
        if (result_.error.kind == ErrorKind::success) result_.error = {ErrorKind::closed, Phase::cleanup, 0, true};
    }
    stage_ = done;
}
void FileDownload::complete() noexcept {
    auto keepalive = std::move(keepalive_); auto output = output_;
    result_.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - admitted_).count();
    if (result_.error.kind == ErrorKind::success && total_ && result_.elapsed_ms >= total_)
        result_.error = {ErrorKind::timeout, Phase::file, 0, true};
    result_.file_completed = written_; result_.bytes.clear(); file_.reset();
    admitted_job_ = false; runtime_.end_native_job();
    output->complete(std::move(result_)); // final access through a pure native result/ticket
}
}
