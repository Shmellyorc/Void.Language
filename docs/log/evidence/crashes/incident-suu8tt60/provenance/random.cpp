#include "void/runtime_abi.hpp"
#include <array>
#include <cerrno>
#include <bit>
#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#elif defined(__APPLE__)
#include <sys/random.h>
#else
#include <sys/random.h>
#endif

extern "C" bool vd_random_seed(std::uint64_t* output, std::int32_t* status, vd::RuntimeFailure* failure) noexcept {
    try {
        std::array<unsigned char,8> bytes{};
        std::int32_t code=0;
        {
            vd::Runtime::Native safe(vd::Runtime::current());
#if defined(_WIN32)
            auto native=BCryptGenRandom(nullptr,bytes.data(),static_cast<ULONG>(bytes.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG);
            if(native<0) code=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(native));
#elif defined(__APPLE__)
            if(getentropy(bytes.data(),bytes.size())!=0) code=errno;
#else
            std::size_t used=0;
            while(used<bytes.size()) {
                auto count=getrandom(bytes.data()+used,bytes.size()-used,0);
                if(count<0) { if(errno==EINTR) continue; code=errno; break; }
                if(count==0) { code=EIO; break; }
                used+=static_cast<std::size_t>(count);
            }
#endif
        }
        if(code) { *status=code; return true; }
        std::uint64_t seed=0;
        for(unsigned n=0;n<8;++n) seed|=std::uint64_t(bytes[n])<<(n*8);
        *output=seed; *status=0; return true;
    } catch(...) { failure->message="Random entropy Runtime coordination failed"; return false; }
}
