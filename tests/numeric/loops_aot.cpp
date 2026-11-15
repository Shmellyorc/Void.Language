#include "void/execution.hpp"
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#ifdef __clang__
#pragma clang fp contract(off)
#endif
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
extern "C" std::int32_t __vd_2f6b65726e656c732e766f69643a3a6269646972656374696f6e616c__void(std::int32_t,vd::RuntimeFailure*);
extern "C" std::int32_t __vd_2f6b65726e656c732e766f69643a3a696e76617269616e745f6469766973696f6e__void(std::int32_t,vd::RuntimeFailure*);
extern "C" std::int32_t __vd_2f6b65726e656c732e766f69643a3a696e76617269616e745f636f6e646974696f6e616c__void(std::int32_t,vd::RuntimeFailure*);
extern "C" std::int32_t __vd_2f6b65726e656c732e766f69643a3a7661726961626c655f64697669736f72__void(std::int32_t,vd::RuntimeFailure*);
extern "C" std::int32_t __vd_2f6b65726e656c732e766f69643a3a76617279696e675f636f6e646974696f6e__void(std::int32_t,vd::RuntimeFailure*);
extern "C" std::int32_t __vd_2f6b65726e656c732e766f69643a3a72616e67655f6564676573__void(std::int32_t,vd::RuntimeFailure*);
extern "C" std::int32_t __vd_2f6b65726e656c732e766f69643a3a6c6174655f6661696c757265__void(std::int32_t,vd::RuntimeFailure*);
extern "C" std::int32_t __vd_2f6b65726e656c732e766f69643a3a646570656e64656e74__void(std::int32_t,vd::RuntimeFailure*);
extern "C" float __vd_2f6b65726e656c732e766f69643a3a66703332__void(std::int32_t,float,vd::RuntimeFailure*);
extern "C" double __vd_2f6b65726e656c732e766f69643a3a66703634__void(std::int32_t,double,vd::RuntimeFailure*);
using Entry=std::int32_t(*)(std::int32_t,vd::RuntimeFailure*);
struct Case{const char* name;Entry entry;};
const Case cases[]={
    {"bidirectional",__vd_2f6b65726e656c732e766f69643a3a6269646972656374696f6e616c__void},
    {"invariant_division",__vd_2f6b65726e656c732e766f69643a3a696e76617269616e745f6469766973696f6e__void},
    {"invariant_conditional",__vd_2f6b65726e656c732e766f69643a3a696e76617269616e745f636f6e646974696f6e616c__void},
    {"variable_divisor",__vd_2f6b65726e656c732e766f69643a3a7661726961626c655f64697669736f72__void},
    {"varying_condition",__vd_2f6b65726e656c732e766f69643a3a76617279696e675f636f6e646974696f6e__void},
    {"range_edges",__vd_2f6b65726e656c732e766f69643a3a72616e67655f6564676573__void},
    {"late_failure",__vd_2f6b65726e656c732e766f69643a3a6c6174655f6661696c757265__void},
    {"dependent",__vd_2f6b65726e656c732e766f69643a3a646570656e64656e74__void},
};

bool fails(std::string name,int n){return (name=="variable_divisor" && n==0) || (name=="late_failure" && n>=4);}
std::int32_t oracle(std::string name,int n){
    std::uint32_t total=0;
    if(name=="range_edges")return 1;
    if(name=="dependent"){
        std::int32_t value=7;
        for(int i=0;i!=n;i+=n>0?1:-1)value=std::bit_cast<std::int32_t>(std::uint32_t(value)*3+std::uint32_t(i))%65521;
        return value;
    }
    if(name=="variable_divisor"){
        for(int i=0;i<1000000;++i)total+=std::uint32_t(n>0?i%127:i/n);
    }else if(name=="invariant_division" || name=="invariant_conditional" || name=="varying_condition"){
        for(int i=0;i<n;++i){
            int value=name=="varying_condition"?(i%2==0?i%127:-((i*3)%127)):
                n%2==0?i%127:name=="invariant_division"?i/n:(i*3)%127;
            total+=std::uint32_t(value);
        }
    }else{
        for(int i=0;i!=n;i+=n>0?1:-1)total+=std::uint32_t(name=="late_failure"?i:i%127);
    }
    return std::bit_cast<std::int32_t>(total);
}
std::int32_t invoke(const Case& c,int n,bool failure=false){
    vd::RuntimeFailure error;auto answer=c.entry(n,&error);
    if(failure){require(error.message && std::strstr(error.message,"Division by zero"),"reached division failure changed");return answer;}
    require(!error.message,"unexpected source failure");return answer;
}
template<class Float,class Bits,class EntryFn> void floating(EntryFn fn,const std::array<Bits,10>& inputs){
    for(int n:{-1,0,1,2,3,7,8,9,17,129,1000,1025})for(auto bits:inputs){
        auto x=std::bit_cast<Float>(bits);volatile Float expected=x;
        for(int i=0;i<n;++i){
            volatile Float product=expected*(n%2==0?Float(sizeof(Float)==4?1.0000001:1.0000000000001):Float(sizeof(Float)==4?0.9999999:0.9999999999999));
            expected=n%2==0?product+Float(.125):product-Float(.125);
        }
        vd::RuntimeFailure error;auto answer=fn(n,x,&error);require(!error.message,"FP source failure");
        require(std::bit_cast<Bits>(answer)==std::bit_cast<Bits>(Float(expected)),"strict FP bits/NaN/signed-zero changed");
    }
}
}
int main(int argc,char**argv){try{
    if(argc==4 && std::string(argv[1])=="--bench"){
        const Case* selected=nullptr;for(const auto& c:cases)if(c.name==std::string(argv[2]))selected=&c;
        require(selected,"unknown loop benchmark");int n=std::stoi(argv[3]);require(!fails(selected->name,n),"benchmark failure input");auto expected=oracle(selected->name,n);
        require(invoke(*selected,n)==expected,"warm loop oracle");
        for(int sample=0;sample<7;++sample){auto begin=std::chrono::steady_clock::now();auto value=invoke(*selected,n);auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-begin).count();require(value==expected,"timed loop oracle");std::cout<<sample<<' '<<ns<<' '<<value<<'\n';}
        return 0;
    }
    require(argc==1,"unexpected loop test arguments");
    for(const auto& c:cases)for(int n:{-129,-17,-1,0,1,2,3,4,7,8,9,17,127,128,129,1025,1000000}){
        if(fails(c.name,n)){invoke(c,n,true);continue;}
        require(invoke(c,n)==oracle(c.name,n),"loop result/wrap/range/condition changed");
    }
    // Huge endpoints exercise finite break/inclusive/reverse ranges, not huge work.
    const auto& edge=cases[5];require(invoke(edge,INT32_MIN)==1 && invoke(edge,INT32_MAX)==1,"integer endpoint boundary changed");
    floating<float,std::uint32_t>(__vd_2f6b65726e656c732e766f69643a3a66703332__void,{{0,0x80000000u,1,0x80000001u,0x3e000000u,0x7f7fffffu,0x7f800000u,0xff800000u,0x7fc00001u,0xffc12345u}});
    floating<double,std::uint64_t>(__vd_2f6b65726e656c732e766f69643a3a66703634__void,{{0,0x8000000000000000ull,1,0x8000000000000001ull,0x3fc0000000000000ull,0x7fefffffffffffffull,0x7ff0000000000000ull,0xfff0000000000000ull,0x7ff8000000000001ull,0xfff8123456789abcull}});
    std::cout<<"LOOP_AOT_PASS 8 integer shapes; exact failures/ranges; 240 strict FP bit oracles\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
