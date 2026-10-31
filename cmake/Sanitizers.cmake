include_guard(GLOBAL)
function(void_enable_sanitizers target scope)
  if(NOT VOID_SANITIZERS)
    return()
  endif()
  if(MSVC)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang" OR
        (CMAKE_CXX_COMPILER_ID AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
      message(FATAL_ERROR "Strict ASan/UBSan requires the matching Clang toolchain")
    endif()
    target_compile_options(${target} ${scope} /clang:-fsanitize=address,undefined /clang:-fno-omit-frame-pointer)
    # clang-cl emits the sanitizer runtime link directives in its COFF objects.
    # The native configuration probe verifies those directives actually link.
  else()
    target_compile_options(${target} ${scope} -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} ${scope} -fsanitize=address,undefined)
  endif()
endfunction()
