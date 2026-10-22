#include "void/format.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
int main() {
    using namespace vd::formatting;
    auto start=std::chrono::steady_clock::now();
    Builder b;std::string chunk(256,'x');
    for(unsigned i=0;i<4096;++i)b.append(chunk);
    auto text=std::move(b).finish();
    auto padded=number({NumberKind::u64,42},"F1000000");
    std::size_t scalars=0;
    for(unsigned i=0;i<100000;++i)scalars+=number({NumberKind::u64,i}).size();
    if(text.size()!=1048576 || padded.size()!=1000003 || padded.substr(0,3)!="42." || scalars!=488890)throw std::runtime_error("workload bytes");
    std::cout<<"builder_bytes="<<text.size()<<" precision_bytes="<<padded.size()<<" scalar_bytes="<<scalars<<" elapsed_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<'\n';
}
