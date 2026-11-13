extern "C" int c_add(int a,int b) {return a+b;}
extern "C" bool c_positive(int n) noexcept {return n>0;}
#include "cpp.hpp"
extern "C" CPoint c_shift(CPoint p) {p.x+=1;return p;}
