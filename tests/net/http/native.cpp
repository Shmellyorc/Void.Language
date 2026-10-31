#include "void/http.hpp"
#include "void/http_bulk.hpp"
#include "void/detail/filesystem_hooks.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <charconv>
#include <cstdlib>
#include <ctime>
#ifndef _WIN32
#include <sys/stat.h>
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

void http_runtime_cases(const std::string&, const std::string&, std::uint64_t);
void http_h2_backpressure_cases(const std::string&, const std::string&);

namespace {
void require(bool condition, const char* context) { if (!condition) throw std::runtime_error(context); }
void success(const vd::http::Result& result, const char* context) {
    if (result.error.kind != vd::http::ErrorKind::success || result.native_allocation_failure) {
        std::cerr << context << ": kind=" << static_cast<int>(result.error.kind) << " code=" << result.error.native_code << '\n';
        throw std::runtime_error(context);
    }
    std::cerr << "passed: " << context << '\n';
}
double process_cpu() {
#ifdef _WIN32
    FILETIME created{},exited{},kernel{},user{};
    require(GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user)!=0,"process CPU measurement");
    ULARGE_INTEGER k{},u{};k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;
    u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;
    return static_cast<double>(k.QuadPart+u.QuadPart)/10000000;
#else
    return static_cast<double>(std::clock())/CLOCKS_PER_SEC;
#endif
}
template<class Send> void measure(const char* name, Send send) {
    for(unsigned n=0;n<8;++n)require(send().error.kind==vd::http::ErrorKind::success,"latency warmup");
    std::vector<double> samples; samples.reserve(64);
    const auto cpu=process_cpu();
    for(unsigned n=0;n<64;++n) {
        const auto start=std::chrono::steady_clock::now();
        require(send().error.kind==vd::http::ErrorKind::success,"latency request");
        samples.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
    }
    const auto cpu_seconds=process_cpu()-cpu;
    std::sort(samples.begin(),samples.end());
    std::cout<<"{\"http_latency\":{\"profile\":\""<<name<<"\",\"samples\":64,\"median_ms\":"<<samples[31]
        <<",\"p95_ms\":"<<samples[60]<<",\"process_cpu_seconds\":"<<cpu_seconds<<"}}\n";
}
}
int main(int argc, char** argv) {
    using namespace vd::http;
    try {
        require(argc == 13, "fixture arguments");
        Driver driver;
        ClientOptions settings; settings.cookies = true;
        Error error;
        auto client = driver.open(settings, error); require(bool(client), "client acquisition");
        const std::string base(argv[1]), secure(argv[2]);
        auto get = [&](const std::string& path, RequestOptions options = {}) {
            Request request; request.target = base + path; return driver.send(client, std::move(request), options)->take();
        };
        RequestOptions tiny; tiny.max_buffer_bytes=1024;
        measure("HTTP/1.1 plaintext",[&]{return get("/plain",tiny);});
        {
            auto result = get("/plain"); success(result, "buffered GET");
            require(result.info && result.info->status == 200 && result.info->version == Version::http1_1, "actual status/version");
            require(result.bytes == std::string("hello\0world", 11), "binary buffered body");
            std::vector<std::string> values;
            for (const auto& header : result.info->headers) if (header.name == "X-Order") values.push_back(header.value);
            require(values == std::vector<std::string>{"a", "b"}, "duplicate headers");
        }
        {
            Request request; request.target = base + "/headers?q=x%2Fy#not-sent";
            request.headers = {{"X-Empty", ""}, {"X-Test", "snapshot"}};
            auto result = driver.send(client, request, {})->take(); success(result, "headers/target");
            require(result.bytes == "/headers?q=x%2Fy||snapshot", "empty header and fragment exclusion");
            request.headers = {{"Host", "other"}};
            require(driver.send(client, request, {})->take().error.kind == ErrorKind::invalid_header, "framing header rejected");
        }
        for (auto method : {"POST", "PUT", "PATCH", "DELETE", "OPTIONS"}) {
            Request request; request.target = base + "/echo"; request.method = method; request.body = Request::bytes; request.data = "payload";
            auto result = driver.send(client, request, {})->take(); success(result, "method upload");
            require(result.bytes == std::string(method) + ":payload", "method and body");
        }
        {
            auto result = get("/gzip"); success(result, "concatenated gzip");
            require(result.bytes == std::string(200000, 'a') + std::string(200000, 'b') && result.info->content_decoded, "decoded concatenated members");
        }
        {
            auto result = get("/deflate"); success(result, "deflate"); require(result.bytes.size() == 140000, "deflate size");
            require(get("/bad-gzip").error.kind != ErrorKind::success, "truncated decoder rejected");
            RequestOptions limited; limited.max_decoded_bytes = 1000;
            require(get("/gzip", limited).error.kind == ErrorKind::limit_exceeded, "decoded budget enforced");
            RequestOptions raw; raw.decoding = Decoding::raw;
            auto encoded = get("/gzip", raw); success(encoded, "raw bytes"); require(!encoded.info->content_decoded && encoded.bytes.size() < 1000, "raw preserves encoding");
        }
        {
            Request request; request.target = base + "/redirect"; request.method = "POST"; request.body = Request::text; request.data = "discard";
            auto result = driver.send(client, request, {})->take(); success(result, "redirect");
            require(result.bytes == "GET:" && result.info->history.size() == 1 && result.info->history[0].method_changed, "303 rewrite");
            RequestOptions limit; limit.max_redirects = 2;
            require(get("/loop", limit).error.kind == ErrorKind::redirect_limit, "redirect limit");
        }
        {
            auto set = get("/set-cookie"); success(set, "session set");
            auto cookie = get("/cookie"); success(cookie, "session get"); require(cookie.bytes == "session=owned", "owned cookie jar");
            require(driver.clear_cookies(client).kind == ErrorKind::success, "cookie clear");
            require(get("/cookie").bytes.empty(), "cookie clear observed");
        }
        {
            Request request; request.target = base + "/slow";
            auto transfer = driver.start(client, request, {}, error); require(bool(transfer), "stream acquisition");
            auto info = driver.response(transfer)->take(); success(info, "headers before body"); require(info.info->status == 200, "header response");
            auto pending = driver.read(transfer, 65536);
            std::this_thread::sleep_for(std::chrono::milliseconds(20)); require(!pending->ready(), "read genuinely waits");
            auto release = get("/release"); success(release, "independent request progresses while read waits");
            auto body = pending->take(); success(body, "paused response read"); require(body.bytes == "released", "stream bytes");
            require(driver.read(transfer, 65536)->take().eof, "clean EOF");
            success(driver.finish(transfer)->take(), "stream finish"); success(driver.close(transfer)->take(), "stream close");
        }
        for (std::int64_t length : {-1, 150000}) {
            Request request; request.target = base + "/echo"; request.method = "POST"; request.body = Request::stream; request.stream_length = length;
            auto transfer = driver.start(client, request, {}, error); require(bool(transfer), "stream upload acquisition");
            for (auto count : {65536, 65536, 18928}) success(driver.write(transfer, std::string(count, 'u'))->take(), "bounded upload write");
            require(driver.finish_upload(transfer).kind == ErrorKind::success, "producer EOF");
            std::string received;
            for (;;) { auto block = driver.read(transfer, 65536)->take(); success(block, "upload response"); if (block.eof) break; received += block.bytes; }
            require(received == "POST:" + std::string(150000, 'u'), "known/unknown framed upload");
            success(driver.finish(transfer)->take(), "upload finish"); success(driver.close(transfer)->take(), "upload close");
        }
        {
            Request request; request.target = base + "/large?8388608";
            auto transfer = driver.start(client, request, {}, error); require(bool(transfer), "large stream admission");
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            auto before = driver.snapshot(); require(before.decoded_queued <= queue_bytes && before.encoded_queued <= queue_bytes, "consumer backpressure");
            require(driver.finish(transfer)->take().error.kind == ErrorKind::busy, "finish unread returns busy");
            std::size_t total = 0;
            for (;;) { auto block = driver.read(transfer, 65536)->take(); success(block, "large stream read"); if (block.eof) break;
                require(!block.bytes.empty() && std::all_of(block.bytes.begin(), block.bytes.end(), [](char c) { return c == 'x'; }), "stream block integrity"); total += block.bytes.size(); }
            require(total == 8388608, "sustained stream byte count"); success(driver.finish(transfer)->take(), "large finish"); success(driver.close(transfer)->take(), "large close");
        }
        {
            RequestOptions options; options.header_timeout_ms = 20;
            require(get("/delay-headers", options).error.kind == ErrorKind::timeout, "header timer");
            auto response = get("/status"); success(response, "404 remains response"); require(response.info->status == 404 && response.bytes == "inspectable", "status metadata/body");
        }
        {
            Request request; request.target = base + "/plain";
            RequestOptions options; options.total_timeout_ms = 500;
            auto transfer = driver.start(client, request, options, error); require(bool(transfer), "total deadline stream");
            success(driver.response(transfer)->take(), "total deadline headers");
            const auto before_pause = std::chrono::steady_clock::now();
            while (!driver.snapshot().decoded_queued && std::chrono::steady_clock::now() - before_pause < std::chrono::milliseconds(250))
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            require(driver.snapshot().decoded_queued == 11, "body queued before consumer pause");
            std::this_thread::sleep_for(std::chrono::milliseconds(550));
            require(driver.read(transfer, 65536)->take().bytes.size() == 11, "deadline preserves received prefix");
            require(driver.finish(transfer)->take().error.kind == ErrorKind::timeout, "total deadline covers detached consumer pause");
            success(driver.close(transfer)->take(), "timed out stream close");
        }
        {
            namespace fs = vd::filesystem;
            fs::Result status;
            auto file = fs::open(std::string(argv[9]) + "/bulk.bin", fs::OpenMode::create, status);
            require(file && status.error == fs::ErrorKind::success, "bulk file acquisition");
            auto file_call = [&](fs::Operation operation, std::int64_t first = 0, std::string bytes = {}) {
                fs::Request input(operation); input.file = file; input.first = first; input.bytes = std::move(bytes);
                auto output = fs::run(input); require(output.error == fs::ErrorKind::success, "bulk borrowed file remains usable"); return output;
            };
            file_call(fs::Operation::write_bytes, 0, "prefix");
            Request request; request.target = base + "/plain";
            auto downloaded = download(driver, client, request, file, {}); success(downloaded.transfer, "native bulk download");
            require(downloaded.file_completed == 11 && file_call(fs::Operation::position).number == 17, "download writes at existing cursor");
            request.target = base + "/status";
            auto rejected = download(driver, client, request, file, {});
            require(rejected.transfer.error.kind == ErrorKind::http_status && rejected.file_completed == 0 &&
                file_call(fs::Operation::position).number == 17, "non-2xx download never writes destination");
            file_call(fs::Operation::seek); require(file_call(fs::Operation::read_bytes, 17).bytes == std::string("prefixhello\0world", 17), "download preserves prefix");
            file_call(fs::Operation::seek); file_call(fs::Operation::write_bytes, 0, std::string(150000, 'f'));
            request = {}; request.target = base + "/echo"; request.method = "POST";
            for (auto count : {-1, 149900}) {
                file_call(fs::Operation::seek, 100);
                auto uploaded = upload(driver, client, request, file, count, {}); success(uploaded.transfer, "native bulk upload");
                require(uploaded.file_completed == 149900 && uploaded.transfer.bytes == "POST:" + std::string(149900, 'f'), "bulk source cursor/count/framing");
            }
            file_call(fs::Operation::seek, 149990);
            auto short_file = upload(driver, client, request, file, 100, {});
            require(short_file.transfer.error.kind == ErrorKind::unexpected_eof && short_file.file_completed == 10, "known bulk source short EOF");
            require(fs::close(*file).error == fs::ErrorKind::success, "borrowed file explicit close");
            auto hooks = std::make_shared<fs::testing::Hooks>(); hooks->write_failure_after = 23; hooks->write_chunk = 7;
            auto partial = fs::open(std::string(argv[9]) + "/partial.bin", fs::OpenMode::create, status, hooks);
            require(bool(partial), "partial file acquisition");
            request = {}; request.target = base + "/large?65536";
            auto failed = download(driver, client, request, partial, {});
            require(failed.transfer.error.kind == ErrorKind::io_failure && failed.io_cause.error == fs::ErrorKind::io_failure &&
                failed.file_completed == 23 && failed.io_cause.os_code != 0, "bulk partial write preserves IO cause/count");
            require(fs::close(*partial).error == fs::ErrorKind::success, "partial file explicit close");
        }
        {
            Request request; request.target = secure + "/plain";
            auto rejected = driver.send(client, request, {})->take(); require(rejected.error.kind == ErrorKind::tls_verification, "untrusted TLS rejected");
            ClientOptions trusted; trusted.tls.ca_bundle = argv[3];
            {
                namespace fs = vd::filesystem;
                const auto saved = fs::read_regular(argv[3], 1048576);
                require(saved.error == fs::ErrorKind::success && !saved.bytes.empty(), "regular bounded TLS input");
                auto store = [&](const std::string& path, std::string bytes) {
                    fs::Request output(fs::Operation::write_all); output.path = path;
                    output.first = static_cast<std::int64_t>(fs::OpenMode::create); output.bytes = std::move(bytes);
                    require(fs::run(output).error == fs::ErrorKind::success, "TLS input fixture write");
                };
                trusted.tls.ca_bundle = std::string(argv[9]) + "/trust-snapshot.pem";
                store(trusted.tls.ca_bundle, saved.bytes);
                auto frozen = driver.open(trusted, error); require(bool(frozen), "owned TLS trust snapshot");
                store(trusted.tls.ca_bundle, "changed after client creation");
                success(driver.send(frozen, request, {})->take(), "owned trust survives caller file mutation");
                success(driver.close(frozen)->take(), "TLS snapshot close");
                trusted.tls.ca_bundle = std::string(argv[9]) + "/oversized-trust.pem";
                store(trusted.tls.ca_bundle, std::string(1048577, 'x'));
                require(!driver.open(trusted, error) && error.kind == ErrorKind::limit_exceeded, "TLS material limit before provider parsing");
#ifndef _WIN32
                trusted.tls.ca_bundle = std::string(argv[9]) + "/trust-fifo";
                require(::mkfifo(trusted.tls.ca_bundle.c_str(), 0600) == 0, "TLS FIFO fixture");
                require(!driver.open(trusted, error) && error.kind == ErrorKind::tls_verification, "TLS FIFO cannot block the driver");
#endif
                trusted.tls.ca_bundle = argv[3];
            }
            auto tls = driver.open(trusted, error); require(bool(tls), "custom trust acquisition");
            auto accepted = driver.send(tls, request, {})->take(); success(accepted, "verified TLS"); require(accepted.bytes.size() == 11, "verified body");
            measure("HTTP/1.1 verified TLS",[&]{return driver.send(tls,request,tiny)->take();});
            for (auto kind : {Authentication::basic, Authentication::digest, Authentication::bearer}) {
                request.target = secure + (kind == Authentication::basic ? "/auth-basic" : kind == Authentication::digest ? "/auth-digest" : "/auth-bearer");
                request.auth.kind = kind; request.auth.user = "user"; request.auth.password = kind == Authentication::bearer ? "exact-token" : "password";
                request.method = "POST"; request.body = Request::bytes; request.data = "replayed-body";
                auto result = driver.send(tls, request, {})->take(); success(result, "TLS authentication");
                require(result.info->status == 200 && result.bytes == (kind == Authentication::digest ? "authenticated:replayed-body" : "authenticated"), "authentication and digest replay");
                request.auth.password = "wrong";
                auto denied = driver.send(tls, request, {})->take(); success(denied, "rejected credentials remain response");
                require(denied.info->status == 401 && denied.bytes == "denied", "final authentication failure metadata");
            }
            success(driver.close(tls)->take(), "TLS client close");
        }
        for (auto identity : {TlsOptions::pem, TlsOptions::pkcs12}) {
            ClientOptions options; options.tls.ca_bundle = argv[3]; options.tls.identity = identity;
            options.tls.certificate = argv[identity == TlsOptions::pem ? 6 : 8];
            options.tls.key = identity == TlsOptions::pem ? argv[7] : ""; options.tls.password = "integration";
#ifdef _WIN32
            if (identity == TlsOptions::pem) {
                require(!driver.open(options,error) && error.kind == ErrorKind::unsupported,"Schannel PEM identity is explicitly unsupported");
                continue;
            }
#endif
            auto mutual = driver.open(options, error); require(bool(mutual), "mutual TLS client acquisition");
            Request request; request.target = std::string(argv[5]) + "/plain";
            auto result = driver.send(mutual, request, {})->take(); success(result, "explicit mutual TLS identity");
            require(result.bytes.size() == 11, "mutual TLS response"); success(driver.close(mutual)->take(), "mutual TLS close");
        }
        for (int mode = 0; mode < 5; ++mode) {
            ClientOptions options; options.tls.ca_bundle = argv[3]; options.proxy.mode = ProxyOptions::explicit_proxy;
            if (mode < 2) options.proxy.endpoint = argv[10];
            else if (mode == 2) {
                options.proxy.endpoint = argv[11]; options.proxy.auth.kind = Authentication::basic;
                options.proxy.auth.user = "proxy"; options.proxy.auth.password = "password";
            } else options.proxy.endpoint = std::string(mode == 3 ? "socks5://127.0.0.1:" : "socks5h://127.0.0.1:") + argv[12];
            auto proxy = driver.open(options, error); require(bool(proxy), "proxy acquisition");
            Request request; request.target = mode == 0 || mode == 2 ? base + "/plain" :
                mode > 2 ? "http://localhost:" + base.substr(base.rfind(':') + 1) + "/plain" : secure + "/plain";
            RequestOptions route; route.ip_family = IpFamily::v4;
            auto result = driver.send(proxy, request, route)->take(); success(result, "explicit proxy transport"); require(result.bytes.size() == 11, "proxy response body");
            if (mode == 2) {
                request.target = secure + "/proxy-isolation"; request.auth.kind = Authentication::bearer; request.auth.password = "origin-only";
                auto isolated = driver.send(proxy, request, {})->take(); success(isolated, "verified HTTPS CONNECT proxy");
                require(isolated.bytes == "|Bearer origin-only", "proxy credentials isolated from origin");
            }
            success(driver.close(proxy)->take(), "proxy close");
        }
        {
            ClientOptions insecure; insecure.proxy.mode = ProxyOptions::explicit_proxy; insecure.proxy.endpoint = argv[10];
            insecure.proxy.auth.kind = Authentication::basic; insecure.proxy.auth.user = "proxy"; insecure.proxy.auth.password = "password";
            require(!driver.open(insecure, error) && error.kind == ErrorKind::invalid_argument, "unprotected proxy credentials rejected");
            ClientOptions snapshot; snapshot.proxy.mode = ProxyOptions::environment; snapshot.proxy.http_proxy = argv[10];
            auto proxy = driver.open(snapshot, error); require(bool(proxy), "owned environment proxy snapshot");
            Request request; request.target = base + "/plain";
            auto result = driver.send(proxy, request, {})->take(); success(result, "snapshot ignores poisoned ambient proxy");
            require(result.bytes.size() == 11, "environment proxy body"); success(driver.close(proxy)->take(), "environment proxy close");
        }
        {
            ClientOptions options; options.tls.ca_bundle = argv[3]; options.proxy.mode = ProxyOptions::explicit_proxy;
            options.proxy.endpoint = argv[11]; options.proxy.auth.kind = Authentication::digest;
            options.proxy.auth.user = "proxy"; options.proxy.auth.password = "password";
            auto proxy = driver.open(options, error); require(bool(proxy), "Digest proxy acquisition");
            Request request; request.target = base + "/plain";
            auto result = driver.send(proxy, request, {})->take(); success(result, "HTTPS forward proxy Digest exchange");
            require(result.info->status == 200 && result.bytes.size() == 11, "proxy challenge is not origin response");
            request.target = secure + "/plain";
            success(driver.send(proxy, request, {})->take(), "HTTPS CONNECT Digest exchange");
            success(driver.close(proxy)->take(), "Digest proxy close");
            options.proxy.auth.password = "wrong";
            proxy = driver.open(options, error); require(bool(proxy), "rejected proxy credentials acquisition");
            request.target = base + "/plain";
            require(driver.send(proxy, request, {})->take().error.kind == ErrorKind::proxy_failed, "forward proxy rejection classified");
            request.target = secure + "/plain";
            require(driver.send(proxy, request, {})->take().error.kind == ErrorKind::proxy_failed, "CONNECT proxy rejection classified");
            success(driver.close(proxy)->take(), "rejected proxy close");
        }
        if (std::string_view(argv[4]) != "-") {
            {
                ClientOptions through_proxy; through_proxy.tls.ca_bundle = argv[3];
                through_proxy.proxy.mode = ProxyOptions::explicit_proxy; through_proxy.proxy.endpoint = argv[11];
                through_proxy.proxy.auth.kind = Authentication::basic;
                through_proxy.proxy.auth.user = "proxy"; through_proxy.proxy.auth.password = "password";
                auto tunneled = driver.open(through_proxy, error); require(bool(tunneled), "h2 HTTPS proxy client");
                RequestOptions required; required.protocol = Protocol::http2_required;
                Request request; request.target = std::string(argv[4]) + "/plain";
                auto result = driver.send(tunneled, request, required)->take();
                success(result, "required h2 through verified HTTPS CONNECT proxy");
                require(result.info && result.info->version == Version::http2 && result.bytes == "real-h2", "inner origin ALPN controls required h2");
                request.target = secure + "/plain";
                auto incompatible = driver.send(tunneled, request, required)->take();
                require(incompatible.error.kind == ErrorKind::unsupported && !incompatible.error.remote_outcome_unknown,
                    "required h2 through double TLS rejects origin without h2 before request bytes");
                success(driver.close(tunneled)->take(), "h2 HTTPS proxy close");
            }
            {
                ClientOptions cookie_options; cookie_options.cookies = true; cookie_options.tls.ca_bundle = argv[3];
                cookie_options.max_connections = cookie_options.max_per_origin = cookie_options.max_idle = 1;
                auto owner = driver.open(cookie_options, error); require(bool(owner), "cookie budget client");
                for (unsigned offset = 0; offset < 1024; offset += 32) {
                    Request batch; batch.target = base + "/cookie-batch?" + std::to_string(offset) + ",32,1";
                    success(driver.send(owner, batch, {})->take(), "cookie count preparation");
                }
                Request race; race.target = std::string(argv[4]) + "/cookie-race";
                RequestOptions required; required.protocol = Protocol::http2_required;
                auto first = driver.send(owner, race, required);
                auto second = driver.send(owner, race, required);
                require(first->take().error.kind == ErrorKind::limit_exceeded, "first concurrent cookie overflow owns its error");
                success(second->take(), "sibling cookie mutation follows cleared jar");
                Request inspect; inspect.target = base + "/cookie";
                require(driver.send(owner, inspect, {})->take().bytes == "race1=v", "overflow clears jar before sibling headers");
                require(driver.clear_cookies(owner).kind == ErrorKind::success, "byte budget starts with empty jar");
                bool byte_limit = false;
                for (unsigned offset = 0; offset < 300; offset += 4) {
                    Request batch; batch.target = base + "/cookie-batch?" + std::to_string(offset) + ",4,4000";
                    auto response = driver.send(owner, batch, {})->take();
                    if (response.error.kind == ErrorKind::limit_exceeded) {
                        require(offset >= 200, "serialized cookie budget is independent of count limit");
                        byte_limit = true; break;
                    }
                    success(response, "cookie byte preparation");
                }
                require(byte_limit && driver.send(owner, inspect, {})->take().bytes.empty(), "serialized cookie overflow fails and clears jar");
                success(driver.close(owner)->take(), "cookie budget client close");
            }
            ClientOptions options; options.tls.ca_bundle = argv[3];
            options.max_connections = options.max_per_origin = options.max_idle = 1;
            auto h2 = driver.open(options, error); require(bool(h2), "HTTP/2 client acquisition");
            RequestOptions required; required.protocol = Protocol::http2_required;
            Request request; request.target = std::string(argv[4]) + "/plain";
            { auto result = driver.send(h2, request, required)->take(); success(result, "real HTTP/2 TLS negotiation");
              require(result.info->version == Version::http2 && result.bytes == "real-h2", "actual h2 metadata/body"); }
            auto measured=required; measured.max_buffer_bytes=1024;
            measure("HTTP/2 verified TLS",[&]{return driver.send(h2,request,measured)->take();});
            {
                Request fault;fault.target=std::string(argv[4])+"/large-header";
                auto result=driver.send(h2,fault,required)->take();
                require(result.error.kind==ErrorKind::limit_exceeded,"HTTP/2 inflated header budget");
                fault.target=std::string(argv[4])+"/reset";
                result=driver.send(h2,fault,required)->take();
                require(result.error.kind==ErrorKind::protocol_error && result.bytes=="abc","HTTP/2 reset preserves prefix");
                fault.target=std::string(argv[4])+"/goaway";
                result=driver.send(h2,fault,required)->take();success(result,"HTTP/2 graceful GOAWAY");
                require(result.bytes=="ok","GOAWAY completed response");
                fault.target=std::string(argv[4])+"/plain";
                result=driver.send(h2,fault,required)->take();success(result,"new request after GOAWAY");
            }
            request.target = std::string(argv[4]) + "/slow";
            auto transfer = driver.start(h2, request, required, error); require(bool(transfer), "h2 stream acquisition");
            success(driver.response(transfer)->take(), "h2 headers before data");
            auto pending = driver.read(transfer, 65536); require(!pending->ready(), "h2 read awaits peer");
            request.target = std::string(argv[4]) + "/release";
            { auto result = driver.send(h2, request, required)->take(); success(result, "h2 simultaneous stream on one allowed connection"); require(result.bytes == "ok", "h2 independent body"); }
            require(pending->take().bytes == "released", "h2 multiplexed progress");
            require(driver.read(transfer, 65536)->take().eof, "h2 clean EOF");
            success(driver.close(transfer)->take(), "h2 transfer close");
            request.target = std::string(argv[4]) + "/large";
            { auto result = driver.send(h2, request, required)->take(); success(result, "h2 DATA flow control"); require(result.bytes == std::string(2097152, 'h'), "h2 large payload"); }
            {
                Request mutating; mutating.target = std::string(argv[4]) + "/refused-post";
                mutating.method = "POST"; mutating.body = Request::bytes; mutating.data = "mutating-once";
                auto failed = driver.send(h2, mutating, required)->take();
                require(failed.error.kind == ErrorKind::transport_failure && failed.error.remote_outcome_unknown && failed.uploaded == 13,
                    "refused HTTP/2 stream preserves a failed mutating outcome");
            }
            request.target = std::string(argv[4]) + "/duplex";
            request.method = "POST"; request.body = Request::stream; request.stream_length = 150000;
            auto duplex = driver.start(h2, request, required, error); require(bool(duplex), "duplex acquisition");
            success(driver.response(duplex)->take(), "successful headers while producer waits");
            std::string echoed;
            std::size_t produced = 0;
            for (auto count : {60000, 60000, 30000}) {
                success(driver.write(duplex, std::string(count, 'd'))->take(), "duplex write after final success headers");
                produced += count;
                while (echoed.size() < produced) {
                    auto part = driver.read(duplex, 65536)->take(); success(part, "duplex simultaneous read");
                    require(!part.eof && !part.bytes.empty(), "duplex continues before producer EOF"); echoed += part.bytes;
                }
            }
            require(driver.finish_upload(duplex).kind == ErrorKind::success, "duplex producer EOF");
            require(driver.read(duplex, 65536)->take().eof && echoed == std::string(150000, 'd'), "duplex complete framing");
            success(driver.finish(duplex)->take(), "duplex finish"); success(driver.close(duplex)->take(), "duplex close");
            request.target = std::string(argv[4]) + "/early-error"; request.stream_length = -1;
            auto early = driver.start(h2, request, required, error); require(bool(early), "early error acquisition");
            auto early_info = driver.response(early)->take(); success(early_info, "early error metadata");
            require(early_info.info->status == 413, "early error status inspectable");
            require(driver.write(early, "late")->take().error.kind == ErrorKind::upload_stopped, "early error stops later upload");
            require(driver.read(early, 65536)->take().bytes == "bad", "early error body inspectable");
            success(driver.close(early)->take(), "early error close");
            request = {}; request.target = std::string(argv[4]) + "/slow";
            auto aborted = driver.start(h2, request, required, error); require(bool(aborted), "cancel stream acquisition");
            success(driver.response(aborted)->take(), "cancel headers");
            auto waiting = driver.read(aborted, 65536); waiting->cancel();
            auto cancelled = waiting->take();
            if (cancelled.error.kind != ErrorKind::closed)
                std::cerr << "cancel result: kind=" << static_cast<int>(cancelled.error.kind) << " code=" << cancelled.error.native_code << '\n';
            require(cancelled.error.kind == ErrorKind::closed, "native cancellation waits for detach");
            success(driver.close(aborted)->take(), "cancel cleanup idempotent");
            request.target = secure + "/must-not-send-h1";
            require(driver.send(h2, request, required)->take().error.kind != ErrorKind::success, "required h2 rejects HTTP/1 fallback");
            success(driver.close(h2)->take(), "h2 client close");
            http_h2_backpressure_cases(argv[4], argv[3]);
        }
        success(driver.close(client)->take(), "client close");
        std::uint64_t soak = 134217728;
        if (const auto* setting = std::getenv("VOID_HTTP_TEST_SOAK_BYTES")) {
            const std::string_view value(setting);
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), soak);
            require(parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() && soak >= 134217728 && soak <= 8589934592,
                "bounded soak fixture setting");
        }
        http_runtime_cases(base, argv[9], soak);
        std::cout << "Native HTTP/1.1/TLS, streaming, decoding, policy and backpressure checks passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
