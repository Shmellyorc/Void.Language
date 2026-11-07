#pragma once
#include <string>
#include <vector>

namespace vd {
// Immutable native snapshot; never stores pointers into argv or managed storage.
// Error ordinals use the existing os.error_kind contract.
struct ApplicationArguments {
    int error = 6; // Embedders must explicitly admit an application context.
    std::vector<std::string> values;
};
ApplicationArguments checked_application_arguments(std::vector<std::string>);
ApplicationArguments capture_application_arguments(int argc, const void* argv, bool wide);
}
