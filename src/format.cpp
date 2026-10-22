#include "void/format.hpp"
#include "void/utf8.hpp"
#include "ryu/ryu.h"
#include "ryu/ryu2.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <utility>

namespace vd::formatting {
Error::Error(std::size_t at,const std::string& why)
    :std::invalid_argument("format at byte "+std::to_string(at)+": "+why),offset(at) {}
void Builder::append(std::string_view text) {
    if(text.size()>bytes_.max_size()-bytes_.size())throw std::length_error("Formatted string extent is unrepresentable");
    bytes_.append(text);
}
void Builder::repeat(char byte,std::size_t count) {
    if(count>bytes_.max_size()-bytes_.size())throw std::length_error("Formatted string extent is unrepresentable");
    bytes_.append(count,byte);
}
namespace {
bool digit(char c) {return c>='0' && c<='9';}
std::size_t count(std::string_view text,std::size_t& position) {
    const auto begin=position;std::size_t result=0;
    while(position<text.size() && digit(text[position])) {
        unsigned next=text[position++]-'0';
        if(result>(std::string{}.max_size()-next)/10)throw Error(begin,"Count is unrepresentable");
        result=result*10+next;
    }
    if(position==begin)throw Error(begin,"Expected a decimal count");
    return result;
}
std::string literal(std::string_view text,std::size_t& position) {
    std::string result;const auto begin=position++;
    while(position<text.size()) {
        char c=text[position++];
        if(c!='\''){result+=c;continue;}
        if(position<text.size() && text[position]=='\''){++position;result+='\'';continue;}
        return result;
    }
    throw Error(begin,"Unterminated quoted literal");
}
struct Digits {std::string text;int point=1;bool negative=false;std::string special;};
unsigned width(NumberKind kind) {
    switch(kind){case NumberKind::i8:case NumberKind::u8:return 8;case NumberKind::i16:case NumberKind::u16:return 16;case NumberKind::i32:case NumberKind::u32:case NumberKind::f32:return 32;default:return 64;}
}
bool floating(NumberKind k){return k==NumberKind::f32 || k==NumberKind::f64;}
bool signed_integer(NumberKind k){return k<=NumberKind::i64;}
std::uint64_t mask(Number n){auto w=width(n.kind);return w==64?n.bits:n.bits&((std::uint64_t{1}<<w)-1);}
double real(Number n){return n.kind==NumberKind::f32?static_cast<double>(std::bit_cast<float>(static_cast<std::uint32_t>(n.bits))):std::bit_cast<double>(n.bits);}
Digits exact(Number n) {
    Digits d;
    if(!floating(n.kind)) {
        auto bits=mask(n);auto w=width(n.kind);d.negative=signed_integer(n.kind) && (bits&(std::uint64_t{1}<<(w-1)));
        if(d.negative){bits=~bits+1;if(w!=64)bits&=(std::uint64_t{1}<<w)-1;}
        d.text=std::to_string(bits);d.point=static_cast<int>(d.text.size());return d;
    }
    double value=real(n);d.negative=std::signbit(value);
    if(std::isnan(value)){d.negative=false;d.special="nan";return d;}
    if(std::isinf(value)){d.special="inf";return d;}
    // Every finite IEEE binary64 fraction terminates by decimal place 1074.
    // Widening f32 is exact. This scratch bound is a type fact, not a public
    // requested precision ceiling. Percent shifts this exact expansion.
    char buffer[1400];int length=d2fixed_buffered_n(std::fabs(value),1074,buffer);
    std::string text(buffer,static_cast<std::size_t>(length));auto dot=text.find('.');
    d.point=static_cast<int>(dot);text.erase(dot,1);
    while(text.size()>1 && text.back()=='0')text.pop_back();
    auto first=text.find_first_not_of('0');
    if(first==std::string::npos){d.text="0";d.point=1;}
    else {d.point-=static_cast<int>(first);d.text=text.substr(first);}
    return d;
}
void round(Digits& d,std::int64_t keep) {
    if(keep>=static_cast<std::int64_t>(d.text.size()))return;
    if(keep<0){d.text="0";d.point=1;return;}
    auto retained=static_cast<std::size_t>(keep);
    bool up=d.text[retained]>'5';
    if(d.text[retained]=='5')up=d.text.find_first_not_of('0',retained+1)!=std::string::npos || (retained && ((d.text[retained-1]-'0')%2));
    d.text.resize(retained);
    if(up) {
        auto i=d.text.size();while(i && d.text[i-1]=='9')d.text[--i]='0';
        if(i)++d.text[i-1];else{d.text.insert(d.text.begin(),'1');++d.point;}
    }
    if(d.text.empty()){d.text="0";d.point=1;}
}
void fixed(Builder& b,const Digits& d,std::size_t minimum,std::size_t fraction,std::size_t optional,bool grouping) {
    const auto integer=static_cast<std::size_t>(std::max(1,d.point));
    const auto count=std::max(integer,minimum);
    // Check the complete extent before any potentially enormous padding loop.
    if(count>std::string{}.max_size() || fraction>std::string{}.max_size()-count ||
       (grouping && (count-1)/3>std::string{}.max_size()-count-fraction))throw std::length_error("Formatted numeric extent is unrepresentable");
    if(!grouping){b.repeat('0',count-integer);}
    for(std::size_t i=grouping?0:count-integer;i<count;++i){
        if(grouping && i && (count-i)%3==0)b.append(",");
        const auto index=static_cast<std::int64_t>(i)-static_cast<std::int64_t>(count-integer);
        char c=index>=0 && index<d.point && static_cast<std::size_t>(index)<d.text.size()?d.text[static_cast<std::size_t>(index)]:'0';
        b.append(std::string_view(&c,1));
    }
    // Strip only optional zeros; never enumerate requested padding to find them.
    std::size_t actual=fraction;
    const auto last=static_cast<std::int64_t>(d.text.size())-d.point;
    auto significant=last>0?static_cast<std::size_t>(last):0;
    actual=std::max(fraction-optional,std::min(fraction,significant));
    while(actual>fraction-optional){auto index=static_cast<std::int64_t>(d.point)+static_cast<std::int64_t>(actual)-1;if(index>=0 && static_cast<std::size_t>(index)<d.text.size() && d.text[static_cast<std::size_t>(index)]!='0')break;--actual;}
    if(!actual)return;
    b.append(".");
    auto leading=std::min(actual,static_cast<std::size_t>(std::max(0,-d.point)));b.repeat('0',leading);
    auto start=static_cast<std::size_t>(std::max(0,d.point));auto available=start<d.text.size()?d.text.size()-start:0;
    auto copied=std::min(actual-leading,available);b.append(std::string_view(d.text).substr(std::min(start,d.text.size()),copied));b.repeat('0',actual-leading-copied);
}
std::string shortest(Number n) {
    if(!floating(n.kind)){auto d=exact(n);return (d.negative?"-":"")+d.text;}
    const auto value=real(n);
    if(std::isnan(value))return "nan";
    if(std::isinf(value))return std::signbit(value)?"-inf":"inf";
    char buffer[32];auto length=n.kind==NumberKind::f32?f2s_buffered_n(std::bit_cast<float>(static_cast<std::uint32_t>(n.bits)),buffer):d2s_buffered_n(real(n),buffer);
    std::string text(buffer,static_cast<std::size_t>(length));auto e=text.find('E');int exponent=std::stoi(text.substr(e+1));
    bool negative=text[0]=='-';std::string digits=text.substr(negative?1:0,e-(negative?1:0));auto dot=digits.find('.');if(dot!=std::string::npos)digits.erase(dot,1);
    Builder b;if(negative)b.append("-");
    if(exponent>=-4 && exponent<=15){Digits value{digits,exponent+1,false,{}};auto fraction=static_cast<std::size_t>(std::max(0,static_cast<int>(digits.size())-value.point));fixed(b,value,1,fraction,0,false);}
    else {b.append(std::string_view(digits).substr(0,1));if(digits.size()>1){b.append(".");b.append(std::string_view(digits).substr(1));}b.append(exponent<0?"e-":"e+");auto exp=std::to_string(std::abs(exponent));if(exp.size()<2)b.append("0");b.append(exp);}
    return std::move(b).finish();
}
}
Layout split_layout(std::string_view text) {
    Layout result;std::size_t i=0;
    if(!text.empty() && (text[0]=='<' || text[0]=='>' || text[0]=='^')) {
        result.alignment.direction=text[i++];result.alignment.width=count(text,i);
        if(i<text.size()) {if(text[i++]!='|')throw Error(i-1,"Expected | after alignment");if(i==text.size())throw Error(i,"Missing format pattern");}
    }
    result.pattern_offset=i;result.pattern=text.substr(i);return result;
}
Plan parse_numeric(std::string_view text) {
    auto layout=split_layout(text);Plan p;p.alignment=layout.alignment;auto i=layout.pattern_offset;
    if(i==text.size())return p;
    const auto begin=i;char alias=text[i];
    if(std::string_view("FDNPXxBbEe").find(alias)!=std::string_view::npos) {
        ++i;std::size_t precision=i<text.size()?count(text,i):(alias=='E'||alias=='e'?6:alias=='D'?1:alias=='X'||alias=='x'||alias=='B'||alias=='b'?0:2);
        if(i!=text.size())throw Error(i,"Unexpected alias suffix");
        p.kind=alias=='X'||alias=='x'?Plan::hexadecimal:alias=='B'||alias=='b'?Plan::binary:alias=='E'||alias=='e'?Plan::scientific:Plan::decimal;
        p.uppercase=alias=='X'||alias=='B'||alias=='E';p.grouping=alias=='N';p.percent=alias=='P';
        if(alias=='D'||p.kind==Plan::hexadecimal||p.kind==Plan::binary){p.minimum_integer=precision;p.integer_only=true;}
        else p.minimum_fraction=p.maximum_fraction=precision;
        return p;
    }
    p.kind=Plan::decimal;
    if(text[i]=='+'||text[i]=='-')p.positive_sign=text[i++]=='+';
    while(i<text.size() && text[i]=='\'')p.prefix+=literal(text,i);
    std::size_t group=0,groups=0;bool placeholder=false;p.minimum_integer=0;
    while(i<text.size() && (text[i]=='0'||text[i]=='#'||text[i]==',')) {
        char c=text[i++];if(c==','){if(!group || (groups && group!=3))throw Error(i-1,"Invalid grouping");++groups;group=0;p.grouping=true;}
        else {placeholder=true;++group;if(c=='0')++p.minimum_integer;}
    }
    if(!placeholder || (groups && group!=3))throw Error(i,"Expected integer digits in groups of three");
    if(i<text.size() && text[i]=='.') {
        auto dot=i++;bool optional=false;
        while(i<text.size() && (text[i]=='0'||text[i]=='#')){char c=text[i++];if(c=='0' && optional)throw Error(i-1,"Required fraction follows optional digit");optional|=c=='#';++p.maximum_fraction;if(c=='0')++p.minimum_fraction;}
        if(!p.maximum_fraction)throw Error(dot,"Missing fractional placeholders");
    }
    if(i<text.size() && (text[i]=='e'||text[i]=='E')) {
        p.uppercase=text[i++]=='E';p.kind=Plan::scientific;
        if(p.grouping || group!=1)throw Error(begin,"Scientific mantissa requires one integer placeholder");
        if(text.substr(i,3)!="+00")throw Error(i,"Expected exponent +00");i+=3;
    }
    if(i<text.size() && text[i]=='%'){if(p.kind==Plan::scientific)throw Error(i,"Percent and scientific cannot combine");++i;p.percent=true;}
    while(i<text.size() && text[i]=='\'')p.suffix+=literal(text,i);
    if(i!=text.size())throw Error(i,"Unknown numeric pattern token");
    return p;
}
namespace {
std::string aligned(std::string text,Alignment a) {
    std::size_t scalars=0;for(unsigned char c:text)if((c&0xc0)!=0x80)++scalars;
    if(!a.direction || scalars>=a.width)return text;
    auto extra=a.width-scalars;auto left=a.direction=='<'?0:a.direction=='>'?extra:extra/2;
    if(extra>text.max_size()-text.size())throw std::length_error("Aligned string extent is unrepresentable");
    text.reserve(text.size()+extra);
    text.insert(0,left,' ');text.append(extra-left,' ');return text;
}
}
std::string align(std::string_view text,Alignment a) {return aligned(std::string(text),a);}
Alignment parse_text(std::string_view spec) {
    auto plan=parse_numeric(spec);
    if(plan.kind!=Plan::default_display)throw Error(0,"Text and bool accept only alignment");
    return plan.alignment;
}
std::string text(std::string_view value,std::string_view spec) {return align(value,parse_text(spec));}
void validate(NumberKind kind,const Plan& p) {
    if(p.minimum_fraction>p.maximum_fraction)throw Error(0,"Required precision exceeds maximum precision");
    if(floating(kind) && (p.integer_only || p.kind==Plan::hexadecimal || p.kind==Plan::binary))throw Error(0,"D, X and B require an integer");
}
std::string number(Number n,const Plan& p) {
    validate(n.kind,p);
    if(p.kind==Plan::default_display)return aligned(shortest(n),p.alignment);
    Builder b;
    if(p.kind==Plan::hexadecimal||p.kind==Plan::binary) {
        auto bits=mask(n);std::string out;unsigned base=p.kind==Plan::binary?2:16;
        auto alphabet=p.uppercase?"0123456789ABCDEF":"0123456789abcdef";
        do{out+=alphabet[bits%base];bits/=base;}while(bits);std::reverse(out.begin(),out.end());
        b.repeat('0',p.minimum_integer>out.size()?p.minimum_integer-out.size():0);b.append(out);
    } else {
        auto d=exact(n);
        const char sign=d.negative?'-':p.positive_sign?'+':0;
        if(sign && p.prefix.find(sign)==std::string::npos && p.suffix.find(sign)==std::string::npos)b.append(std::string_view(&sign,1));
        b.append(p.prefix);
        if(!d.special.empty())b.append(d.special);
        else {
            if(p.percent && d.text!="0")d.point+=2;
            int exponent=0;if(p.kind==Plan::scientific){exponent=d.text=="0"?0:d.point-1;d.point=1;}
            // Saturate keep beyond the finite exact expansion rather than
            // narrowing a user size_t precision to a machine signed integer.
            auto precision=std::min(p.maximum_fraction,std::size_t{2048});
            round(d,static_cast<std::int64_t>(d.point)+static_cast<std::int64_t>(precision));
            if(p.kind==Plan::scientific && d.point>1){exponent+=d.point-1;d.point=1;}
            fixed(b,d,p.minimum_integer,p.maximum_fraction,p.maximum_fraction-p.minimum_fraction,p.grouping);
            if(p.kind==Plan::scientific){b.append(p.uppercase?"E":"e");b.append(exponent<0?"-":"+");auto exp=std::to_string(std::abs(exponent));if(exp.size()<2)b.append("0");b.append(exp);}
        }
        if(p.percent)b.append("%");b.append(p.suffix);
    }
    return aligned(std::move(b).finish(),p.alignment);
}
std::string number(Number n,std::string_view spec){return number(n,parse_numeric(spec));}
}
