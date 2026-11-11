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
foreach(library IN LISTS VOID_HOST_LINK_FILES VOID_HTTP_LINK_FILES)
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
  DEPENDS void_runtime void_native_fixture libcurl_static ${VOID_HTTP_LINK_FILES} ${VOID_HOST_LINK_FILES}
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
  string(APPEND native_flags "-Xlinker\n--exclude-libs\n-Xlinker\nlibcurl.a:libssl.a:libcrypto.a:libcares.a:libnghttp2.a:libpsl.a:libz.a:libarchive.a:libsodium.a\n")
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
foreach(library IN LISTS VOID_HOST_LINK_FILES VOID_HTTP_LINK_FILES)
  if(NOT library MATCHES "^\\$<" AND library MATCHES "\\.(a|lib)$")
    install(FILES "${library}" DESTINATION lib/void COMPONENT compiler)
  endif()
endforeach()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # The compiler executes HTTP/URL code in-process too. Its relocated JIT
  # must load the same installed IDN closure as independently linked programs.
  set_property(TARGET voidc APPEND PROPERTY INSTALL_RPATH "$ORIGIN/../lib/void")
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
install(FILES "${PROJECT_SOURCE_DIR}/tools/setup_http_deps.py" "${PROJECT_SOURCE_DIR}/tools/setup_http_deps_native.py" "${PROJECT_SOURCE_DIR}/tools/http_archive.py"
  DESTINATION share/void/licenses/http/corresponding-source COMPONENT compiler)
install(FILES "${VOID_HTTP_DEPS_ROOT}/VOID-HTTP-DEPENDENCIES.json"
  DESTINATION share/void/licenses/http COMPONENT compiler)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/native-install-$<CONFIG>.txt" DESTINATION bin RENAME void-native.txt COMPONENT compiler)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/native-resources-install-$<CONFIG>.txt"
  DESTINATION bin RENAME void-native-resources.txt COMPONENT compiler)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # Use the existing compiler install's notice files, with independent hashes
  # and distinct distribution names for files with the same upstream basename.
  set(package_notices_build "VOID package notices 1\n")
  set(package_notices_install "VOID package notices 1\n")
  foreach(notice IN ITEMS
      "VOID-MIT-LICENSE.txt|LICENSE|VOID-MIT-LICENSE.txt"
      "Ryu-Boost.txt|third_party/ryu/LICENSE-Boost|LICENSE-Boost"
      "date-LICENSE.txt|third_party/date/LICENSE.txt|LICENSE.txt"
      "date-NOTICE.txt|third_party/date/NOTICE|NOTICE"
      "tzdata-LICENSE.txt|third_party/tzdata/LICENSE|LICENSE"
      "Unicode-LICENSE.txt|tools/unicode/LICENSE.txt|UNICODE-LICENSE.txt"
      "LLVM-LICENSE.txt|third_party/llvm-licenses/23.1.4/LICENSE.TXT|llvm-23.1.4/LICENSE.TXT"
      "libarchive-COPYING.txt|third_party/host-licenses/libarchive/COPYING|host/libarchive/COPYING"
      "libarchive-SOURCE-NOTICES.json|third_party/host-licenses/libarchive/SOURCE-NOTICES.json|host/libarchive/SOURCE-NOTICES.json"
      "libsodium-LICENSE.txt|third_party/host-licenses/libsodium/LICENSE|host/libsodium/LICENSE"
      "libsodium-SOURCE-NOTICES.json|third_party/host-licenses/libsodium/SOURCE-NOTICES.json|host/libsodium/SOURCE-NOTICES.json"
      "curl-COPYING.txt|third_party/curl/COPYING|http/COPYING"
      "curl-NOTICE.txt|third_party/curl/NOTICE|http/NOTICE"
      "curl-PROVENANCE.json|third_party/curl/VOID-PROVENANCE.json|http/VOID-PROVENANCE.json"
      "AWS-LC-LICENSE.txt|third_party/http-licenses/aws-lc-5.12.0/LICENSE|http/aws-lc-5.12.0/LICENSE"
      "AWS-LC-NOTICE.txt|third_party/http-licenses/aws-lc-5.12.0/NOTICE|http/aws-lc-5.12.0/NOTICE"
      "c-ares-LICENSE.md|third_party/http-licenses/c-ares-1.34.9/LICENSE.md|http/c-ares-1.34.9/LICENSE.md"
      "nghttp2-COPYING.txt|third_party/http-licenses/nghttp2-1.70.0/COPYING|http/nghttp2-1.70.0/COPYING"
      "zlib-LICENSE.txt|third_party/http-licenses/zlib-1.3.2/LICENSE|http/zlib-1.3.2/LICENSE"
      "libpsl-LICENSE.txt|third_party/http-licenses/libpsl-0.21.5/LICENSE|http/libpsl-0.21.5/LICENSE"
      "libpsl-LICENSE.chromium|third_party/http-licenses/libpsl-0.21.5/LICENSE.chromium|http/libpsl-0.21.5/LICENSE.chromium"
      "PSL-LICENSE.txt|third_party/publicsuffix/LICENSE|http/publicsuffix/LICENSE"
      "PSL-UPSTREAM.json|third_party/publicsuffix/UPSTREAM.json|http/publicsuffix/UPSTREAM.json"
      "public_suffix_list.dat|third_party/publicsuffix/public_suffix_list.dat|http/publicsuffix/public_suffix_list.dat")
    string(REPLACE "|" ";" fields "${notice}")
    list(POP_FRONT fields name source installed)
    set(source "${PROJECT_SOURCE_DIR}/${source}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")
    file(SHA256 "${source}" digest)
    string(APPEND package_notices_build "${digest}\t${name}\t${source}\n")
    string(APPEND package_notices_install "${digest}\t${name}\t../share/void/licenses/${installed}\n")
  endforeach()
  file(GENERATE OUTPUT "$<TARGET_FILE_DIR:voidc>/void-package-notices.txt" CONTENT "${package_notices_build}")
  file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/package-notices-install.txt" CONTENT "${package_notices_install}")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/package-notices-install.txt" DESTINATION bin
    RENAME void-package-notices.txt COMPONENT compiler)
endif()

if(VOID_HOST_DEPS_ROOT)
  install(FILES "${VOID_HOST_DEPS_ROOT}/VOID-HOST-DEPENDENCIES.json" DESTINATION share/void/licenses/host COMPONENT compiler)
  install(DIRECTORY "${PROJECT_SOURCE_DIR}/.toolchains/host/downloads/" DESTINATION share/void/licenses/host/corresponding-source COMPONENT compiler FILES_MATCHING PATTERN "libsodium-1.0.22.tar.gz" PATTERN "libarchive-3.8.11.tar.xz")
endif()
