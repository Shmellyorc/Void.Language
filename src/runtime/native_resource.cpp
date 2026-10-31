#include "void/native_resource.hpp"

#include <exception>
#include <stdexcept>

namespace vd {

void NativeResource::own_thread() const {
    if (owner != std::this_thread::get_id()) {
        throw std::runtime_error("Native resource requires its owning thread");
    }
}

std::shared_ptr<NativeLibrary> NativeLibrary::get(Runtime& runtime, bool cleanup) {
    std::lock_guard lock(runtime.mutex_);
    if (runtime.closing_ && !cleanup) {
        throw std::runtime_error("Native library admission rejected during shutdown");
    }
    if (!runtime.native_library_) {
        runtime.native_library_ = std::make_shared<NativeLibrary>(runtime);
    }
    return runtime.native_library_;
}

void NativeLibrary::publish(void* key, std::shared_ptr<NativeResource> value) {
    std::lock_guard admission(runtime_.mutex_);
    if (runtime_.closing_) {
        throw std::runtime_error("Native resource publication rejected during shutdown");
    }
    std::lock_guard lock(mutex_);
    if (!resources_.emplace(key, std::move(value)).second) {
        throw std::runtime_error("Duplicate native identity");
    }
}

std::shared_ptr<NativeLibrary> NativeLibrary::existing(Runtime& runtime) {
    std::lock_guard lock(runtime.mutex_);
    return runtime.native_library_;
}

std::shared_ptr<NativeResource> NativeLibrary::find(void* key, bool cleanup) {
    std::lock_guard lock(mutex_);
    auto found = resources_.find(key);
    if (found == resources_.end()) {
        if (cleanup) {
            return {};
        }
        throw std::runtime_error("Native resource is closed or invalid");
    }
    return found->second;
}

void* NativeLibrary::identity(const void* kind) {
    std::lock_guard lock(mutex_);
    void* result = nullptr;
    for (const auto& [key, resource] : resources_) {
        if (resource->kind() == kind) {
            if (result) {
                throw std::runtime_error("Provider inspection requires exactly one resource");
            }
            result = key;
        }
    }
    if (!result) {
        throw std::runtime_error("Provider inspection requires exactly one resource");
    }
    return result;
}

void NativeLibrary::erase(void* key, const NativeResource* expected) {
    std::shared_ptr<NativeResource> retired;
    {
        std::lock_guard lock(mutex_);
        auto found = resources_.find(key);
        if (found != resources_.end() && (!expected || found->second.get() == expected)) {
            retired = std::move(found->second);
            resources_.erase(found);
        }
    }
}

std::error_code NativeLibrary::shutdown() {
    std::lock_guard drain(shutdown_mutex_);
    std::map<void*, std::shared_ptr<NativeResource>> retired;
    {
        std::lock_guard lock(mutex_);
        retired.swap(resources_);
    }
    std::exception_ptr failure;
    std::error_code first_error;
    for (auto& [key, resource] : retired) {
        (void)key;
        try {
            auto error = resource->quiesce();
            if (!first_error && error) first_error = error;
        } catch (...) { if (!failure) failure = std::current_exception(); }
    }
    for (auto& [key, resource] : retired) {
        try {
            auto error = resource->close();
            if (!first_error && error) first_error = error;
        } catch (...) {
            if (!failure) {
                failure = std::current_exception();
            }
        }
    }
    if (failure) {
        std::rethrow_exception(failure);
    }
    return first_error;
}

void NativeLibrary::request_stop() noexcept {
    std::lock_guard lock(mutex_);
    for (auto& [key, resource] : resources_) {
        (void)key;
        resource->request_stop();
    }
}

void native_request_stop(Runtime& runtime) {
    if (auto library = NativeLibrary::existing(runtime)) library->request_stop();
}

std::error_code native_shutdown(Runtime& runtime) {
    std::shared_ptr<NativeLibrary> library;
    {
        std::lock_guard lock(runtime.mutex_);
        library = runtime.native_library_;
    }
    if (library) {
        return library->shutdown();
    }
    return {};
}

} // namespace vd
