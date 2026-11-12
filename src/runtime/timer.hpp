#pragma once

#include "void/async.hpp"
#include "void/native_resource.hpp"
#include <chrono>
#include <map>
#include <optional>

namespace vd {

// A Runtime ledger resource driven by the existing async executor. Only
// shutdown accesses the queue off-executor. Entries own native tickets only.
class AsyncTimers final : public NativeResource {
public:
    using Clock = std::chrono::steady_clock;
    enum class Result : std::int32_t { accepted, invalid_duration, capacity, clock_range };
    static constexpr std::size_t capacity = 256;
    static constexpr std::int64_t max_seconds = 86'400;

    explicit AsyncTimers(Runtime& owner) : NativeResource(owner, nullptr) {}
    static std::shared_ptr<AsyncTimers> get(Runtime&);
    const void* kind() const noexcept override;
    Result start(const NativeCompletion&, std::int64_t seconds, std::int64_t nanoseconds,
                 Clock::time_point now);
    void cancel(const NativeCompletion&) noexcept;
    void expire(Clock::time_point now) noexcept;
    std::optional<Clock::time_point> next_deadline() noexcept;
    void request_stop() noexcept override;
    std::error_code close() override;

private:
    std::mutex mutex_;
    std::multimap<Clock::time_point, NativeCompletion> pending_;
    bool stopping_ = false;
};

} // namespace vd
