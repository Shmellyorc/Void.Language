#include "void/c_primitive.hpp"
#include "void/callback.hpp"
#include "void/execution.hpp"
#include "void/native_resource.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <stdexcept>

namespace {
using namespace vd;
const char resource_kind = 0;

// Passed only to the admitted adapter. C gets an ordinary C trampoline plus
// this native context, with managed state and code ownership behind the bridge.
struct CallbackContext {
    std::uint32_t (*invoke)(void*, const std::uint64_t*, std::uint32_t, std::uint64_t*);
    void* owner;
};

class CheckedCallback final : public CallbackProvider {
    struct Active { const void* owner; Active* previous; };
    static thread_local Active* active_;
    Runtime& runtime_;
    Runtime::Handle capture_;
    const std::shared_ptr<void> generation_;
    const std::uint64_t function_, thread_;
    CCallbackEntry* const entry_;
    const std::uint32_t count_;
    const bool returns_;
    CallbackContext context_{invoke, this};
    const void* owner_ = nullptr; // Assigned before the context is published to C.
    std::atomic<bool> sealed_{false};
    std::mutex error_mutex_;
    OwnedRuntimeFailure error_;
    CallbackRegistration registration_;

    static std::uint32_t invoke(void* opaque, const std::uint64_t* words,
                                std::uint32_t count, std::uint64_t* result) noexcept {
        auto& self = *static_cast<CheckedCallback*>(static_cast<CallbackContext*>(opaque)->owner);
        if (self.sealed_) return 1;
        if (count != self.count_ || (count && !words) || bool(result) != self.returns_ ||
            (self.thread_ && self.thread_ != Runtime::current_thread_identity())) {
            self.record({"C callback signature or thread rejected"});
            return 1;
        }
        bool succeeded = false;
        if (!self.registration_.run([&] {
            Active invocation{self.owner_, active_};
            active_ = &invocation;
            struct Restore { Active* previous; ~Restore() { active_ = previous; } } restore{invocation.previous};
            NativeCode::LeaseScope lease(self.generation_);
            RuntimeFailure failure;
            self.entry_(self.function_, self.capture_.get(), words, result, &failure);
            if (failure.message) self.record(failure);
            else succeeded = true;
        })) self.record({"C callback Runtime admission rejected"});
        return succeeded ? 0 : 1;
    }

    void record(const RuntimeFailure& failure) noexcept {
        std::lock_guard lock(error_mutex_);
        if (!error_.value().message) error_.assign(failure);
        sealed_ = true;
    }

    // Scoped callers supply the barrier by returning. Retained callers supply
    // it by completing their terminal release before closing this object.
    void unregister_and_quiesce() noexcept override {}

public:
    CheckedCallback(Runtime& runtime, std::uint64_t function, void* capture, CCallbackEntry* entry,
                    std::uint32_t count, bool returns, bool retained)
        : runtime_(runtime), capture_(runtime, capture), generation_(NativeCode::active_lease()),
          function_(function), thread_(retained ? 0 : Runtime::current_thread_identity()),
          entry_(entry), count_(count), returns_(returns), registration_(runtime, *this) {}
    ~CheckedCallback() override {
        if (runtime_.is_managed()) {
            Runtime::Native safe(runtime_);
            registration_.close();
        } else registration_.close();
    }
    void seal() noexcept { sealed_ = true; }
    void resume_after_refusal() noexcept {
        std::lock_guard lock(error_mutex_);
        if (!error_.value().message) sealed_ = false;
    }
    void set_owner(const void* owner) noexcept { owner_ = owner; }
    static bool active(const void* owner) noexcept {
        for (auto* invocation = active_; invocation; invocation = invocation->previous)
            if (invocation->owner == owner) return true;
        return false;
    }
    void* context() noexcept { return &context_; }
    bool report(RuntimeFailure* failure) {
        std::lock_guard lock(error_mutex_);
        if (!error_.value().message) return false;
        if (failure) retain_native_failure(error_.value(), failure);
        return true;
    }
};
thread_local CheckedCallback::Active* CheckedCallback::active_ = nullptr;

class Resource final : public NativeResource {
    std::string descriptor_, identity_;
    CPrimitiveThunk* thunk_;
    const std::uint32_t release_;
    const std::uint64_t shutdown_thread_;
    std::shared_ptr<void> generation_;
    std::mutex mutex_;
    std::uint64_t native_ = 0;
    std::atomic<std::uint64_t> exclusive_ = 0;
    std::unique_ptr<CheckedCallback> callback_;

    std::uint32_t dispatch(std::uint32_t ordinal, const std::uint64_t* args,
                           std::uint32_t count, std::uint64_t* result) {
        if (runtime.is_managed()) {
            Runtime::Native native(runtime);
            return thunk_(descriptor_.data(), static_cast<std::uint32_t>(descriptor_.size()),
                          ordinal, args, count, result);
        }
        return thunk_(descriptor_.data(), static_cast<std::uint32_t>(descriptor_.size()),
                      ordinal, args, count, result);
    }

public:
    Resource(Runtime& runtime, void* wrapper, std::string_view descriptor, std::string_view identity,
             CPrimitiveThunk* thunk, std::uint32_t release, bool shutdown_owner)
        : NativeResource(runtime, wrapper), descriptor_(descriptor), identity_(identity), thunk_(thunk),
          release_(release), shutdown_thread_(shutdown_owner ? Runtime::current_thread_identity() : 0),
          generation_(NativeCode::active_lease()) {}
    ~Resource() override {
        if (native_) {
            try { (void)close(); }
            catch (...) { std::terminate(); } // A verified terminal release must be dispatchable.
        }
    }
    const void* kind() const noexcept override { return &resource_kind; }
    std::uint64_t exclusive_native_identity() const noexcept override { return exclusive_.load(); }
    void discard_duplicate() noexcept { native_ = 0; exclusive_ = 0; }
    void require_shutdown_thread() const override {
        if (shutdown_thread_ && shutdown_thread_ != Runtime::current_thread_identity())
            throw std::runtime_error("Native shutdown requires the application owner thread");
    }
    bool matches(std::string_view descriptor, std::string_view identity,
                 CPrimitiveThunk* thunk, std::uint32_t release, bool shutdown_owner) const {
        return descriptor_ == descriptor && identity_ == identity && thunk_ == thunk &&
            release_ == release && bool(shutdown_thread_) == shutdown_owner;
    }

    bool acquire(std::uint32_t ordinal, const std::uint64_t* args, std::uint32_t count,
                 const CSpanRequest* spans = nullptr, std::uint32_t span_count = 0) {
        std::uint64_t acquired = 0;
        const auto status = span_count ? vd_c_span_dispatch_v2(thunk_, descriptor_.data(), descriptor_.size(),
            ordinal, args, count, &acquired, spans, span_count) : dispatch(ordinal, args, count, &acquired);
        if (status)
            throw std::runtime_error("C resource creation transport rejected");
        native_ = acquired; // Only a successful transport transfers ownership.
        exclusive_ = native_;
        return native_ != 0;
    }
    void set_callback(std::unique_ptr<CheckedCallback> callback) {
        callback->set_owner(this);
        callback_ = std::move(callback);
    }
    bool report_callback(RuntimeFailure* failure) { return callback_ && callback_->report(failure); }
    std::unique_lock<std::mutex> pin() {
        own_thread();
        require_shutdown_thread();
        if (CheckedCallback::active(this)) throw std::logic_error("C callback cannot reenter its own resource");
        auto lock = std::unique_lock(mutex_);
        if (!native_) throw std::runtime_error("Native resource is closed");
        return lock;
    }
    std::uint64_t native_word() const { return native_; } // Only while pinned.
    std::uint32_t borrow(std::uint32_t ordinal, const std::uint64_t* args,
                         std::uint32_t count, std::uint64_t* result) {
        if (CheckedCallback::active(this)) return 7;
        std::lock_guard lock(mutex_);
        if (!native_) return 1;
        std::array<std::uint64_t, c_primitive_max_arguments> words{};
        std::copy_n(args, count, words.begin());
        words[0] = native_;
        if (dispatch(ordinal, words.data(), count, result)) return 4;
        return report_callback(nullptr) ? 8 : 0;
    }
    std::error_code close() override { return close(nullptr); }
    std::error_code close(RuntimeFailure* failure) {
        if (CheckedCallback::active(this)) throw std::logic_error("C callback cannot close its own registration");
        std::shared_ptr<void> retired;
        std::unique_ptr<CheckedCallback> callback;
        std::int32_t provider_status;
        {
            std::lock_guard lock(mutex_);
            if (!native_) return {};
            require_shutdown_thread();
            std::uint64_t result = 0;
            // A conforming allocator can reuse the address as soon as C frees
            // it. Close invalidates the ledger claim before that handoff.
            exclusive_ = 0;
            if (callback_) callback_->seal();
            if (dispatch(release_, &native_, 1, &result)) {
                // Transport refusal precedes the C operation. Both the
                // native owner and its registered callback pair remain live.
                exclusive_ = native_;
                if (callback_) callback_->resume_after_refusal();
                throw std::runtime_error("C terminal release transport rejected");
            }
            native_ = 0; // Accepted release consumes even when its result reports failure.
            provider_status = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(result));
            retired = std::move(generation_);
            callback = std::move(callback_);
        }
        const bool fault = callback && callback->report(failure);
        callback.reset();
        wrapper.reset(nullptr);
        // Retire the acquisition generation outside the resource and GC locks.
        if (runtime.is_managed()) {
            Runtime::Native native(runtime);
            retired.reset();
        } else retired.reset();
        if (!provider_status && fault && !failure) return std::make_error_code(std::errc::operation_canceled);
        return {provider_status, std::generic_category()};
    }
};

class NativeJob {
    Runtime& runtime_;
public:
    explicit NativeJob(Runtime& runtime) : runtime_(runtime) { runtime.begin_native_job(); }
    ~NativeJob() { runtime_.end_native_job(); }
};
std::uint32_t dispatch_resource(CPrimitiveThunk* thunk, const char* descriptor,
    std::uint32_t size, std::uint32_t ordinal, const std::uint64_t* words, std::uint32_t count,
    std::uint64_t* result, void** handle, const char* identity, std::uint32_t release, CResourceRole role,
    bool shutdown_owner, std::unique_ptr<CheckedCallback> callback = {},
    RuntimeFailure* failure = nullptr, const CSpanRequest* spans = nullptr, std::uint32_t span_count = 0) noexcept {
    if (!thunk || !descriptor || !size || size > c_conventional_descriptor_limit || !identity || !handle ||
        ordinal >= c_conventional_binding_limit || release >= c_conventional_binding_limit || count > c_primitive_max_arguments || (count && !words) ||
        role < CResourceRole::acquire || role > CResourceRole::close ||
        (role != CResourceRole::acquire && !count) ||
        (role == CResourceRole::close && (ordinal != release || count != 1 || !result)))
        return 5;
    try {
        auto& runtime = Runtime::current();
        (void)runtime.root_word(handle);
        NativeJob job(runtime);
        auto library = NativeLibrary::get(runtime);
        const std::string_view contract(descriptor, size);
        if (role == CResourceRole::acquire) {
            if (ordinal == release || result) return 5;
            *handle = nullptr;
            if (shutdown_owner && !runtime.is_application_thread()) return 3;
            try {
                auto nominal = runtime.descriptor(identity);
                if (!nominal || nominal->layout.size != sizeof(std::uint64_t) ||
                    !nominal->layout.references.empty()) return 5;
                auto layout = nominal->layout;
                layout.descriptor = nominal;
                runtime.allocate_into(handle, layout);
                auto owner = std::make_shared<Resource>(runtime, *handle, contract, identity, thunk, release, shutdown_owner);
                if (callback) owner->set_callback(std::move(callback));
                const bool acquired = owner->acquire(ordinal, words, count, spans, span_count);
                if (acquired && !library->publish_exclusive(*handle, owner)) {
                    owner->discard_duplicate();
                    *handle = nullptr;
                    return 6;
                }
                if (owner->report_callback(failure)) {
                    if (acquired) {
                        (void)owner->close();
                        library->erase(*handle, owner.get());
                    }
                    *handle = nullptr;
                    return 8;
                }
                if (!acquired) *handle = nullptr;
                return 0;
            } catch (...) {
                *handle = nullptr;
                throw;
            }
        }
        auto owner = library->find(*handle, true);
        if (!owner) return 1;
        if (owner->kind() != &resource_kind) return 2;
        auto resource = std::static_pointer_cast<Resource>(owner);
        if (!resource->matches(contract, identity, thunk, release, shutdown_owner)) return 2;
        try {
            resource->own_thread();
            resource->require_shutdown_thread();
        }
        catch (const std::runtime_error&) { return 3; }
        if (role != CResourceRole::close)
            return ordinal == release ? 5 : resource->borrow(ordinal, words, count, result);
        if (CheckedCallback::active(resource.get())) return 7;
        const auto error = resource->close();
        library->erase(*handle, resource.get());
        *result = static_cast<std::uint32_t>(error.value());
        return 0;
    } catch (const std::exception&) {
        return 4;
    }
}
} // namespace

extern "C" std::uint32_t vd::vd_c_resource_dispatch_v3(CPrimitiveThunk* thunk, const char* descriptor,
    std::uint32_t size, std::uint32_t ordinal, const std::uint64_t* words, std::uint32_t count,
    std::uint64_t* result, void** handle, const char* identity, std::uint32_t release, CResourceRole role) noexcept {
    return dispatch_resource(thunk, descriptor, size, ordinal, words, count, result, handle, identity, release, role, false);
}

extern "C" std::uint32_t vd::vd_c_resource_dispatch_owner_v3(CPrimitiveThunk* thunk, const char* descriptor,
    std::uint32_t size, std::uint32_t ordinal, const std::uint64_t* words, std::uint32_t count,
    std::uint64_t* result, void** handle, const char* identity, std::uint32_t release, CResourceRole role) noexcept {
    return dispatch_resource(thunk, descriptor, size, ordinal, words, count, result, handle, identity, release, role, true);
}

extern "C" std::uint32_t vd::vd_c_resources_dispatch_v3(CPrimitiveThunk* thunk, const char* descriptor,
    std::uint32_t size, std::uint32_t ordinal, const std::uint64_t* words, std::uint32_t count,
    std::uint64_t* result, const CResourceArgument* arguments, std::uint32_t argument_count,
    void** result_root, const char* result_identity, std::uint32_t release, std::uint32_t shutdown_owner,
    const CSpanRequest* spans, std::uint32_t span_count) noexcept {
    if (!thunk || !descriptor || !size || size > c_conventional_descriptor_limit ||
        ordinal >= c_conventional_binding_limit || count > c_primitive_max_arguments ||
        argument_count > count || (count && !words) || (argument_count && !arguments) ||
        bool(result_root) != bool(result_identity) || shutdown_owner > 1 || span_count > 2)
        return 5;
    try {
        auto& runtime = Runtime::current();
        NativeJob job(runtime);
        auto library = NativeLibrary::get(runtime);
        std::array<std::uint64_t, c_primitive_max_arguments> native_words{};
        if (count) std::copy_n(words, count, native_words.begin());
        std::array<bool, c_primitive_max_arguments> occupied{};
        std::vector<std::shared_ptr<Resource>> resources;
        std::vector<std::pair<unsigned, std::shared_ptr<Resource>>> operands;
        for (unsigned n = 0; n < argument_count; ++n) {
            const auto& arg = arguments[n];
            if (!arg.root || !arg.identity || arg.argument >= count || occupied[arg.argument] ||
                arg.release >= c_conventional_binding_limit || arg.shutdown_owner > 1) return 5;
            occupied[arg.argument] = true;
            (void)runtime.root_word(arg.root);
            auto found = library->find(*arg.root, true);
            if (!found) return 1;
            if (found->kind() != &resource_kind) return 2;
            auto resource = std::static_pointer_cast<Resource>(found);
            if (!resource->matches({descriptor, size}, arg.identity, thunk, arg.release, arg.shutdown_owner)) return 2;
            operands.emplace_back(arg.argument, resource);
            resources.push_back(std::move(resource));
        }
        std::sort(resources.begin(), resources.end(), [](const auto& a, const auto& b) {
            return std::less<const void*>{}(a.get(), b.get());
        });
        resources.erase(std::unique(resources.begin(), resources.end()), resources.end());
        std::vector<std::unique_lock<std::mutex>> pins;
        for (const auto& resource : resources) pins.push_back(resource->pin());
        for (const auto& [argument, resource] : operands) native_words[argument] = resource->native_word();
        std::uint32_t status;
        if (result_root) {
            if (result) return 5;
            status = dispatch_resource(thunk, descriptor, size, ordinal, native_words.data(), count,
                nullptr, result_root, result_identity, release, CResourceRole::acquire, shutdown_owner,
                {}, nullptr, spans, span_count);
        } else if (span_count) {
            status = vd_c_span_dispatch_v2(thunk, descriptor, size, ordinal,
                native_words.data(), count, result, spans, span_count);
        } else {
            Runtime::Native safe(runtime);
            status = thunk(descriptor, size, ordinal, native_words.data(), count, result);
        }
        for (const auto& resource : resources)
            if (resource->report_callback(nullptr)) return 8;
        return status;
    } catch (const std::exception&) { return 4; }
}

extern "C" std::uint32_t vd::vd_c_callback_dispatch_v1(CPrimitiveThunk* thunk, const char* descriptor,
    std::uint32_t size, std::uint32_t ordinal, std::uint64_t* words, std::uint32_t count,
    std::uint64_t* result, void** handle, const char* identity, std::uint32_t release, std::uint32_t shutdown_owner,
    std::uint64_t function, void* capture, CCallbackEntry* entry, std::uint32_t arity, std::uint32_t returns,
    RuntimeFailure* failure) noexcept {
    if (!thunk || !descriptor || !size || size > c_conventional_descriptor_limit || ordinal >= c_conventional_binding_limit ||
        !words || !count || count > c_primitive_max_arguments || words[count - 1] ||
        !function || !entry || arity > c_primitive_max_arguments || !failure ||
        bool(handle) != bool(identity) || (handle && result) || shutdown_owner > 1 || returns > 1)
        return 5;
    try {
        auto& runtime = Runtime::current();
        if (shutdown_owner && !runtime.is_application_thread()) return 3;
        NativeJob job(runtime);
        auto callback = std::make_unique<CheckedCallback>(runtime, function, capture, entry, arity, returns, bool(handle));
        words[count - 1] = reinterpret_cast<std::uintptr_t>(callback->context());
        if (handle)
            return dispatch_resource(thunk, descriptor, size, ordinal, words, count, nullptr, handle,
                                     identity, release, CResourceRole::acquire, shutdown_owner,
                                     std::move(callback), failure);
        std::uint32_t status;
        {
            Runtime::Native safe(runtime);
            status = thunk(descriptor, size, ordinal, words, count, result);
        }
        return callback->report(failure) ? 8 : status ? 4 : 0;
    } catch (...) {
        return 4;
    }
}
