#include "void/http_provider.hpp"
#include <curl/curl.h>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string_view>

namespace vd::http {
namespace {
struct Process {
    std::mutex mutex;
    std::size_t references = 0;
};
Process& process() { static Process state; return state; }
void require_features() {
    const auto* version = curl_version_info(CURLVERSION_NOW);
    constexpr int required = CURL_VERSION_SSL | CURL_VERSION_HTTP2 |
        CURL_VERSION_ASYNCHDNS | CURL_VERSION_IPV6 | CURL_VERSION_PSL |
        CURL_VERSION_LARGEFILE | CURL_VERSION_IDN | CURL_VERSION_THREADSAFE;
    if (!version || version->version_num != 0x081700 ||
        (version->features & required) != required || !version->ares ||
        !version->ssl_version || !version->nghttp2_version) {
        throw std::runtime_error("Pinned HTTP provider version/features do not match");
    }
    bool http = false, https = false;
    for (auto p = version->protocols; p && *p; ++p) {
        const std::string_view name(*p);
        if (name == "http") http = true;
        else if (name == "https") https = true;
        else throw std::runtime_error("Unexpected protocol in HTTP provider");
    }
    if (!http || !https) throw std::runtime_error("HTTP/HTTPS provider unavailable");
#ifdef __linux__
    if (std::string_view(version->ssl_version) != "AWS-LC/5.12.0" || !version->libidn)
        throw std::runtime_error("Pinned Linux TLS provider version does not match");
#elif defined(__APPLE__)
    if (std::string_view(version->ssl_version) != "AWS-LC/5.12.0")
        throw std::runtime_error("Pinned macOS TLS provider version does not match");
#elif defined(_WIN32)
    if (std::string_view(version->ssl_version) != "Schannel" || !(version->features & CURL_VERSION_UNICODE))
        throw std::runtime_error("Native Windows TLS provider does not match");
#endif
}
}
ProviderLease::ProviderLease() {
    auto& state = process();
    std::lock_guard lock(state.mutex);
    if (state.references == std::numeric_limits<std::size_t>::max())
        throw std::length_error("HTTP provider reference limit");
    if (!state.references) {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
            throw std::runtime_error("HTTP provider initialization failed");
        try { require_features(); }
        catch (...) { curl_global_cleanup(); throw; }
    }
    ++state.references;
}
ProviderLease::~ProviderLease() {
    auto& state = process();
    std::lock_guard lock(state.mutex);
    if (!state.references) std::terminate();
    if (!--state.references) curl_global_cleanup();
}
std::shared_ptr<const ProviderLease> ProviderLease::acquire() {
    return std::shared_ptr<const ProviderLease>(new ProviderLease);
}
ProviderFeatures provider_features() {
    auto lease = ProviderLease::acquire();
    const auto* version = curl_version_info(CURLVERSION_NOW);
#ifdef _WIN32
    const std::string idn = "WinIDN";
#elif defined(__APPLE__)
    const std::string idn = "Apple ICU UTS46";
#else
    const auto idn = std::string("libidn2/") + version->libidn;
#endif
    return {version->version, version->ssl_version, std::string("c-ares/") + version->ares,
            idn, true, true, true, true};
}
}
