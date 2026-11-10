# The synthetic provider is a test host capability. None of these C objects
# are linked into the ordinary compiler or installed runtime.
if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang" OR NOT CMAKE_C_COMPILER_VERSION VERSION_EQUAL "23.1.4"
   OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  return()
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(G1_TRIPLE x86_64-unknown-linux-gnu)
  set(G1_PROFILE sysv-x64)
  set(G1_LONG i64)
  set(G1_LONG_MIN -9223372036854775808)
  set(G1_LONG_MAX 9223372036854775807)
elseif(WIN32 AND MSVC)
  set(G1_TRIPLE x86_64-pc-windows-msvc)
  set(G1_PROFILE msvc-x64)
  set(G1_LONG i32)
  set(G1_LONG_MIN -2147483648)
  set(G1_LONG_MAX 2147483647)
else()
  return()
endif()

set(g1_src "${PROJECT_SOURCE_DIR}/tests/ffi")
set(g1_bin "${CMAKE_CURRENT_BINARY_DIR}/ffi-g1")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${g1_src}/primitives_thunk.c" "${g1_src}/primitives_fixture.c")
file(SHA256 "${g1_src}/primitives_thunk.c" G1_THUNK_SHA)
file(SHA256 "${g1_src}/primitives_fixture.c" G1_FIXTURE_SHA)
configure_file("${g1_src}/primitives.descriptor.in" "${g1_bin}/primitives.descriptor" @ONLY NEWLINE_STYLE LF)
configure_file("${g1_src}/primitives.void.in" "${g1_bin}/primitives.void" @ONLY NEWLINE_STYLE LF)
file(READ "${g1_bin}/primitives.descriptor" G1_DESCRIPTOR_STRING)
string(REPLACE "\n" "\\n\"\n\"" G1_DESCRIPTOR_STRING "${G1_DESCRIPTOR_STRING}")
configure_file("${g1_src}/g1_descriptor.h.in" "${g1_bin}/g1_descriptor.h" @ONLY NEWLINE_STYLE LF)

add_library(void_g1_fixture OBJECT "${g1_src}/primitives_fixture.c")
add_library(void_g1_wrong_fixture OBJECT "${g1_src}/primitives_fixture.c")
add_library(void_g1_thunk OBJECT "${g1_src}/primitives_thunk.c")
foreach(target IN ITEMS void_g1_fixture void_g1_wrong_fixture void_g1_thunk)
  set_target_properties("${target}" PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO C_VISIBILITY_PRESET hidden INTERPROCEDURAL_OPTIMIZATION OFF)
  target_link_libraries("${target}" PRIVATE void_options)
endforeach()
target_compile_definitions(void_g1_fixture PRIVATE G1_FIXTURE_IDENTITY="${G1_FIXTURE_SHA}")
target_compile_definitions(void_g1_wrong_fixture PRIVATE G1_MISMATCHED_FIXTURE=1)
target_include_directories(void_g1_thunk PRIVATE "${g1_bin}")
if(WIN32)
  target_compile_definitions(void_g1_thunk PRIVATE G1_LLP64=1)
endif()

add_executable(void_g1_tests "${g1_src}/primitives.cpp"
  $<TARGET_OBJECTS:void_g1_thunk> $<TARGET_OBJECTS:void_g1_fixture>)
target_link_libraries(void_g1_tests PRIVATE void_llvm void_cache_identity)
void_add_output_test(ffi_g1_primitive_jit_ir "$<TARGET_FILE:void_g1_tests>" "${g1_bin}/primitives.descriptor"
  "${g1_bin}/primitives.void" "@VOID_TEST_OUTPUT@/ffi-g1/tests")
foreach(mode IN ITEMS normal optimized)
  set(g1_object "${g1_bin}/application-${mode}${CMAKE_CXX_OUTPUT_EXTENSION}")
  add_custom_command(OUTPUT "${g1_object}"
    COMMAND void_g1_tests --emit "${g1_bin}/primitives.descriptor" "${g1_bin}/primitives.void" "${g1_object}" "${mode}"
    DEPENDS void_g1_tests "${g1_bin}/primitives.descriptor" "${g1_bin}/primitives.void" VERBATIM)
  set_source_files_properties("${g1_object}" PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
  add_executable("void_g1_aot_${mode}" "${PROJECT_SOURCE_DIR}/tests/async/aot.cpp"
    "${g1_object}" $<TARGET_OBJECTS:void_g1_thunk> $<TARGET_OBJECTS:void_g1_fixture>)
  target_link_libraries("void_g1_aot_${mode}" PRIVATE void_runtime)
  add_test(NAME "ffi_g1_primitive_aot_${mode}" COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tests/async/aot.py" "$<TARGET_FILE:void_g1_aot_${mode}>" 73)
endforeach()
add_executable(void_g1_aot_mismatch "${PROJECT_SOURCE_DIR}/tests/async/aot.cpp"
  "${g1_object}" $<TARGET_OBJECTS:void_g1_thunk> $<TARGET_OBJECTS:void_g1_wrong_fixture>)
target_link_libraries(void_g1_aot_mismatch PRIVATE void_runtime)
add_test(NAME ffi_g1_primitive_aot_mismatch COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/tests/async/aot.py" "$<TARGET_FILE:void_g1_aot_mismatch>"
  "failure:C primitive provider rejected the exact descriptor or fixture identity")
add_executable(void_g1_aot_missing EXCLUDE_FROM_ALL "${PROJECT_SOURCE_DIR}/tests/async/aot.cpp" "${g1_object}")
target_link_libraries(void_g1_aot_missing PRIVATE void_runtime)
add_test(NAME ffi_g1_primitive_aot_missing COMMAND "${Python3_EXECUTABLE}" "${g1_src}/missing.py"
  "${CMAKE_COMMAND}" "${CMAKE_CURRENT_BINARY_DIR}" "$<CONFIG>")
set_tests_properties(ffi_g1_primitive_jit_ir ffi_g1_primitive_aot_missing PROPERTIES TIMEOUT 180)
