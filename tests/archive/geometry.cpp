#include "../../src/runtime/zip_geometry.hpp"

#include <limits>

using vd::host::ZipGeometry;

constexpr auto sentinel=std::numeric_limits<std::uint32_t>::max();
constexpr std::uint64_t overhead=22+30+46+2;

constexpr bool limits() {
    ZipGeometry empty(22);
    if(empty.size()!=22 || empty.payload_room(1) || empty.add(1,0))return false;

    ZipGeometry exact(overhead);
    if(exact.add(0,0) || exact.add(256,0) || exact.add(1,1) ||
       !exact.add(1,0) || exact.size()!=overhead || exact.add(1,0))return false;

    ZipGeometry classic(std::numeric_limits<std::uint64_t>::max());
    if(classic.add(1,sentinel) || classic.add(1,std::numeric_limits<std::uint64_t>::max()) ||
       classic.add(1,sentinel-overhead) || classic.size()!=22)return false;
    if(!classic.add(1,sentinel-overhead-1) || classic.size()!=sentinel-1 ||
       classic.add(1,0))return false;

    ZipGeometry offsets(sentinel);
    if(!offsets.add(1,sentinel-overhead-128) || offsets.add(255,0) ||
       !offsets.add(1,49) || offsets.size()!=sentinel-1)return false;
    return true;
}

int main() {
    static_assert(limits());
    return limits() ? 0 : 1;
}
