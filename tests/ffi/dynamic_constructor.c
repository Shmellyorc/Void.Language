// If the loader ever maps this unapproved dependency, it exits before the
// host can publish bindings or print a diagnostic. No CRT import is needed.
__attribute__((constructor)) static void prohibited_initializer(void) {
    __asm__ volatile("syscall" : : "a"(231L), "D"(121L) : "rcx", "r11", "memory");
}
