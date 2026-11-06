#include "void/runtime.hpp"
#include <bit>
#include <cstdint>
#include <iostream>
extern "C" float void_recur32(std::int32_t, vd::RuntimeFailure*);
extern "C" double void_recur64(std::int32_t, vd::RuntimeFailure*);
int main(){
 for(int n : {-129,-17,0,1,17,129,1025,1000000}){
  volatile float want32=0.125f;volatile double want64=0.125;
  for(int i=0;i<(n<0?-n:n);++i){volatile float product32=want32*1.0000001f;want32=product32+0.125f;volatile double product64=want64*1.0000001;want64=product64+0.125;}
  vd::RuntimeFailure error32{},error64{};auto got32=void_recur32(n,&error32);auto got64=void_recur64(n,&error64);
  auto a=std::bit_cast<std::uint32_t>(float(want32)),b=std::bit_cast<std::uint32_t>(got32);auto c=std::bit_cast<std::uint64_t>(double(want64)),d=std::bit_cast<std::uint64_t>(got64);
  std::cout<<n<<','<<std::hex<<a<<','<<b<<','<<c<<','<<d<<std::dec<<'\n';
  if(error32.message||error64.message||a!=b||c!=d)return 1;
 }
 return 0;
}
