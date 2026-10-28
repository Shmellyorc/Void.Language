#pragma once
#include <array>
#include <string_view>
namespace vd::temporal {
// Private, same-build compiler/Runtime operation contracts. 's' is i64, 't' string;
// result_words=0 selects string, 1 i64, 3 the private clock sample value.
struct Operation {std::string_view name,arguments;unsigned result_words;bool clock=false;};
inline constexpr std::array operations={
    Operation{"_date_check","sss",1},Operation{"_time_check","ssss",1},
    Operation{"_civil_seconds","ssssss",1},Operation{"_project","ss",1},
    Operation{"_seconds","ssss",1},Operation{"_zone","t",0},
    Operation{"_zone_offset","st",1},Operation{"_resolve","sts",1},
    Operation{"_now","",3,true},Operation{"_monotonic","",3,true},
    Operation{"_utc_precision","",1,true},Operation{"_monotonic_precision","",1,true},
    Operation{"_date_format","ssst",0},Operation{"_time_format","sssst",0},
    Operation{"_local_format","ssssssst",0},Operation{"_instant_format","sst",0},
    Operation{"_offset_format","sssssssst",0},Operation{"_zoned_format","sstt",0},
    Operation{"_duration_format","sst",0}
};
}
