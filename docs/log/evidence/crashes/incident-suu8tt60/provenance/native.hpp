#pragma once
#include <string>
#include <string_view>
namespace vd {
// Trusted bridge helper, not a second language string type. Owns terminated
// bytes for a borrowed native call. Retaining APIs require a longer-lived owner.
std::string native_utf8(std::string_view text);
}
