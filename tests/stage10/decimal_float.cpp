#include "../../src/runtime/decimal_float.hpp"
#include "void/runtime_abi.hpp"
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

void require(bool value) { if (!value) throw std::runtime_error("decimal float regression"); }
template<class T> auto bits(T value) {
    if constexpr (sizeof(T)==4) return std::uint64_t(std::bit_cast<std::uint32_t>(value));
    else return std::bit_cast<std::uint64_t>(value);
}
template<class T> void check(const std::string& text, T expected) {
    for (int mode : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
        require(fesetround(mode)==0);
        feclearexcept(FE_ALL_EXCEPT); feraiseexcept(FE_INVALID); errno=EDOM;
        auto value=vd::detail::decimal_float<T>(text);
        require(bits(value)==bits(expected));
        require(fegetround()==mode && fetestexcept(FE_ALL_EXCEPT)==FE_INVALID && errno==EDOM);
    }
}
template<class T> void corpus() {
    check<T>("0", T(0)); check<T>("-0", -T(0)); check<T>("0e999999", T(0));
    check<T>("1.5", T(1.5)); check<T>("1e9999", std::numeric_limits<T>::infinity());
    check<T>("-1e-9999", -T(0));
#ifndef __APPLE__
    // Existing Linux std::from_chars oracle; Apple has no deployable floating
    // overload at the supported target. Golden bit patterns below run there.
    for (int exponent=-330; exponent<=310; ++exponent) {
        for (const char* mantissa : {"1", "-1", "1.234567890123456789", "9.99999999999999999"}) {
            std::string text=std::string(mantissa)+"e"+std::to_string(exponent);
            fesetround(FE_TONEAREST);
            T expected{}; auto parsed=std::from_chars(text.data(),text.data()+text.size(),expected);
            if (parsed.ec==std::errc{}) check<T>(text,expected);
            else {
                require(parsed.ec==std::errc::result_out_of_range);
                auto value=vd::detail::decimal_float<T>(text);
                require(!std::isfinite(value) || value==0);
            }
        }
    }
#endif
}
void boundary() {
    vd::Runtime runtime; vd::Runtime::Attachment attached(runtime); vd::Runtime::Managed managed(runtime);
    void* input=nullptr; constexpr std::size_t zero=0;
    vd::Runtime::Slot slot{&input,sizeof(input),&zero,1}; vd::Runtime::Frame roots(runtime,&slot,1);
    auto parse=[&](std::string_view text, vd::ScalarEncoding type, unsigned status, std::uint64_t expected=0) {
        runtime.string_into(&input,text); std::uint64_t result=123; vd::RuntimeFailure failure{};
        require(vd_convert_parse(&result,input,static_cast<unsigned>(type),nullptr,0,&failure)==status);
        require(!failure.message); if(!status)require(result==expected);
    };
    for(auto type:{vd::ScalarEncoding::f32,vd::ScalarEncoding::f64}) {
        for(auto text:{"", " 1", "1 ", "nan", "inf", "0x1p0", "1,5", "1_0", "1e", "1e999x"})parse(text,type,1);
        parse(std::string_view("1\0",2),type,1);
        for(auto text:{"1e9999","-1e9999","1e-9999","-1e-9999"})parse(text,type,2);
        parse("+0e9999",type,0);
    }
    parse("-0",vd::ScalarEncoding::f32,0,0x80000000);
    parse("-0.0",vd::ScalarEncoding::f64,0,0x8000000000000000ULL);
    parse("1.401298464324817e-45",vd::ScalarEncoding::f32,0,1);
    parse("5e-324",vd::ScalarEncoding::f64,0,1);
}
int main() {
    // Exercise a comma-decimal thread locale where the host provides it.
    locale_t comma=newlocale(LC_NUMERIC_MASK,"de_DE.UTF-8",nullptr);
    if (!comma) comma=newlocale(LC_NUMERIC_MASK,"de_DE.utf8",nullptr);
    locale_t old=comma?uselocale(comma):nullptr;
    boundary();
    corpus<float>(); corpus<double>();
    check<float>("1.000000059604644775390625",1.0f);
    check<float>("1.000000059604644775390626",std::bit_cast<float>(std::uint32_t(0x3f800001)));
    check<float>("1.401298464324817070923729583289916131280e-45",std::numeric_limits<float>::denorm_min());
    check<float>("3.40282346638528859811704183484516925440e38",std::numeric_limits<float>::max());
    check<double>("1.00000000000000011102230246251565404236316680908203125",1.0);
    check<double>("4.940656458412465441765687928682213723651e-324",std::numeric_limits<double>::denorm_min());
    check<float>("7.006492321624085e-46",0.0f);
    check<float>("7.006492321624086e-46",std::numeric_limits<float>::denorm_min());
    check<double>("2.2250738585072014e-308",std::numeric_limits<double>::min());
    check<double>("1.7976931348623157e308",std::numeric_limits<double>::max());
    if(comma) { uselocale(old); freelocale(comma); }
    fesetround(FE_TONEAREST); feclearexcept(FE_ALL_EXCEPT);
    std::cout << "Decimal float boundaries/oracle passed; comma locale " << (comma?"tested":"unavailable") << '\n';
}
