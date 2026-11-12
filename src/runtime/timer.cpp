#include "timer.hpp"
#include <algorithm>
#include <stdexcept>

namespace vd {
namespace {
char timer_kind;
}

std::shared_ptr<AsyncTimers> AsyncTimers::get(Runtime& runtime) {
    auto ledger = NativeLibrary::get(runtime);
    if (auto existing = ledger->find(&timer_kind, true)) {
        if (existing->kind() != &timer_kind) throw std::logic_error("Invalid async timer resource");
        return std::static_pointer_cast<AsyncTimers>(existing);
    }
    auto timers = std::make_shared<AsyncTimers>(runtime);
    ledger->publish(&timer_kind, timers);
    return timers;
}

const void* AsyncTimers::kind() const noexcept { return &timer_kind; }

AsyncTimers::Result AsyncTimers::start(const NativeCompletion& ticket, std::int64_t seconds,
                                       std::int64_t nanoseconds, Clock::time_point now) {
    std::lock_guard lock(mutex_);
    if (stopping_ || ticket.cancellation_requested()) {
        ticket.complete_cancelled();
        return Result::accepted; // Cancellation prevents owner-side result publication.
    }
    constexpr auto nanos_per_second = 1'000'000'000;
    if (seconds < 0 || seconds > max_seconds || nanoseconds < 0 ||
        nanoseconds >= nanos_per_second || (seconds == max_seconds && nanoseconds != 0))
        return Result::invalid_duration;
    if (seconds == 0 && nanoseconds == 0) {
        ticket.complete();
        return Result::accepted;
    }
    // The bounded conversion fits in nanoseconds. Round up if the clock has
    // coarser ticks so a positive request cannot complete early.
    static_assert(std::ratio_greater_equal_v<Clock::period, std::nano>);
    const auto delay = std::chrono::ceil<Clock::duration>(
        std::chrono::nanoseconds{seconds * nanos_per_second + nanoseconds});
    if (now > Clock::time_point::max() - delay) return Result::clock_range;
    if (pending_.size() == capacity) return Result::capacity;
    pending_.emplace(now + delay, ticket);
    return Result::accepted;
}

void AsyncTimers::cancel(const NativeCompletion& ticket) noexcept {
    std::lock_guard lock(mutex_);
    const auto entry = std::find_if(pending_.begin(), pending_.end(),
        [&](const auto& timer) { return timer.second == ticket; });
    if (entry == pending_.end()) return;
    pending_.erase(entry);
    ticket.complete_cancelled();
}

void AsyncTimers::expire(Clock::time_point now) noexcept {
    std::lock_guard lock(mutex_);
    while (!pending_.empty() && pending_.begin()->first <= now) {
        auto entry = pending_.extract(pending_.begin());
        entry.mapped().complete();
    }
}

std::optional<AsyncTimers::Clock::time_point> AsyncTimers::next_deadline() noexcept {
    std::lock_guard lock(mutex_);
    if (pending_.empty()) return std::nullopt;
    return pending_.begin()->first;
}

void AsyncTimers::request_stop() noexcept {
    std::lock_guard lock(mutex_);
    stopping_ = true;
    while (!pending_.empty()) {
        auto entry = pending_.extract(pending_.begin());
        entry.mapped().complete_cancelled();
    }
}

std::error_code AsyncTimers::close() {
    request_stop();
    return {};
}

} // namespace vd
