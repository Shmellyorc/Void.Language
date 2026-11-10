#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#define IMPORT __declspec(dllimport)
#else
#define EXPORT __attribute__((visibility("default")))
#define IMPORT
#endif
IMPORT int g4_dependency(void);
EXPORT int g4_provider(void) { return g4_dependency() + 35; }
EXPORT int (*g4_dependency_address(void))(void) { return g4_dependency; }
