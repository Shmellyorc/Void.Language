#include "void/temporal.hpp"
#include "void/format.hpp"
#include "date/tz.h"
#include <bit>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <algorithm>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <ctime>
#include <cerrno>
#endif
namespace vd::temporal {
namespace {
constexpr std::uint32_t billion=1000000000;
void fraction(std::uint32_t ns){if(ns>=billion)throw std::invalid_argument("Nanoseconds must be in 0..999999999");}
// A two-word signed intermediate avoids compiler-specific integer extensions and
// permits a final fractional carry/borrow to rescue a seconds boundary overflow.
std::int64_t seconds_sum(std::int64_t a,std::int64_t b,bool subtract,int adjustment) {
    std::uint64_t low=std::bit_cast<std::uint64_t>(a),high=a<0?~std::uint64_t{}:0;
    auto add=[&](std::uint64_t word,std::uint64_t upper){auto old=low;low+=word;high+=upper+(low<old);};
    auto word=std::bit_cast<std::uint64_t>(b);auto upper=b<0?~std::uint64_t{}:0;
    add(subtract?~word:word,subtract?~upper:upper);if(subtract)add(1,0);
    if(adjustment>0)add(1,0);else if(adjustment<0)add(~std::uint64_t{},~std::uint64_t{});
    if(high!=((low>>63)?~std::uint64_t{}:0))throw std::overflow_error("Temporal seconds overflow");
    return std::bit_cast<std::int64_t>(low);
}
template<class A,class B> A combine(A a,B b,bool subtract){fraction(a.nanosecond);fraction(b.nanosecond);int carry=0;std::uint32_t ns;
    if(subtract){carry=a.nanosecond<b.nanosecond?-1:0;ns=static_cast<std::uint32_t>(a.nanosecond+std::uint64_t(billion)-b.nanosecond)%billion;}
    else {auto sum=std::uint64_t(a.nanosecond)+b.nanosecond;carry=sum>=billion?1:0;ns=static_cast<std::uint32_t>(sum%billion);}
    return {seconds_sum(a.seconds,b.seconds,subtract,carry),ns};
}
date::sys_seconds civil_seconds(Local value){make_local(value.date,value.time);return date::sys_days{date::year{value.date.year}/value.date.month/value.date.day}+std::chrono::hours{value.time.hour}+std::chrono::minutes{value.time.minute}+std::chrono::seconds{value.time.second};}
Local project(Instant value){fraction(value.nanosecond);static const auto first=date::sys_days{date::year{1}/1/1}.time_since_epoch().count()*std::int64_t(86400);static const auto last=date::sys_days{date::year{10000}/1/1}.time_since_epoch().count()*std::int64_t(86400)-1;
    if(value.seconds<first || value.seconds>last)throw std::out_of_range("Calendar projection requires year 1..9999");
    date::sys_seconds point{std::chrono::seconds{value.seconds}};auto day=date::floor<date::days>(point);date::year_month_day ymd{day};auto time=point-day;date::hh_mm_ss clock{time};
    return {{int(ymd.year()),unsigned(ymd.month()),unsigned(ymd.day())},{static_cast<unsigned>(clock.hours().count()),static_cast<unsigned>(clock.minutes().count()),static_cast<unsigned>(clock.seconds().count()),value.nanosecond}};
}
const date::time_zone* rules(const Zone& zone){if(zone.version!=date::get_tzdb().version)throw std::invalid_argument("Time-zone rules version mismatch");auto* result=date::locate_zone(zone.name);if(result->name()!=zone.name)throw std::invalid_argument("Noncanonical time-zone handle");return result;}
std::string digits(std::uint64_t value,unsigned width=0){auto text=std::to_string(value);if(text.size()<width)text.insert(0,width-text.size(),'0');return text;}
std::string fraction_text(std::uint32_t ns,unsigned width,bool trim){auto value=digits(ns,9).substr(0,width);if(trim)while(!value.empty() && value.back()=='0')value.pop_back();return value;}
std::string clock_default(Time t){auto text=digits(t.hour,2)+":"+digits(t.minute,2)+":"+digits(t.second,2);if(t.nanosecond)text+="."+fraction_text(t.nanosecond,9,true);return text;}
std::string offset_text(int value,bool exact){auto magnitude=static_cast<unsigned>(value<0?-value:value);if(!exact && magnitude%60)throw std::invalid_argument("Offset has nonzero seconds; use zzzz");auto text=std::string(value<0?"-":"+")+digits(magnitude/3600,2)+":"+digits(magnitude/60%60,2);if(exact)text+=":"+digits(magnitude%60,2);return text;}
struct View {Local local{{1,1,1},{0,0,0,0}};bool date=false,time=false,offset=false;int seconds=0;std::string_view zone;};
std::string calendar(View value,std::string_view spec,std::string default_text){
    auto layout=formatting::split_layout(spec);if(layout.pattern.empty())return formatting::align(default_text,layout.alignment);
    formatting::Builder out;bool twelve=false,marker=false;auto pattern=layout.pattern;
    auto fail=[&](std::size_t at,const char* message)->void{throw formatting::Error(layout.pattern_offset+at,message);};
    const char* months[]={"January","February","March","April","May","June","July","August","September","October","November","December"};
    const char* weekdays[]={"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
    for(std::size_t p=0;p<pattern.size();){auto start=p;char c=pattern[p++];
        if(c=='\''){if(p<pattern.size() && pattern[p]=='\''){out.append("'");++p;continue;}bool closed=false;while(p<pattern.size()){char next=pattern[p++];if(next=='\''){if(p<pattern.size() && pattern[p]=='\''){out.append("'");++p;}else{closed=true;break;}}else out.append(std::string_view(&next,1));}if(!closed)fail(start,"Unterminated temporal literal");continue;}
        if(c=='-' || c=='/' || c==':' || c=='.' || c==',' || c==' '){out.append(pattern.substr(start,1));continue;}
        while(p<pattern.size() && pattern[p]==c)++p;auto count=p-start;
        auto require=[&](bool available){if(!available)fail(start,"Temporal token does not apply to this value");};
        auto width=[&](unsigned maximum){if(count>maximum)fail(start,"Invalid temporal token length");};
        switch(c){
        case 'y':require(value.date);if(count!=2 && count!=4)fail(start,"Year token requires yy or yyyy");out.append(digits(count==2?value.local.date.year%100:value.local.date.year,static_cast<unsigned>(count)));break;
        case 'M':require(value.date);width(4);if(count<=2)out.append(digits(value.local.date.month,static_cast<unsigned>(count)));else {std::string_view month=months[value.local.date.month-1];out.append(count==3?month.substr(0,3):month);}break;
        case 'd':require(value.date);width(4);if(count<=2)out.append(digits(value.local.date.day,static_cast<unsigned>(count)));else {auto weekday=date::weekday{date::floor<date::days>(civil_seconds(value.local))}.c_encoding();std::string_view name=weekdays[weekday];out.append(count==3?name.substr(0,3):name);}break;
        case 'H':case 'h':case 'm':case 's':require(value.time);width(2);if(c=='h')twelve=true;out.append(digits(c=='H'?value.local.time.hour:c=='h'?(value.local.time.hour%12?value.local.time.hour%12:12):c=='m'?value.local.time.minute:value.local.time.second,static_cast<unsigned>(count)));break;
        case 't':require(value.time);if(count!=2)fail(start,"AM/PM token requires tt");marker=true;out.append(value.local.time.hour<12?"AM":"PM");break;
        case 'f':case 'F':require(value.time);width(9);out.append(fraction_text(value.local.time.nanosecond,static_cast<unsigned>(count),c=='F'));break;
        case 'z':require(value.offset);if(count!=3 && count!=4)fail(start,"Offset token requires zzz or zzzz");if(count==3 && value.seconds%60)fail(start,"Offset has nonzero seconds; use zzzz");out.append(offset_text(value.seconds,count==4));break;
        case 'V':require(!value.zone.empty());if(count!=2)fail(start,"Zone token requires VV");out.append(value.zone);break;
        default:fail(start,"Unknown temporal token");
        }
    }
    if(twelve && !marker)fail(0,"12-hour patterns require tt");return formatting::align(std::move(out).finish(),layout.alignment);
}
}
std::int64_t checked_seconds(std::int64_t a,std::int64_t b,bool subtract,int adjustment){if(adjustment < -1 || adjustment > 1)throw std::invalid_argument("Invalid seconds adjustment");return seconds_sum(a,b,subtract,adjustment);}
std::int64_t unix_seconds(Local value){return civil_seconds(value).time_since_epoch().count();}
Local utc_components(Instant value){return project(value);}
Date make_date(int y,unsigned m,unsigned d){if(y<1 || y>9999 || m<1 || m>12 || d<1 || d>31 || !date::year_month_day{date::year{y}/m/d}.ok())throw std::invalid_argument("Invalid Gregorian date");return {y,m,d};}
Time make_time(unsigned h,unsigned m,unsigned s,unsigned ns){fraction(ns);if(h>23 || m>59 || s>59)throw std::invalid_argument("Invalid time of day");return {h,m,s,ns};}
Local make_local(Date d,Time t){return {make_date(d.year,d.month,d.day),make_time(t.hour,t.minute,t.second,t.nanosecond)};}
Instant make_instant(std::int64_t s,std::uint32_t ns){fraction(ns);return {s,ns};}
Duration make_duration(std::int64_t s,std::uint32_t ns){fraction(ns);return {s,ns};}
Instant add(Instant a,Duration b){return combine(a,b,false);}Instant subtract(Instant a,Duration b){return combine(a,b,true);}
Duration difference(Instant a,Instant b){auto value=combine(a,b,true);return {value.seconds,value.nanosecond};}
Duration add(Duration a,Duration b){return combine(a,b,false);}Duration subtract(Duration a,Duration b){return combine(a,b,true);}
Offset at_offset(Instant value,int offset){if(offset<=-86400 || offset>=86400)throw std::invalid_argument("UTC offset must be less than 24 hours");return {project({seconds_sum(value.seconds,offset,false,0),value.nanosecond}),offset};}
Zone find_zone(std::string_view name){auto* zone=date::locate_zone(std::string(name));return {zone->name(),date::get_tzdb().version};}
Zoned in_zone(Instant value,const Zone& zone){Zoned result{make_instant(value.seconds,value.nanosecond),zone};components(result);return result;}
Offset components(const Zoned& value){auto* zone=rules(value.zone);fraction(value.instant.nanosecond);const auto first=date::sys_days{date::year{1}/1/1}.time_since_epoch().count()*std::int64_t(86400)-86400;const auto last=date::sys_days{date::year{10000}/1/1}.time_since_epoch().count()*std::int64_t(86400)+86400;if(value.instant.seconds<first || value.instant.seconds>last)throw std::out_of_range("Calendar projection requires year 1..9999");auto info=zone->get_info(date::sys_seconds{std::chrono::seconds{value.instant.seconds}});return at_offset(value.instant,static_cast<int>(info.offset.count()));}
Zoned resolve_local(Local value,const Zone& zone,Choice choice){make_local(value.date,value.time);auto* selected=rules(zone);date::local_seconds local{civil_seconds(value).time_since_epoch()};auto info=selected->get_info(local);if(info.result==date::local_info::nonexistent)throw std::invalid_argument("Local time lies in a time-zone gap");if(info.result==date::local_info::ambiguous && choice==Choice::reject)throw std::invalid_argument("Local time is ambiguous; select earlier or later");if(choice!=Choice::reject && choice!=Choice::earlier && choice!=Choice::later)throw std::invalid_argument("Invalid local time choice");auto offset=info.first.offset;if(info.result==date::local_info::ambiguous && choice==Choice::later)offset=info.second.offset;return in_zone({seconds_sum(local.time_since_epoch().count(),offset.count(),true,0),value.time.nanosecond},zone);}
namespace {
#ifdef _WIN32
std::uint64_t performance_frequency(){LARGE_INTEGER value;if(!QueryPerformanceFrequency(&value) || value.QuadPart<=0)throw std::runtime_error("Performance clock frequency unavailable");return static_cast<std::uint64_t>(value.QuadPart);}
std::uint32_t fractional_nanoseconds(std::uint64_t value,std::uint64_t denominator){
    std::uint64_t quotient=0,remainder=0;
    for(int bit=29;bit>=0;--bit){remainder*=2;quotient*=2;if(remainder>=denominator){remainder-=denominator;++quotient;}if((1000000000u>>bit)&1){remainder+=value;if(remainder>=denominator){remainder-=denominator;++quotient;}}}
    return static_cast<std::uint32_t>(quotient);
}
#else
ClockSample read_clock(clockid_t clock){timespec value{};if(clock_gettime(clock,&value))throw std::system_error(errno,std::generic_category(),"Clock read failed");if(value.tv_nsec<0 || value.tv_nsec>=1000000000)throw std::runtime_error("Invalid native clock fraction");return {value.tv_sec,static_cast<std::uint32_t>(value.tv_nsec)};}
std::uint64_t clock_precision(clockid_t clock){timespec value{};if(clock_getres(clock,&value))throw std::system_error(errno,std::generic_category(),"Clock resolution unavailable");if(value.tv_sec<0 || value.tv_nsec<0 || value.tv_nsec>=1000000000 || static_cast<std::uint64_t>(value.tv_sec)>(std::numeric_limits<std::uint64_t>::max()-value.tv_nsec)/billion)throw std::overflow_error("Native clock resolution is unrepresentable");auto result=std::uint64_t(value.tv_sec)*billion+value.tv_nsec;if(!result)throw std::runtime_error("Invalid zero clock resolution");return result;}
#endif
}
ClockSample read_monotonic(){
#ifdef _WIN32
    LARGE_INTEGER counter;if(!QueryPerformanceCounter(&counter) || counter.QuadPart<0)throw std::runtime_error("Performance clock read failed");auto frequency=performance_frequency();auto count=static_cast<std::uint64_t>(counter.QuadPart);return {static_cast<std::int64_t>(count/frequency),fractional_nanoseconds(count%frequency,frequency)};
#else
    return read_clock(CLOCK_MONOTONIC);
#endif
}
std::uint64_t monotonic_precision_ns(){
#ifdef _WIN32
    auto frequency=performance_frequency();return billion/frequency+(billion%frequency!=0);
#else
    return clock_precision(CLOCK_MONOTONIC);
#endif
}
Instant now_utc(){
#ifdef _WIN32
    FILETIME stamp;GetSystemTimePreciseAsFileTime(&stamp);auto ticks=(std::uint64_t(stamp.dwHighDateTime)<<32)|stamp.dwLowDateTime;return make_instant(static_cast<std::int64_t>(ticks/10000000)-11644473600LL,static_cast<std::uint32_t>(ticks%10000000)*100);
#else
    auto value=read_clock(CLOCK_REALTIME);return make_instant(value.seconds,value.nanosecond);
#endif
}
std::uint64_t utc_precision_ns(){
#ifdef _WIN32
    return std::max<std::uint64_t>(100,monotonic_precision_ns());
#else
    return clock_precision(CLOCK_REALTIME);
#endif
}
std::string format(Date value,std::string_view spec){make_date(value.year,value.month,value.day);return calendar({{value,{0,0,0,0}},true,false,false,0,{}},spec,digits(value.year,4)+"-"+digits(value.month,2)+"-"+digits(value.day,2));}
std::string format(Time value,std::string_view spec){make_time(value.hour,value.minute,value.second,value.nanosecond);return calendar({{{1,1,1},value},false,true,false,0,{}},spec,clock_default(value));}
std::string format(Local value,std::string_view spec){make_local(value.date,value.time);return calendar({value,true,true,false,0,{}},spec,format(value.date)+"T"+clock_default(value.time));}
std::string format(Instant value,std::string_view spec){auto local=project(value);return calendar({local,true,true,true,0,{}},spec,format(local)+"Z");}
std::string format(Offset value,std::string_view spec){make_local(value.local.date,value.local.time);if(value.seconds<=-86400 || value.seconds>=86400)throw std::invalid_argument("UTC offset must be less than 24 hours");return calendar({value.local,true,true,true,value.seconds,{}},spec,format(value.local)+offset_text(value.seconds,value.seconds%60!=0));}
std::string format(const Zoned& value,std::string_view spec){auto civil=components(value);return calendar({civil.local,true,true,true,civil.seconds,value.zone.name},spec,format(civil)+"["+value.zone.name+"]");}
std::string format(Duration value,std::string_view spec){fraction(value.nanosecond);bool negative=value.seconds<0;auto seconds=negative?~std::bit_cast<std::uint64_t>(value.seconds)+(value.nanosecond?0:1):static_cast<std::uint64_t>(value.seconds);auto ns=negative && value.nanosecond?billion-value.nanosecond:value.nanosecond;auto layout=formatting::split_layout(spec);formatting::Builder out;if(negative)out.append("-");auto pattern=layout.pattern;
    if(pattern.empty()){out.append(digits(seconds/86400)+":"+digits(seconds/3600%24,2)+":"+digits(seconds/60%60,2)+":"+digits(seconds%60,2));if(ns)out.append("."+fraction_text(ns,9,true));}
    else {bool days=false,hours=false;for(std::size_t p=0;p<pattern.size();){auto start=p;auto c=pattern[p++];auto fail=[&](const char* why)->void{throw formatting::Error(layout.pattern_offset+start,why);};
        if(c=='\''){if(p<pattern.size() && pattern[p]=='\''){out.append("'");++p;continue;}bool closed=false;while(p<pattern.size()){char next=pattern[p++];if(next=='\''){if(p<pattern.size() && pattern[p]=='\''){out.append("'");++p;}else{closed=true;break;}}else out.append(std::string_view(&next,1));}if(!closed)fail("Unterminated duration literal");continue;}
        if(c=='-' || c=='/' || c==':' || c=='.' || c==',' || c==' '){out.append(pattern.substr(start,1));continue;}while(p<pattern.size() && pattern[p]==c)++p;auto count=p-start;
        if(c=='d' && count<=2){days=true;out.append(digits(seconds/86400,static_cast<unsigned>(count)));}
        else if(c=='H' && count==2){hours=true;out.append(digits(seconds/3600,2));}
        else if((c=='h' || c=='m' || c=='s') && count==2)out.append(digits(c=='h'?seconds/3600%24:c=='m'?seconds/60%60:seconds%60,2));
        else if((c=='f' || c=='F') && count<=9)out.append(fraction_text(ns,static_cast<unsigned>(count),c=='F'));
        else fail("Invalid duration token");
    }if(days && hours)throw formatting::Error(layout.pattern_offset,"Duration HH cannot occur with d or dd");}
    return formatting::align(std::move(out).finish(),layout.alignment);
}
void validate(Kind kind,std::string_view spec) {
    auto layout=formatting::split_layout(spec);if(layout.pattern.empty())return;
    try {
        if(kind==Kind::duration){format(Duration{0,0},layout.pattern);return;}
        View value;value.local={{2000,1,1},{0,0,0,0}};
        value.date=kind!=Kind::time;value.time=kind!=Kind::date;
        value.offset=kind==Kind::instant || kind==Kind::offset || kind==Kind::zoned;
        if(kind==Kind::zoned)value.zone="UTC";
        calendar(value,layout.pattern,"");
    }catch(const formatting::Error& failure){auto reason=std::string(failure.what());auto prefix=reason.find(": ");throw formatting::Error(layout.pattern_offset+failure.offset,prefix==std::string::npos?reason:reason.substr(prefix+2));}
}

}
