if(NOT TARGET void_g1_thunk)
  return()
endif()

set(g2_src "${PROJECT_SOURCE_DIR}/tests/ffi")
set(g2_bin "${CMAKE_CURRENT_BINARY_DIR}/ffi-g2")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${g2_src}/records_thunk.c" "${g2_src}/records_fixture.c")
file(SHA256 "${g2_src}/records_thunk.c" G2_THUNK_SHA)
file(SHA256 "${g2_src}/records_fixture.c" G2_FIXTURE_SHA)
configure_file("${g2_src}/records.descriptor.in" "${g2_bin}/records.descriptor" @ONLY NEWLINE_STYLE LF)
file(READ "${g2_bin}/records.descriptor" G2_DESCRIPTOR_STRING)
string(REPLACE "\n" "\\n\"\n\"" G2_DESCRIPTOR_STRING "${G2_DESCRIPTOR_STRING}")
configure_file("${g2_src}/g2_descriptor.h.in" "${g2_bin}/g2_descriptor.h" @ONLY NEWLINE_STYLE LF)

add_library(void_g2_fixture OBJECT "${g2_src}/records_fixture.c")
add_library(void_g2_thunk OBJECT "${g2_src}/records_thunk.c")
foreach(target IN ITEMS void_g2_fixture void_g2_thunk)
  set_target_properties("${target}" PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO C_VISIBILITY_PRESET hidden INTERPROCEDURAL_OPTIMIZATION OFF)
  target_link_libraries("${target}" PRIVATE void_options)
endforeach()
target_compile_definitions(void_g2_fixture PRIVATE G2_FIXTURE_IDENTITY="${G2_FIXTURE_SHA}")
target_include_directories(void_g2_thunk PRIVATE "${g2_bin}")
if(WIN32)
  target_compile_definitions(void_g2_thunk PRIVATE G2_LLP64=1)
endif()

add_executable(void_g2_tests "${g2_src}/records.cpp"
  $<TARGET_OBJECTS:void_g2_thunk> $<TARGET_OBJECTS:void_g2_fixture>)
target_link_libraries(void_g2_tests PRIVATE void_llvm)
add_test(NAME ffi_g2_jit_ir COMMAND void_g2_tests "${g2_bin}/records.descriptor" "${g2_src}/records.void")
set_tests_properties(ffi_g2_jit_ir PROPERTIES TIMEOUT 180)
foreach(mode IN ITEMS normal optimized)
  set(g2_object "${g2_bin}/application-${mode}${CMAKE_CXX_OUTPUT_EXTENSION}")
  add_custom_command(OUTPUT "${g2_object}"
    COMMAND void_g2_tests "${g2_bin}/records.descriptor" "${g2_src}/records.void" "${g2_object}" "${mode}"
    DEPENDS void_g2_tests "${g2_bin}/records.descriptor" "${g2_src}/records.void" VERBATIM)
  set_source_files_properties("${g2_object}" PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
  add_executable("void_g2_aot_${mode}" "${PROJECT_SOURCE_DIR}/tests/async/aot.cpp"
    "${g2_object}" $<TARGET_OBJECTS:void_g2_thunk> $<TARGET_OBJECTS:void_g2_fixture>)
  target_link_libraries("void_g2_aot_${mode}" PRIVATE void_runtime)
  add_test(NAME "ffi_g2_aot_${mode}" COMMAND "${Python3_EXECUTABLE}"
    "${g2_src}/records_process.py" "$<TARGET_FILE:void_g2_aot_${mode}>")
endforeach()
