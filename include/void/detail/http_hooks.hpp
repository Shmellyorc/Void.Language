#pragma once
#include <string>

namespace vd::http::testing {
// Native test fixture authority only. No source configuration field, global
// resolver setting, environment mutation or generated callback is involved.
struct Hooks {
    std::string dns_servers;
};
}
