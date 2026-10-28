#pragma once
#include "void/console.hpp"
#include "void/utf8.hpp"
#include <algorithm>

namespace vd::console::detail {
// Native boundary transcoding only, called under the existing input/output
// leases. No line handling, read-ahead or allocations. The input carry is at most
// one UTF-16 unit and one encoded scalar, needed even for read_bytes(1).
// Callbacks use Transfer.completed in UTF-16 units and retain storage until their
// actual OS request completes. This same code is exercised on non-Windows hosts.
class WideConsole {
    std::optional<char16_t> high_, replay_;
    Error replay_error_, after_;
    std::array<std::uint8_t,4> encoded_{};
    unsigned begin_=0,end_=0;
    bool eof_=false;
    static bool high(char16_t u){return u>=0xd800 && u<=0xdbff;}
    static bool low(char16_t u){return u>=0xdc00 && u<=0xdfff;}
    static Error encoding(){return {ErrorKind::invalid_encoding,1113};} // ERROR_NO_UNICODE_TRANSLATION
    void encode(unsigned scalar) noexcept {
        begin_=0;
        if(scalar<0x80){end_=1;encoded_[0]=static_cast<std::uint8_t>(scalar);}
        else {
            end_=scalar<0x800?2:scalar<0x10000?3:4;
            for(unsigned n=end_-1;n;--n){encoded_[n]=static_cast<std::uint8_t>(0x80|(scalar&63));scalar>>=6;}
            encoded_[0]=static_cast<std::uint8_t>((end_==2?0xc0:end_==3?0xe0:0xf0)|scalar);
        }
    }
public:
    bool pending() const noexcept {return high_.has_value() || replay_.has_value() || begin_!=end_;}

    template<class Read>
    Transfer read(std::span<std::uint8_t> out,const Request& request,Read&& native) {
        if(out.empty())return {};
        for(;;){
            if(begin_<end_){
                auto count=std::min<std::size_t>(out.size(),end_-begin_);
                std::copy_n(encoded_.begin()+begin_,count,out.begin());begin_+=static_cast<unsigned>(count);
                // Report this native completion's error to its originating
                // request even when a raw byte read takes only part of the
                // scalar. Remaining converted bytes retain their ownership.
                auto error=after_;after_={};
                return {count,false,error};
            }
            if(after_){auto error=after_;after_={};return {0,false,error};}
            if(eof_){eof_=false;return {0,true,{}};}
            char16_t unit=0;Transfer part;
            if(replay_){unit=*replay_;replay_.reset();part={1,false,replay_error_};replay_error_={};}
            else part=native(std::span<char16_t>(&unit,1),request);
            if(part.completed>1 || (part.eof && (part.completed || part.error)))throw std::logic_error("invalid wide input completion");
            if(!part.completed){
                if(part.eof && high_){high_.reset();eof_=true;return {0,false,encoding()};}
                return part; // Keep a high surrogate through timeout/cancellation.
            }
            if(high_){
                auto first=*high_;high_.reset();
                if(!low(unit)){replay_=unit;replay_error_=part.error;return {0,false,encoding()};}
                encode(0x10000u+((first-0xd800u)<<10)+(unit-0xdc00u));after_=part.error;
            }else if(high(unit)){
                high_=unit;
                if(part.error)return {0,false,part.error};
                continue;
            }else if(low(unit)){
                after_=part.error;return {0,false,encoding()};
            }else {encode(unit);after_=part.error;}
        }
    }
    template<class Write>
    static Transfer write(std::span<const std::uint8_t> bytes,const Request& request,Write&& native) {
        std::string_view text(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        // Validate the ENTIRE source record before producing any device output.
        if(!vd::valid_utf8(text))return {0,false,{ErrorKind::unsupported,1113}};
        std::array<char16_t,2048> units{};
        std::array<std::size_t,2049> offsets{};
        Transfer result;
        while(result.completed<bytes.size()){
            std::size_t count=0,end=result.completed;offsets[0]=0;
            while(end<bytes.size()){
                auto width=vd::utf8_scalar_bytes(text,end);
                if(count+(width==4?2:1)>units.size())break;
                unsigned scalar=bytes[end]&(width==1?127:width==2?31:width==3?15:7);
                for(unsigned n=1;n<width;++n)scalar=(scalar<<6)|(bytes[end+n]&63);
                if(width==4){
                    scalar-=0x10000;units[count++]=static_cast<char16_t>(0xd800+(scalar>>10));
                    offsets[count]=end-result.completed; // No complete source scalar yet.
                    units[count++]=static_cast<char16_t>(0xdc00+(scalar&1023));
                }else units[count++]=static_cast<char16_t>(scalar);
                end+=width;offsets[count]=end-result.completed;
            }
            auto part=native(std::span<const char16_t>(units).first(count),request);
            if(part.completed>count || part.eof)throw std::logic_error("invalid wide output completion");
            auto consumed=part.completed;
            result.completed+=offsets[consumed];
            if(consumed && high(units[consumed-1])){
                // The OS itself accepted half a pair. Do not issue an isolated
                // low surrogate, duplicate the high half or invent UTF-8 bytes.
                result.error=part.error;
                if(!result.error)result.error={ErrorKind::io_failure,29}; // ERROR_WRITE_FAULT
                result.error.partial_utf16_units=1;return result;
            }
            if(part.error){result.error=part.error;return result;}
            if(!consumed){result.error={ErrorKind::io_failure,29};return result;}
        }
        return result;
    }
};
}
