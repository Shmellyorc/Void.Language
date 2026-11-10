#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif
#ifndef FIXTURE_VALUE
#define FIXTURE_VALUE 7
#endif
EXPORT int g4_dependency(void) { return FIXTURE_VALUE; }
