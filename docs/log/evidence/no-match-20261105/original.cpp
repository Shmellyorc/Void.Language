#define main unused_main
#include "base.cpp"
#undef main
int main(int argc,char** argv)try{check(argc==3,"root/workspace");fs::current_path(argv[2]);struct rlimit stack{};check(getrlimit(RLIMIT_STACK,&stack)==0 && stack.rlim_cur==1048576 && stack.rlim_max==1048576,"stack");benchmarks(fs::absolute(argv[1]),true);return 0;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
