#pragma once
#include "void/ir.hpp"
#include <string>
#include <vector>
namespace vd {
// Explicit host trust descriptor. No filesystem discovery or native language grammar.
struct NativeBinding {std::string name,symbol;std::vector<std::string> parameters;std::string result;bool owning_thread=false,blocking=false,callbacks=false;};
struct NativeProject {std::string library="void.fixture.v1",module="/native_bridge.void";std::vector<NativeBinding> bindings;};
const std::vector<NativeBinding>& native_fixture_catalog();
const std::vector<NativeBinding>& native_sink_catalog();
NativeProject native_sink_project(std::string module="/native_sink.void");
const std::vector<NativeBinding>& native_catalog(std::string_view library);
std::string_view native_provider(std::string_view symbol);
unsigned native_operation(std::string_view symbol);
NativeProject native_fixture_project(std::string module="/native_bridge.void");
void validate_native_project(const NativeProject&);
std::string native_project_source(const NativeProject&);
const NativeBinding& native_binding(std::string_view);
bool native_signature(std::string_view,const std::vector<ir::Type>&,ir::Type);
}
