#include <stddef.h>
int accepted(int);
int accepted(int);
int variadic(int, ...);
void* unresolved(void*);
typedef struct Opaque Opaque;
Opaque* acquire_opaque(void);
typedef void (*Callback)(int);
void set_callback(Callback);
union Overlay { int i; double d; };
struct Flexible { int count; char data[]; };
struct __attribute__((packed)) Packed { char c; int n; };
struct Bits { unsigned n:3; };
#define STRING_MACRO "not numeric"
#define FUNCTION_MACRO(x) ((x)+1)
#define REMOVED 9
#undef REMOVED
#define REDEFINED 1
#undef REDEFINED
#define REDEFINED 2
