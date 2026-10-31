#pragma once
#include <array>
#include <string_view>
namespace vd::url {
inline constexpr std::string_view source_module = "/@stdlib/url.void";
enum class Operation : unsigned { parse, resolve, encode, decode, parse_query, encode_query, with_query };
struct SourceOperation { std::string_view name, arguments; }; // t string, b bool, s i64, q readonly query list
inline constexpr std::array source_operations = {
    SourceOperation{"_url_parse", "tb"}, SourceOperation{"_url_resolve", "tt"},
    SourceOperation{"_url_encode", "tb"}, SourceOperation{"_url_decode", "tbb"},
    SourceOperation{"_url_parse_query", "tbs"}, SourceOperation{"_url_encode_query", "qb"},
    SourceOperation{"_url_with_query", "tq"}
};
}
