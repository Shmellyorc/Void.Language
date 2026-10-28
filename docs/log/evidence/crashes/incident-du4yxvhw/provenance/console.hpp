#pragma once
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <stdexcept>
#include <exception>
#include <utility>
#include <stop_token>
#include <thread>

namespace vd { class Runtime; }
namespace vd::console {
// Native embedding boundary only. There is no source representation of these types.
enum class Stream { input, output, error };
enum class Kind { unavailable, terminal, file, pipe, character, socket, other };
struct Endpoint {
    Stream stream;
    Kind kind;
    std::int64_t os_code=0;
    bool available() const noexcept { return kind!=Kind::unavailable; }
    bool redirected() const noexcept { return kind!=Kind::terminal; }
};
using Bytes=std::vector<std::uint8_t>;
enum class ErrorKind { none, invalid_argument, invalid_encoding, unsupported, would_block,
                       timed_out, closed, broken_pipe, interrupted, io_failure };
struct Error {
    ErrorKind kind=ErrorKind::none;
    std::int64_t os_code=0;
    std::int64_t cleanup_code=0; // Secondary failure never replaces the original.
    // Trailing UTF-16 units accepted from an incomplete output scalar. completed
    // counts complete source UTF-8 scalars only; this failure must not be retried.
    unsigned partial_utf16_units=0;
    explicit operator bool() const noexcept {return kind!=ErrorKind::none;}
};
enum class Key { character, enter, tab, backspace, escape, up, down, left, right, home, end, insert, delete_key, page_up, page_down, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, unknown };
struct PhysicalKey { Key key=Key::unknown; unsigned modifiers=0, repeat=1; bool known=false, down=true,down_known=false; };
struct Transfer {
    std::size_t completed=0;
    bool eof=false;
    Error error{};
    std::optional<PhysicalKey> key;
    Transfer(std::size_t n=0,bool end=false,Error problem={},std::optional<PhysicalKey> physical={}):completed(n),eof(end),error(problem),key(physical){}
};
struct ReadResult {
    Bytes bytes; // Includes ALL consumed bytes, also when transfer.error is set.
    Transfer transfer;
};
struct TextResult {
    std::string text; // Valid UTF-8 only; empty on error. Line terminators stripped.
    Bytes partial;   // All logically consumed bytes on error, including invalid bytes.
    Transfer transfer; // Count includes consumed line delimiters/CRLF continuation.
    std::size_t error_offset=0; // Invalid prefix start within partial; native errors at end.
};
struct KeyResult { Transfer transfer; Key key=Key::unknown; std::string character; Bytes bytes; unsigned modifiers=0,repeat=1; bool modifiers_known=false, key_down_known=false, key_down=true; };
struct ModeOptions { bool echo=true,line_input=true,control_c_as_input=false,preserve_interrupt=false; };
struct InputMode { alignas(std::max_align_t) std::array<std::uint8_t,256> native{}; };
struct ModeResult { Error error; std::int64_t token=0; };
enum Capability : std::uint64_t { text_input=1, text_output=2, key_input=4, input_modes=8,
    dimensions=16, cursor_position=32, cursor_visibility=64, color=128, clearing=256, title=512, bell=1024 };
enum class Presentation { size, cursor_position, set_cursor, cursor_visible, color, reset_color, clear, title, bell };
struct TerminalResult {
    Transfer transfer;
    std::int64_t first=0,second=0;
    Bytes sequence; // Prepared native control record, written by the existing output lease.
    bool response=false; // Parse a report using the existing input queue, never another reader.
};
struct Request {
    std::stop_token cancellation;
    std::optional<std::chrono::steady_clock::time_point> deadline;
    bool polling=false;
    bool keys=false;
};
class Adapter {
public:
    virtual ~Adapter()=default;
    // Implementations borrow endpoints. Inspection never reads or changes modes.
    virtual std::array<Endpoint,3> inspect() const=0;
    virtual std::chrono::steady_clock::time_point now() const noexcept=0;
    // One checked native transfer, including readiness. Spans MUST be native-owned,
    // never GC borrows. A stopped/deadline request may still complete an entered
    // syscall; the caller retains the span/lease until it actually returns.
    virtual std::uint64_t capabilities(Stream stream) {auto e=inspect().at(static_cast<unsigned>(stream));return e.available()?(stream==Stream::input?text_input:text_output):0;}
    virtual TerminalResult present(Stream,Presentation,std::int64_t,std::int64_t,std::string_view,const Request&) {TerminalResult r;r.transfer.error={ErrorKind::unsupported};return r;}
    virtual std::optional<TerminalResult> key_ready() {return {};} // Native peek, no event ownership transfer.
    virtual Error save_mode(InputMode&) {return {ErrorKind::unsupported};}
    virtual Error apply_mode(const InputMode&, ModeOptions) {return {ErrorKind::unsupported};}
    virtual Error restore_mode(const InputMode&) {return {ErrorKind::unsupported};}
    virtual std::vector<std::pair<std::string,Key>> key_sequences() {return {};}
    virtual Transfer read_key(std::span<std::uint8_t> bytes,const Request& request) {return read(bytes,request);}
    virtual Transfer read(std::span<std::uint8_t>, const Request&)=0;
    virtual Transfer write(Stream, std::span<const std::uint8_t>, const Request&)=0;
};
class ModeCleanupFailure final : public std::exception {
public:
    std::exception_ptr primary, secondary;
    Error cleanup;
    ModeCleanupFailure(std::exception_ptr cause,Error error,std::exception_ptr other={}) noexcept :primary(std::move(cause)),secondary(std::move(other)),cleanup(error){}
    const char* what() const noexcept override {return "Console operation and mode cleanup failed";}
};
class Closed final : public std::runtime_error {
public: Closed():std::runtime_error("console context is closing"){}
};
class Context;
class Operation;
// One shared input queue/decoder and one stdout/stderr record gate. No per-Runtime
// read-ahead ownership; native state survives individual Runtime sessions.
class Streams {
public:
    static std::shared_ptr<Streams> process();
    static std::shared_ptr<Streams> isolated(std::shared_ptr<Adapter>);
    ~Streams();
    Streams(const Streams&)=delete;
    Streams& operator=(const Streams&)=delete;
private:
    friend class Context;
    friend class Operation;
    friend struct Session;
    struct State;
    explicit Streams(std::shared_ptr<Adapter>);
    std::unique_ptr<State> state_;
};
struct Session;
class Context {
public:
    explicit Context(std::shared_ptr<Streams>);
    ~Context(); // Cancels; operation leases retain native state until release.
    Context(const Context&)=delete;
    Context& operator=(const Context&)=delete;
    Endpoint endpoint(Stream) const;
    std::shared_ptr<Streams> streams() const noexcept;
    Operation acquire(Stream); // NativeSafe; input before output if nesting.
    // NativeSafe host boundary. Input allocation/overflow throws normally so the
    // existing checked native boundary can report allocation failure. No fake IO error.
    ReadResult read(std::int64_t max_bytes, std::int64_t timeout_ms=-1); // Byte alias.
    ReadResult read_bytes(std::int64_t max_bytes, std::int64_t timeout_ms=-1);
    TextResult read_char(std::int64_t timeout_ms=-1);
    TextResult read_text(std::int64_t max_scalars, std::int64_t timeout_ms=-1);
    TextResult read_line(std::int64_t timeout_ms=-1);
    Transfer write(Stream, Bytes bytes, std::int64_t timeout_ms=-1);
    std::uint64_t capabilities(Stream); // NativeSafe, live endpoint capability snapshot.
    TerminalResult present(Stream,Presentation,std::int64_t a=0,std::int64_t b=0,std::string_view text={});
    KeyResult read_key(bool echo=false,std::int64_t timeout_ms=-1);
    TerminalResult key_available();
    ModeResult begin_mode(ModeOptions options={});
    Error end_mode(std::int64_t token);
    std::int64_t mode_checkpoint() const;
    Error finish_modes(std::int64_t checkpoint);
    Error restore_modes(); // Shutdown-only native cleanup after admitted input drains.
    void begin_close();       // NativeSafe; cancellation, no drain wait.
    Error close();            // NativeSafe; cancellation and drain; idempotent.
    struct Snapshot { bool closing; std::size_t admitted; };
    Snapshot snapshot() const; // NativeSafe; includes operations waiting for a gate.
private:
    friend class vd::Runtime;
    void check_drain() const;
    Operation acquire(Stream, const Request&);
    ModeResult begin_mode(ModeOptions,const Request&);
    TextResult read_text_impl(std::int64_t, bool line, std::int64_t);
    std::shared_ptr<Session> session_;
};
class Operation {
public:
    ~Operation(); // Release before returning to Managed; same thread as acquire.
    Operation(Operation&&);
    Operation(const Operation&)=delete;
    Operation& operator=(const Operation&)=delete;
    Operation& operator=(Operation&&)=delete;
    Endpoint endpoint() const;
    Adapter& adapter() const;
    std::stop_token cancellation() const;
    bool cancelled() const;
    Transfer read(std::span<std::uint8_t>, const Request&);
    Transfer write(std::span<const std::uint8_t>, const Request&);
private:
    friend class Context;
    struct InputReader;
    ReadResult read_bytes(std::size_t,const Request&);
    TextResult read_text(std::size_t,bool,const Request&);
    KeyResult read_key(const Request&);
    TerminalResult available(const Request&);
    Error restore_modes(std::int64_t checkpoint,bool owning_thread);
    TerminalResult before_report(const Request&);
    TerminalResult cursor_report(const Request&,std::int64_t,std::int64_t,std::size_t);
    Transfer read_native(std::span<std::uint8_t>,const Request&);
    Operation(std::shared_ptr<Session>,Stream) noexcept;
    void require_owner() const;
    void unlink() noexcept;
    static thread_local Operation* active_;
    std::shared_ptr<Session> session_;
    Stream stream_;
    std::thread::id owner_;
    Operation* previous_=nullptr;
};
}
