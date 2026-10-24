// The same deterministic clock/input model, linked to before/after Runtime archives.
#define main keyboard_driver_main
#include "../../../../tests/runtime/console_keyboard.cpp"
#undef main
int main(){
    auto f=std::make_shared<Fake>();f->pending="\033[Aq";
    f->describing=[&]{f->clock+=std::chrono::milliseconds(31);};
    Context c(Streams::isolated(f));auto key=c.read_key();
    std::cout<<"key="<<static_cast<unsigned>(key.key)<<" bytes=";
    for(auto b:key.bytes)std::cout<<"0123456789abcdef"[b>>4]<<"0123456789abcdef"[b&15];
    std::cout<<" native_batch="<<f->at<<" clock_ms="<<std::chrono::duration_cast<std::chrono::milliseconds>(f->clock.time_since_epoch()).count()<<'\n';
    return key.transfer.error?1:0;
}
