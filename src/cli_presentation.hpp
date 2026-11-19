#pragma once
#include "void/compiler_observation.hpp"
#include "void/project_resolution.hpp"
#include "void/path.hpp"
#include "frontend/project_files.hpp"
#include "cli_report_path.hpp"
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/JSON.h>
#include <llvm/Support/raw_ostream.h>
#include <iomanip>
#include <locale>
#include <memory>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#endif

namespace vd::cli {
// New cosmetic writes must not terminate a successful compilation on a closed
// pipe. Block only this thread's SIGPIPE during this write, consume only the
// signal it generated, then restore the caller's mask. No signal handler or
// process-wide cancellation policy is installed.
inline void status_write(std::string_view text) noexcept {
#ifdef _WIN32
    auto handle = GetStdHandle(STD_ERROR_HANDLE);
    auto saved = GetLastError();
    while (!text.empty()) {
        DWORD count = 0;
        if (!WriteFile(handle, text.data(), static_cast<DWORD>(text.size()), &count, nullptr) || !count) break;
        text.remove_prefix(count);
    }
    SetLastError(saved);
#else
    auto saved = errno;
    sigset_t pipe, previous, pending;
    sigemptyset(&pipe); sigaddset(&pipe, SIGPIPE);
    if (pthread_sigmask(SIG_BLOCK, &pipe, &previous)) return;
    sigpending(&pending);
    bool already_pending = sigismember(&pending, SIGPIPE) == 1;
    bool broken = false;
    while (!text.empty()) {
        auto count = ::write(STDERR_FILENO, text.data(), text.size());
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { broken = errno == EPIPE; break; }
        text.remove_prefix(static_cast<std::size_t>(count));
    }
    if (broken && !already_pending) {
        sigpending(&pending);
        if (sigismember(&pending, SIGPIPE)) { int signal; sigwait(&pipe, &signal); }
    }
    pthread_sigmask(SIG_SETMASK, &previous, nullptr);
    errno = saved;
#endif
}
inline bool interactive_status() noexcept {
    if (std::getenv("CI")) return false;
    const auto* term = std::getenv("TERM");
    if (term && (std::string_view(term)=="dumb" || std::string_view(term)=="unknown")) return false;
#ifdef _WIN32
    DWORD mode; auto saved = GetLastError();
    bool result = GetConsoleMode(GetStdHandle(STD_ERROR_HANDLE), &mode);
    SetLastError(saved); return result;
#else
    auto saved = errno;
    bool result = term && *term && isatty(STDERR_FILENO);
    errno = saved; return result;
#endif
}
inline std::string seconds(observation::Clock::duration value) {
    if (value < std::chrono::milliseconds(10)) return "<0.01s";
    std::ostringstream out; out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(2) << std::chrono::duration<double>(value).count() << 's';
    return out.str();
}
struct PresentationOptions {
    std::string status = "auto", format = "text";
    bool timings = false, status_seen = false, format_seen = false;
    std::optional<std::filesystem::path> output;
};
class Presentation {
    using Clock = observation::Clock;
    Clock::time_point start{}, build_end{}, app_start{}, app_end{};
    bool started = false, built = false, app_started = false, app_returned = false, status = false;
    bool configured = false, authorization_wait = false;
    std::string_view command, outcome = "failure";
    std::string cache = "not_applicable";
    std::optional<std::string> profile, optimization, debug_info;
    std::optional<bool> project_mode;
    std::optional<int> application_status;
    bool application_contract = false;
    PresentationOptions options;
    observation::Sink sink;
    std::optional<observation::Binding> binding;
    std::unique_ptr<project::FreshProjectFile> report;
    std::array<bool, observation::names.size()> announced{};
    void line(std::string_view label, std::string_view value) noexcept {
        if (!status) return;
        try { status_write(std::string(label) + std::string(label.size()<11 ? 11-label.size() : 2, ' ') + std::string(value) + '\n'); } catch (...) {}
    }
    static void enter(observation::Phase phase, void* context) noexcept {
        auto& self = *static_cast<Presentation*>(context);
        if (phase==observation::Phase::authorization_wait) self.authorization_wait=true;
        auto index = static_cast<unsigned>(phase);
        if (self.announced[index]) return;
        self.announced[index]=true;
        switch (phase) {
        case observation::Phase::frontend: self.line("Checking", "project sources"); break;
        case observation::Phase::typed_ir: self.line("Compiling", "source"); break;
        case observation::Phase::native_link_driver: self.line("Linking", "executable"); break;
        case observation::Phase::package_assembly: self.line("Packaging", "application"); break;
        default: break;
        }
    }
    std::string operation() const {
        return command=="check" ? "check" : command=="emit" ? "object emission" : command=="package" ? "package" : "build";
    }
    llvm::json::Value json(int code) const {
        using namespace llvm::json;
        auto nullable = [](const std::optional<std::string>& v) -> Value {return v ? Value(*v) : Value(nullptr);};
        Array phases;
        for (unsigned i=0; i<observation::names.size(); ++i) {
            auto phase = static_cast<observation::Phase>(i);
            const auto& record=sink.records[i];
            std::string state, reason;
            if (record.count) state=record.partial ? "partial" : "complete";
            else if (record.skipped) {state="skipped"; reason="optimization_disabled";}
            else if (cache=="hit" && (phase==observation::Phase::typed_ir || phase==observation::Phase::llvm_ir ||
                    phase==observation::Phase::llvm_verification || phase==observation::Phase::llvm_optimization || phase==observation::Phase::native_object_emission)) {
                state="skipped"; reason="cache_hit";
            } else { state="not_reached"; reason=built ? "not_applicable" : "earlier_failure"; }
            bool frontend_child = i>=1 && i<=7;
            phases.emplace_back(Object{{"id",std::string(observation::names[i])},
                {"parent_id", frontend_child && sink.records[0].count ? Value("frontend") : Value(nullptr)},
                {"kind",record.count>1 ? "aggregate" : record.count ? "scope" : "unavailable"},
                {"state",state}, {"duration_us",record.count ? Value(observation::microseconds(record.duration)) : Value(nullptr)},
                {"count",static_cast<std::int64_t>(record.count)}, {"reason",reason.empty()?Value(nullptr):Value(reason)}});
        }
        auto end=built?build_end:Clock::now();
        return Object{{"schema","void.cli.timings"},{"schema_version",1},{"tool","voidc"},
            {"compiler_version",VOID_COMPILER_VERSION},{"compiler_build_identity",std::string(compiler_build_identity())},
            {"llvm_version",LLVM_VERSION_STRING},{"target",LLVM_DEFAULT_TARGET_TRIPLE},
            {"command",std::string(command)},{"execution_mode",command=="run"?"jit":command=="check"?"check":command=="package"?"package":"aot"},
            {"configuration",Object{{"profile",nullable(profile)},{"optimization",nullable(optimization)},
                {"debug_info",nullable(debug_info)},{"project_mode",project_mode?Value(*project_mode):Value(nullptr)}}},
            {"cache",Object{{"status",cache}}},
            {"clock",Object{{"kind","monotonic"},{"unit","us"},
                {"nominal_tick_ns",double(Clock::period::num)*1e9/Clock::period::den},{"effective_resolution_us",nullptr}}},
            {"build",Object{{"outcome",std::string(built?"success":outcome)},
                {"wall_duration_us",observation::microseconds(end-start)}, {"includes_authorization_wait",authorization_wait},{"complete",built}}},
            {"application",Object{{"entry_contract",app_started?Value(application_contract?"application":"legacy_i32"):Value(nullptr)},
                {"state",app_returned?"returned":app_started?"interrupted":"not_started"},
                {"wall_duration_us",app_returned?Value(observation::microseconds(app_end-app_start)):Value(nullptr)},
                {"exit_code",application_status && application_contract?Value(*application_status):Value(nullptr)},
                {"legacy_result",application_status && !application_contract?Value(*application_status):Value(nullptr)}}},
            {"process_exit_code",code}, {"phases",std::move(phases)},
            {"measurement",Object{{"durations","inclusive_wall"},{"phase_sum_is_total",false},{"detailed",true}}}};
    }
public:
    void begin(std::string_view value) noexcept {command=value; started=true; start=Clock::now();if(value=="run")cache="unknown";}
    void configure(PresentationOptions selected, const std::filesystem::path& source, const std::optional<std::filesystem::path>& artifact) {
        if (selected.status!="auto" && selected.status!="plain" && selected.status!="off") throw std::invalid_argument("--status requires auto, plain or off.");
        if ((selected.format!="text" && selected.format!="json") || (selected.format_seen && !selected.timings)) throw std::invalid_argument("--timings-format requires --timings and text or json.");
        if ((selected.format=="json") != bool(selected.output) || (selected.output && !selected.timings)) throw std::invalid_argument("JSON timings require --timings --timings-format json --timings-output PATH; text timings use stderr.");
        options=std::move(selected);
        if (options.output) {
            // Conservatively require reports outside all selected source trees
            // and manifest projects. Staging must not change a locked package.
            auto path=std::filesystem::absolute(*options.output).lexically_normal();
            auto source_root=std::filesystem::is_directory(source)?source:std::filesystem::absolute(source).parent_path();
            source_root=std::filesystem::weakly_canonical(std::filesystem::absolute(source_root));
            const auto location=std::filesystem::weakly_canonical(path);
            if (path.filename().empty() || report_within(source_root,location) || (artifact &&
                report_within(std::filesystem::weakly_canonical(std::filesystem::absolute(*artifact)),location)))
                throw std::invalid_argument("Timing report must be fresh and outside source and artifact trees.");
            for (const auto& part:path) {
                auto name=project::fold_project_name(path_utf8(part));
                if (name==".void" || name==".toolchains" || name=="libs" || name=="native" || name=="stdlib")
                    throw std::invalid_argument("Timing report cannot use a reserved source/resource directory.");
            }
            for (auto parent=path.parent_path();!parent.empty();) {
                if (project::creation_manifest_present(parent)) throw std::invalid_argument("Timing report must be outside manifest project trees.");
                auto next=parent.parent_path(); if(next==parent)break; parent=next;
            }
            project::require_outside_user_package_store(path);
            project::require_outside_native_approval_store(path,true);
            report=std::make_unique<project::FreshProjectFile>(path);
        }
        status=options.format!="json" && (options.status=="plain" || (options.status=="auto" && interactive_status()));
        sink.detailed=options.timings;
        sink.enter=&enter; sink.context=this;
        if (options.timings || status) binding.emplace(&sink);
        configured=true;
    }
    void project_context(bool project) {project_mode=project;}
    void configuration(bool project, std::optional<std::string> selected_profile, bool optimize, std::string debug) {
        project_mode=project; profile=std::move(selected_profile); optimization=optimize?"O2":"none"; debug_info=std::move(debug);
    }
    void cache_status(const std::string& value) {cache=value;}
    void failure(std::string_view value) noexcept {outcome=value;}
    void build_finished() noexcept {
        build_end=Clock::now(); built=true; binding.reset();
        try { line("Finished",(profile?(*profile=="debug"?"Debug ":"Release "):"")+operation()+" in "+seconds(build_end-start)+(authorization_wait?" (wall time includes authorization wait)":"")); } catch (...) {}
    }
    void application_begin(bool modern) noexcept {
        application_contract=modern; line("Running","JIT application"); app_started=true; app_start=Clock::now();
    }
    void application_finished(int code) noexcept {app_end=Clock::now();application_status=code;app_returned=true;}
    void finish(int code) noexcept {
        if (!started || !configured) return;
        binding.reset();
        try {
            if (!built) line("Failed",operation()+" after "+seconds(Clock::now()-start));
            if (!options.timings) return;
            if (report) {
                std::string text; llvm::raw_string_ostream out(text); out << json(code) << '\n';
                report->publish(text); return;
            }
            std::string text="Build wall time: "+seconds((built?build_end:Clock::now())-start)+" ("+std::string(built?"success":outcome)+")\n";
            for (unsigned i=0;i<observation::names.size();++i) {
                const auto& r=sink.records[i];
                if (r.count) text+="  "+std::string(observation::names[i])+": "+std::to_string(observation::microseconds(r.duration))+" us, "+std::to_string(r.count)+" scope(s)"+(r.partial?", partial":"")+"\n";
            }
            text+="Inclusive wall scopes overlap; phase sums are not the total.\n";
            if (app_returned) text+="Application wall time: "+seconds(app_end-app_start)+(application_contract?" (exit ":" (legacy result ")+std::to_string(*application_status)+")\n";
            status_write(text);
        } catch (const std::exception& error) {
            try {
                std::string escaped; llvm::raw_string_ostream out(escaped); llvm::printEscapedString(error.what(),out);
                status_write("Timing report failed: "+escaped+"\n");
            } catch (...) { status_write("Timing report failed.\n"); }
        } catch (...) { status_write("Timing report failed.\n"); }
    }
};
}
