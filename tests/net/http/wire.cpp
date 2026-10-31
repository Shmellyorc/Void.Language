#include "void/http.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using namespace vd::http;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}
int main(int argc, char** argv) {
    try {
        require(argc == 2, "wire fixture URL required");
        Driver driver; Error error; auto client = driver.open({}, error); require(bool(client), "wire client");
        auto get = [&](std::string path, RequestOptions options = {}) {
            Request request; request.target = std::string(argv[1]) + path;
            return driver.send(client, request, options)->take();
        };
        auto plain = get("/chunked");
        require(plain.error.kind == ErrorKind::success && plain.bytes == "hello" && plain.trailers.size() == 1 &&
            plain.trailers[0].name == "X-End" && plain.trailers[0].value == "done", "independent chunk/trailer framing");
        auto large = get("/large-head-trailer");
        if (large.error.kind != ErrorKind::success) std::cerr << "large header/trailer kind=" << static_cast<int>(large.error.kind)
            << " phase=" << static_cast<int>(large.error.phase) << " code=" << large.error.native_code << '\n';
        require(large.error.kind == ErrorKind::success, "header and trailer budgets are independent");
        auto http10 = get("/http10");
        require(http10.error.kind == ErrorKind::success && http10.info->version == Version::http1_0 && http10.bytes == "old", "actual HTTP/1.0 response metadata");
        for (auto path : {"/conflicting-length", "/te-cl", "/switching", "/interim-overflow", "/forbidden-trailer"}) {
            auto result = get(path);
            if (result.error.kind != ErrorKind::protocol_error) {
                std::cerr << path << ": kind=" << static_cast<int>(result.error.kind) << " code=" << result.error.native_code << '\n';
                throw std::runtime_error("malformed wire must be classified as protocol failure");
            }
            require(result.error.remote_outcome_unknown, "response framing failure follows sent request bytes");
        }
        for (auto path : {"/large-field", "/field-overflow", "/trailer-overflow", "/large-trailer-field"})
            require(get(path).error.kind == ErrorKind::limit_exceeded, "independent header/trailer limit");
        for (auto path : {"/partial-length", "/partial-chunk"}) {
            auto result = get(path);
            require(result.error.kind == ErrorKind::unexpected_eof && result.bytes.starts_with("abc"), "truncated framing retains received prefix");
        }
        for (auto path : {"/gzip-crc", "/gzip-junk", "/raw-deflate", "/unknown-encoding", "/stacked-encoding"})
            require(get(path).error.kind == ErrorKind::invalid_encoding, "decoder rejects checksum, garbage and unsupported coding");
        auto empty = get("/gzip-empty");
        require(empty.error.kind == ErrorKind::success && empty.bytes.empty() && empty.info->content_decoded, "valid empty gzip");
        RequestOptions raw; raw.decoding = Decoding::raw;
        require(get("/unknown-encoding", raw).bytes == "opaque", "raw mode preserves unknown representation");
        RequestOptions read_timer; read_timer.read_timeout_ms = 20;
        auto stalled = get("/stall-body", read_timer);
        require(stalled.error.kind == ErrorKind::timeout && stalled.error.phase == Phase::download && stalled.bytes == "a" &&
            stalled.error.remote_outcome_unknown, "read deadline retains prefix and actual sent outcome");
        RequestOptions none; none.redirects = RedirectPolicy::none;
        auto untouched = get("/loop", none); require(untouched.error.kind == ErrorKind::success && untouched.info->status == 302, "explicit no-follow returns redirect");
        RequestOptions zero; zero.max_redirects = 0;
        auto limited = get("/loop", zero);
        require(limited.error.kind == ErrorKind::redirect_limit && limited.info && limited.info->status == 302 && limited.info->history.empty(), "zero-hop limit retains rejected response metadata");
        Request cross; cross.target = std::string(argv[1]) + "/cross"; cross.headers = {{"X-Secret", "caller"}};
        auto stripped = driver.send(client, cross, {})->take();
        require(stripped.error.kind == ErrorKind::success && stripped.bytes.empty() && stripped.info->history.size() == 1, "cross-origin GET drops every caller header");
        cross.headers[0].sensitive = true;
        auto denied = driver.send(client, cross, {})->take();
        require(denied.error.kind == ErrorKind::redirect_denied && denied.info && denied.info->status == 302, "sensitive redirect is denied with metadata");
        Request upload; upload.target = std::string(argv[1]) + "/early-303"; upload.method = "POST";
        upload.body = Request::stream; upload.stream_length = -1;
        auto transfer = driver.start(client, upload, {}, error); require(bool(transfer), "early redirect stream");
        auto final = driver.response(transfer)->take();
        require(final.error.kind == ErrorKind::success && final.info && final.info->status == 200 && final.info->history.size() == 1, "stream rewrites to GET after early 303");
        require(driver.write(transfer, "late")->take().error.kind == ErrorKind::upload_stopped &&
            driver.finish_upload(transfer).kind == ErrorKind::upload_stopped, "redirect permanently stops the abandoned producer");
        require(driver.read(transfer, 65536)->take().bytes == "get", "redirected response remains readable");
        require(driver.read(transfer, 65536)->take().eof, "redirected clean EOF");
        require(driver.finish(transfer)->take().error.kind == ErrorKind::success, "redirected transfer completion");
        require(driver.close(transfer)->take().error.kind == ErrorKind::success && driver.close(client)->take().error.kind == ErrorKind::success, "wire resource close");
        std::cout << "Independent malformed HTTP/1 framing, metadata, decoding, deadlines and redirect probes passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
