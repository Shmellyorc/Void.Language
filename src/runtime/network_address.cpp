#include "network.hpp"
#include <algorithm>
#include <charconv>
#include <cstring>
#ifndef _WIN32
#include <arpa/inet.h>
#endif

namespace vd::net {
std::uint16_t Endpoint::port() const {
    return ntohs(family()==AF_INET6?reinterpret_cast<const sockaddr_in6*>(&storage)->sin6_port:
        reinterpret_cast<const sockaddr_in*>(&storage)->sin_port);
}
std::uint32_t Endpoint::scope() const {
    return family()==AF_INET6?reinterpret_cast<const sockaddr_in6*>(&storage)->sin6_scope_id:0;
}
std::string Endpoint::ip() const {
    std::array<char,INET6_ADDRSTRLEN> text{};
    const void* bytes=family()==AF_INET6?static_cast<const void*>(&reinterpret_cast<const sockaddr_in6*>(&storage)->sin6_addr):
        &reinterpret_cast<const sockaddr_in*>(&storage)->sin_addr;
    if(!inet_ntop(family(),bytes,text.data(),static_cast<AddressSize>(text.size())))
        throw std::logic_error("Invalid native numeric socket address");
    std::string result(text.data());
    if(auto index=scope())result+="%"+std::to_string(index);
    return result;
}
std::string Endpoint::format() const {
    auto address=ip();
    if(family()==AF_INET6)address="["+address+"]";
    return address+":"+std::to_string(port());
}
bool Endpoint::multicast() const {
    if(family()==AF_INET6)return IN6_IS_ADDR_MULTICAST(&reinterpret_cast<const sockaddr_in6*>(&storage)->sin6_addr);
    const auto address=ntohl(reinterpret_cast<const sockaddr_in*>(&storage)->sin_addr.s_addr);
    return (address>>28)==14 || address==0xffffffffu;
}
bool Endpoint::unspecified() const {
    if(family()==AF_INET6)return IN6_IS_ADDR_UNSPECIFIED(&reinterpret_cast<const sockaddr_in6*>(&storage)->sin6_addr);
    return reinterpret_cast<const sockaddr_in*>(&storage)->sin_addr.s_addr==0;
}
bool Endpoint::link_without_scope() const {
    return family()==AF_INET6 && !scope() &&
        IN6_IS_ADDR_LINKLOCAL(&reinterpret_cast<const sockaddr_in6*>(&storage)->sin6_addr);
}

std::optional<Endpoint> parse_ip(std::string_view value, std::uint16_t port) {
    if(value.empty() || value.size()>64 || value.find('\0')!=value.npos)return {};
    const auto separator=value.find('%');
    std::uint32_t scope=0;
    if(separator!=value.npos) {
        auto suffix=value.substr(separator+1);
        const auto result=std::from_chars(suffix.data(),suffix.data()+suffix.size(),scope);
        if(result.ec!=std::errc{} || result.ptr!=suffix.data()+suffix.size())return {};
        value=value.substr(0,separator);
    }
    std::array<char,65> literal{};
    std::copy(value.begin(),value.end(),literal.begin());
    Endpoint result;
    if(value.find(':')!=value.npos) {
        auto& address=*reinterpret_cast<sockaddr_in6*>(&result.storage);
        if(inet_pton(AF_INET6,literal.data(),&address.sin6_addr)!=1 || IN6_IS_ADDR_V4MAPPED(&address.sin6_addr))
            return {};
        address.sin6_family=AF_INET6;
        address.sin6_port=htons(port);
        address.sin6_scope_id=scope;
        result.size=sizeof(address);
        return result;
    }
    if(separator!=value.npos)return {};
    // Use one strict decimal grammar on both native hosts.
    std::array<unsigned char,4> bytes{};
    for(unsigned i=0;i<bytes.size();++i) {
        auto dot=value.find('.');
        if((i==bytes.size()-1)!=(dot==value.npos))return {};
        auto part=value.substr(0,dot);
        if(part.empty() || part.size()>3 || (part.size()>1 && part.front()=='0'))return {};
        unsigned number=0;
        const auto parsed=std::from_chars(part.data(),part.data()+part.size(),number);
        if(parsed.ec!=std::errc{} || parsed.ptr!=part.data()+part.size() || number>255)return {};
        bytes[i]=static_cast<unsigned char>(number);
        if(dot!=value.npos)value.remove_prefix(dot+1);
    }
    auto& address=*reinterpret_cast<sockaddr_in*>(&result.storage);
    std::memcpy(&address.sin_addr,bytes.data(),bytes.size());
    address.sin_family=AF_INET;
    address.sin_port=htons(port);
    result.size=sizeof(address);
    return result;
}
bool dns_name(std::string_view name) {
    if(name.ends_with('.'))name.remove_suffix(1);
    if(name.empty() || name.size()>253)return false;
    bool numeric=true;
    auto alnum=[](char c){return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9');};
    for(;;) {
        const auto dot=name.find('.');
        const auto label=name.substr(0,dot);
        if(label.empty() || label.size()>63 || !alnum(label.front()) || !alnum(label.back()))return false;
        const auto hex_literal=label.size()>2 && label[0]=='0' && (label[1]=='x' || label[1]=='X') &&
            std::ranges::all_of(label.substr(2),[](char c){
                return (c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F');
            });
        if(!hex_literal)for(char c:label) {
            if(!alnum(c) && c!='-')return false;
            if(c<'0' || c>'9')numeric=false;
        }
        if(dot==name.npos)return !numeric;
        name.remove_prefix(dot+1);
    }
}
Reply address_reply(const Endpoint& endpoint) {
    Reply reply;
    reply.port=endpoint.port();
    reply.family=endpoint.public_family();
    reply.detail=endpoint.scope();
    reply.text=endpoint.ip();
    return reply;
}
}
