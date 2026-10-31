cmake_minimum_required(VERSION 3.24)
project(VoidTlsCandidateReview LANGUAGES C CXX)
get_filename_component(VOID_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
find_package(Threads REQUIRED)
foreach(required VOID_TLS_REVIEW_PREFIX VOID_TLS_REVIEW_RUNTIME VOID_HTTP_DEPS_ROOT)
  if(NOT IS_DIRECTORY "${${required}}")
    message(FATAL_ERROR "Explicit review directory required: ${required}")
  endif()
endforeach()
# This hook overrides TLS only in the isolated curl project, after the ordinary
# production importer has verified the unchanged dependency profile. It is never
# included by VOID's production CMake build.
set(CMAKE_PROJECT_CURL_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/override.cmake")
set(VOID_CURL_PROFILE HTTP CACHE STRING "Review full provider" FORCE)
add_subdirectory("${VOID_SOURCE_ROOT}/tests/net/http/provider" provider)
# Other pinned dependencies share the original prefix include directory. Put
# candidate TLS headers first so they cannot be mixed with original OpenSSL.
target_include_directories(libcurl_static BEFORE PRIVATE "${VOID_TLS_REVIEW_PREFIX}/include")
add_executable(candidate_native "${VOID_SOURCE_ROOT}/tests/net/http/native.cpp"
  "${VOID_SOURCE_ROOT}/tests/net/http/runtime.cpp"
  "${VOID_SOURCE_ROOT}/src/runtime/http.cpp" "${VOID_SOURCE_ROOT}/src/runtime/http_bulk.cpp"
  "${VOID_SOURCE_ROOT}/src/runtime/http_resource.cpp"
  "${VOID_SOURCE_ROOT}/src/runtime/http_provider.cpp" "${VOID_SOURCE_ROOT}/src/runtime/url.cpp")
target_compile_features(candidate_native PRIVATE cxx_std_20)
target_compile_options(candidate_native PRIVATE -fno-rtti)
target_include_directories(candidate_native PRIVATE "${VOID_SOURCE_ROOT}/include" "${VOID_HTTP_DEPS_ROOT}/include")
target_link_libraries(candidate_native PRIVATE
  "${VOID_TLS_REVIEW_RUNTIME}/libvoid_runtime.a" "${VOID_TLS_REVIEW_RUNTIME}/libvoid_native_fixture.a"
  CURL::libcurl_static void_http_compression Threads::Threads ${CMAKE_DL_LIBS})
if(VOID_SANITIZERS)
  target_compile_options(candidate_native PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(candidate_native PRIVATE -fsanitize=address,undefined)
endif()
