#include "void/http_bulk.hpp"
#include <algorithm>

namespace vd::http {
namespace {
struct Cleanup {
    Driver& driver;
    std::shared_ptr<Transfer> transfer;
    ~Cleanup() { if (transfer) driver.close(transfer)->wait(); }
};
class Deadline {
    BulkStart start;
    std::int64_t total;
public:
    Deadline(BulkStart admitted, RequestOptions& options) : start(admitted), total(options.total_timeout_ms) {
        if (total) options.total_timeout_ms = std::max<std::int64_t>(1, total - elapsed());
    }
    std::int64_t elapsed() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    }
    bool check(Result& result) const {
        result.elapsed_ms = elapsed();
        if (total && result.elapsed_ms >= total) {
            if (result.error.kind == ErrorKind::success) result.error = {ErrorKind::timeout, Phase::file};
            return false;
        }
        return true;
    }
};
bool cancelled(const std::atomic<bool>* flag, Result& result) {
    if (!flag || !flag->load(std::memory_order_acquire)) return false;
    if (result.error.kind == ErrorKind::success) result.error = {ErrorKind::closed, Phase::cleanup};
    return true;
}
void file_error(BulkResult& result, filesystem::Result error) {
    result.io_cause = std::move(error);
    result.transfer.error = {ErrorKind::io_failure, Phase::file, result.io_cause.os_code,
        result.transfer.uploaded > 0 || result.transfer.info != nullptr, NativeDomain::filesystem};
}
}
BulkResult download(Driver& driver, const std::shared_ptr<Client>& client, Request request,
    const std::shared_ptr<filesystem::File>& file, RequestOptions options,
    const std::atomic<bool>* cancellation, BulkStart admitted) {
    BulkResult result;
    if (!file || request.body == Request::stream) {
        result.transfer.error = {ErrorKind::invalid_argument, Phase::prepare}; return result;
    }
    Deadline deadline(admitted, options);
    if (!deadline.check(result.transfer) || cancelled(cancellation, result.transfer)) return result;
    Error error;
    Cleanup cleanup{driver, driver.start(client, std::move(request), options, error)};
    if (!cleanup.transfer) { result.transfer.error = error; return result; }
    result.transfer = driver.response(cleanup.transfer)->take(cancellation);
    if (result.transfer.error.kind != ErrorKind::success) return result;
    if (!result.transfer.info || result.transfer.info->status < 200 || result.transfer.info->status >= 300) {
        result.transfer.error = {ErrorKind::http_status, Phase::headers}; return result;
    }
    for (;;) {
        result.transfer = driver.read(cleanup.transfer, queue_bytes)->take(cancellation);
        if (result.transfer.error.kind != ErrorKind::success || cancelled(cancellation, result.transfer)) return result;
        if (result.transfer.eof) break;
        filesystem::Request write(filesystem::Operation::write_bytes);
        write.file = file; write.bytes = std::move(result.transfer.bytes);
        auto output = filesystem::run(write, cancellation);
        result.file_completed += output.completed;
        if (output.error != filesystem::ErrorKind::success) { file_error(result, std::move(output)); return result; }
        if (!deadline.check(result.transfer) || cancelled(cancellation, result.transfer)) return result;
    }
    result.transfer = driver.finish(cleanup.transfer)->take(cancellation);
    deadline.check(result.transfer);
    return result;
}
BulkResult upload(Driver& driver, const std::shared_ptr<Client>& client, Request request,
    const std::shared_ptr<filesystem::File>& file, std::int64_t count, RequestOptions options,
    const std::atomic<bool>* cancellation, BulkStart admitted) {
    BulkResult result;
    if (!file || request.body != Request::empty || count < -1) {
        result.transfer.error = {ErrorKind::invalid_argument, Phase::prepare}; return result;
    }
    Deadline deadline(admitted, options);
    if (!deadline.check(result.transfer) || cancelled(cancellation, result.transfer)) return result;
    request.body = Request::stream; request.stream_length = count;
    Error error;
    Cleanup cleanup{driver, driver.start_buffered_upload(client, std::move(request), options, error)};
    if (!cleanup.transfer) { result.transfer.error = error; return result; }
    auto response = driver.buffered_result(cleanup.transfer);
    while (!response->ready() && (count < 0 || result.file_completed < count)) {
        if (!deadline.check(result.transfer) || cancelled(cancellation, result.transfer)) return result;
        filesystem::Request read(filesystem::Operation::read_bytes);
        read.file = file;
        read.first = count < 0 ? queue_bytes : std::min<std::int64_t>(queue_bytes, count - result.file_completed);
        auto input = filesystem::run(read, cancellation);
        result.file_completed += input.completed;
        if (input.error != filesystem::ErrorKind::success) { file_error(result, std::move(input)); return result; }
        if (!input.bytes.empty()) {
            result.transfer = driver.write(cleanup.transfer, std::move(input.bytes))->take(cancellation);
            if (result.transfer.error.kind == ErrorKind::upload_stopped || result.transfer.error.kind == ErrorKind::closed) break;
            if (result.transfer.error.kind != ErrorKind::success) return result;
        }
        if (input.eof) break;
    }
    if (!response->ready()) {
        error = driver.finish_upload(cleanup.transfer);
        if (error.kind != ErrorKind::success && error.kind != ErrorKind::closed) {
            result.transfer.error = error; return result;
        }
    }
    result.transfer = response->take(cancellation);
    deadline.check(result.transfer);
    return result;
}
}
