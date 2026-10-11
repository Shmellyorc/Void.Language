#include "void/lsp.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace vd::lsp {
std::optional<Position> Positions::position(std::size_t offset) const {
    const auto& text=source_.text(); const auto& lines=source_.line_starts();
    if (offset>text.size()) return {};
    auto line=std::size_t(std::upper_bound(lines.begin(),lines.end(),offset)-lines.begin()-1);
    if (line>std::numeric_limits<std::uint32_t>::max()) return {};
    std::uint64_t column=0;
    for (auto i=lines[line];i<offset;) {
        if (text[i]=='\r' || text[i]=='\n') return {}; // Interior CRLF boundary.
        auto bytes=utf8_scalar_bytes(text,i);
        // Rejected disk UTF-8 is shown using one replacement unit per bad byte.
        if (!bytes) bytes=1;
        if (bytes>offset-i) return {};
        column+=bytes==4 ? 2 : 1; i+=bytes;
    }
    if (column>std::numeric_limits<std::uint32_t>::max()) return {};
    return Position{static_cast<std::uint32_t>(line),static_cast<std::uint32_t>(column)};
}
std::optional<std::size_t> Positions::offset(Position pos) const {
    const auto& text=source_.text(); const auto& lines=source_.line_starts();
    if (pos.line>=lines.size()) return {};
    std::uint64_t column=0; auto i=lines[pos.line];
    while (i<text.size() && text[i]!='\r' && text[i]!='\n') {
        if (column==pos.character) return i;
        auto bytes=utf8_scalar_bytes(text,i); if (!bytes) bytes=1;
        auto units=bytes==4 ? 2u : 1u;
        if (column<pos.character && pos.character<column+units) return {};
        column+=units; i+=bytes;
    }
    return i; // Includes exact EOL and mandated LSP overlong-character clamping.
}
namespace {
int hex(char c) {
    if(c>='0' && c<='9') return c-'0';
    if(c>='a' && c<='f') return c-'a'+10;
    if(c>='A' && c<='F') return c-'A'+10;
    return -1;
}
bool letter(char c) { return (c>='A' && c<='Z') || (c>='a' && c<='z'); }
}
std::string file_uri(const std::filesystem::path& path) {
    if (!path.is_absolute()) throw std::invalid_argument("File URI requires an absolute path");
    auto utf8=path.generic_u8string(); std::string raw(utf8.begin(),utf8.end());
#ifdef _WIN32
    if (raw.starts_with("//")) throw std::invalid_argument("Network file URIs are unsupported");
    raw="/"+raw;
#endif
    std::string result="file://";
    constexpr char digits[]="0123456789ABCDEF";
    for (unsigned char c:raw) {
        if (letter(c) || (c>='0' && c<='9') || std::string_view("-._~/:").find(c)!=std::string_view::npos) result+=static_cast<char>(c);
        else { result+='%'; result+=digits[c>>4]; result+=digits[c&15]; }
    }
    return result;
}
std::filesystem::path uri_path(const std::string& uri) {
    if (uri.size()<7) throw std::invalid_argument("Expected an absolute file URI");
    auto scheme=uri.substr(0,7);
    for(auto& c:scheme) if(c>='A' && c<='Z') c+='a'-'A';
    if(scheme!="file://") throw std::invalid_argument("Only local file URIs are supported");
    auto slash=uri.find('/',7);
    if(slash==std::string::npos) throw std::invalid_argument("File URI lacks an absolute path");
    auto authority=uri.substr(7,slash-7);
    for(auto& c:authority) if(c>='A' && c<='Z') c+='a'-'A';
    if(!authority.empty() && authority!="localhost") throw std::invalid_argument("Network file URIs are unsupported");
    std::string raw;
    for(auto i=slash;i<uri.size();++i) {
        auto c=uri[i];
        if(c=='?' || c=='#' || c=='\\' || static_cast<unsigned char>(c)<32) throw std::invalid_argument("Escape reserved file URI characters");
        if(c=='%') {
            if(i+2>=uri.size() || hex(uri[i+1])<0 || hex(uri[i+2])<0) throw std::invalid_argument("Malformed file URI escape");
            c=static_cast<char>((hex(uri[i+1])<<4)|hex(uri[i+2])); i+=2;
        }
        if(c=='\0' || c=='\\') throw std::invalid_argument("Invalid file URI path byte");
        raw+=c;
    }
    for(std::size_t i=0;i<raw.size();) {
        auto bytes=utf8_scalar_bytes(raw,i);
        if(!bytes) throw std::invalid_argument("File URI path is not UTF-8");
        i+=bytes;
    }
#ifdef _WIN32
    if(raw.size()<4 || raw[0]!='/' || !letter(raw[1]) || raw[2]!=':' || raw[3]!='/') throw std::invalid_argument("Expected a local Windows drive file URI");
    raw.erase(0,1);
#else
    if(raw.starts_with("//")) throw std::invalid_argument("Network file URIs are unsupported");
#endif
    auto path=utf8_path(raw);
    if(!path.is_absolute() || path.lexically_normal()!=path) throw std::invalid_argument("Use a normalized absolute file URI");
    return path;
}
}
