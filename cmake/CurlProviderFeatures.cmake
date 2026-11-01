# Inspect the configuration actually compiled by the curl target. Normal
# variables set in curl's add_subdirectory scope are not parent feature evidence.
function(void_validate_curl_apple_features target)
  get_target_property(curl_binary_dir ${target} BINARY_DIR)
  set(config "${curl_binary_dir}/curl_config.h")
  if(NOT EXISTS "${config}")
    message(FATAL_ERROR "curl generated configuration missing: ${config}")
  endif()
  foreach(feature IN ITEMS USE_APPLE_IDN USE_APPLE_SECTRUST USE_OPENSSL)
    file(STRINGS "${config}" definition
      REGEX "^[ \t]*#[ \t]*define[ \t]+${feature}[ \t]+1([ \t]|$)")
    if(NOT definition)
      message(FATAL_ERROR
        "macOS HTTP requires native ICU IDN, SecTrust and the pinned TLS provider: ${feature} absent in ${config}")
    endif()
  endforeach()
endfunction()
