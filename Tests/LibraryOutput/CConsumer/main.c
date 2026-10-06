#include <stdint.h>
#include <stdio.h>

extern int32_t void099_add(int32_t left, int32_t right);
extern void void099_adjust(int32_t *value, int32_t amount);
extern void void099_write(int32_t *value);
extern int32_t void099_read(const int32_t *value);
extern int32_t void099_increment(void);
extern int32_t void099_init_count(void);
extern int32_t void099_gc_value(void);
extern int32_t void099_replace_gc_value(int32_t value);

static void check(int condition)
{
    puts(condition ? "True" : "False");
}

int main(void)
{
    check(void099_add(10, 32) == 42);
    int32_t value = 3;
    void099_adjust(&value, 9);
    check(value == 12);
    check(void099_read(&value) == 12);
    void099_write(&value);
    check(value == 99);
    check(void099_init_count() == 1);
    check(void099_increment() == 1);
    check(void099_increment() == 2);
    check(void099_gc_value() == 42);
    check(void099_replace_gc_value(55) == 55);
    check(void099_gc_value() == 57);
    return 0;
}
