#pragma once
#include <string>
#include <cstdint>

namespace vd::http::testing {
// Native test fixture authority only. No source configuration field, global
// resolver setting, environment mutation or generated callback is involved.
struct Hooks {
    std::string dns_servers;
    // Defer curl's next producer callback with its real upload rate limiter.
    // Used to test idle-producer deadlines independently of socket scheduling.
    std::int64_t upload_bytes_per_second = 0;
    // Exercise deadlines when curl has sent bytes without notifying XFERINFO.
    bool suppress_progress_notifications = false;
    // Native fixture diagnostics only. Called on the driver before handle
    // cleanup; text is borrowed for the call. Must not reenter the driver.
    void (*provider_error)(int code, const char* text) noexcept = nullptr;
};
}
