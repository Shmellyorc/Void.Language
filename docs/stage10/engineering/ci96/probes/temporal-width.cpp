// ABI-width reproduction, not native Windows execution. MSVC STL uses int
// representations for hours/minutes; libstdc++ commonly uses 64-bit reps.
#include "date/date.h"
#include <chrono>
#include <iostream>
int main() {
    using hours32=std::chrono::duration<int,std::ratio<3600>>;
    using minutes32=std::chrono::duration<int,std::ratio<60>>;
    auto day=date::sys_days{date::year{9999}/1/1};
#ifdef VOID_REPRO_SAFE
    auto point=date::sys_seconds{day}+hours32{23}+minutes32{59}+std::chrono::seconds{59};
#else
    auto point=day+hours32{23}+minutes32{59}+std::chrono::seconds{59};
#endif
    std::cout<<"seconds="<<point.time_since_epoch().count()<<" date="
             <<date::year_month_day{date::floor<date::days>(point)}<<'\n';
    return point.time_since_epoch().count()==253370851199LL?0:1;
}
