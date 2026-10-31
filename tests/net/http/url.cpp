#include "void/url.hpp"
#include "void/http_provider.hpp"
#include <curl/curl.h>
#include <atomic>
#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <thread>

namespace {
void check(bool value, const char* context) {
    if (!value) throw std::runtime_error(context);
}
}
int main() {
    using namespace vd::url;
    try {
        // A host owns its independent reference before/between/after VOID users.
        check(curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK, "host init");
        {
            auto first = vd::http::ProviderLease::acquire();
            auto second = vd::http::ProviderLease::acquire();
            auto parsed = parse("HTTPS://EXAMPLE.COM:443/a/../b//c%2Fd?x=1&x=2#frag");
            check(parsed.error == ErrorKind::success, "absolute parse");
            check(parsed.address.text == "https://example.com/a/../b//c%2Fd?x=1&x=2#frag", "canonical signed path");
            check(parsed.address.port == 443 && parsed.address.host == "example.com", "authority");
            check(parse("https://example.com/a/../b", true).address.path == "/b", "explicit dots");
            check(parse("http://[::1]:8080/?#").address.text == "http://[::1]:8080/?#", "IPv6/empty query and fragment");
            auto idn = parse("https://b\xc3\xbc" "cher.example/");
            if (idn.error != ErrorKind::success || idn.address.host != "xn--bcher-kva.example")
                std::cerr << "IDN test kind=" << static_cast<int>(idn.error) << " host=" << idn.address.host << '\n';
            check(idn.error == ErrorKind::success && idn.address.host == "xn--bcher-kva.example", "actual IDN mapping");
            auto relative = resolve("https://example.com/a/b?old#old", "../c?new#new");
            check(relative.error == ErrorKind::success && relative.address.text == "https://example.com/c?new#new", "relative resolution");
            check(resolve("https://example.com/a/../b?old#old", "").address.text == "https://example.com/a/../b?old",
                "empty reference retains base path/query and clears fragment");
            check(resolve("https://example.com/a?old#old", "#").address.text == "https://example.com/a?old#",
                "explicit empty fragment is retained");
            for (auto invalid : {"example.com", "http:/example.com", "http:///example.com", "https://u:p@example.com/",
                                 "http://example.com:0/", "http://example.com:65536/", "http://[fe80::1%25lo]/",
                                 "https://example.com/ x", "https://example.com/\\x", "https://example.com/%GG"})
                check(parse(invalid).error != ErrorKind::success, "unsafe URL rejection");
            check(parse(std::string("https://example.com/\0x", 22)).error != ErrorKind::success, "embedded NUL");
            check(parse("ftp://example.com/").error == ErrorKind::unsupported_scheme, "scheme");
            check(parse("https://example.com/" + std::string(max_url_bytes, 'a')).error == ErrorKind::limit_exceeded, "URL cap");
            check(encode_component("a b+/%\xc3\xa9") == "a%20b%2B%2F%25%C3%A9", "component encode");
            check(decode("a+b%00").bytes == std::string("a+b\0", 4), "plus/NUL component decode");
            check(decode("%FF").error == ErrorKind::invalid_utf8, "strict UTF8");
            check(decode("%FF", false, false).bytes == std::string("\xff", 1), "arbitrary bytes");
            check(decode("x%Q1").offset == 1, "escape offset");
            auto query = parse_query("x=1&x=2&flag&empty=&a%26b=c%3Dd");
            check(query.error == ErrorKind::success && query.items.size() == 5, "ordered query");
            check(!query.items[2].has_value && query.items[3].has_value, "bare versus empty");
            check(query.items[4].name == "a&b" && query.items[4].value == "c=d", "split before decoding");
            check(encode_query(query.items).text == "x=1&x=2&flag&empty=&a%26b=c%3Dd", "query round trip");
            check(parse_query("a+b=c%2Bd", true).items[0].name == "a b", "form space");
            check(encode_query({{"a b", "c+d", true}}, true).text == "a+b=c%2Bd", "form encode");
            check(encode_query({{"", "", false}, {"", "", false}}).text == "&", "empty item order");
            check(encode_query({{"name", std::string(32768, 'x'), true}}, true).error == ErrorKind::limit_exceeded, "URL query keeps 16 KiB limit");
            check(encode_query({{"name", std::string(32768, 'x'), true}}, true, max_form_bytes).text.size() == 32773, "HTTP form uses body limit");
            check(encode_query({{"x", "+", true}}, true, 5).text == "x=%2B", "exact encoded form limit");
            const auto expansion = encode_query({{"x", "+", true}}, true, 4);
            check(expansion.error == ErrorKind::limit_exceeded && expansion.text.empty(), "form expansion rejected before output allocation");
            check(encode_query({{"", "", false}}, true, 0).error == ErrorKind::success &&
                encode_query({{"", "", true}}, true, 0).error == ErrorKind::limit_exceeded, "zero budget includes equals signs");
            check(parse_query("a&b", false, 1).error == ErrorKind::limit_exceeded, "query count");
            check(with_query("https://example.com/p?old#f", {{"x", "a+b", true}}).address.text ==
                  "https://example.com/p?x=a%2Bb#f", "query replacement, encode once");
            std::atomic<bool> failed{false};
            std::thread parallel([&] {
                try { for (unsigned n = 0; n < 200; ++n)
                    if (parse("https://example.com/").error != ErrorKind::success) failed = true; }
                catch (...) { failed = true; }
            });
            for (unsigned n = 0; n < 200; ++n) {
                auto another = vd::http::ProviderLease::acquire();
                check(parse("http://example.com/").error == ErrorKind::success, "concurrent parse");
            }
            parallel.join();
            check(!failed, "concurrent provider leases");
        }
        auto* host = curl_easy_init();
        check(host != nullptr, "host survives VOID last release");
        curl_easy_cleanup(host);
        curl_global_cleanup();
        {
            auto lease = vd::http::ProviderLease::acquire(); auto* easy = curl_easy_init();
            check(easy != nullptr, "trust inspection allocation");
            char* bundle = nullptr; char* directory = nullptr;
            const auto bundle_status = curl_easy_getinfo(easy, CURLINFO_CAINFO, &bundle);
            const auto directory_status = curl_easy_getinfo(easy, CURLINFO_CAPATH, &directory);
#ifdef __linux__
            const bool native_locations = (bundle && std::filesystem::is_regular_file(bundle)) ||
                (directory && std::filesystem::is_directory(directory));
            curl_easy_cleanup(easy);
            check(bundle_status == CURLE_OK && directory_status == CURLE_OK && native_locations, "Linux distribution trust locations absent after configuration");
#else
            (void)bundle_status; (void)directory_status; curl_easy_cleanup(easy);
#endif
        }
        auto features = vd::http::provider_features();
        check(features.http2 && features.psl && features.ipv6, "required features");
        std::cout << "URL/provider native checks passed: " << features.transport << "; "
                  << features.tls << "; " << features.resolver << "; " << features.idn << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
