#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace vd::url {
inline constexpr std::size_t max_url_bytes = 16384;
// Private HTTP form payload encoding shares the URL percent codec, but uses
// the request snapshot budget instead of the serialized-address budget.
inline constexpr std::size_t max_form_bytes = 64 * 1024 * 1024;
enum class ErrorKind : std::int64_t {
    success, invalid_url, invalid_encoding, invalid_utf8, unsupported_scheme, limit_exceeded
};
struct Address {
    std::string text, scheme, host, path, query, fragment;
    std::int64_t port = 0;
    bool has_query = false, has_fragment = false;
};
struct Result {
    ErrorKind error = ErrorKind::success;
    std::int64_t offset = -1;
    Address address;
    std::string bytes;
};
struct QueryItem { std::string name, value; bool has_value = true; };
// One shared CURLU structural parser/profile for source URLs and HTTP redirects.
Result parse(std::string_view, bool remove_dot_segments = false);
// Private proxy authority profile; source url remains HTTP(S)-only. Shares all
// structural, UTF-8, IDN, userinfo, zone and percent validation with parse().
Result parse_proxy(std::string_view);
Result resolve(std::string_view base, std::string_view reference);
std::string encode_component(std::string_view, bool form = false);
Result decode(std::string_view, bool form = false, bool require_utf8 = true);
struct QueryResult {
    ErrorKind error = ErrorKind::success;
    std::int64_t offset = -1;
    std::vector<QueryItem> items;
    std::string text;
};
QueryResult parse_query(std::string_view, bool form = false, std::int64_t max_items = 1024);
QueryResult encode_query(const std::vector<QueryItem>&, bool form = false, std::size_t max_bytes = max_url_bytes);
Result with_query(std::string_view, const std::vector<QueryItem>&);
}
