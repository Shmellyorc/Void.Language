cmake_minimum_required(VERSION 3.24)
project(VoidLLVMDependencies LANGUAGES C CXX)

# The official Windows 23.1.4 SDK is /MT (release), including in Debug consumers.
# Build ordinary upstream development packages; LLVM's exports stay untouched.
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
# zstd's older policy range must keep the same MSVC runtime abstraction.
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
set(CMAKE_INSTALL_LIBDIR lib CACHE STRING "Library installation directory")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static dependencies" FORCE)
include(FetchContent)

set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(zlib
  URL https://zlib.net/fossils/zlib-1.3.2.tar.gz
  URL_HASH SHA256=bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(zlib)
# CMake 3.24-3.31 FindZLIB recognizes this name (upstream 1.3.2 uses zs.lib).
set_target_properties(zlibstatic PROPERTIES OUTPUT_NAME zlibstatic)

set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_USE_STATIC_RUNTIME ON CACHE BOOL "" FORCE)
FetchContent_Declare(zstd
  URL https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz
  URL_HASH SHA256=eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3
  SOURCE_SUBDIR build/cmake
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(zstd)

# LLVM's Windows SDK also declares LibXml2. Keep XML parsing; avoid unrelated
# optional external codecs, iconv and ICU. Upstream exports carry bcrypt on Windows.
foreach(feature IN ITEMS PROGRAMS TESTS PYTHON ICONV ICU ZLIB MODULES)
  set(LIBXML2_WITH_${feature} OFF CACHE BOOL "" FORCE)
endforeach()
FetchContent_Declare(libxml2
  URL https://download.gnome.org/sources/libxml2/2.15/libxml2-2.15.4.tar.xz
  URL_HASH SHA256=98087fd181d9070724f3fbc65c7377db03038eb92bd882374daff44940138821
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(libxml2)
