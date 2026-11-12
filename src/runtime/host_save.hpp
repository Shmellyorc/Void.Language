#pragma once

#include "host_internal.hpp"
#include "void/detail/filesystem_hooks.hpp"

#include <stdexcept>
#include <system_error>

namespace vd::host {
using filesystem::testing::SaveStep;

inline int save_fault(const filesystem::testing::Hooks* hooks, SaveStep step) {
    return hooks && hooks->save_error ? hooks->save_error(step) : 0;
}

inline void save_cleanup_error(Result& result, int code) {
    if (!code) return;
    if (!result.kind) {
        const auto failure = native_error(code);
        result.kind = failure.kind;
        result.code = failure.code;
    }
    if (!result.detail) result.detail = code;
}

// Both hosts implement the same state transitions. A successful native rename
// is recorded before directory synchronization or the retirement of any handle.
template<class Stage>
Result save_atomic(Stage& stage, const Request& request) {
    const auto& bytes = request.bytes.front();
    if (bytes.size() > byte_snapshot_limit) return error(12);
    if (request.numbers.front() != 0 && request.numbers.front() != 1) return error(6);
    const auto hooks = filesystem::testing::current();

    auto publish = [&] {
        auto ready = stage.prepare(request.numbers.front() != 0);
        if (ready.kind) return ready;
        if (auto code = save_fault(hooks.get(), SaveStep::before_create)) return native_error(code);
        ready = stage.create();
        if (ready.kind) return ready;
        auto written = filesystem::write_staged(stage.file(), bytes, hooks.get());
        if (written.error != filesystem::ErrorKind::success)
            return error(static_cast<std::int64_t>(written.error), written.os_code);
        if (auto code = save_fault(hooks.get(), SaveStep::content_sync)) return native_error(code);
        ready = stage.sync_file();
        if (ready.kind) return ready;
        if (auto code = save_fault(hooks.get(), SaveStep::before_publish)) return native_error(code);
        return stage.publish();
    };
    Result result;
    auto record = [&](const Result& failure) {
        result.kind = failure.kind;
        result.code = failure.code;
    };
    try {
        result = publish();
        if (result.flag) {
            const auto code = save_fault(hooks.get(), SaveStep::directory_sync);
            const auto synchronized = code ? native_error(code) : stage.sync_directory();
            record(synchronized);
            result.number = synchronized.kind == 0;
        }
    } catch (const std::system_error& e) {
        record(native_error(e.code().value()));
    } catch (const std::bad_alloc&) {
        record(error(12));
    } catch (const std::length_error&) {
        record(error(12));
    }

    if (stage.has_file()) {
        if (!result.flag) {
            auto code = save_fault(hooks.get(), SaveStep::remove_stage);
            if (!code) code = stage.remove();
            save_cleanup_error(result, code);
        }
        const auto closed = stage.close_file();
        const auto injected = save_fault(hooks.get(), SaveStep::close_stage);
        save_cleanup_error(result, closed ? closed : injected);
    }
    save_cleanup_error(result, stage.close_target());
    save_cleanup_error(result, stage.close_lock());
    return result;
}
}
