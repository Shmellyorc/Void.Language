#pragma once

#include "void/filesystem.hpp"

#include <functional>
#include <limits>
#include <memory>

namespace vd::filesystem::testing {
// Trusted native test seam, never registered as a source provider or source API.
// Install on the owner before admission; requests retain an immutable native
// snapshot so worker waits never borrow the test thread's payload or managed data.
struct Hooks {
    std::size_t read_chunk = std::numeric_limits<std::size_t>::max();
    std::size_t write_chunk = std::numeric_limits<std::size_t>::max();
    std::int64_t write_failure_after = -1;
    bool zero_write = false;
    int close_error = 0;
    std::function<void(Operation)> before;
};

std::shared_ptr<const Hooks> current();
bool noninheritable(const std::shared_ptr<File>&);
class Observe {
public:
    explicit Observe(std::shared_ptr<const Hooks>);
    ~Observe();
    Observe(const Observe&) = delete;
    Observe& operator=(const Observe&) = delete;
private:
    std::shared_ptr<const Hooks> previous_;
};
}
