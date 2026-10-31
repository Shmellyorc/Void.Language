#pragma once
#include <memory>
#include <string>

namespace vd::http {
// Native-only shared ownership of one matching libcurl global reference. Host
// references remain libcurl's responsibility; this manager never replaces its
// allocators, signal policy, trust store or process environment.
class ProviderLease final {
    ProviderLease();
public:
    ~ProviderLease();
    ProviderLease(const ProviderLease&) = delete;
    ProviderLease& operator=(const ProviderLease&) = delete;
    static std::shared_ptr<const ProviderLease> acquire();
};
struct ProviderFeatures {
    std::string transport, tls, resolver, idn;
    bool http1 = false, http2 = false, ipv6 = false, psl = false;
};
ProviderFeatures provider_features();
}
