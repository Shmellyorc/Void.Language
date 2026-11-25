#include "void/private_diagnostics.hpp"
#include "frontend/project_digest.hpp"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <stdexcept>

namespace vd::diagnostics {
namespace {
using namespace std::string_view_literals;
constexpr std::string_view magic = "VOID-PRIVATE-DIAGNOSTICS\0\1"sv;
bool digest(std::string_view s) {
    return s.size() == 64 && std::all_of(s.begin(), s.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}
std::string hash(std::string_view s) { project::Digest d; d.bytes(s); return d.finish(); }
void number(std::string& out, std::uint32_t n) { for (unsigned i=0;i<4;++i) out += char(n >> (8*i)); }
void field(std::string& out, std::string_view s) { number(out,static_cast<std::uint32_t>(s.size())); out += s; }
struct Reader {
    std::string_view bytes;
    std::uint32_t number() {
        if (bytes.size()<4) throw std::invalid_argument("Truncated private diagnostic map.");
        std::uint32_t n=0; for(unsigned i=0;i<4;++i)n|=std::uint32_t(static_cast<unsigned char>(bytes[i]))<<(8*i);
        bytes.remove_prefix(4); return n;
    }
    std::string field() {
        auto n=number(); if(n>bytes.size() || n>1024*1024)throw std::invalid_argument("Invalid private diagnostic map field.");
        std::string s(bytes.substr(0,n));bytes.remove_prefix(n);return s;
    }
};
bool valid(Category c) { return c>=Category::runtime && c<=Category::global; }
std::string token(std::string_view build, std::uint32_t id, Category c) {
    return "VOID-D1 " + std::string(build) + " " + std::to_string(id) + " " + std::to_string(static_cast<unsigned>(c)) + "\n";
}
}
PrivateMap::PrivateMap(std::string_view inputs) {
    if(!digest(inputs))throw std::invalid_argument("Private diagnostic inputs require a lowercase SHA-256 digest.");
    project::Digest d;d.field("void.private-diagnostics.v1");d.field(inputs);build_=d.finish();
}
std::string PrivateMap::add(Site s) {
    if(!image_.empty())throw std::invalid_argument("Private diagnostic map is already image-bound.");
    if(!valid(s.category) || sites_.size()>=site_limit)throw std::invalid_argument("Invalid private diagnostic site or site limit.");
    std::size_t size=28;
    for(const auto* value:{&s.function,&s.path,&s.excerpt,&s.description}) {
        if(value->size()>1024*1024)throw std::invalid_argument("Private diagnostic field limit.");
        size+=value->size();
    }
    if(size>map_byte_limit-256-bytes_)throw std::invalid_argument("Private diagnostic map byte limit.");
    auto result=token(build_,static_cast<std::uint32_t>(sites_.size()+1),s.category);
    bytes_+=size;sites_.push_back(std::move(s));return result;
}
void PrivateMap::bind_image(std::string_view bytes) {
    if(bytes.empty() || !image_.empty())throw std::invalid_argument("Private diagnostic map requires one nonempty final image.");
    image_=hash(bytes);
}
std::string PrivateMap::encode() const {
    if(!digest(image_))throw std::invalid_argument("Private diagnostic map has no final image identity.");
    std::string out(magic);field(out,build_);field(out,image_);number(out,static_cast<std::uint32_t>(sites_.size()));
    for(const auto& s:sites_) {number(out,static_cast<unsigned>(s.category));number(out,s.line);number(out,s.column);field(out,s.function);field(out,s.path);field(out,s.excerpt);field(out,s.description);}
    return out;
}
PrivateMap PrivateMap::decode(std::string_view bytes) {
    if(bytes.empty())throw std::invalid_argument("Missing private diagnostic map.");
    if(bytes.size()>map_byte_limit || !bytes.starts_with(magic))throw std::invalid_argument("Unsupported or oversized private diagnostic map.");
    Reader r{bytes.substr(magic.size())};auto build=r.field(),image=r.field();
    if(!digest(build) || !digest(image))throw std::invalid_argument("Invalid private diagnostic map identity.");
    PrivateMap map(std::string(64,'0'));map.build_=std::move(build);
    auto count=r.number();if(count>site_limit)throw std::invalid_argument("Private diagnostic site limit.");
    for(std::uint32_t i=0;i<count;++i) {
        Site s;s.category=static_cast<Category>(r.number());s.line=r.number();s.column=r.number();
        s.function=r.field();s.path=r.field();s.excerpt=r.field();s.description=r.field();(void)map.add(std::move(s));
    }
    if(!r.bytes.empty())throw std::invalid_argument("Trailing private diagnostic map bytes.");
    map.image_=std::move(image);return map;
}
Site PrivateMap::resolve(std::string_view bytes, std::string_view image, std::string_view public_token) {
    auto map=decode(bytes);
    if(map.image_!=hash(image))throw std::invalid_argument("Mismatched private diagnostic image.");
    const auto prefix="VOID-D1 "+map.build_+" ";
    if(!public_token.starts_with(prefix))throw std::invalid_argument("Mismatched diagnostic build or token version.");
    auto rest=public_token.substr(prefix.size());std::uint32_t id=0;
    auto result=std::from_chars(rest.data(),rest.data()+rest.size(),id);
    if(result.ec!=std::errc{} || !id || id>map.sites_.size() || public_token!=token(map.build_,id,map.sites_[id-1].category))
        throw std::invalid_argument("Unknown or malformed private diagnostic site.");
    return map.sites_[id-1];
}
Category category(std::string_view value) {
    std::string lowered(value);for(auto& c:lowered)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    std::string_view description=lowered;
    if(description.find("null")!=description.npos)return Category::null_value;
    if(description.find("bounds")!=description.npos || description.find("index")!=description.npos)return Category::bounds;
    if(description.find("overflow")!=description.npos || description.find("division")!=description.npos || description.find("shift")!=description.npos || description.find("numeric")!=description.npos)return Category::arithmetic;
    return Category::runtime;
}
}
