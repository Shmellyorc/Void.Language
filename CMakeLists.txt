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
add_library(void_core src/frontend/source.cpp src/frontend/compiler.cpp src/frontend/tooling.cpp src/numeric.cpp src/ir/module.cpp src/ir/native_storage.cpp)
target_link_libraries(void_core PUBLIC void_options)
add_library(void_runtime src/runtime/gc_schema.cpp src/runtime/heap.cpp src/runtime/coordination.cpp src/runtime/callback.cpp src/runtime/abi.cpp src/runtime/reload.cpp src/runtime/native.cpp)
target_link_libraries(void_runtime PUBLIC void_options Threads::Threads)
add_library(void_llvm src/backend/llvm/backend.cpp src/backend/llvm/storage_audit.cpp src/backend/llvm/encoded_cfg.cpp)
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
llvm_map_components_to_libnames(VOID_LLVM_LIBS Core Support native OrcJIT Passes Target MC MCDisassembler Object CodeGen)
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
add_library(void_lsp src/lsp/positions.cpp)
target_link_libraries(void_lsp PUBLIC void_core)
add_executable(voidls src/lsp/main.cpp)
target_include_directories(voidls SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_options(voidls PRIVATE ${VOID_LLVM_DEFINITIONS})
target_link_libraries(voidls PRIVATE void_lsp ${VOID_NUMERIC_LIBS})
add_executable(voidc src/main.cpp examples/proof.cpp)
target_link_libraries(voidc PRIVATE void_llvm)
include(CTest)
if(BUILD_TESTING)
  add_executable(void_arrays_storage_audit tests/arrays/storage_audit.cpp examples/proof.cpp)
  target_include_directories(void_arrays_storage_audit SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
  llvm_map_components_to_libnames(VOID_AUDIT_TEST_LIBS AsmParser)
  target_link_libraries(void_arrays_storage_audit PRIVATE void_llvm ${VOID_AUDIT_TEST_LIBS})
  add_test(NAME arrays_storage_audit COMMAND void_arrays_storage_audit "${CMAKE_CURRENT_BINARY_DIR}/storage shape fixtures" "${PROJECT_SOURCE_DIR}")
  set_tests_properties(arrays_storage_audit PROPERTIES TIMEOUT 90)
  add_executable(void_gc_schemas tests/runtime/schemas.cpp)
  target_link_libraries(void_gc_schemas PRIVATE void_runtime)
  add_test(NAME gc_schemas COMMAND void_gc_schemas)
  set_tests_properties(gc_schemas PROPERTIES TIMEOUT 90)
  add_executable(void_arrays_foundation tests/arrays/foundation.cpp)
  target_link_libraries(void_arrays_foundation PRIVATE void_core)
  add_test(NAME arrays_foundation COMMAND void_arrays_foundation)
  set_tests_properties(arrays_foundation PROPERTIES TIMEOUT 90)
  add_executable(void_runtime_tests tests/runtime/coordination.cpp)
  target_link_libraries(void_runtime_tests PRIVATE void_runtime)
  add_test(NAME runtime_coordination COMMAND void_runtime_tests)
  set_tests_properties(runtime_coordination PROPERTIES TIMEOUT 90)
  add_executable(void_runtime_generated tests/runtime/generated.cpp)
  # Instrument only this executable's poll entry. Compile the SAME ABI source
  # with its poll symbol renamed; the test forwards to it after its rendezvous
  # gate. All ABI symbols are supplied here, so the archive's ABI object is not
  # extracted. Production executables and separately linked AOT retain the ABI.
  add_library(void_runtime_generated_abi OBJECT src/runtime/abi.cpp)
  target_link_libraries(void_runtime_generated_abi PRIVATE void_options Threads::Threads)
  target_compile_definitions(void_runtime_generated_abi PRIVATE vd_gc_poll=vd_gc_real_poll)
  target_sources(void_runtime_generated PRIVATE $<TARGET_OBJECTS:void_runtime_generated_abi>)
  target_link_libraries(void_runtime_generated PRIVATE void_llvm)
  add_test(NAME runtime_generated COMMAND void_runtime_generated)
  set_tests_properties(runtime_generated PROPERTIES TIMEOUT 180)
  # Source tests observe the identical ABI implementation at poll/publication
  # boundaries, with no production hooks or alternate allocator/code generator.
  add_library(void_managed_test_abi OBJECT src/runtime/abi.cpp)
  target_link_libraries(void_managed_test_abi PRIVATE void_options Threads::Threads)
  target_compile_definitions(void_managed_test_abi PRIVATE vd_gc_poll=vd_gc_real_poll vd_gc_construct=vd_gc_real_construct vd_gc_construct_typed=vd_gc_real_construct_typed vd_gc_frame_enter=vd_gc_real_frame_enter vd_gc_frame_leave=vd_gc_real_frame_leave)
  function(void_managed_source_tests family folder fixture programs)
    set(abi void_managed_test_abi)
    set(extra)
    if(family STREQUAL "strings")
      set(abi void_string_test_abi)
      set(extra tests/strings/probe.cpp)
    endif()
    add_executable(void_${family}_tests tests/${family}/tests.cpp tests/managed/probe.cpp ${extra} $<TARGET_OBJECTS:${abi}>)
    target_link_libraries(void_${family}_tests PRIVATE void_llvm)
    add_test(NAME ${family}_source COMMAND void_${family}_tests "${PROJECT_SOURCE_DIR}")
    set_tests_properties(${family}_source PROPERTIES TIMEOUT 180)
    file(GLOB_RECURSE source_files CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/examples/${folder}/*.void")
    foreach(mode normal optimized)
      set(flags)
      if(mode STREQUAL "normal")
        set(flags --no-opt)
      endif()
      set(library_object "${CMAKE_CURRENT_BINARY_DIR}/${family}-library-${mode}${CMAKE_CXX_OUTPUT_EXTENSION}")
      add_custom_command(OUTPUT "${library_object}"
        COMMAND void_${family}_tests --emit-library "${PROJECT_SOURCE_DIR}/examples/${folder}/${fixture}.void" "${library_object}" "${mode}"
        DEPENDS void_${family}_tests ${source_files} VERBATIM)
      set_source_files_properties("${library_object}" PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
      add_executable(void_${family}_aot_${mode} tests/${family}/aot.cpp tests/managed/probe.cpp ${extra} "${library_object}" $<TARGET_OBJECTS:${abi}>)
      target_link_libraries(void_${family}_aot_${mode} PRIVATE void_runtime)
      add_test(NAME ${family}_aot_${mode} COMMAND void_${family}_aot_${mode})
      set_tests_properties(${family}_aot_${mode} PROPERTIES TIMEOUT 180)
      foreach(program ${programs})
        set(object "${CMAKE_CURRENT_BINARY_DIR}/${family}-cli-${program}-${mode}${CMAKE_CXX_OUTPUT_EXTENSION}")
        add_custom_command(OUTPUT "${object}"
          COMMAND voidc emit "${PROJECT_SOURCE_DIR}/examples/${folder}/${program}.void" -o "${object}" ${flags}
          DEPENDS voidc ${source_files} VERBATIM)
        set_source_files_properties("${object}" PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
        # A generated object has no host compiler CRT-selection directives.
        # Compile a direct input under the target's configured runtime policy;
        # a runtime archive may contribute no members for a value-only program.
        add_executable(void_${family}_cli_${program}_${mode} tests/integration/native_link.cpp "${object}")
        set_target_properties(void_${family}_cli_${program}_${mode} PROPERTIES LINKER_LANGUAGE CXX)
        target_link_libraries(void_${family}_cli_${program}_${mode} PRIVATE void_runtime)
        if(WIN32)
          target_link_libraries(void_${family}_cli_${program}_${mode} PRIVATE legacy_stdio_definitions)
        endif()
      endforeach()
    endforeach()
    add_test(NAME ${family}_cli COMMAND "${CMAKE_COMMAND}"
      "-DCOMPILER=$<TARGET_FILE:voidc>" "-DROOT=${PROJECT_SOURCE_DIR}"
      "-DBIN=$<TARGET_FILE_DIR:voidc>" "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
      -P "${PROJECT_SOURCE_DIR}/tests/integration/${family}.cmake")
    set_tests_properties(${family}_cli PROPERTIES TIMEOUT 180)
  endfunction()
  void_managed_source_tests(managed managed_structs main "main;handoffs;order;imports;numeric;null_failure")
  void_managed_source_tests(managed_initialization managed_initialization stress "main;stress;numeric;imports;failure;fail_default;fail_argument;fail_child")
  void_managed_source_tests(reference_values reference_values stress "copies;readonly;initialization;templates;methods;staged;order;imports;required;stress;null_failure")
  void_managed_source_tests(composition composition stress "main;stress;imports;order;staged;receiver_order")
  void_managed_source_tests(inheritance inheritance stress "main;initialization;stress;order;imports;receiver_order;nested;results;failure")
  void_managed_source_tests(interfaces interfaces stress "main;stress;nullable;selection;handoffs;imports;cycles;static")
  add_library(void_string_test_abi OBJECT src/runtime/abi.cpp)
  target_link_libraries(void_string_test_abi PRIVATE void_options Threads::Threads)
  target_compile_definitions(void_string_test_abi PRIVATE vd_gc_poll=vd_gc_real_poll vd_gc_construct=vd_gc_real_construct vd_gc_construct_typed=vd_gc_real_construct_typed vd_gc_frame_enter=vd_gc_real_frame_enter vd_gc_frame_leave=vd_gc_real_frame_leave vd_string_literal=vd_real_string_literal vd_string_concat=vd_real_string_concat)
  void_managed_source_tests(strings strings stress "main;stress;initialization;unicode")
  target_link_libraries(void_strings_tests PRIVATE native_fixture)
  foreach(mode normal optimized)
    target_link_libraries(void_strings_aot_${mode} PRIVATE native_fixture)
    set(object "${CMAKE_CURRENT_BINARY_DIR}/strings-reloaded-${mode}${CMAKE_CXX_OUTPUT_EXTENSION}")
    add_custom_command(OUTPUT "${object}"
      COMMAND void_strings_tests --emit-reload "${PROJECT_SOURCE_DIR}/examples/strings/stress.void" "${object}" "${mode}"
      DEPENDS void_strings_tests "${PROJECT_SOURCE_DIR}/examples/strings/stress.void" VERBATIM)
    set_source_files_properties("${object}" PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
    target_sources(void_strings_aot_${mode} PRIVATE "${object}")
  endforeach()
  foreach(mode normal optimized)
    set(interface_reloaded_object "${CMAKE_CURRENT_BINARY_DIR}/interfaces-reloaded-${mode}${CMAKE_CXX_OUTPUT_EXTENSION}")
    add_custom_command(OUTPUT "${interface_reloaded_object}"
      COMMAND void_interfaces_tests --emit-reload "${PROJECT_SOURCE_DIR}/examples/interfaces/stress.void" "${interface_reloaded_object}" "${mode}"
      DEPENDS void_interfaces_tests "${PROJECT_SOURCE_DIR}/examples/interfaces/stress.void" VERBATIM)
    set_source_files_properties("${interface_reloaded_object}" PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
    target_sources(void_interfaces_aot_${mode} PRIVATE "${interface_reloaded_object}")
  endforeach()
  # Two independently lowered generations share exact nominal descriptors.
  # This remains a same-build native harness, not a source entry/FFI policy.
  foreach(mode normal optimized)
    set(reloaded_object "${CMAKE_CURRENT_BINARY_DIR}/inheritance-reloaded-${mode}${CMAKE_CXX_OUTPUT_EXTENSION}")
    add_custom_command(OUTPUT "${reloaded_object}"
      COMMAND void_inheritance_tests --emit-reload "${PROJECT_SOURCE_DIR}/examples/inheritance/stress.void" "${reloaded_object}" "${mode}"
      DEPENDS void_inheritance_tests "${PROJECT_SOURCE_DIR}/examples/inheritance/stress.void" VERBATIM)
    set_source_files_properties("${reloaded_object}" PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
    target_sources(void_inheritance_aot_${mode} PRIVATE "${reloaded_object}")
  endforeach()
  add_executable(void_runtime_callbacks tests/runtime/callbacks.cpp)
  target_link_libraries(void_runtime_callbacks PRIVATE void_llvm)
  add_test(NAME runtime_callbacks COMMAND void_runtime_callbacks)
  set_tests_properties(runtime_callbacks PROPERTIES TIMEOUT 90)
  add_executable(void_runtime_failures tests/runtime/failures.cpp)
  target_link_libraries(void_runtime_failures PRIVATE void_runtime)
  add_test(NAME runtime_failures COMMAND void_runtime_failures)
  set_tests_properties(runtime_failures PROPERTIES TIMEOUT 90)
  foreach(mode IN ITEMS normal optimized)
    set(runtime_object "${CMAKE_CURRENT_BINARY_DIR}/runtime-${mode}${CMAKE_CXX_OUTPUT_EXTENSION}")
    add_custom_command(OUTPUT "${runtime_object}" COMMAND void_runtime_generated "${runtime_object}" "${mode}"
      DEPENDS void_runtime_generated VERBATIM)
    set_source_files_properties("${runtime_object}" PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
    add_executable(void_runtime_aot_${mode} tests/runtime/aot.cpp "${runtime_object}")
    target_link_libraries(void_runtime_aot_${mode} PRIVATE void_runtime)
    add_test(NAME runtime_aot_${mode} COMMAND void_runtime_aot_${mode})
    set_tests_properties(runtime_aot_${mode} PROPERTIES TIMEOUT 180)
  endforeach()
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_executable(void_runtime_invariants tests/runtime/invariants.cpp)
  target_link_libraries(void_runtime_invariants PRIVATE void_runtime)
  add_test(NAME runtime_invariants COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/runtime/invariants.py" "$<TARGET_FILE:void_runtime_invariants>")
  set_tests_properties(runtime_invariants PROPERTIES TIMEOUT 60)
  add_executable(void_lsp_tests tests/lsp/positions.cpp)
  target_link_libraries(void_lsp_tests PRIVATE void_lsp)
  add_test(NAME lsp_positions COMMAND void_lsp_tests)
  set_tests_properties(lsp_positions PROPERTIES TIMEOUT 60)
  add_test(NAME lsp_protocol COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/lsp/protocol.py"
    "$<TARGET_FILE:voidls>" "${CMAKE_CURRENT_BINARY_DIR}/lsp fixtures")
  set_tests_properties(lsp_protocol PROPERTIES TIMEOUT 180)
  add_test(NAME lsp_demo COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/lsp_demo.py" "$<TARGET_FILE:voidls>")
  set_tests_properties(lsp_demo PROPERTIES TIMEOUT 60)
  add_library(native_fixture STATIC tests/integration/native_fixture.c)
  target_include_directories(native_fixture PUBLIC tests/integration)
  target_link_libraries(native_fixture PRIVATE void_options)
  add_executable(void_tests tests/sessions/tests.cpp tests/tooling/tests.cpp tests/analysis/tests.cpp tests/tests.cpp tests/frontend/tests.cpp tests/frontend/values.cpp tests/frontend/structs.cpp tests/frontend/methods.cpp tests/frontend/initialization.cpp tests/numeric/tests.cpp tests/modules/tests.cpp examples/proof.cpp)
  target_link_libraries(void_tests PRIVATE void_llvm void_runtime native_fixture)
  foreach(suite core jit reload gc native)
    add_test(NAME ${suite} COMMAND void_tests ${suite})
    set_tests_properties(${suite} PROPERTIES TIMEOUT 90)
  endforeach()
  add_executable(void_sessions_example examples/sessions.cpp)
  target_link_libraries(void_sessions_example PRIVATE void_core)
  add_test(NAME sessions_demo COMMAND "${CMAKE_COMMAND}"
    "-DDEMO=$<TARGET_FILE:void_sessions_example>" "-DROOT=${PROJECT_SOURCE_DIR}"
    -P "${PROJECT_SOURCE_DIR}/tests/integration/sessions.cmake")
  set_tests_properties(sessions_demo PROPERTIES TIMEOUT 60)
  add_executable(void_tooling_example examples/tooling.cpp)
  target_link_libraries(void_tooling_example PRIVATE void_core)
  add_test(NAME tooling_demo COMMAND "${CMAKE_COMMAND}"
    "-DDEMO=$<TARGET_FILE:void_tooling_example>" "-DROOT=${PROJECT_SOURCE_DIR}"
    "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/tooling demonstration"
    -P "${PROJECT_SOURCE_DIR}/tests/integration/tooling.cmake")
  set_tests_properties(tooling_demo PROPERTIES TIMEOUT 60)
  add_test(NAME sessions COMMAND void_tests sessions "${CMAKE_CURRENT_BINARY_DIR}/session fixtures")
  set_tests_properties(sessions PROPERTIES TIMEOUT 120)
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
    "-DPYTHON=${Python3_EXECUTABLE}"
    "-DLINKER=${VOID_LINKER}" "-DCOMPILER=$<TARGET_FILE:voidc>" "-DCLANG=${VOID_CLANG}"
    "-DWINDOWS=${WIN32}" "-DDWARFDUMP=${VOID_DWARFDUMP}" "-DROOT=${PROJECT_SOURCE_DIR}"
    "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/source programs with spaces" "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${PROJECT_SOURCE_DIR}/tests/integration/source.cmake")
  set_tests_properties(source_cli PROPERTIES TIMEOUT 180)
  add_test(NAME value_structs_cli COMMAND "${CMAKE_COMMAND}"
    "-DLINKER=${VOID_LINKER}" "-DCOMPILER=$<TARGET_FILE:voidc>" "-DCLANG=${VOID_CLANG}"
    "-DWINDOWS=${WIN32}" "-DDWARFDUMP=${VOID_DWARFDUMP}" "-DROOT=${PROJECT_SOURCE_DIR}"
    "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/value struct programs with spaces" "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${PROJECT_SOURCE_DIR}/tests/integration/structs.cmake")
  set_tests_properties(value_structs_cli PROPERTIES TIMEOUT 180)
  add_test(NAME receiver_methods_cli COMMAND "${CMAKE_COMMAND}"
    "-DMETHODS=ON" "-DLINKER=${VOID_LINKER}" "-DCOMPILER=$<TARGET_FILE:voidc>" "-DCLANG=${VOID_CLANG}"
    "-DWINDOWS=${WIN32}" "-DDWARFDUMP=${VOID_DWARFDUMP}" "-DROOT=${PROJECT_SOURCE_DIR}"
    "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/receiver method programs with spaces" "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${PROJECT_SOURCE_DIR}/tests/integration/structs.cmake")
  set_tests_properties(receiver_methods_cli PROPERTIES TIMEOUT 180)
  add_test(NAME initialization_cli COMMAND "${CMAKE_COMMAND}"
    "-DINITIALIZERS=ON" "-DLINKER=${VOID_LINKER}" "-DCOMPILER=$<TARGET_FILE:voidc>" "-DCLANG=${VOID_CLANG}"
    "-DWINDOWS=${WIN32}" "-DDWARFDUMP=${VOID_DWARFDUMP}" "-DROOT=${PROJECT_SOURCE_DIR}"
    "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/initialization programs with spaces" "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${PROJECT_SOURCE_DIR}/tests/integration/structs.cmake")
  set_tests_properties(initialization_cli PROPERTIES TIMEOUT 180)
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
