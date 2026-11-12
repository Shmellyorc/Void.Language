#include "host_internal.hpp"
#include "zip_geometry.hpp"
#include "void/utf8.hpp"
#ifndef VOID_HOST_PROVIDERS_UNAVAILABLE
#include <archive.h>
#include <archive_entry.h>
#include <zlib.h>
#endif
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <limits>
#include <set>
#include <stdexcept>
namespace vd::host {
#ifndef VOID_HOST_PROVIDERS_UNAVAILABLE
namespace {
constexpr std::uint64_t metadata_limit=16*1024*1024;
std::uint16_t u16(const char* p) {return static_cast<unsigned char>(p[0])|(static_cast<unsigned char>(p[1])<<8);}
std::uint32_t u32(const char* p) {return u16(p)|(static_cast<std::uint32_t>(u16(p+2))<<16);}
std::uint64_t u64(const char* p) {return u32(p)|(static_cast<std::uint64_t>(u32(p+4))<<32);}
struct Member {
    std::string name;
    std::uint64_t size=0,compressed=0,local=0,data_end=0;
    std::uint32_t crc=0;
    std::uint16_t flags=0,method=0;
    bool directory=false,wide=false;
};
struct Zip final : Object {
    std::shared_ptr<filesystem::FileBorrow> file;
    struct archive* reader=nullptr;
    std::vector<Member> members;
    std::array<char,65536> buffer{};
    std::uint64_t length=0,offset=0,expanded=0,limit=0,entry_bytes=0;
    std::size_t index=0;
    bool done=true,failed=false,eof=false;
    ~Zip() override {close();}
    std::error_code close() noexcept override {
        int code=reader?archive_read_free(reader):ARCHIVE_OK;reader=nullptr;file.reset();
        return code==ARCHIVE_OK?std::error_code{}:std::make_error_code(std::errc::io_error);
    }
    // Release the input reservation before the ledger closes ordinary IO files.
    std::error_code quiesce() noexcept override {return close();}
    bool at(std::uint64_t where,char* output,std::size_t count) const {
        if(where>length || count>length-where)return false;
        auto result=filesystem::read_at(*file->file(),where,output,count);
        return result.error==filesystem::ErrorKind::success && static_cast<std::size_t>(result.completed)==count;
    }
    static la_ssize_t read(struct archive* a,void* data,const void** output) {
        auto& z=*static_cast<Zip*>(data);auto count=static_cast<std::size_t>(std::min<std::uint64_t>(z.buffer.size(),z.length-z.offset));
        if(!z.at(z.offset,z.buffer.data(),count)){archive_set_error(a,EIO,"ZIP input read failed");return -1;}
        *output=z.buffer.data();z.offset+=count;return count;
    }
    static la_int64_t seek(struct archive* a,void* data,la_int64_t offset,int origin) {
        auto& z=*static_cast<Zip*>(data);
        auto base=origin==SEEK_SET?0:origin==SEEK_CUR?z.offset:origin==SEEK_END?z.length:UINT64_MAX;
        if(base==UINT64_MAX || (offset<0 && static_cast<std::uint64_t>(-(offset+1))+1>base) ||
           (offset>=0 && static_cast<std::uint64_t>(offset)>z.length-base)) {archive_set_error(a,EINVAL,"ZIP seek outside input");return ARCHIVE_FATAL;}
        z.offset=offset<0?base-(static_cast<std::uint64_t>(-(offset+1))+1):base+offset;return z.offset;
    }
};
bool name(const std::string& value,bool directory) {
    if(value.empty() || value.size()>4096 || value[0]=='/' || value.find_first_of("\\:\0",0,3)!=std::string::npos || !valid_utf8(value))return false;
    // Portable curated ZIP profile: ASCII components, without platform aliases.
    std::size_t start=0;
    while(start<value.size()) {
        auto end=value.find('/',start);auto part=value.substr(start,end==std::string::npos?end:end-start);
        if(part.empty() || part=="." || part==".." || part.back()=='.' || part.back()==' ' || part.size()>255)return false;
        for(unsigned char c:part)if(c<32 || c>=127)return false;
        auto stem=part.substr(0,part.find('.'));std::transform(stem.begin(),stem.end(),stem.begin(),[](unsigned char c){return c>='a' && c<='z'?c-32:c;});
        if(stem=="CON" || stem=="PRN" || stem=="AUX" || stem=="NUL" ||
          (stem.size()==4 && (stem.substr(0,3)=="COM" || stem.substr(0,3)=="LPT") && stem[3]>='1' && stem[3]<='9'))return false;
        if(end==std::string::npos)return !directory;
        if(end==value.size()-1)return directory;
        start=end+1;
    }
    return false;
}
bool ascii_alphanumeric(unsigned char c) {
    return (c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9');
}

bool export_name(const std::string& value) {
    return !value.empty() && ascii_alphanumeric(value.front()) &&
        std::all_of(value.begin(),value.end(),[](unsigned char c) {
            return ascii_alphanumeric(c) || c=='.' || c=='_' || c=='-';
        }) && name(value,false);
}

struct ExportMember {
    std::string name,bytes;
    std::uint32_t offset=0,crc=0;
};

void put16(std::string& image,std::uint16_t value) {
    image.push_back(static_cast<char>(value & 0xff));
    image.push_back(static_cast<char>(value >> 8));
}

void put32(std::string& image,std::uint32_t value) {
    put16(image,static_cast<std::uint16_t>(value & 0xffff));
    put16(image,static_cast<std::uint16_t>(value >> 16));
}

void file_fields(std::string& image,const ExportMember& member) {
    put16(image,10); // ZIP 1.0 needed, no flags, stored, 1980-01-01 00:00:00.
    put16(image,0);
    put16(image,0);
    put16(image,0);
    put16(image,33);
    put32(image,member.crc);
    const auto bytes=static_cast<std::uint32_t>(member.bytes.size());
    put32(image,bytes);
    put32(image,bytes);
    put16(image,static_cast<std::uint16_t>(member.name.size()));
    put16(image,0); // Extra fields.
}

std::string zip_image(std::vector<ExportMember>& members,const ZipGeometry& geometry) {
    std::sort(members.begin(),members.end(),[](const auto& a,const auto& b){return a.name<b.name;});
    std::string image;
    image.reserve(geometry.size());
    for(auto& member:members) {
        member.offset=static_cast<std::uint32_t>(image.size());
        member.crc=static_cast<std::uint32_t>(crc32_z(0,
            reinterpret_cast<const Bytef*>(member.bytes.data()),member.bytes.size()));
        put32(image,0x04034b50);
        file_fields(image,member);
        image+=member.name;
        image+=member.bytes;
    }
    const auto central=static_cast<std::uint32_t>(image.size());
    for(const auto& member:members) {
        put32(image,0x02014b50);
        put16(image,20); // Made by DOS, ZIP 2.0.
        file_fields(image,member);
        put16(image,0); // Comment length.
        put16(image,0); // Disk number.
        put16(image,0); // Internal attributes.
        put32(image,0); // External attributes.
        put32(image,member.offset);
        image+=member.name;
    }
    const auto central_size=static_cast<std::uint32_t>(image.size())-central;
    const auto count=static_cast<std::uint16_t>(members.size());
    put32(image,0x06054b50);
    put16(image,0);
    put16(image,0);
    put16(image,count);
    put16(image,count);
    put32(image,central_size);
    put32(image,central);
    put16(image,0);
    if(image.size()!=geometry.size())throw std::logic_error("ZIP output geometry mismatch");
    return image;
}

// Reject metadata extensions that could override names, add links, encryption or
// additional decoders. ZIP64 and bounded timestamp/uid records are sufficient.
bool extra(const std::string& fields,std::uint64_t& size,std::uint64_t& compressed,std::uint64_t& local,bool need_local) {
    bool zip64=false;
    const bool ns=size==UINT32_MAX,nc=compressed==UINT32_MAX,nl=need_local && local==UINT32_MAX;
    for(std::size_t n=0;n<fields.size();) {
        if(fields.size()-n<4)return false;auto tag=u16(fields.data()+n),count=u16(fields.data()+n+2);n+=4;
        if(count>fields.size()-n)return false;auto* data=fields.data()+n;
        if(tag==1) {
            if(zip64)return false;zip64=true;std::size_t pos=0;
            for(auto pair:{std::pair{ns,&size},std::pair{nc,&compressed},std::pair{nl,&local}})if(pair.first) {
                if(count-pos<8)return false;*pair.second=u64(data+pos);pos+=8;
            }
            if(pos!=count)return false;
        } else if(!((tag==0x5455 && count<=13) || (tag==0x7875 && count<=11) || (tag==0x000a && count<=32)))return false;
        n+=count;
    }
    return !(ns || nc || nl) || zip64;
}
Result preflight(Zip& z,std::uint64_t count_limit) {
    if(z.length<22)return error(18);
    auto tail_size=static_cast<std::size_t>(std::min<std::uint64_t>(z.length,65557));std::string tail(tail_size,'\0');
    if(!z.at(z.length-tail_size,tail.data(),tail.size()))return error(17);
    std::size_t found=std::string::npos;
    for(std::size_t n=tail.size()-22;;--n) {
        if(u32(tail.data()+n)==0x06054b50 && n+22+u16(tail.data()+n+20)==tail.size()){found=n;break;}
        if(!n)break;
    }
    if(found==std::string::npos)return error(18);
    auto* end=tail.data()+found;auto end_offset=z.length-tail.size()+found;
    if(u16(end+4) || u16(end+6) || u16(end+8)!=u16(end+10))return error(18);
    std::uint64_t count=u16(end+10),central_size=u32(end+12),central=u32(end+16),central_end=end_offset;
    if(count==65535 || central_size==UINT32_MAX || central==UINT32_MAX) {
        std::array<char,20> locator{};std::array<char,56> wide{};
        if(end_offset<20 || !z.at(end_offset-20,locator.data(),locator.size()) || u32(locator.data())!=0x07064b50 || u32(locator.data()+4) || u32(locator.data()+16)!=1)return error(18);
        auto location=u64(locator.data()+8);
        if(location>end_offset-20 || end_offset-20-location!=56 || !z.at(location,wide.data(),wide.size()) || u32(wide.data())!=0x06064b50 || u64(wide.data()+4)!=44 || u32(wide.data()+16) || u32(wide.data()+20) || u64(wide.data()+24)!=u64(wide.data()+32))return error(18);
        count=u64(wide.data()+32);central_size=u64(wide.data()+40);central=u64(wide.data()+48);central_end=location;
    }
    if(count>count_limit || central_size>metadata_limit)return error(12);
    if(central>central_end || central_size!=central_end-central || count>central_size/46)return error(18);
    z.members.reserve(count);std::uint64_t position=central,total=0,metadata=central_size;
    std::set<std::string> names,files;
    for(std::uint64_t n=0;n<count;++n) {
        std::array<char,46> header{};
        if(position>central_end || central_end-position<46 || !z.at(position,header.data(),header.size()) || u32(header.data())!=0x02014b50)return error(18);
        auto* h=header.data();Member m;m.flags=u16(h+8);m.method=u16(h+10);m.crc=u32(h+16);m.compressed=u32(h+20);m.size=u32(h+24);m.local=u32(h+42);
        auto nl=u16(h+28),xl=u16(h+30),cl=u16(h+32);auto attributes=u32(h+38);auto mode=attributes>>16;
        if(u16(h+34) || (m.method!=0 && m.method!=8) || (m.flags&~0x080e) || (m.method==0 && (m.flags&6)))return error(18);
        auto extent=static_cast<std::uint64_t>(46)+nl+xl+cl;if(extent>central_end-position)return error(18);
        m.name.resize(nl);std::string fields(xl,'\0');
        if(!z.at(position+46,m.name.data(),nl) || !z.at(position+46+nl,fields.data(),xl) || !extra(fields,m.size,m.compressed,m.local,true))return error(18);
        m.directory=!m.name.empty() && m.name.back()=='/';
        if((mode&0170000) && (mode&0170000)!=(m.directory?0040000:0100000))return error(18);
        if(mode&07000)return error(18);
        if(!name(m.name,m.directory) || (m.directory && (m.size || m.compressed || m.crc)) || (m.method==0 && m.compressed!=m.size))return error(18);
        auto canonical=m.name;
        if(m.directory)canonical.pop_back();
        std::transform(canonical.begin(),canonical.end(),canonical.begin(),[](unsigned char c){return c>='A' && c<='Z'?c+32:c;});
        if(!names.insert(canonical).second)return error(18);
        if(!m.directory)files.insert(std::move(canonical));
        if(m.size>z.limit-total)return error(12);total+=m.size;
        // Bound local names and extras before the provider can parse them.
        std::array<char,30> local{};
        if(m.local>=central || !z.at(m.local,local.data(),local.size()) || u32(local.data())!=0x04034b50)return error(18);
        auto* l=local.data();auto ln=u16(l+26),lx=u16(l+28);
        if(u16(l+6)!=m.flags || u16(l+8)!=m.method || ln!=nl || metadata_limit-metadata<static_cast<std::uint64_t>(ln)+lx+30)return error(12);
        metadata+=ln+lx+30;
        std::string local_name(ln,'\0'),local_extra(lx,'\0');std::uint64_t ls=u32(l+22),lc=u32(l+18),unused=0;
        m.wide=ls==UINT32_MAX || lc==UINT32_MAX;
        if(!z.at(m.local+30,local_name.data(),ln) || local_name!=m.name || !z.at(m.local+30+ln,local_extra.data(),lx) || !extra(local_extra,ls,lc,unused,false))return error(18);
        auto data=m.local+30+ln+lx;
        if(data>central || m.compressed>central-data)return error(18);m.data_end=data+m.compressed;
        if(m.flags&8) {
            std::array<char,24> descriptor{};auto bytes=m.wide?20u:12u;
            if(!z.at(m.data_end,descriptor.data(),4))return error(18);
            bool signature=u32(descriptor.data())==0x08074b50;auto length=bytes+(signature?4:0);
            if(!z.at(m.data_end,descriptor.data(),length))return error(18);auto* d=descriptor.data()+(signature?4:0);
            if(u32(d)!=m.crc || (m.wide?u64(d+4):u32(d+4))!=m.compressed || (m.wide?u64(d+12):u32(d+8))!=m.size)return error(18);
            m.data_end+=length;
        } else if(ls!=m.size || lc!=m.compressed || u32(l+14)!=m.crc)return error(18);
        z.members.push_back(std::move(m));position+=extent;
    }
    if(position!=central_end)return error(18);
    for(const auto& member:z.members) {
        auto canonical=member.name;
        std::transform(canonical.begin(),canonical.end(),canonical.begin(),[](unsigned char c){return c>='A' && c<='Z'?c+32:c;});
        for(auto slash=canonical.find('/');slash!=std::string::npos;slash=canonical.find('/',slash+1))
            if(files.contains(canonical.substr(0,slash)))return error(18);
    }
    std::sort(z.members.begin(),z.members.end(),[](const auto& a,const auto& b){return a.local<b.local;});
    std::uint64_t boundary=0;
    for(const auto& m:z.members){if(m.local!=boundary)return error(18);boundary=m.data_end;}
    if(boundary!=central)return error(18);
    return {};
}
}
#endif
Result encode_zip(Runtime& runtime,void* input,const NativeArrayElement* element,
                  std::span<const std::int64_t,4> limits) {
#ifdef VOID_HOST_PROVIDERS_UNAVAILABLE
    (void)runtime;(void)input;(void)element;(void)limits;
    return error(11);
#else
    const auto [count_limit,entry_limit,expanded_limit,archive_limit]=
        std::array{limits[0],limits[1],limits[2],limits[3]};
    if(count_limit<0 || count_limit>64 || entry_limit<0 || entry_limit>4*1024*1024 ||
       expanded_limit<0 || expanded_limit>static_cast<std::int64_t>(byte_snapshot_limit) ||
       archive_limit<static_cast<std::int64_t>(zip_end_bytes) || archive_limit>static_cast<std::int64_t>(byte_snapshot_limit))
        return error(6);

    struct Input {void* name;void* bytes;};
    constexpr std::array references{offsetof(Input,name),offsetof(Input,bytes)};
    if(!element || element->bytes!=sizeof(Input) || element->count!=2 || !element->fields ||
       element->fields[0].kind!=12 || element->fields[0].offset!=references[0] ||
       element->fields[1].kind!=15 || element->fields[1].offset!=references[1])
        throw std::logic_error("Invalid ZIP input ABI");
    constexpr std::size_t zero=0;
    Runtime::Slot slot{&input,sizeof(input),&zero,1};
    Runtime::Frame root(runtime,&slot,1);
    const auto layout=runtime.native_array_layout(*element,0);
    const auto count=runtime.array_count(runtime.root_word(&input),layout,true);
    if(count>static_cast<std::uint64_t>(count_limit))return error(12);

    ZipGeometry geometry(static_cast<std::uint64_t>(archive_limit));
    auto expanded_room=static_cast<std::uint64_t>(expanded_limit);
    std::vector<ExportMember> members;
    members.reserve(count);
    std::set<std::string> names;
    for(std::uint64_t n=0;n<count;++n) {
        Input item{};
        Runtime::Slot item_slot{&item,sizeof(item),references.data(),references.size()};
        Runtime::Frame item_root(runtime,&item_slot,1);
        runtime.array_read(&item,runtime.root_word(&input),layout,true,n);
        const auto text=runtime.string_bytes(item.name);
        if(text.size()>zip_name_limit)return error(20);
        std::string filename(text);
        if(!export_name(filename))return error(20);
        auto canonical=filename;
        for(char& c:canonical)if(c>='A' && c<='Z')c+=('a'-'A');
        if(!names.insert(std::move(canonical)).second)return error(20);
        const auto room=geometry.payload_room(filename.size());
        if(!room)return error(12);
        auto bytes=runtime.bounded_byte_array_bytes(runtime.root_word(&item.bytes),
            std::min({*room,expanded_room,static_cast<std::uint64_t>(entry_limit)}));
        if(!bytes)return error(12);
        if(!geometry.add(filename.size(),bytes->size()))throw std::logic_error("ZIP payload exceeds preflight");
        expanded_room-=bytes->size();
        members.push_back({std::move(filename),std::move(*bytes)});
    }
    Runtime::Native safe(runtime);
    Result result;
    result.bytes=zip_image(members,geometry);
    return result;
#endif
}
Result zip_execute(Request& q) {
#ifdef VOID_HOST_PROVIDERS_UNAVAILABLE
    (void)q;return error(11);
#else
    if(q.operation==zip_open) {
        if(q.numbers[0]<22 || q.numbers[0]>16ll*1024*1024*1024 || q.numbers[1]<1 || q.numbers[1]>100000 || q.numbers[2]<0 || q.numbers[2]>64ll*1024*1024*1024)return error(6);
        auto z=std::make_shared<Zip>();z->file=q.file;z->limit=q.numbers[2];
        auto extent=filesystem::regular_length(*q.file->file());
        if(extent.error!=filesystem::ErrorKind::success)return error(18,extent.os_code);
        z->length=extent.number;
        if(z->length>static_cast<std::uint64_t>(q.numbers[0]))return error(12);
        auto r=preflight(*z,q.numbers[1]);if(r.kind)return r;
        z->reader=archive_read_new();if(!z->reader)throw std::bad_alloc();
        if(archive_read_support_filter_none(z->reader)!=ARCHIVE_OK || archive_read_support_format_zip_seekable(z->reader)!=ARCHIVE_OK ||
           archive_read_set_seek_callback(z->reader,&Zip::seek)!=ARCHIVE_OK || archive_read_open(z->reader,z.get(),nullptr,&Zip::read,nullptr)!=ARCHIVE_OK)return error(18);
        q.acquired=z;return {};
    }
    auto z=std::static_pointer_cast<Zip>(q.objects[0]);if(!z->reader || z->failed)return error(7);
    if(q.operation==zip_next) {
        if(!z->done)return error(8);
        if(z->eof){Result r;r.flag=true;return r;}
        struct archive_entry* entry=nullptr;auto status=archive_read_next_header(z->reader,&entry);
        if(status==ARCHIVE_EOF && z->index==z->members.size()){z->eof=true;Result r;r.flag=true;return r;}
        if(status!=ARCHIVE_OK || !entry || z->index>=z->members.size()){z->failed=true;return error(18,status);}
        auto& m=z->members[z->index];auto* path=archive_entry_pathname(entry);
        if(!path || path!=m.name || archive_entry_size(entry)<0 || static_cast<std::uint64_t>(archive_entry_size(entry))!=m.size ||
           archive_entry_is_encrypted(entry) || archive_entry_symlink(entry) || archive_entry_hardlink(entry) || archive_entry_filetype(entry)!=(m.directory?AE_IFDIR:AE_IFREG)) {
            z->failed=true;return error(18);
        }
        ++z->index;z->entry_bytes=0;z->done=m.directory;
        Result r;r.text=m.name;r.number=m.size;r.detail=m.directory?1:0;return r;
    }
    if(q.numbers[0]<1 || q.numbers[0]>1048576)return error(6);
    if(!z->index || z->eof)return error(20);
    if(z->done){Result r;r.flag=true;return r;}
    Result r;r.bytes.resize(q.numbers[0]);auto size=archive_read_data(z->reader,r.bytes.data(),r.bytes.size());
    if(size<0){z->failed=true;return error(19,size);}
    r.bytes.resize(size);const auto expected=z->members[z->index-1].size;
    if(static_cast<std::uint64_t>(size)>expected-z->entry_bytes || static_cast<std::uint64_t>(size)>z->limit-z->expanded){z->failed=true;return error(12);}
    z->entry_bytes+=size;z->expanded+=size;
    if(!size) {
        if(z->entry_bytes!=expected){z->failed=true;return error(19);}
        z->done=true;r.flag=true;
    }
    return r;
#endif
}
}
