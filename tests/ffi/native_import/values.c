#include "values.h"
int G25Add(int a,int b) { return a+b; }
G25Point G25Shift(G25Point p,int amount) { p.x+=amount;p.y+=0.5f;return p; }
G25Bundle G25Transform(G25Bundle b) { b.points[1].x+=5;b.label[2]='Z';b.gain*=2;return b; }
long long G25Wide(long long n) { return n+17; }
unsigned long G25UnsignedLong(unsigned long n) { return n+9; }
G25Mode G25Select(G25Mode m) { return m==G25_NEGATIVE?G25_POSITIVE:G25_NEGATIVE; }
_Bool G25IsPositive(int n) { return n>0; }
size_t G25Extent(size_t n) { return n+3; }
G25Anonymous G25Echo(G25Anonymous value) {value.n+=2;return value;}
int vdi_call_0(int n) {return n+100;}
vdi_record_0 vdi_echo(vdi_record_0 v) {v.data+=2;return v;}
