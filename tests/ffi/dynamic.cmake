if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT TARGET void_g3_tests)
  return()
endif()

set(g4_src "${PROJECT_SOURCE_DIR}/tests/ffi")
set(g4_bin "${CMAKE_CURRENT_BINARY_DIR}/ffi-g4")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${g4_src}/dynamic_thunk.c" "${g4_src}/dynamic_fixture.c" "${g4_src}/dynamic_fixture.h" "${g4_src}/dynamic_dep.c")
file(SHA256 "${g4_src}/dynamic_thunk.c" G4_THUNK_SHA)
file(SHA256 "${g4_src}/dynamic_fixture.c" g4_provider_sha)
file(SHA256 "${g4_src}/dynamic_fixture.h" g4_header_sha)
file(SHA256 "${g4_src}/dynamic_dep.c" g4_dependency_sha)
string(SHA256 G4_FIXTURE_SHA "${g4_provider_sha}${g4_header_sha}${g4_dependency_sha}")
configure_file("${g4_src}/dynamic.descriptor.in" "${g4_bin}/dynamic.descriptor" @ONLY NEWLINE_STYLE LF)
file(READ "${g4_bin}/dynamic.descriptor" G4_DESCRIPTOR_STRING)
string(REPLACE "\n" "\\n\"\n\"" G4_DESCRIPTOR_STRING "${G4_DESCRIPTOR_STRING}")
configure_file("${g4_src}/g4_descriptor.h.in" "${g4_bin}/g4_descriptor.h" @ONLY NEWLINE_STYLE LF)

function(g4_image target output root)
  add_library(${target} SHARED ${ARGN})
  set_target_properties(${target} PROPERTIES OUTPUT_NAME "${output}"
    LIBRARY_OUTPUT_DIRECTORY "${g4_bin}/${root}" SKIP_BUILD_RPATH TRUE
    C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO
    C_VISIBILITY_PRESET hidden INTERPROCEDURAL_OPTIMIZATION OFF)
  target_compile_options(${target} PRIVATE -ffreestanding -fno-builtin -fno-stack-protector -Wall -Wextra -Wpedantic)
  target_link_options(${target} PRIVATE -nostdlib -fuse-ld=lld
    "LINKER:--hash-style=sysv,-z,now,-z,relro,-z,defs")
  # This profile admits no dynamic CRT, sanitizer runtime or initializer.
  # Use freestanding UBSan traps for these C images in the strict build.
  if(VOID_SANITIZERS)
    target_compile_options(${target} PRIVATE -fsanitize=undefined -fsanitize-trap=undefined)
  endif()
endfunction()

g4_image(void_g4_dep void_g4_dep native "${g4_src}/dynamic_dep.c")
g4_image(void_g4_main void_g4_main native "${g4_src}/dynamic_fixture.c" "${g4_src}/dynamic_thunk.c")
target_include_directories(void_g4_main PRIVATE "${g4_bin}")
target_link_libraries(void_g4_main PRIVATE void_g4_dep)
g4_image(void_g4_competitor void_g4_dep outside "${g4_src}/dynamic_dep.c")
target_compile_definitions(void_g4_competitor PRIVATE G4_BIAS=99)
g4_image(void_g4_initializer void_g4_dep initializer "${g4_src}/dynamic_dep.c" "${g4_src}/dynamic_constructor.c")

set(g4_plan "${g4_bin}/closure.txt")
add_custom_command(OUTPUT "${g4_plan}"
  COMMAND "${Python3_EXECUTABLE}" "${g4_src}/dynamic_plan.py" "${g4_bin}/dynamic.descriptor"
    "$<TARGET_FILE:void_g4_main>" "$<TARGET_FILE:void_g4_dep>" "${g4_plan}"
  DEPENDS void_g4_main void_g4_dep "${g4_bin}/dynamic.descriptor" "${g4_src}/dynamic_plan.py" VERBATIM)
add_custom_target(void_g4_plan DEPENDS "${g4_plan}")
add_executable(void_g4_tests "${g4_src}/dynamic.cpp")
target_include_directories(void_g4_tests SYSTEM PRIVATE "${VOID_HOST_DEPS_ROOT}/include")
target_link_libraries(void_g4_tests PRIVATE void_llvm)
add_dependencies(void_g4_tests void_g4_plan void_g4_competitor void_g4_initializer)
add_test(NAME ffi_g4_jit_ir COMMAND void_g4_tests "${g4_plan}" "${g4_src}/dynamic.void" "${g4_bin}/native"
  "${g4_bin}/jit-probes" "$<TARGET_FILE:void_g4_competitor>" "$<TARGET_FILE:void_g4_initializer>")
set_tests_properties(ffi_g4_jit_ir PROPERTIES TIMEOUT 180)

foreach(mode IN ITEMS normal optimized startup)
  set(source "${g4_src}/dynamic.void")
  if(mode STREQUAL "startup")
    set(source "${g4_src}/dynamic_startup.void")
  endif()
  set(object "${g4_bin}/application-${mode}.o")
  add_custom_command(OUTPUT "${object}" BYPRODUCTS "${object}.source.txt"
    COMMAND "${Python3_EXECUTABLE}" "${g4_src}/dynamic_process.py" --emit "$<TARGET_FILE:void_g4_tests>"
      "${g4_plan}" "${source}" "${object}" "${mode}"
    DEPENDS void_g4_tests "${g4_plan}" "${source}" "${g4_src}/dynamic_process.py" VERBATIM)
  set_source_files_properties("${object}" PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
  add_executable("void_g4_aot_${mode}" "${object}")
  set_target_properties("void_g4_aot_${mode}" PROPERTIES LINKER_LANGUAGE CXX SKIP_BUILD_RPATH TRUE)
  target_link_libraries("void_g4_aot_${mode}" PRIVATE void_runtime)
  add_test(NAME "ffi_g4_aot_${mode}" COMMAND "${Python3_EXECUTABLE}" "${g4_src}/dynamic_process.py"
    "$<TARGET_FILE:void_g4_aot_${mode}>" "${g4_bin}/native" "${object}.source.txt" "${mode}"
    "$<TARGET_FILE:void_g4_competitor>")
endforeach()
