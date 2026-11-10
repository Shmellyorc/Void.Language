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
  install(FILES "${root}/third_party/tomlplusplus/LICENSE"
    "${root}/third_party/tomlplusplus/upstream.json"
    "${root}/third_party/tomlplusplus/void-bounds-and-locations.patch"
    DESTINATION share/void/licenses/tomlplusplus COMPONENT ${component})
  install(FILES "${root}/third_party/tomlplusplus/toml.hpp"
    "${root}/third_party/tomlplusplus/README.md"
    DESTINATION share/void/licenses/tomlplusplus/corresponding-source COMPONENT ${component})
  # LLVM may use zlib independently of the compiler's HTTP dependency closure.
  install(FILES "${root}/third_party/http-licenses/zlib-1.3.2/LICENSE"
    DESTINATION share/void/licenses/zlib-1.3.2 COMPONENT ${component})
  if(component STREQUAL "compiler")
    install(DIRECTORY "${root}/third_party/host-licenses/" DESTINATION share/void/licenses/host COMPONENT ${component})
    install(DIRECTORY "${root}/third_party/host-patches/" DESTINATION share/void/licenses/host/patches COMPONENT ${component})
    install(FILES "${root}/third_party/host-versions.json" "${root}/tools/setup_host_deps.py" "${root}/tools/setup_host_deps_native.py" "${root}/cmake/host-sodium.targets" "${root}/tools/verify_host_deps.py" DESTINATION share/void/licenses/host COMPONENT ${component})
    set(host_repro share/void/licenses/host/corresponding-source/repro)
    install(FILES "${root}/tools/setup_host_deps.py" "${root}/tools/setup_host_deps_native.py" "${root}/tools/verify_host_deps.py" "${root}/tools/verify_http_dependencies.py"
      DESTINATION ${host_repro}/tools COMPONENT ${component})
    install(FILES "${root}/tools/ci/llvm_sdk.py" DESTINATION ${host_repro}/tools/ci COMPONENT ${component})
    install(FILES "${root}/cmake/host-sodium.targets" DESTINATION ${host_repro}/cmake COMPONENT ${component})
    install(FILES "${root}/third_party/host-versions.json" "${root}/third_party/http-versions.json" DESTINATION ${host_repro}/third_party COMPONENT ${component})
    install(DIRECTORY "${root}/third_party/host-patches/" DESTINATION ${host_repro}/third_party/host-patches COMPONENT ${component})
  endif()
  if(WIN32)
    install(DIRECTORY "${root}/third_party/llvm-dependency-licenses/libxml2-2.15.4/"
      DESTINATION share/void/licenses/libxml2-2.15.4 COMPONENT ${component})
  endif()
endfunction()
