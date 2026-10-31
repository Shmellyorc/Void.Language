#include "void/url.hpp"
#include "void/http_provider.hpp"
#include "void/utf8.hpp"
#include <curl/curl.h>
#if defined(__APPLE__)
#include <unicode/uidna.h>
#elif !defined(_WIN32)
#include <idn2.h>
#endif
#include <array>
#include <algorithm>
#include <charconv>
#include <memory>
#include <stdexcept>

namespace vd::url {
namespace {
struct Free { void operator()(char* p) const noexcept { curl_free(p); } };
struct Close { void operator()(CURLU* p) const noexcept { curl_url_cleanup(p); } };
using Url = std::unique_ptr<CURLU, Close>;
int hex(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}
bool unreserved(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~';
}
void append_encoded(std::string& output, std::string_view input, bool form) {
    constexpr char digits[] = "0123456789ABCDEF";
    for (const unsigned char c : input) {
        if (unreserved(c)) output += static_cast<char>(c);
        else if (form && c == ' ') output += '+';
        else { output += '%'; output += digits[c >> 4]; output += digits[c & 15]; }
    }
}
ErrorKind provider_error(CURLUcode code) {
    if (code == CURLUE_OUT_OF_MEMORY) throw std::bad_alloc();
    return code == CURLUE_UNSUPPORTED_SCHEME ? ErrorKind::unsupported_scheme : ErrorKind::invalid_url;
}
bool validate(std::string_view input, Result& result) {
    if (input.size() > max_url_bytes) { result.error = ErrorKind::limit_exceeded; return false; }
    for (std::size_t i = 0; i < input.size();) {
        const auto c = static_cast<unsigned char>(input[i]);
        if (c <= 32 || c == 127 || c == '\\') {
            result.error = ErrorKind::invalid_url; result.offset = static_cast<std::int64_t>(i); return false;
        }
        if (c == '%' && (i + 2 >= input.size() || hex(input[i + 1]) < 0 || hex(input[i + 2]) < 0)) {
            result.error = ErrorKind::invalid_encoding; result.offset = static_cast<std::int64_t>(i); return false;
        }
        auto size = utf8_scalar_bytes(input, i);
        if (!size) { result.error = ErrorKind::invalid_utf8; result.offset = static_cast<std::int64_t>(i); return false; }
        i += size;
    }
    return true;
}
bool absolute_prefix(std::string_view input) {
    auto colon = input.find(':');
    return colon != std::string_view::npos && input.substr(colon + 1, 2) == "//" &&
           colon + 3 < input.size() && input[colon + 3] != '/';
}
std::string get(CURLU* url, CURLUPart part, unsigned flags = 0, bool* present = nullptr) {
    char* raw = nullptr;
    const auto code = curl_url_get(url, part, &raw, flags);
    std::unique_ptr<char, Free> data(raw);
    if (code == CURLUE_OUT_OF_MEMORY) throw std::bad_alloc();
    if (present) *present = code == CURLUE_OK;
    if (code != CURLUE_OK) return {};
    return raw;
}
Result inspect(CURLU* url, bool proxy = false) {
    Result result;
    auto& address = result.address;
    address.scheme = get(url, CURLUPART_SCHEME);
    if (address.scheme != "http" && address.scheme != "https" &&
        !(proxy && (address.scheme == "socks5" || address.scheme == "socks5h"))) {
        result.error = ErrorKind::unsupported_scheme; return result;
    }
    bool user = false, password = false, zone = false;
    get(url, CURLUPART_USER, CURLU_GET_EMPTY, &user);
    get(url, CURLUPART_PASSWORD, CURLU_GET_EMPTY, &password);
    get(url, CURLUPART_ZONEID, CURLU_GET_EMPTY, &zone);
    if (user || password || zone) { result.error = ErrorKind::invalid_url; return result; }
    address.host = get(url, CURLUPART_HOST);
    if (address.host.empty()) { result.error = ErrorKind::invalid_url; return result; }
    if (std::any_of(address.host.begin(), address.host.end(), [](unsigned char c) { return c >= 128; })) {
#ifdef _WIN32
        address.host = get(url, CURLUPART_HOST, CURLU_PUNYCODE);
        if (address.host.empty()) { result.error = ErrorKind::invalid_url; return result; }
#elif defined(__APPLE__)
        // Source strings are UTF-8. The provider's locale conversion is not
        // appropriate here; use the same native IDN implementation explicitly.
        UErrorCode error = U_ZERO_ERROR;
        UIDNA* raw = uidna_openUTS46(UIDNA_CHECK_BIDI | UIDNA_NONTRANSITIONAL_TO_ASCII, &error);
        std::unique_ptr<UIDNA, decltype(&uidna_close)> converter(raw, uidna_close);
        if (error == U_MEMORY_ALLOCATION_ERROR) throw std::bad_alloc();
        if (U_FAILURE(error) || !raw) { result.error = ErrorKind::invalid_url; return result; }
        UIDNAInfo info = UIDNA_INFO_INITIALIZER;
        std::array<char, 512> ascii{};
        const auto count = uidna_nameToASCII_UTF8(raw, address.host.data(), static_cast<int32_t>(address.host.size()),
            ascii.data(), static_cast<int32_t>(ascii.size()), &info, &error);
        if (error == U_MEMORY_ALLOCATION_ERROR) throw std::bad_alloc();
        if (U_FAILURE(error) || info.errors || count <= 0 || static_cast<std::size_t>(count) >= ascii.size()) {
            result.error = ErrorKind::invalid_url; return result;
        }
        address.host.assign(ascii.data(), static_cast<std::size_t>(count));
#else
        // Source strings are UTF-8 independently of the embedding process's C
        // locale. curl's Unix IDN path uses idn2_lookup_ul (locale text). Apply
        // the same pinned IDN provider's explicit UTF-8 API, then give CURLU
        // the resulting ASCII host. Never call setlocale or change host state.
        std::uint8_t* ascii = nullptr;
        auto status = idn2_lookup_u8(reinterpret_cast<const std::uint8_t*>(address.host.c_str()),
                                     &ascii, IDN2_NONTRANSITIONAL);
        if (status != IDN2_OK && status != IDN2_MALLOC)
            status = idn2_lookup_u8(reinterpret_cast<const std::uint8_t*>(address.host.c_str()),
                                     &ascii, IDN2_TRANSITIONAL);
        std::unique_ptr<std::uint8_t, decltype(&idn2_free)> converted(ascii, idn2_free);
        if (status == IDN2_MALLOC) throw std::bad_alloc();
        if (status != IDN2_OK || !ascii) { result.error = ErrorKind::invalid_url; return result; }
        address.host = reinterpret_cast<const char*>(ascii);
#endif
    }
    for (auto& c : address.host) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    auto code = curl_url_set(url, CURLUPART_HOST, address.host.c_str(), 0);
    if (code != CURLUE_OK) { result.error = provider_error(code); return result; }
    auto port = get(url, CURLUPART_PORT);
    if (port.empty()) port = address.scheme == "http" ? "80" : address.scheme == "https" ? "443" : "1080";
    auto converted = std::from_chars(port.data(), port.data() + port.size(), address.port);
    if (converted.ec != std::errc{} || converted.ptr != port.data() + port.size() ||
        address.port < 1 || address.port > 65535) { result.error = ErrorKind::invalid_url; return result; }
    address.path = get(url, CURLUPART_PATH);
    address.query = get(url, CURLUPART_QUERY, CURLU_GET_EMPTY, &address.has_query);
    address.fragment = get(url, CURLUPART_FRAGMENT, CURLU_GET_EMPTY, &address.has_fragment);
    if (proxy && (address.path != "/" || address.has_query || address.has_fragment)) {
        result.error = ErrorKind::invalid_url; return result;
    }
    address.text = get(url, CURLUPART_URL, CURLU_NO_DEFAULT_PORT | CURLU_GET_EMPTY);
    if (address.text.empty() || !validate(address.text, result)) return result;
    return result;
}
Result parse_profile(std::string_view input, bool remove_dot_segments, bool proxy) {
    Result result;
    if (!validate(input, result)) return result;
    if (!absolute_prefix(input)) { result.error = ErrorKind::invalid_url; return result; }
    auto provider = http::ProviderLease::acquire();
    Url url(curl_url());
    if (!url) throw std::bad_alloc();
    const auto code = curl_url_set(url.get(), CURLUPART_URL, std::string(input).c_str(),
        CURLU_DISALLOW_USER | (remove_dot_segments ? 0 : CURLU_PATH_AS_IS) | (proxy ? CURLU_NON_SUPPORT_SCHEME : 0));
    if (code != CURLUE_OK) { result.error = provider_error(code); return result; }
    return inspect(url.get(), proxy);
}
}
Result parse(std::string_view input, bool remove_dot_segments) { return parse_profile(input, remove_dot_segments, false); }
Result parse_proxy(std::string_view input) { return parse_profile(input, false, true); }
Result resolve(std::string_view base, std::string_view reference) {
    auto result = parse(base);
    if (result.error != ErrorKind::success || !validate(reference, result)) return result;
    auto provider = http::ProviderLease::acquire();
    Url url(curl_url());
    if (!url) throw std::bad_alloc();
    auto code = curl_url_set(url.get(), CURLUPART_URL, result.address.text.c_str(), CURLU_DISALLOW_USER | CURLU_PATH_AS_IS);
    if (code == CURLUE_OK) {
        // RFC 3986 section 5.2.2: an empty reference retains the base path and
        // query, but its undefined fragment never inherits the base fragment.
        code = reference.empty() ? curl_url_set(url.get(), CURLUPART_FRAGMENT, nullptr, 0) :
            curl_url_set(url.get(), CURLUPART_URL, std::string(reference).c_str(), CURLU_DISALLOW_USER);
    }
    if (code != CURLUE_OK) { result.error = provider_error(code); return result; }
    return inspect(url.get());
}
std::string encode_component(std::string_view input, bool form) {
    std::string result;
    append_encoded(result, input, form);
    return result;
}
Result decode(std::string_view input, bool form, bool require_utf8) {
    Result result;
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%') {
            if (i + 2 >= input.size() || hex(input[i + 1]) < 0 || hex(input[i + 2]) < 0) {
                result.error = ErrorKind::invalid_encoding; result.offset = static_cast<std::int64_t>(i); return result;
            }
            result.bytes += static_cast<char>((hex(input[i + 1]) << 4) | hex(input[i + 2])); i += 2;
        } else result.bytes += form && input[i] == '+' ? ' ' : input[i];
    }
    if (require_utf8 && !valid_utf8(result.bytes)) result.error = ErrorKind::invalid_utf8;
    return result;
}
QueryResult parse_query(std::string_view input, bool form, std::int64_t max_items) {
    QueryResult result;
    if (max_items < 0 || input.size() > max_url_bytes) { result.error = ErrorKind::limit_exceeded; return result; }
    if (input.empty()) return result;
    for (std::size_t start = 0;;) {
        if (result.items.size() >= static_cast<std::uint64_t>(max_items)) { result.error = ErrorKind::limit_exceeded; return result; }
        auto end = input.find('&', start);
        auto item = input.substr(start, end == std::string_view::npos ? end : end - start);
        auto equal = item.find('=');
        auto name = decode(item.substr(0, equal), form);
        auto value = decode(equal == std::string_view::npos ? std::string_view{} : item.substr(equal + 1), form);
        if (name.error != ErrorKind::success || value.error != ErrorKind::success) {
            auto& failure = name.error != ErrorKind::success ? name : value;
            result.error = failure.error;
            if (failure.offset >= 0) result.offset = static_cast<std::int64_t>(start) + failure.offset +
                (name.error != ErrorKind::success ? 0 : static_cast<std::int64_t>(equal + 1));
            return result;
        }
        result.items.push_back({std::move(name.bytes), std::move(value.bytes), equal != std::string_view::npos});
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return result;
}
QueryResult encode_query(const std::vector<QueryItem>& input, bool form, std::size_t max_bytes) {
    QueryResult result;
    if (!input.empty() && input.size() - 1 > max_bytes) { result.error = ErrorKind::limit_exceeded; return result; }
    std::size_t encoded_bytes = input.empty() ? 0 : input.size() - 1;
    auto measure = [&](std::string_view text) {
        for (const unsigned char c : text) {
            const std::size_t bytes = unreserved(c) || (form && c == ' ') ? 1 : 3;
            if (bytes > max_bytes - encoded_bytes) return false;
            encoded_bytes += bytes;
        }
        return true;
    };
    for (const auto& item : input) {
        if (!valid_utf8(item.name) || !valid_utf8(item.value)) { result.error = ErrorKind::invalid_utf8; return result; }
        // Exact preflight includes escaping, separators and equals signs. A
        // rejected expansion never allocates a partially oversized body.
        bool fits = measure(item.name);
        if (fits && item.has_value) {
            fits = encoded_bytes < max_bytes;
            if (fits) { ++encoded_bytes; fits = measure(item.value); }
        }
        if (!fits) {
            result.error = ErrorKind::limit_exceeded; return result;
        }
    }
    result.text.reserve(encoded_bytes);
    bool first = true;
    for (const auto& item : input) {
        if (!first) result.text += '&';
        first = false;
        append_encoded(result.text, item.name, form);
        if (item.has_value) { result.text += '='; append_encoded(result.text, item.value, form); }
    }
    return result;
}
Result with_query(std::string_view input, const std::vector<QueryItem>& items) {
    auto result = parse(input);
    if (result.error != ErrorKind::success) return result;
    auto query = encode_query(items);
    if (query.error != ErrorKind::success) { result.error = query.error; return result; }
    auto provider = http::ProviderLease::acquire();
    Url url(curl_url());
    if (!url) throw std::bad_alloc();
    auto code = curl_url_set(url.get(), CURLUPART_URL, result.address.text.c_str(), CURLU_DISALLOW_USER | CURLU_PATH_AS_IS);
    if (code == CURLUE_OK) code = curl_url_set(url.get(), CURLUPART_QUERY, items.empty() ? nullptr : query.text.c_str(), 0);
    if (code != CURLUE_OK) { result.error = provider_error(code); return result; }
    return inspect(url.get());
}
}
