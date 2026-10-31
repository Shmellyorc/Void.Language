#pragma once
#include "void/http.hpp"
#include "void/filesystem.hpp"
#include <chrono>

namespace vd::http {
// Native file authority must be acquired on the owning executor before a bulk
// job is submitted. These kernels neither acquire source handles nor seek,
// flush, truncate or close the borrowed file. Run on the existing IO worker.
struct BulkResult {
    Result transfer;
    filesystem::Result io_cause;
    std::int64_t file_completed = 0;
};
using BulkStart = std::chrono::steady_clock::time_point;
BulkResult download(Driver&, const std::shared_ptr<Client>&, Request,
    const std::shared_ptr<filesystem::File>&, RequestOptions,
    const std::atomic<bool>* cancellation = nullptr, BulkStart = std::chrono::steady_clock::now());
BulkResult upload(Driver&, const std::shared_ptr<Client>&, Request,
    const std::shared_ptr<filesystem::File>&, std::int64_t count, RequestOptions,
    const std::atomic<bool>* cancellation = nullptr, BulkStart = std::chrono::steady_clock::now());
}
