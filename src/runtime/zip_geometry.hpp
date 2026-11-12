#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

namespace vd::host {

inline constexpr std::uint64_t zip_local_bytes=30, zip_central_bytes=46, zip_end_bytes=22;
inline constexpr std::uint64_t zip_name_limit=255;

// Charge both headers before admitting payload. The all-ones 32-bit value is
// reserved by ZIP64, including when a caller supplies a larger output ceiling.
class ZipGeometry {
    std::uint64_t ceiling_;
    std::uint64_t total_=zip_end_bytes;

public:
    explicit constexpr ZipGeometry(std::uint64_t ceiling)
        : ceiling_(std::min<std::uint64_t>(ceiling, UINT32_MAX-1)) {}

    constexpr std::optional<std::uint64_t> payload_room(std::uint64_t name_bytes) const {
        if(name_bytes==0 || name_bytes>zip_name_limit || total_>ceiling_)return {};
        const auto headers=zip_local_bytes+zip_central_bytes+2*name_bytes;
        const auto room=ceiling_-total_;
        if(headers>room)return {};
        return room-headers;
    }

    constexpr bool add(std::uint64_t name_bytes,std::uint64_t payload) {
        const auto room=payload_room(name_bytes);
        if(!room || payload>*room)return false;
        total_+=zip_local_bytes+zip_central_bytes+2*name_bytes+payload;
        return true;
    }

    constexpr std::uint32_t size() const {return static_cast<std::uint32_t>(total_);}
};

}
