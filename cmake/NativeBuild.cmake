# Same-build, host-only native link resources; never search the caller's cwd.
find_program(VOID_CLANG NAMES clang clang-23 NAMES_PER_DIR HINTS "${LLVM_TOOLS_BINARY_DIR}" REQUIRED)
execute_process(COMMAND "${VOID_CLANG}" --version OUTPUT_VARIABLE native_version RESULT_VARIABLE native_status)
if(NOT native_status EQUAL 0 OR NOT native_version MATCHES "clang version 23\\.1\\.4([^0-9]|$)")
  message(FATAL_ERROR "Native builds require Clang 23.1.4; set VOID_CLANG")
endif()
set(native_flags "")
if(UNIX)
  string(APPEND native_flags "-pthread\n")
endif()
if(WIN32)
  string(APPEND native_flags "-fms-runtime-lib=static\n-llegacy_stdio_definitions\n-lbcrypt\n-lshell32\n-lole32\n-luuid\n")
elseif(APPLE)
  # Keep independently compiled native hosts on the same vendor C++ ABI too.
  string(APPEND native_flags "-stdlib=libc++\n-nostdinc++\n-isystem\n${VOID_APPLE_LIBCXX_INCLUDE_DIR}\n")
  string(APPEND native_flags "-framework\nFoundation\n-lobjc\n")
  # Reuse the physical SDK resolved with the vendor C++ headers. The native
  # staging working directory must never reinterpret an SDK name or relative path.
  string(APPEND native_flags "-isysroot\n${VOID_APPLE_SYSROOT}\n")
  foreach(arch IN LISTS CMAKE_OSX_ARCHITECTURES)
    string(APPEND native_flags "-arch\n${arch}\n")
  endforeach()
  if(CMAKE_OSX_DEPLOYMENT_TARGET)
    string(APPEND native_flags "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}\n")
  endif()
endif()
foreach(library IN LISTS VOID_HTTP_SYSTEM_LINK_LIBRARIES)
  if(library MATCHES "^-framework (.+)$")
    string(APPEND native_flags "-framework\n${CMAKE_MATCH_1}\n")
  else()
    string(APPEND native_flags "-l${library}\n")
  endif()
endforeach()
if(VOID_SANITIZERS)
  string(APPEND native_flags "-fsanitize=address,undefined\n")
endif()
set(native_header "VOID native 23.1.4 v2\n${VOID_CLANG}\n${VOID_LINKER}\n")
set(native_http_build "")
set(native_http_install "")
set(native_inventory "$<TARGET_FILE:void_runtime>\n@VOID_NATIVE_DIR@/../lib/void/$<TARGET_FILE_NAME:void_runtime>\n$<TARGET_FILE:void_native_fixture>\n@VOID_NATIVE_DIR@/../lib/void/$<TARGET_FILE_NAME:void_native_fixture>\n")
foreach(library IN LISTS VOID_HTTP_LINK_FILES)
  string(APPEND native_http_build "${library}\n")
  if(library MATCHES "^\\$<TARGET_FILE:")
    set(library_name "$<TARGET_FILE_NAME:libcurl_static>")
  else()
    get_filename_component(library_name "${library}" NAME)
  endif()
  string(APPEND native_http_install "@VOID_NATIVE_DIR@/../lib/void/${library_name}\n")
  string(APPEND native_inventory "${library}\n@VOID_NATIVE_DIR@/../lib/void/${library_name}\n")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/native-inventory-inputs-$<CONFIG>.txt" CONTENT "${native_inventory}")
set(native_resources_output "${CMAKE_CURRENT_BINARY_DIR}")
if(CMAKE_CONFIGURATION_TYPES)
  string(APPEND native_resources_output "/$<CONFIG>")
endif()
add_custom_command(OUTPUT "${native_resources_output}/void-native-resources.txt"
    "${CMAKE_CURRENT_BINARY_DIR}/native-resources-install-$<CONFIG>.txt"
  COMMAND "${CMAKE_COMMAND}" "-DINPUTS=${CMAKE_CURRENT_BINARY_DIR}/native-inventory-inputs-$<CONFIG>.txt"
    "-DBUILD_OUTPUT=${native_resources_output}/void-native-resources.txt"
    "-DINSTALL_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/native-resources-install-$<CONFIG>.txt"
    -P "${CMAKE_CURRENT_LIST_DIR}/NativeInventory.cmake"
  DEPENDS void_runtime void_native_fixture libcurl_static ${VOID_HTTP_LINK_FILES}
    "${CMAKE_CURRENT_BINARY_DIR}/native-inventory-inputs-$<CONFIG>.txt"
    "${CMAKE_CURRENT_LIST_DIR}/NativeInventory.cmake"
  VERBATIM)
add_custom_target(void_native_inventory
  COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:voidc>"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${native_resources_output}/void-native-resources.txt" "$<TARGET_FILE_DIR:voidc>/void-native-resources.txt"
  DEPENDS "${native_resources_output}/void-native-resources.txt"
  VERBATIM)
add_dependencies(voidc void_native_inventory)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  string(APPEND native_flags "-Xlinker\n--exclude-libs\n-Xlinker\nlibcurl.a:libssl.a:libcrypto.a:libcares.a:libnghttp2.a:libpsl.a:libz.a\n")
  string(APPEND native_http_build "-Xlinker\n-rpath\n-Xlinker\n${VOID_HTTP_DEPS_ROOT}/lib\n")
  string(APPEND native_http_install "-Xlinker\n-rpath\n-Xlinker\n@VOID_NATIVE_DIR@/../lib/void\n")
endif()
file(GENERATE OUTPUT "$<TARGET_FILE_DIR:voidc>/void-native.txt"
  CONTENT "${native_header}$<TARGET_FILE:void_runtime>\n$<TARGET_FILE:void_native_fixture>\n${native_http_build}${native_flags}")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/native-install-$<CONFIG>.txt"
  CONTENT "${native_header}../lib/void/$<TARGET_FILE_NAME:void_runtime>\n../lib/void/$<TARGET_FILE_NAME:void_native_fixture>\n${native_http_install}${native_flags}")
install(TARGETS voidc RUNTIME DESTINATION bin COMPONENT compiler)
install(TARGETS void_runtime void_native_fixture ARCHIVE DESTINATION lib/void COMPONENT compiler)
install(TARGETS libcurl_static ARCHIVE DESTINATION lib/void COMPONENT compiler)
foreach(library IN LISTS VOID_HTTP_LINK_FILES)
  if(NOT library MATCHES "^\\$<" AND library MATCHES "\\.(a|lib)$")
    install(FILES "${library}" DESTINATION lib/void COMPONENT compiler)
  endif()
endforeach()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  install(DIRECTORY "${VOID_HTTP_DEPS_ROOT}/lib/" DESTINATION lib/void COMPONENT compiler
    FILES_MATCHING PATTERN "libidn2.so*" PATTERN "libunistring.so*")
endif()
install(DIRECTORY "${PROJECT_SOURCE_DIR}/third_party/http-licenses/" DESTINATION share/void/licenses/http COMPONENT compiler)
install(FILES "${PROJECT_SOURCE_DIR}/third_party/curl/COPYING" "${PROJECT_SOURCE_DIR}/third_party/curl/NOTICE"
  "${PROJECT_SOURCE_DIR}/third_party/curl/VOID-PROVENANCE.json" "${PROJECT_SOURCE_DIR}/third_party/http-versions.json"
  DESTINATION share/void/licenses/http COMPONENT compiler)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/third_party/publicsuffix/" DESTINATION share/void/licenses/http/publicsuffix COMPONENT compiler)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  install(FILES "${PROJECT_SOURCE_DIR}/.toolchains/http/downloads/libidn2-2.3.8.tar.gz"
    "${PROJECT_SOURCE_DIR}/.toolchains/http/downloads/libunistring-1.5.tar.xz"
    DESTINATION share/void/licenses/http/corresponding-source COMPONENT compiler)
endif()
install(FILES "${PROJECT_SOURCE_DIR}/tools/setup_http_deps.py" "${PROJECT_SOURCE_DIR}/tools/setup_http_deps_native.py"
  DESTINATION share/void/licenses/http/corresponding-source COMPONENT compiler)
install(FILES "${VOID_HTTP_DEPS_ROOT}/VOID-HTTP-DEPENDENCIES.json"
  DESTINATION share/void/licenses/http COMPONENT compiler)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/native-install-$<CONFIG>.txt" DESTINATION bin RENAME void-native.txt COMPONENT compiler)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/native-resources-install-$<CONFIG>.txt"
  DESTINATION bin RENAME void-native-resources.txt COMPONENT compiler)
