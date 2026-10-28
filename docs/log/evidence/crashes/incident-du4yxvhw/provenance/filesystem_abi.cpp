#include "void/filesystem_abi.hpp"
#include "void/detail/filesystem_hooks.hpp"
#include "void/filesystem.hpp"
#include "void/filesystem_ops.hpp"
#include "void/async.hpp"
#include "void/native_resource.hpp"
#include "void/runtime_abi.hpp"

#include <array>
#include <bit>
#include <charconv>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace vd::filesystem {
namespace {
char file_kind;

void close_failure(const char* context, std::int64_t code, RuntimeFailure* failure) noexcept {
    // Retain through the existing diagnostic owner; never export stack or
    // worker-local text to a later owner resumption.
    std::array<char, 192> text{};
    auto length = std::strlen(context);
    std::memcpy(text.data(), context, length);
    constexpr char suffix[] = "; native code=";
    std::memcpy(text.data() + length, suffix, sizeof(suffix) - 1);
    auto first = text.data() + length + sizeof(suffix) - 1;
    auto converted = std::to_chars(first, text.data() + text.size() - 1, code);
    *converted.ptr = '\0';
    RuntimeFailure detail;
    detail.message = text.data();
    retain_native_failure(detail, failure);
}

class FileResource final : public NativeResource {
public:
    std::shared_ptr<File> file;
    const void* identity;
    std::mutex mutex;
    std::condition_variable changed;
    unsigned active = 0;
    unsigned claims = 0;
    const void* claimant = nullptr;
    bool closing = false;

    FileResource(Runtime& runtime, void* object) : NativeResource(runtime, object), identity(object) {}

    const void* kind() const noexcept override { return &file_kind; }

    bool same_context() const noexcept {
        auto* current = async_execution_context();
        return claimant == current || (claimant && async_context_descends_from(claimant));
    }

    ErrorKind claim() {
        own_thread();
        std::lock_guard lock(mutex);
        if (closing) return ErrorKind::closed;
        if (active && !claims) return ErrorKind::busy;
        if (claims && !same_context()) return ErrorKind::busy;
        if (!claims) claimant = async_execution_context();
        ++claims;
        return ErrorKind::success;
    }

    void release() {
        own_thread();
        std::lock_guard lock(mutex);
        if (!claims || !same_context()) throw std::logic_error("Invalid file reservation release");
        if (!--claims) claimant = nullptr;
    }

    ErrorKind begin() {
        own_thread();
        std::lock_guard lock(mutex);
        if (closing) return ErrorKind::closed;
        if (active || (claims && !same_context())) return ErrorKind::busy;
        ++active;
        return ErrorKind::success;
    }

    void end() noexcept {
        std::lock_guard lock(mutex);
        if (!active) std::terminate();
        --active;
        changed.notify_all();
    }

    void forget() { wrapper.reset(nullptr); }

    std::error_code close() override {
        if (runtime.is_managed()) {
            Runtime::Native safe(runtime);
            return close();
        }
        {
            std::unique_lock lock(mutex);
            closing = true;
            changed.wait(lock, [&] { return !active; });
        }
        auto result = file ? filesystem::close(*file) : Result{};
        forget();
        if (result.error != ErrorKind::success) {
            return {static_cast<int>(result.os_code), std::system_category()};
        }
        return {};
    }
};

std::shared_ptr<FileResource> lookup(Runtime& runtime, void* identity) {
    auto ledger = NativeLibrary::existing(runtime);
    if (!ledger) return {};
    auto resource = ledger->find(identity, true);
    if (!resource) return {};
    if (resource->kind() != &file_kind) throw std::logic_error("Invalid native file identity");
    return std::static_pointer_cast<FileResource>(resource);
}

class Reply {
public:
    const ReplyLayout& layout;
    std::vector<std::byte> storage;
    std::array<std::size_t, 3> references;
    Runtime::Slot slot;
    Runtime::Frame roots;

    Reply(Runtime& runtime, const ReplyLayout& geometry)
        : layout(geometry), storage(geometry.size),
          references{geometry.fields[7], geometry.fields[8], geometry.fields[9]},
          slot{storage.data(), storage.size(), references.data(), references.size()},
          roots(runtime, &slot, 1) {}

    void** reference(unsigned field) {
        return reinterpret_cast<void**>(storage.data() + layout.fields[field]);
    }

    void scalar(unsigned field, std::int64_t value) {
        std::memcpy(storage.data() + layout.fields[field], &value, sizeof(value));
    }

    void build(Runtime& runtime, const Result& result) {
        scalar(0, static_cast<std::int64_t>(result.error));
        scalar(1, result.os_code);
        scalar(2, result.completed);
        scalar(3, result.cleanup_code);
        scalar(4, result.entries_processed);
        scalar(5, result.number);
        std::memcpy(storage.data() + layout.fields[6], &result.eof, sizeof(result.eof));
        runtime.string_into(reference(7), result.text);
        runtime.bytes_into(reference(8), result.bytes);
        auto native = runtime.native_array_layout(*layout.entry, 0);
        runtime.array_new(reference(9), native, true);
        if (layout.entry->count != 6 || layout.entry->fields[0].kind != 12 ||
            layout.entry->fields[1].kind != 12 || layout.entry->fields[2].kind != 2) {
            throw std::logic_error("Invalid native metadata geometry");
        }
        std::vector<std::byte> payload(layout.entry->bytes);
        std::array<std::size_t, 2> fields = {
            layout.entry->fields[0].offset, layout.entry->fields[1].offset
        };
        Runtime::Slot entry_slot{payload.data(), payload.size(), fields.data(), fields.size()};
        Runtime::Frame entry_roots(runtime, &entry_slot, 1);
        for (const auto& entry : result.entries) {
            runtime.string_into(reinterpret_cast<void**>(payload.data() + fields[0]), entry.path);
            runtime.string_into(reinterpret_cast<void**>(payload.data() + fields[1]), entry.name);
            auto kind = static_cast<std::int32_t>(entry.kind);
            std::memcpy(payload.data() + layout.entry->fields[2].offset, &kind, sizeof(kind));
            std::array<std::int64_t, 3> numbers = {
                entry.size, entry.modified_seconds, entry.modified_nanoseconds
            };
            for (unsigned n = 0; n < numbers.size(); ++n) {
                std::memcpy(payload.data() + layout.entry->fields[n + 3].offset, &numbers[n], sizeof(numbers[n]));
            }
            runtime.array_push(runtime.root_word(reference(9)), native->schema(), payload.data());
        }
    }

    void publish(Runtime& runtime, void* task) {
        for (unsigned field = 0; field < 7; ++field) {
            auto count = field == 6 ? sizeof(bool) : sizeof(std::int64_t);
            std::memcpy(static_cast<char*>(task) + layout.task_result + layout.fields[field],
                        storage.data() + layout.fields[field], count);
        }
        for (unsigned field = 7; field < 10; ++field) {
            runtime.store(task, layout.task_result + layout.fields[field], *reference(field));
        }
    }
};

class Job final : public NativeJob {
public:
    Runtime& runtime;
    const ReplyLayout& layout;
    Request request;
    Result result;
    std::shared_ptr<FileResource> resource;
    std::shared_ptr<NativeLibrary> ledger;
    bool pending = false;
    bool opening = false;
    bool acquisition = false;
    bool registered = false;
    bool noop = false;
    bool admitted = false;

    Job(Runtime& owner, const ReplyLayout& geometry, Request input)
        : runtime(owner), layout(geometry), request(std::move(input)) {
        runtime.begin_native_job();
        admitted = true;
    }

    void retire_admission() noexcept {
        if (std::exchange(admitted, false)) runtime.end_native_job();
    }

    ~Job() override {
        // Admission failure occurs before execution and owns no opened handle.
        // Executed acquisitions are transferred or drained before scope exit.
        if (pending) resource->end();
        retire_admission();
    }

    void cancel_queued() noexcept override {
        if (pending) {
            pending = false;
            resource->end();
        }
        if (!acquisition) retire_admission();
    }

    void execute(const std::atomic<bool>& cancellation, RuntimeFailure* failure) noexcept override {
        try {
            if (!noop && result.error == ErrorKind::success) {
                if (opening) {
                    resource->file = filesystem::open(request.path, static_cast<OpenMode>(request.first), result, request.hooks);
                    acquisition = static_cast<bool>(resource->file);
                } else result = filesystem::run(request, &cancellation);
            }
        } catch (const std::bad_alloc&) {
            failure->message = "Filesystem native allocation failed";
        } catch (const std::length_error&) {
            failure->message = "Filesystem extent is unrepresentable";
        } catch (const std::filesystem::filesystem_error& error) {
            result.error = ErrorKind::invalid_path;
            result.os_code = error.code().value();
        } catch (...) {
            failure->message = "Filesystem native invariant failed";
        }
        bool retired = pending && request.operation == Operation::close;
        cancel_queued();
        if (retired) {
            ledger->erase(const_cast<void*>(resource->identity));
            resource->forget();
        }
    }

    void publish(Runtime& owner, void* task) override {
        Reply reply(owner, layout);
        reply.build(owner, result);
        if (opening && acquisition) {
            ledger->publish(const_cast<void*>(resource->identity), resource);
            registered = true;
        }
        if (request.operation == Operation::close && resource && result.error != ErrorKind::busy) {
            ledger->erase(const_cast<void*>(resource->identity));
            resource->forget();
        }
        reply.publish(owner, task);
    }

    bool acquired() const noexcept override { return acquisition; }

    void commit() noexcept override {
        acquisition = false;
        retire_admission();
    }

    void abandon(RuntimeFailure* failure) noexcept override {
        auto cleanup = resource && resource->file ? filesystem::close(*resource->file) : Result{};
        acquisition = false;
        if (registered) {
            ledger->erase(const_cast<void*>(resource->identity));
            registered = false;
        }
        if (resource) resource->forget();
        retire_admission();
        if (cleanup.error != ErrorKind::success) {
            close_failure("Native file acquisition rollback close failed", cleanup.os_code, failure);
        }
    }
};

std::shared_ptr<Job> prepare(Runtime& runtime, const ReplyLayout& layout, unsigned operation,
                             void* const* references, const std::int64_t* numbers) {
    if (operation >= source_operations.size()) throw std::logic_error("Invalid native filesystem operation");
    Request request(static_cast<Operation>(operation));
    request.hooks = testing::current();
    const auto& contract = source_operations[operation];
    unsigned text = 0;
    unsigned number = 0;
    unsigned reference = 0;
    void* identity = nullptr;
    for (char argument : contract.arguments) {
        if (argument == 's') {
            auto value = numbers[number];
            switch (number++) {
            case 0: request.first = value; break;
            case 1: request.second = value; break;
            case 2: request.third = value; break;
            case 3: request.fourth = value; break;
            default: throw std::logic_error("Too many filesystem scalar arguments");
            }
        } else {
            auto* value = references[reference++];
            if (argument == 'f' || argument == 'F') identity = value;
            else if (argument == 't') {
                auto bytes = std::string(runtime.string_bytes(value));
                switch (text++) {
                case 0: request.path = std::move(bytes); break;
                case 1: request.target = std::move(bytes); break;
                case 2: request.pattern = std::move(bytes); break;
                default: throw std::logic_error("Too many filesystem path arguments");
                }
            } else if (argument == 'b') {
                // Register the copied reference word before asking the shared
                // array implementation for an independent owned snapshot.
                constexpr std::size_t zero = 0;
                Runtime::Slot slot{&value, sizeof(value), &zero, 1};
                Runtime::Frame root(runtime, &slot, 1);
                request.bytes = runtime.byte_array_bytes(runtime.root_word(&value));
            }
        }
    }
    if (request.operation == Operation::entries || request.operation == Operation::get_files ||
        request.operation == Operation::get_directories) request.pattern = std::move(request.target);
    auto job = std::make_shared<Job>(runtime, layout, std::move(request));
    if (job->request.operation == Operation::open) {
        job->ledger = NativeLibrary::get(runtime);
        if (job->ledger->find(identity, true)) throw std::logic_error("Duplicate file acquisition");
        job->resource = std::make_shared<FileResource>(runtime, identity);
        job->opening = true;
    } else if (identity) {
        job->ledger = NativeLibrary::existing(runtime);
        job->resource = lookup(runtime, identity);
        if (!job->resource) {
            if (operation != cleanup_operation && operation != release_operation &&
                operation != static_cast<unsigned>(Operation::close)) job->result.error = ErrorKind::closed;
            else job->noop = true;
        } else {
            job->resource->own_thread();
            if (operation == claim_operation) job->result.error = job->resource->claim();
            else if (operation == release_operation) job->resource->release();
            else {
                job->result.error = job->resource->begin();
                if (job->result.error == ErrorKind::success) {
                    job->pending = true;
                    job->request.file = job->resource->file;
                }
            }
        }
    }
    return job;
}
}
} // namespace vd::filesystem

extern "C" void vd_filesystem_call(void* output, const vd::filesystem::ReplyLayout* layout,
    unsigned operation, void* const* references, const std::int64_t* numbers,
    const void* generation, bool asynchronous, vd::RuntimeFailure* failure) noexcept {
    using namespace vd;
    using namespace vd::filesystem;
    std::shared_ptr<Job> job;
    try {
        auto& runtime = Runtime::current();
        job = prepare(runtime, *layout, operation, references, numbers);
        if (asynchronous) {
            TypeLayout task{layout->task_size, {}};
            if (layout->task_reference_count) {
                task.references.assign(layout->task_references,
                                       layout->task_references + layout->task_reference_count);
            }
            task.descriptor = runtime.descriptor(layout->task_key);
            async_submit_native(static_cast<void**>(output), task, generation, job);
            return;
        }
        if (operation == cleanup_operation) {
            if (job->result.error != ErrorKind::success) {
                failure->message = "File deinit cannot close a busy resource";
                return;
            }
            job->request.operation = Operation::close;
        }
        if (operation != claim_operation && operation != release_operation) {
            std::atomic<bool> cancellation{false};
            Runtime::Native safe(runtime);
            job->execute(cancellation, failure);
        }
        if (failure->message) {
            if (job->acquired()) {
                Runtime::Native safe(runtime);
                RuntimeFailure cleanup;
                job->abandon(&cleanup);
                if (cleanup.message) vd_cleanup_failure(failure, &cleanup);
            }
            return;
        }
        Reply reply(runtime, *layout);
        reply.build(runtime, job->result);
        if (job->opening && job->acquisition) {
            job->ledger->publish(const_cast<void*>(job->resource->identity), job->resource);
            job->registered = true;
            job->commit();
        }
        if (job->request.operation == Operation::close && job->resource &&
            job->result.error != ErrorKind::busy) {
            job->ledger->erase(const_cast<void*>(job->resource->identity));
            job->resource->forget();
        }
        std::memcpy(output, reply.storage.data(), layout->size);
        if (operation == cleanup_operation && job->result.error != ErrorKind::success) {
            close_failure("File deinit close failed", job->result.os_code, failure);
        }
    } catch (const std::bad_alloc&) {
        failure->message = "Filesystem boundary allocation failed";
    } catch (const std::length_error&) {
        failure->message = "Filesystem boundary extent is unrepresentable";
    } catch (...) {
        failure->message = "Filesystem boundary invariant failed";
    }
    if (job && job->acquired()) {
        Runtime::Native safe(Runtime::current());
        RuntimeFailure cleanup;
        job->abandon(&cleanup);
        if (cleanup.message) vd_cleanup_failure(failure, &cleanup);
    }
}

extern "C" std::int64_t vd_file_control(unsigned operation, void* identity,
                                         vd::RuntimeFailure* failure) noexcept {
    using namespace vd;
    using namespace vd::filesystem;
    try {
        auto& runtime = Runtime::current();
        auto resource = lookup(runtime, identity);
        if (operation == release_operation) {
            if (resource) resource->release();
            return 0;
        }
        if (operation == claim_operation) {
            return static_cast<std::int64_t>(resource ? resource->claim() : ErrorKind::closed);
        }
        if (operation != cleanup_operation) throw std::logic_error("Invalid file lifecycle operation");
        if (!resource) return 0;
        auto admission = resource->begin();
        if (admission == ErrorKind::closed) return 0;
        if (admission != ErrorKind::success) {
            failure->message = "File deinit cannot close a busy resource";
            return 0;
        }
        Result result;
        {
            Runtime::Native safe(runtime);
            result = resource->file ? filesystem::close(*resource->file) : Result{};
            resource->end();
            NativeLibrary::existing(runtime)->erase(identity);
            resource->forget();
        }
        if (result.error != ErrorKind::success) {
            close_failure("File deinit close failed", result.os_code, failure);
        }
    } catch (...) {
        failure->message = "File lifecycle invariant failed";
    }
    return 0;
}
