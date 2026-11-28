#pragma once
#include <string>
#include <vector>

namespace vd::host {
struct DistributionFile {
    std::string name, bytes;
    bool executable = false;
};
// Stored ZIP with UTF-8 names and destination POSIX permissions.
std::string distribution_zip(std::vector<DistributionFile>);
}
