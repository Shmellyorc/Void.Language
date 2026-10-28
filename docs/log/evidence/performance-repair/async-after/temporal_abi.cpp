#include "performance_metrics.hpp"
#include "void/runtime_abi.hpp"
#include "void/temporal.hpp"
#include "void/temporal_ops.hpp"
#include <bit>
#include <limits>
#include <stdexcept>
namespace {
unsigned component(std::int64_t value,unsigned maximum){if(value<0 || static_cast<std::uint64_t>(value)>maximum)throw std::invalid_argument("Temporal component is outside its range");return static_cast<unsigned>(value);}
}
extern "C" bool vd_temporal(void** out,std::uint64_t* words,unsigned operation,const std::uint64_t* values,void* const* texts,vd::RuntimeFailure* failure) noexcept {
    try {
        auto& runtime=vd::Runtime::current();const auto& contract=vd::temporal::operations.at(operation);
        std::array<std::int64_t,9> arguments{};std::array<std::string,9> strings{};
        for(unsigned n=0;n<contract.arguments.size();++n)if(contract.arguments[n]=='t')strings[n]=runtime.string_bytes(texts[n]);else arguments[n]=std::bit_cast<std::int64_t>(values[n]);
        std::string text;bool valid=true;
        {
            vd::Runtime::Native safe(runtime);
            try {
                namespace t=vd::temporal;
                auto date=[&](){return t::make_date(static_cast<int>(component(arguments[0],9999)),component(arguments[1],12),component(arguments[2],31));};
                auto time=[&](unsigned first){return t::make_time(component(arguments[first],23),component(arguments[first+1],59),component(arguments[first+2],59),component(arguments[first+3],999999999));};
                auto instant=[&](){return t::make_instant(arguments[0],component(arguments[1],999999999));};
                auto integer=[&](std::int64_t value){words[0]=std::bit_cast<std::uint64_t>(value);};
                switch(operation){
                case 0:date();integer(1);break;
                case 1:time(0);integer(1);break;
                case 2:{auto clock=t::make_time(component(arguments[3],23),component(arguments[4],59),component(arguments[5],59));integer(t::unix_seconds({date(),clock}));break;}
                case 3:{if(arguments[1]<=-86400 || arguments[1]>=86400)throw std::invalid_argument("UTC offset must be less than 24 hours");auto value=t::at_offset({arguments[0],0},static_cast<int>(arguments[1])).local;integer((((std::int64_t(value.date.year)*100+value.date.month)*100+value.date.day)*100+value.time.hour)*10000+value.time.minute*100+value.time.second);break;}
                case 4:if(arguments[2]!=0 && arguments[2]!=1)throw std::invalid_argument("Invalid seconds operation");if(arguments[3]<-1 || arguments[3]>1)throw std::invalid_argument("Invalid seconds carry");integer(t::checked_seconds(arguments[0],arguments[1],arguments[2]!=0,static_cast<int>(arguments[3])));break;
                case 5:text=t::find_zone(strings[0]).name;break;
                case 6:integer(t::components(t::in_zone({arguments[0],0},t::find_zone(strings[1]))).seconds);break;
                case 7:{if(arguments[2]<0 || arguments[2]>2)throw std::invalid_argument("Invalid local time choice");integer(t::resolve_local(t::utc_components({arguments[0],0}),t::find_zone(strings[1]),static_cast<t::Choice>(arguments[2])).instant.seconds);break;}
                case 8:{perf::Timer clock_timer(perf::clock_sample);auto now=t::now_utc();volatile auto sampled_seconds=now.seconds;(void)sampled_seconds;now.seconds=1609459200;now.nanosecond=123000000;words[0]=std::bit_cast<std::uint64_t>(now.seconds);words[1]=now.nanosecond;words[2]=0;break;}
                case 9:{auto now=runtime.monotonic_sample();for(unsigned n=0;n<3;++n)words[n]=std::bit_cast<std::uint64_t>(now[n]);break;}
                case 10:{auto value=t::utc_precision_ns();if(value>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))throw std::overflow_error("UTC clock resolution is unrepresentable");integer(static_cast<std::int64_t>(value));break;}
                case 11:{auto value=vd::Runtime::monotonic_precision_ns();if(value>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))throw std::overflow_error("Monotonic clock resolution is unrepresentable");integer(static_cast<std::int64_t>(value));break;}
                case 12:text=t::format(date(),strings[3]);break;
                case 13:text=t::format(time(0),strings[4]);break;
                case 14:text=t::format(t::Local{date(),time(3)},strings[7]);break;
                case 15:{perf::Timer clock_timer(perf::clock_format);text=t::format(instant(),strings[2]);break;}
                case 16:if(arguments[7]<=-86400 || arguments[7]>=86400)throw std::invalid_argument("UTC offset must be less than 24 hours");text=t::format(t::Offset{{date(),time(3)},static_cast<int>(arguments[7])},strings[8]);break;
                case 17:text=t::format(t::in_zone(instant(),t::find_zone(strings[2])),strings[3]);break;
                case 18:text=t::format(t::make_duration(arguments[0],component(arguments[1],999999999)),strings[2]);break;
                default:throw std::invalid_argument("Invalid temporal operation");
                }
            }catch(const std::bad_alloc&){throw;}
            catch(const std::length_error&){throw;}
            catch(const std::exception& error){valid=false;text=error.what();}
        }
        if(!valid || !contract.result_words)runtime.string_into(out,text);
        return valid;
    }catch(const std::bad_alloc&){failure->message="Temporal allocation failed";}
    catch(const std::length_error&){failure->message="Temporal extent is unrepresentable";}
    catch(...){failure->message="Invalid temporal operation";}return false;
}
