# Shared notices must accompany either independently installed component.
function(void_install_licenses component)
  set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
  install(FILES "${root}/LICENSE" DESTINATION share/void/licenses
    RENAME VOID-MIT-LICENSE.txt COMPONENT ${component})
  install(FILES "${root}/docs/release/LICENSING_SCOPE.md"
    DESTINATION share/void/licenses COMPONENT ${component})
  install(FILES "${root}/docs/release/REDISTRIBUTION_NOTICES.md"
    DESTINATION share/void/licenses RENAME REDISTRIBUTION.txt COMPONENT ${component})
  install(FILES "${root}/docs/release/NOTICE_INVENTORY.json"
    DESTINATION share/void/licenses COMPONENT ${component})
  install(DIRECTORY "${root}/third_party/llvm-licenses/23.1.4/"
    DESTINATION share/void/licenses/llvm-23.1.4 COMPONENT ${component})
  install(DIRECTORY "${root}/third_party/llvm-dependency-licenses/zstd-1.5.7/"
    DESTINATION share/void/licenses/zstd-1.5.7 COMPONENT ${component})
  # LLVM may use zlib independently of the compiler's HTTP dependency closure.
  install(FILES "${root}/third_party/http-licenses/zlib-1.3.2/LICENSE"
    DESTINATION share/void/licenses/zlib-1.3.2 COMPONENT ${component})
  if(component STREQUAL "compiler")
    install(DIRECTORY "${root}/third_party/host-licenses/" DESTINATION share/void/licenses/host COMPONENT ${component})
    install(FILES "${root}/third_party/host-versions.json" "${root}/tools/setup_host_deps.py" "${root}/tools/setup_host_deps_native.py" "${root}/cmake/host-sodium.targets" "${root}/tools/verify_host_deps.py" DESTINATION share/void/licenses/host COMPONENT ${component})
  endif()
  if(WIN32)
    install(DIRECTORY "${root}/third_party/llvm-dependency-licenses/libxml2-2.15.4/"
      DESTINATION share/void/licenses/libxml2-2.15.4 COMPONENT ${component})
  endif()
endfunction()
