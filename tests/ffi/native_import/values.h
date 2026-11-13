#ifndef G25_VALUES_H
#define G25_VALUES_H
#include <stdint.h>
#include <stddef.h>
#define VDI_EXPORT 11
typedef struct vdi_record_0 { int data; } vdi_record_0;
int vdi_call_0(int);
vdi_record_0 vdi_echo(vdi_record_0);
#define G25_BASE (6 * 7)
#define G25_SCALE (1.25f * 2.0f)
#define G25_U64 UINT64_C(18446744073709551615)
typedef enum G25Mode { G25_NEGATIVE=-3, G25_POSITIVE=7 } G25Mode;
typedef struct G25Point { int x; float y; } G25Point;
typedef struct G25Bundle { G25Point points[2]; char label[4]; double gain; } G25Bundle;
typedef struct { int n; double d; } G25Anonymous;
G25Anonymous G25Echo(G25Anonymous);
int G25Add(int, int);
G25Point G25Shift(G25Point, int);
G25Bundle G25Transform(G25Bundle);
long long G25Wide(long long);
unsigned long G25UnsignedLong(unsigned long);
G25Mode G25Select(G25Mode);
_Bool G25IsPositive(int);
size_t G25Extent(size_t);
#endif
