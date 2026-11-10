
macro(include)
  if("${ARGV0}" STREQUAL "cmake/HostDependencies.cmake")
    _include("C:/Users/a/VS7I/HostDependencies-attested.cmake")
  else()
    _include(${ARGV})
  endif()
endmacro()
