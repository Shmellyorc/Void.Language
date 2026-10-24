// Native standard-library representation probe; no Windows simulation.
#include "date/date.h"
#include <chrono>
#include <iostream>
void report(const char* label,date::sys_seconds point) {
    auto day=date::floor<date::days>(point);
    date::hh_mm_ss time{point-day};
    std::cout<<label<<" seconds="<<point.time_since_epoch().count()<<" date="
             <<date::year_month_day{day}<<" time="<<time<<'\n';
}
int main() {
    std::cout<<"hours_rep_bytes="<<sizeof(std::chrono::hours::rep)
             <<" minutes_rep_bytes="<<sizeof(std::chrono::minutes::rep)<<'\n';
    auto day=date::sys_days{date::year{9999}/1/1};
    auto before=day+std::chrono::hours{23}+std::chrono::minutes{59}+std::chrono::seconds{59};
    auto after=date::sys_seconds{day}+std::chrono::hours{23}+std::chrono::minutes{59}+std::chrono::seconds{59};
    report("before",before);report("after",after);
    return after.time_since_epoch().count()==253370851199LL?0:1;
}
