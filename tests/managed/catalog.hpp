#pragma once
#include "probe.hpp"
// The separately linked source module owns these immutable records. Admission
// occurs before entering Managed, matching the production CLI bootstrap.
extern "C" const vd::RuntimeTypeRecord __vd_types[];
extern "C" const std::size_t __vd_type_count;
namespace managed_test {
inline void admit(vd::Runtime& runtime) {
    vd::RuntimeFailure failure;
    vd_runtime_admit(&runtime,__vd_types,__vd_type_count,&failure);
    require(!failure.message,"native source catalog admission");
}
}
