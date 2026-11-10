#pragma once
#include <stddef.h>

typedef int (*Bias)(void);
typedef struct { Bias bias; } Table;
typedef struct { int tag; double value; } Sample;
typedef struct Token Token;
