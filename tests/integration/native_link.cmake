# Shared native link driver for proof and real-source acceptance tests.
function(void_native_link object executable)
  set(native_libraries)
  set(native_linker)
  if(WINDOWS)
    # Direct generated-code printf needs the UCRT compatibility definitions.
    # A library request lets the native linker search the active MSVC LIB paths.
    set(native_libraries -llegacy_stdio_definitions)
  endif()
  if(LINKER)
    set(native_linker "--ld-path=${LINKER}")
  endif()
  execute_process(COMMAND "${CLANG}" ${native_linker} "${object}" ${native_libraries} -o "${executable}" RESULT_VARIABLE status)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "Native link failed: ${status}")
  endif()
endfunction()
