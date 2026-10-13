// This translation unit intentionally defines no symbols or entry point.
// Object-only native executables still need the host toolchain's ABI/CRT link
// directives (notably /MD on MSVC). CMake compiles this direct executable input
// with the same configured runtime and usage requirements as the target.
// An unused static runtime archive cannot provide those directives: the linker
// only reads them from extracted members. The generated VOID object owns main.
