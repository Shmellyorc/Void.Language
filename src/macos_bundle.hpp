#pragma once
#include "void/project.hpp"

namespace vd::publish {
inline std::string xml_text(std::string_view input) {
    std::string text;
    for (const char c : input) {
        switch (c) {
        case '&': text += "&amp;"; break;
        case '<': text += "&lt;"; break;
        case '>': text += "&gt;"; break;
        case '"': text += "&quot;"; break;
        case '\'': text += "&apos;"; break;
        default:
            if (static_cast<unsigned char>(c) < 32) throw std::invalid_argument("Invalid control character in bundle metadata.");
            text += c;
        }
    }
    return text;
}

inline std::string bundle_plist(const project::Identity& identity, bool icon) {
    const auto& core = identity.version.core();
    const auto version = core[0] + "." + core[1] + "." + core[2];
    std::string plist = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\"><dict>\n";
    const auto entry = [&](std::string_view key, std::string_view value) {
        plist += "<key>" + std::string(key) + "</key><string>" + xml_text(value) + "</string>\n";
    };
    entry("CFBundleIdentifier", identity.id);
    entry("CFBundleName", identity.name);
    entry("CFBundleDisplayName", identity.name);
    entry("CFBundleVersion", version);
    entry("CFBundleShortVersionString", version);
    entry("CFBundleExecutable", "app");
    entry("CFBundleInfoDictionaryVersion", "6.0");
    entry("CFBundlePackageType", "APPL");
    entry("LSMinimumSystemVersion", "14.0");
    if (icon) entry("CFBundleIconFile", "AppIcon.icns");
    return plist + "</dict></plist>\n";
}
}
