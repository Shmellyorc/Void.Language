if(NOT TARGET void_g1_thunk)
  return()
endif()

set(g3_src "${PROJECT_SOURCE_DIR}/tests/ffi")
set(g3_bin "${CMAKE_CURRENT_BINARY_DIR}/ffi-g3")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${g3_src}/resources_thunk.c" "${g3_src}/resources_fixture.c")
file(SHA256 "${g3_src}/resources_thunk.c" G3_THUNK_SHA)
file(SHA256 "${g3_src}/resources_fixture.c" G3_FIXTURE_SHA)
configure_file("${g3_src}/resources.descriptor.in" "${g3_bin}/resources.descriptor" @ONLY NEWLINE_STYLE LF)
file(READ "${g3_bin}/resources.descriptor" G3_DESCRIPTOR_STRING)
string(REPLACE "\n" "\\n\"\n\"" G3_DESCRIPTOR_STRING "${G3_DESCRIPTOR_STRING}")
configure_file("${g3_src}/g3_descriptor.h.in" "${g3_bin}/g3_descriptor.h" @ONLY NEWLINE_STYLE LF)

add_library(void_g3_fixture OBJECT "${g3_src}/resources_fixture.c")
add_library(void_g3_thunk OBJECT "${g3_src}/resources_thunk.c")
foreach(target IN ITEMS void_g3_fixture void_g3_thunk)
  set_target_properties("${target}" PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO C_VISIBILITY_PRESET hidden INTERPROCEDURAL_OPTIMIZATION OFF)
  target_link_libraries("${target}" PRIVATE void_options)
endforeach()
target_compile_definitions(void_g3_fixture PRIVATE G3_FIXTURE_IDENTITY="${G3_FIXTURE_SHA}")
target_include_directories(void_g3_thunk PRIVATE "${g3_bin}")
if(WIN32)
  target_compile_definitions(void_g3_thunk PRIVATE G3_LLP64=1)
endif()

add_executable(void_g3_tests "${g3_src}/resources.cpp"
  $<TARGET_OBJECTS:void_g3_thunk> $<TARGET_OBJECTS:void_g3_fixture>)
target_link_libraries(void_g3_tests PRIVATE void_llvm)
void_add_output_test(ffi_g3_jit_ir "$<TARGET_FILE:void_g3_tests>" "${g3_bin}/resources.descriptor"
  "${g3_src}/resources.void" "@VOID_TEST_OUTPUT@/ffi-g3/tests")
set_tests_properties(ffi_g3_jit_ir PROPERTIES TIMEOUT 180)
foreach(mode IN ITEMS normal optimized)
  set(g3_object "${g3_bin}/application-${mode}${CMAKE_CXX_OUTPUT_EXTENSION}")
  add_custom_command(OUTPUT "${g3_object}"
    COMMAND void_g3_tests --emit "${g3_bin}/resources.descriptor" "${g3_src}/resources.void" "${g3_object}" "${mode}"
    DEPENDS void_g3_tests "${g3_bin}/resources.descriptor" "${g3_src}/resources.void" VERBATIM)
  set_source_files_properties("${g3_object}" PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
  add_executable("void_g3_aot_${mode}" "${g3_src}/resources_exit.cpp"
    "${g3_object}" $<TARGET_OBJECTS:void_g3_thunk> $<TARGET_OBJECTS:void_g3_fixture>)
  target_link_libraries("void_g3_aot_${mode}" PRIVATE void_runtime)
  add_test(NAME "ffi_g3_aot_${mode}" COMMAND "${Python3_EXECUTABLE}"
    "${g3_src}/resources_process.py" "$<TARGET_FILE:void_g3_aot_${mode}>")
endforeach()
