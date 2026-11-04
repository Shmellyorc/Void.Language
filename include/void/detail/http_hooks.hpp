#pragma once
#include <string>
#include <cstdint>

namespace vd::http::testing {
struct Prerequisite {
    const char* target = nullptr;
    const char* decision = nullptr;
    std::int64_t connection_id = -1, request_bytes = 0;
    int tls_info_code = -1, alpn_query_code = -1, alpn_status = -1, alpn_length = 0;
    bool alpn_omitted = false;
};
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
    void (*prerequisite)(const Prerequisite&) noexcept = nullptr;
    // Model Schannel's successful, empty ALPN observation after a handshake
    // update. Can only remove evidence, never manufacture h2 negotiation.
    bool (*omit_schannel_alpn)(const char* target) noexcept = nullptr;
};
}
