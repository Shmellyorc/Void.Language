#pragma once
#include "void/runtime.hpp"
#include "void/execution.hpp"
namespace vd {
class NativeLibrary;
using NativeCallbackEntry=std::int32_t(*)(std::uint64_t,void*,std::int32_t,RuntimeFailure*);
std::error_code native_shutdown(Runtime&);
std::size_t native_fixture_live() noexcept;
std::size_t native_sink_live() noexcept;
void* native_sink_identity(Runtime&);
void* native_fixture_identity(Runtime&); // Trusted host fixture inspection; managed identity, never a C pointer.
}
// Private compiled bridge ABI: scalar bits are marshalled into exact-width C
// prototypes by the host C++ compiler, never an unchecked aggregate C ABI.
extern "C" std::uint64_t vd_native_fixture_call(std::uint32_t,std::uint64_t,void*,void*,std::uint64_t,void*,vd::NativeCallbackEntry,void**,const char*,vd::RuntimeFailure*) noexcept;

extern "C" std::uint64_t vd_native_sink_call(std::uint32_t,std::uint64_t,void*,void*,std::uint64_t,void*,vd::NativeCallbackEntry,void**,const char*,vd::RuntimeFailure*) noexcept;
