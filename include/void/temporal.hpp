#pragma once
#include <cstdint>
#include <string>
#include <string_view>
namespace vd::temporal {
// Validated at every public/native boundary. Seconds and fractions remain separate.
struct Date { int year; unsigned month,day; };
struct Time { unsigned hour,minute,second,nanosecond; };
struct Local { Date date; Time time; };
struct Instant { std::int64_t seconds; std::uint32_t nanosecond; };
struct Duration { std::int64_t seconds; std::uint32_t nanosecond; };
struct Offset { Local local; int seconds; };
struct Zone { std::string name,version; };
struct Zoned { Instant instant; Zone zone; };
enum class Kind { date, time, local, instant, offset, zoned, duration };
void validate(Kind,std::string_view);
enum class Choice { reject, earlier, later };
std::int64_t checked_seconds(std::int64_t,std::int64_t,bool,int);
std::int64_t unix_seconds(Local);
Local utc_components(Instant);
Date make_date(int,unsigned,unsigned);
Time make_time(unsigned,unsigned,unsigned,unsigned=0);
Local make_local(Date,Time);
Instant make_instant(std::int64_t,std::uint32_t=0);
Duration make_duration(std::int64_t,std::uint32_t=0);
Instant add(Instant,Duration);
Instant subtract(Instant,Duration);
Duration difference(Instant,Instant);
Duration add(Duration,Duration);
Duration subtract(Duration,Duration);
Offset at_offset(Instant,int);
Zone find_zone(std::string_view);
Zoned in_zone(Instant,const Zone&);
Zoned resolve_local(Local,const Zone&,Choice=Choice::reject);
Offset components(const Zoned&);
struct ClockSample {std::int64_t seconds;std::uint32_t nanosecond;};
ClockSample read_monotonic();
std::uint64_t monotonic_precision_ns();
Instant now_utc();
std::uint64_t utc_precision_ns();
std::string format(Date,std::string_view={});
std::string format(Time,std::string_view={});
std::string format(Local,std::string_view={});
std::string format(Instant,std::string_view={});
std::string format(Offset,std::string_view={});
std::string format(const Zoned&,std::string_view={});
std::string format(Duration,std::string_view={});
}
