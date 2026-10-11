cmake_minimum_required(VERSION 3.24)
project(VoidLanguage VERSION 0.1.0 LANGUAGES C CXX)
# Official Windows LLVM SDK libraries use the release dynamic MSVC runtime.
# Match that ABI in both Debug and Release while retaining project debug symbols.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(LLVM 19.1.7 EXACT REQUIRED CONFIG)
if(NOT LLVM_PACKAGE_VERSION VERSION_EQUAL "19.1.7")
  message(FATAL_ERROR "VOID requires the tested LLVM 19.1.7 SDK")
endif()
include(cmake/LLVMPlatform.cmake)
void_resolve_llvm_dia()
message(STATUS "VOID LLVM: ${LLVM_PACKAGE_VERSION} at ${LLVM_DIR}")
find_package(Threads REQUIRED)
# Official Linux SDK archives can contain LLVM bitcode. GNU ld cannot read
# those members without an LTO plugin; the matching LLD supports them directly.
set(VOID_LINKER "" CACHE FILEPATH "Matching LLVM 19.1.7 LLD executable for ELF links")
if(VOID_LINKER)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "VOID_LINKER currently requires Clang on Linux")
  endif()
  execute_process(COMMAND "${VOID_LINKER}" --version
    OUTPUT_VARIABLE VOID_LINKER_VERSION RESULT_VARIABLE VOID_LINKER_STATUS)
  if(NOT VOID_LINKER_STATUS EQUAL 0 OR NOT VOID_LINKER_VERSION MATCHES "LLD 19\\.1\\.7([^0-9]|$)")
    message(FATAL_ERROR "VOID_LINKER must be LLD 19.1.7")
  endif()
endif()
option(VOID_SANITIZERS "Instrument compiler/runtime with ASan and UBSan" OFF)
add_library(void_options INTERFACE)
# C++ standard-library template objects cross the LLVM/backend/runtime boundary.
# A backend-only -fno-rtti produces control-block vtables without typeinfo while
# RTTI-enabled callers (including UBSan) expect it. ORC also instantiates futures
# in the SDK itself. Match the SDK throughout the linked C++ program, not just
# where LLVM headers are included. Keep exception handling enabled for diagnostics.
if(MSVC)
  if(LLVM_ENABLE_RTTI)
    set(VOID_LLVM_RTTI_FLAG /GR)
  else()
    set(VOID_LLVM_RTTI_FLAG /GR-)
  endif()
else()
  if(LLVM_ENABLE_RTTI)
    set(VOID_LLVM_RTTI_FLAG -frtti)
  else()
    set(VOID_LLVM_RTTI_FLAG -fno-rtti)
  endif()
endif()
target_compile_options(void_options INTERFACE "$<$<COMPILE_LANGUAGE:CXX>:${VOID_LLVM_RTTI_FLAG}>")
message(STATUS "VOID C++ RTTI: ${LLVM_ENABLE_RTTI} (matches LLVM SDK across all targets)")
if(VOID_SANITIZERS AND NOT LLVM_ENABLE_RTTI)
  message(STATUS "ASan/UBSan enabled; vptr checking requires an RTTI-enabled LLVM SDK and is unavailable in this ABI")
endif()
if(VOID_LINKER)
  target_link_options(void_options INTERFACE "--ld-path=${VOID_LINKER}")
endif()
target_include_directories(void_options INTERFACE "${PROJECT_SOURCE_DIR}/include")
if(MSVC)
  target_compile_options(void_options INTERFACE /W4 /permissive- /EHsc)
  target_compile_definitions(void_options INTERFACE _ITERATOR_DEBUG_LEVEL=0)
else()
  target_compile_options(void_options INTERFACE -Wall -Wextra -Wpedantic)
  if(VOID_SANITIZERS)
    target_compile_options(void_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(void_options INTERFACE -fsanitize=address,undefined)
  endif()
endif()
add_library(void_core src/frontend/source.cpp src/frontend/compiler.cpp src/frontend/tooling.cpp src/numeric.cpp src/ir/module.cpp)
target_link_libraries(void_core PUBLIC void_options)
add_library(void_runtime src/runtime/heap.cpp src/runtime/reload.cpp src/runtime/native.cpp)
target_link_libraries(void_runtime PUBLIC void_options Threads::Threads)
add_library(void_llvm src/backend/llvm/backend.cpp)
target_include_directories(void_llvm SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
separate_arguments(VOID_LLVM_DEFINITIONS NATIVE_COMMAND "${LLVM_DEFINITIONS}")
target_compile_options(void_llvm PRIVATE ${VOID_LLVM_DEFINITIONS})
# Clang's UBSan function check reads two metadata words before an indirect
# target. Generate the ABI metadata with the actual host compiler, then attach
# it to published ORC entries; otherwise a page-aligned JIT entry can fault
# before the checker can discover that the function is uninstrumented.
if(VOID_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT MSVC)
  foreach(entry_kind IN ITEMS FUNCTION CHECKED)
    if(entry_kind STREQUAL "CHECKED")
      set(entry_parameters "std::int32_t x, vd::RuntimeFailure*")
    else()
      set(entry_parameters "std::int32_t x")
    endif()
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/jit-entry-type.cpp"
      "#include <cstdint>\nnamespace vd { struct RuntimeFailure; }\nstd::int32_t vd_entry_type(${entry_parameters}) { return x; }\n")
    execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -std=c++20 -fsanitize=undefined -S -emit-llvm
      "${CMAKE_CURRENT_BINARY_DIR}/jit-entry-type.cpp" -o "${CMAKE_CURRENT_BINARY_DIR}/jit-entry-type.ll"
      RESULT_VARIABLE entry_type_status ERROR_VARIABLE entry_type_errors)
    if(NOT entry_type_status EQUAL 0)
      message(FATAL_ERROR "Cannot generate host UBSan function metadata: ${entry_type_errors}")
    endif()
    file(READ "${CMAKE_CURRENT_BINARY_DIR}/jit-entry-type.ll" entry_type_ir)
    string(REGEX MATCH "!\\{i32 (-?[0-9]+), i32 (-?[0-9]+)\\}" entry_type_metadata "${entry_type_ir}")
    if(NOT entry_type_metadata)
      message(FATAL_ERROR "Host compiler did not emit the expected UBSan function metadata")
    endif()
    target_compile_definitions(void_llvm PRIVATE
      "VOID_${entry_kind}_SANITIZER_MAGIC=${CMAKE_MATCH_1}" "VOID_${entry_kind}_SANITIZER_HASH=${CMAKE_MATCH_2}")
  endforeach()
endif()
llvm_map_components_to_libnames(VOID_LLVM_LIBS Core Support native OrcJIT Passes Target MC CodeGen)
# Validate the selected SDK by linking and executing a real ORC host before
# building the project. Header/library presence alone missed the first CI errors.
include(CMakePushCheckState)
include(CheckCXXSourceRuns)
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_INCLUDES ${LLVM_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${VOID_LLVM_LIBS})
set(CMAKE_REQUIRED_FLAGS "${VOID_LLVM_RTTI_FLAG}")
if(VOID_LINKER)
  set(CMAKE_REQUIRED_LINK_OPTIONS "--ld-path=${VOID_LINKER}")
endif()
if(MSVC)
  set(CMAKE_REQUIRED_DEFINITIONS -D_ITERATOR_DEBUG_LEVEL=0)
endif()
# Recheck when changing SDK, toolchain or platform dependency resolution.
unset(VOID_LLVM_ORC_RUNS CACHE)
check_cxx_source_runs("#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/Support/TargetSelect.h>
int main() {
  if (llvm::InitializeNativeTarget() || llvm::InitializeNativeTargetAsmPrinter()) return 1;
  auto jit = llvm::orc::LLJITBuilder().create();
  if (!jit) { llvm::consumeError(jit.takeError()); return 2; }
  return 0;
}" VOID_LLVM_ORC_RUNS)
cmake_pop_check_state()
if(NOT VOID_LLVM_ORC_RUNS)
  message(FATAL_ERROR "The LLVM 19.1.7 SDK could not link and execute an ORC host. Check the linker, SDK ABI and exported dependencies in the CMakeFiles configure logs.")
endif()
target_include_directories(void_core SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_options(void_core PRIVATE ${VOID_LLVM_DEFINITIONS})
llvm_map_components_to_libnames(VOID_NUMERIC_LIBS Support)
target_link_libraries(void_core PRIVATE ${VOID_NUMERIC_LIBS})
target_link_libraries(void_llvm PUBLIC void_core void_runtime PRIVATE ${VOID_LLVM_LIBS})
add_executable(voidc src/main.cpp examples/proof.cpp)
target_link_libraries(voidc PRIVATE void_llvm)
include(CTest)
if(BUILD_TESTING)
  add_library(native_fixture STATIC tests/integration/native_fixture.c)
  target_include_directories(native_fixture PUBLIC tests/integration)
  target_link_libraries(native_fixture PRIVATE void_options)
  add_executable(void_tests tests/tooling/tests.cpp tests/analysis/tests.cpp tests/tests.cpp tests/frontend/tests.cpp tests/frontend/values.cpp tests/numeric/tests.cpp tests/modules/tests.cpp examples/proof.cpp)
  target_link_libraries(void_tests PRIVATE void_llvm void_runtime native_fixture)
  foreach(suite core jit reload gc native)
    add_test(NAME ${suite} COMMAND void_tests ${suite})
    set_tests_properties(${suite} PROPERTIES TIMEOUT 90)
  endforeach()
  add_executable(void_tooling_example examples/tooling.cpp)
  target_link_libraries(void_tooling_example PRIVATE void_core)
  add_test(NAME tooling_demo COMMAND "${CMAKE_COMMAND}"
    "-DDEMO=$<TARGET_FILE:void_tooling_example>" "-DROOT=${PROJECT_SOURCE_DIR}"
    "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/tooling demonstration"
    -P "${PROJECT_SOURCE_DIR}/tests/integration/tooling.cmake")
  set_tests_properties(tooling_demo PROPERTIES TIMEOUT 60)
  add_test(NAME tooling COMMAND void_tests tooling "${CMAKE_CURRENT_BINARY_DIR}/tooling fixtures")
  set_tests_properties(tooling PROPERTIES TIMEOUT 120)
  add_test(NAME analysis COMMAND void_tests analysis "${CMAKE_CURRENT_BINARY_DIR}/analysis fixtures")
  set_tests_properties(analysis PROPERTIES TIMEOUT 180)
  add_test(NAME numeric COMMAND void_tests numeric "${PROJECT_SOURCE_DIR}")
  set_tests_properties(numeric PROPERTIES TIMEOUT 120)
  add_test(NAME frontend COMMAND void_tests frontend "${PROJECT_SOURCE_DIR}")
  set_tests_properties(frontend PROPERTIES TIMEOUT 90)
  add_test(NAME modules COMMAND void_tests modules "${CMAKE_CURRENT_BINARY_DIR}/module fixtures with spaces")
  set_tests_properties(modules PROPERTIES TIMEOUT 90)
  find_program(VOID_CLANG NAMES clang-19 clang HINTS "${LLVM_TOOLS_BINARY_DIR}" REQUIRED)
  find_program(VOID_DWARFDUMP NAMES llvm-dwarfdump-19 llvm-dwarfdump HINTS "${LLVM_TOOLS_BINARY_DIR}" REQUIRED)
  execute_process(COMMAND "${VOID_CLANG}" --version OUTPUT_VARIABLE VOID_CLANG_VERSION RESULT_VARIABLE VOID_CLANG_STATUS)
  if(NOT VOID_CLANG_STATUS EQUAL 0 OR NOT VOID_CLANG_VERSION MATCHES "clang version 19\\.1\\.7")
    message(FATAL_ERROR "AOT tests require matching clang 19.1.7; set VOID_CLANG")
  endif()
  add_test(NAME aot COMMAND "${CMAKE_COMMAND}"
    "-DLINKER=${VOID_LINKER}" "-DCOMPILER=$<TARGET_FILE:voidc>" "-DCLANG=${VOID_CLANG}"
    "-DWINDOWS=${WIN32}" "-DDWARFDUMP=${VOID_DWARFDUMP}" "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/aot proof" "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${PROJECT_SOURCE_DIR}/tests/integration/aot.cmake")
  add_test(NAME source_cli COMMAND "${CMAKE_COMMAND}"
    "-DLINKER=${VOID_LINKER}" "-DCOMPILER=$<TARGET_FILE:voidc>" "-DCLANG=${VOID_CLANG}"
    "-DWINDOWS=${WIN32}" "-DDWARFDUMP=${VOID_DWARFDUMP}" "-DROOT=${PROJECT_SOURCE_DIR}"
    "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/source programs with spaces" "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${PROJECT_SOURCE_DIR}/tests/integration/source.cmake")
  set_tests_properties(source_cli PROPERTIES TIMEOUT 180)
  add_test(NAME modules_cli COMMAND "${CMAKE_COMMAND}"
    "-DLINKER=${VOID_LINKER}" "-DCOMPILER=$<TARGET_FILE:voidc>" "-DCLANG=${VOID_CLANG}"
    "-DWINDOWS=${WIN32}" "-DDWARFDUMP=${VOID_DWARFDUMP}" "-DROOT=${PROJECT_SOURCE_DIR}"
    "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/module native programs with spaces" "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${PROJECT_SOURCE_DIR}/tests/integration/modules.cmake")
  set_tests_properties(modules_cli PROPERTIES TIMEOUT 180)
  add_test(NAME analysis_cli COMMAND "${CMAKE_COMMAND}"
    "-DLINKER=${VOID_LINKER}" "-DCOMPILER=$<TARGET_FILE:voidc>" "-DCLANG=${VOID_CLANG}"
    "-DWINDOWS=${WIN32}" "-DROOT=${PROJECT_SOURCE_DIR}"
    "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/analysis native programs" "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${PROJECT_SOURCE_DIR}/tests/integration/analysis.cmake")
  set_tests_properties(analysis_cli PROPERTIES TIMEOUT 180)
  add_test(NAME numeric_cli COMMAND "${CMAKE_COMMAND}"
    "-DLINKER=${VOID_LINKER}" "-DCOMPILER=$<TARGET_FILE:voidc>" "-DCLANG=${VOID_CLANG}"
    "-DWINDOWS=${WIN32}" "-DROOT=${PROJECT_SOURCE_DIR}"
    "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/numeric native programs" "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${PROJECT_SOURCE_DIR}/tests/integration/numeric.cmake")
  set_tests_properties(numeric_cli PROPERTIES TIMEOUT 240)
endif()
