#include "host_internal.hpp"
#ifndef VOID_HOST_PROVIDERS_UNAVAILABLE
#include <sodium.h>
#endif
#include <mutex>
namespace vd::host {
#ifdef VOID_HOST_PROVIDERS_UNAVAILABLE
Result crypto_execute(Request&) {return error(11);}
#else
namespace {
void initialize() {
    static const bool initialized=[] {return sodium_init()>=0;}();
    if(!initialized)throw std::runtime_error("libsodium initialization failed");
}
struct Hash final : Object {
    crypto_hash_sha256_state state{};
    bool closed=false,finalized=false;
    std::uint64_t bytes=0;
    Hash() {initialize();crypto_hash_sha256_init(&state);}
    ~Hash() override {close();}
    std::error_code close() noexcept override {sodium_memzero(&state,sizeof(state));closed=true;return {};}
};
}
Result crypto_execute(Request& q) {
    initialize();
    if(q.operation==verify) {
        if(q.bytes[0].size()!=crypto_sign_PUBLICKEYBYTES || q.bytes[1].size()!=crypto_sign_BYTES)return error(6);
        Result r;r.flag=crypto_sign_verify_detached(reinterpret_cast<const unsigned char*>(q.bytes[1].data()),
            reinterpret_cast<const unsigned char*>(q.bytes[2].data()),q.bytes[2].size(),reinterpret_cast<const unsigned char*>(q.bytes[0].data()))==0;
        return r;
    }
    if(q.operation==hash_open) {q.acquired=std::make_shared<Hash>();return {};}
    auto h=std::static_pointer_cast<Hash>(q.objects[0]);
    if(h->closed || h->finalized)return error(7);
    if(q.operation==hash_update) {
        // SHA-256's bit count is 64-bit. Never silently wrap it.
        if(q.bytes[0].size()>(UINT64_MAX/8)-h->bytes)return error(12);
        crypto_hash_sha256_update(&h->state,reinterpret_cast<const unsigned char*>(q.bytes[0].data()),q.bytes[0].size());
        h->bytes+=q.bytes[0].size();return {};
    }
    Result r;r.bytes.resize(crypto_hash_sha256_BYTES);
    crypto_hash_sha256_final(&h->state,reinterpret_cast<unsigned char*>(r.bytes.data()));
    h->finalized=true;sodium_memzero(&h->state,sizeof(h->state));return r;
}
#endif
}
