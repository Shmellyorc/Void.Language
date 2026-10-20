#pragma once
#include "void/runtime.hpp"
#include "void/execution.hpp"
namespace vd {
class NativeLibrary;
using NativeCallbackEntry=std::int32_t(*)(std::uint64_t,void*,std::int32_t,RuntimeFailure*);
void native_shutdown(Runtime&);
std::size_t native_fixture_live() noexcept;
void* native_fixture_identity(Runtime&); // Trusted host fixture inspection; managed identity, never a C pointer.
}
// Private compiled bridge ABI: scalar bits are marshalled into exact-width C
// prototypes by the host C++ compiler, never an unchecked aggregate C ABI.
extern "C" std::uint64_t vd_native_fixture_call(std::uint32_t,std::uint64_t,void*,void*,std::uint64_t,void*,vd::NativeCallbackEntry,void**,const char*,vd::RuntimeFailure*) noexcept;
