// Independent POSIX syscalls: distinguish host bookkeeping from Runtime changes.
#include "../../../../tests/runtime/console_posix.hpp"
#include <termios.h>
#include <unistd.h>
#include <iostream>
void mode(std::ostream& out,const termios& t){
    out<<"{\"iflag\":"<<t.c_iflag<<",\"oflag\":"<<t.c_oflag<<",\"cflag\":"<<t.c_cflag<<",\"lflag\":"<<t.c_lflag
       <<",\"ispeed\":"<<cfgetispeed(&t)<<",\"ospeed\":"<<cfgetospeed(&t)<<",\"cc\":[";
    for(unsigned n=0;n<NCCS;++n){if(n)out<<',';out<<static_cast<unsigned>(t.c_cc[n]);}out<<"]}";
}
int main(int argc,char** argv){
    if(argc!=2)return 2;
    auto descriptors=console_native_test::flags();auto wrote=write(1,"HOST",4);auto write_error=wrote<0?errno:0;
    bool flags=console_native_test::flags_unchanged(descriptors,argv[1]);
    termios original{},raw{},active{},restored{};
    auto get_original=tcgetattr(0,&original);auto get_original_error=get_original<0?errno:0;
    raw=original;raw.c_lflag&=~(ECHO|ECHONL|ICANON);raw.c_cc[VMIN]=1;raw.c_cc[VTIME]=0;
    auto set_raw=tcsetattr(0,TCSANOW,&raw);auto set_raw_error=set_raw<0?errno:0;
    auto get_raw=tcgetattr(0,&active);auto get_raw_error=get_raw<0?errno:0;
    auto set_original=tcsetattr(0,TCSANOW,&original);auto set_original_error=set_original<0?errno:0;
    auto get_restored=tcgetattr(0,&restored);auto get_restored_error=get_restored<0?errno:0;
    std::ofstream out(argv[1]);out<<"{\"native_returns\":["<<wrote<<','<<get_original<<','<<set_raw<<','<<get_raw<<','<<set_original<<','<<get_restored
        <<"],\"native_errno\":["<<write_error<<','<<get_original_error<<','<<set_raw_error<<','<<get_raw_error<<','<<set_original_error<<','<<get_restored_error
        <<"],\"original\":";mode(out,original);out<<",\"requested_raw\":";mode(out,raw);out<<",\"active\":";mode(out,active);out<<",\"restored\":";mode(out,restored);out<<"}\n";
    return !flags || wrote!=4 || get_original || set_raw || get_raw || set_original || get_restored || !out;
}
