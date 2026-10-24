#include "void/temporal.hpp"
#include "void/format.hpp"
#include "void/runtime.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
namespace t=vd::temporal;
namespace {unsigned checks=0;
void check(bool value){if(!value)throw std::runtime_error("temporal check "+std::to_string(checks));++checks;}
template<class F>void reject(F operation){try{operation();}catch(const std::exception&){++checks;return;}throw std::runtime_error("invalid temporal input accepted at "+std::to_string(checks));}
}
int main(){try{
    auto leap=t::make_date(2024,2,29);check(t::format(leap)=="2024-02-29");
    check(t::format(leap,"dddd, MMMM dd, yyyy")=="Thursday, February 29, 2024");
    check(t::format(t::make_time(0,3,4,120000000))=="00:03:04.12");
    check(t::format(t::make_time(0,3,4,123456789),"hh:mm:ss.fff tt")=="12:03:04.123 AM");
    check(t::format(t::make_time(13,3,4,120000000),"h:m:s.FFFFFFFFF tt")=="1:3:4.12 PM");
    check(t::format(t::make_local(leap,t::make_time(2,3,4)))=="2024-02-29T02:03:04");
    check(t::format(t::make_instant(-1,500000000))=="1969-12-31T23:59:59.5Z");
    check(t::format(t::at_offset({0,0},-561))=="1969-12-31T23:50:39-00:09:21");
    check(t::format(leap,">12|yyyy-MM-dd")=="  2024-02-29");
    check(t::format(leap,"''yyyy'it''s'")=="'2024it's");
    for(auto invalid:{t::Date{0,1,1},t::Date{10000,1,1},t::Date{2023,2,29},t::Date{2024,257,1},t::Date{2024,1,257}})reject([&]{t::format(invalid);});
    reject([&]{t::make_time(0,0,60);});reject([&]{t::make_instant(0,1000000000);});reject([&]{t::at_offset({0,0},86400);});
    for(auto pattern:{"HH","yyyyy","MMMMy","yyyy'open","%","<999999999999999999999999"})reject([&]{t::format(leap,pattern);});
    reject([&]{t::format(t::make_time(2,3,4),"hh:mm");});reject([&]{t::format(t::make_time(2,3,4),"yyyy");});
    reject([&]{t::format(t::at_offset({0,0},561),"zzz");});
    check(t::format(t::make_duration(-1,500000000))=="-0:00:00:00.5");
    check(t::format(t::make_duration(90061,123456789),"dd:hh:mm:ss.fff")=="01:01:01:01.123");
    check(t::format(t::make_duration(90061),"HH:mm:ss")=="25:01:01");
    check(t::format(t::make_duration(std::numeric_limits<std::int64_t>::min())).starts_with("-106751991167300:"));
    reject([&]{t::format(t::make_duration(0),"d HH");});reject([&]{t::format(t::make_duration(0),"yyyy");});
    constexpr auto maximum=std::numeric_limits<std::int64_t>::max(),minimum=std::numeric_limits<std::int64_t>::min();
    auto rescued=t::add(t::Duration{minimum,999999999},t::Duration{-1,1});check(rescued.seconds==minimum && rescued.nanosecond==0);
    auto difference=t::difference({maximum,0},{-1,1});check(difference.seconds==maximum && difference.nanosecond==999999999);
    reject([&]{t::difference({minimum,999999999},{1,0});});
    reject([&]{t::add(t::Duration{maximum,999999999},t::Duration{0,1});});
    reject([&]{t::subtract(t::Duration{minimum,0},t::Duration{0,1});});
    auto negative=t::subtract(t::Duration{0,0},t::Duration{0,1});check(negative.seconds==-1 && negative.nanosecond==999999999);
    auto ny=t::find_zone("America/New_York");check(ny.version=="2026e");
    auto overlap=t::make_local(t::make_date(2026,11,1),t::make_time(1,30,0));
    reject([&]{t::resolve_local(overlap,ny);});
    auto early=t::resolve_local(overlap,ny,t::Choice::earlier),late=t::resolve_local(overlap,ny,t::Choice::later);
    check(t::difference(late.instant,early.instant).seconds==3600);
    check(t::format(early,"yyyy-MM-dd HH:mm:ss zzz VV")=="2026-11-01 01:30:00 -04:00 America/New_York");
    check(t::format(late)=="2026-11-01T01:30:00-05:00[America/New_York]");
    auto gap=t::make_local(t::make_date(2026,3,8),t::make_time(2,30,0));for(auto choice:{t::Choice::reject,t::Choice::earlier,t::Choice::later})reject([&]{t::resolve_local(gap,ny,choice);});
    reject([&]{t::find_zone("Void/Absent");});reject([&]{t::in_zone({0,0},{ny.name,"other"});});
    reject([&]{t::format(t::Instant{maximum,0});});reject([&]{t::format(t::Instant{minimum,0});});
    auto current=t::now_utc();check(current.nanosecond<1000000000);check(t::utc_precision_ns()>0);
    auto oldest=t::resolve_local(t::make_local(t::make_date(1,1,1),t::make_time(0,0,0)),t::find_zone("Etc/GMT-14"));
    check(t::format(oldest)=="0001-01-01T00:00:00+14:00[Etc/GMT-14]");
    vd::Runtime first_runtime,second_runtime;auto first_clock=first_runtime.monotonic_sample(),second_clock=second_runtime.monotonic_sample();
    check(first_clock[0]==0 && first_clock[1]==0 && second_clock[0]==0 && second_clock[1]==0 && first_clock[2]!=second_clock[2]);
    check(vd::Runtime::monotonic_precision_ns()>0);
    for(auto kind:{t::Kind::date,t::Kind::time,t::Kind::local,t::Kind::instant,t::Kind::offset,t::Kind::zoned,t::Kind::duration})t::validate(kind,">1000000000");
    reject([&]{t::validate(t::Kind::date,">12|HH");});
    check(t::format(t::make_duration(-1,500000000),"ss")=="-00");
    check(t::format(t::make_duration(0),"HH:mm:ss.FFF")=="00:00:00.");
    check(t::format(t::make_local(t::make_date(1999,12,31),t::make_time(23,59,59,999999999)),"yyyy-MM-dd HH:mm:ss.fff")=="1999-12-31 23:59:59.999");
    for(int year:{1,4,100,400,1600,1900,1970,2000,2024,2100,9999})for(unsigned month=1;month<=12;++month){
        const unsigned lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};auto last=lengths[month-1]+(month==2 && (year%400==0 || (year%4==0 && year%100!=0)));
        for(unsigned day:{1u,28u,29u,30u,31u})if(day<=last){auto date=t::make_date(year,month,day);auto local=t::make_local(date,t::make_time(23,59,59,999999999));auto point=t::make_instant(t::unix_seconds(local),999999999);auto restored=t::utc_components(point);check(restored.date.year==year && restored.date.month==month && restored.date.day==day && restored.time.hour==23 && restored.time.minute==59 && restored.time.second==59 && restored.time.nanosecond==999999999);check(t::format(point)==t::format(local)+"Z");}else reject([&]{t::make_date(year,month,day);});
    }
    std::cout<<checks<<" temporal core checks passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
