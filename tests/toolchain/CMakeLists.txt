cmake_minimum_required(VERSION 3.24)
project(VoidLLVMDependencyProbe LANGUAGES C CXX)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
include(../../cmake/LLVMPlatform.cmake)
# Exercise the same discovery with real libraries on any available native host.
# A Linux pass verifies discovery/API/linking only, never the Windows ABI.
void_find_llvm_windows_dependencies()
add_executable(void_llvm_dependency_probe Dependencies.cpp)
target_link_libraries(void_llvm_dependency_probe PRIVATE ZLIB::ZLIB
  zstd::libzstd_static LibXml2::LibXml2)
enable_testing()
add_test(NAME toolchain_llvm_dependencies COMMAND void_llvm_dependency_probe)
set_tests_properties(toolchain_llvm_dependencies PROPERTIES TIMEOUT 30)
