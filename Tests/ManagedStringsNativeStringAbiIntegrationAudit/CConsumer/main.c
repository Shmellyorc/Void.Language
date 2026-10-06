#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct VcExportStringView
{
    const char *data;
    size_t length;
} VcExportStringView;

typedef void (*VcExportStringReleaseFn)(const char *data);

typedef struct VcExportStringResult
{
    const char *data;
    size_t length;
    VcExportStringReleaseFn release;
} VcExportStringResult;

extern int32_t void300_export_check(VcExportStringView text);
extern VcExportStringResult void300_export_echo(VcExportStringView text);
extern VcExportStringResult void300_export_empty(void);
extern VcExportStringResult void300_export_null(void);

static void check(int condition)
{
    puts(condition ? "True" : "False");
}

static void release_result(VcExportStringResult result)
{
    if (result.release != NULL)
        result.release(result.data);
}

int main(void)
{
    static const char embedded[] = {'e','x','p','o','r','t','\0','v','a','l','u','e'};
    VcExportStringView view = { embedded, sizeof(embedded) };
    check(void300_export_check(view) == 300);

    VcExportStringResult echo = void300_export_echo(view);
    check(echo.data != NULL);
    check(echo.length == sizeof(embedded));
    check(memcmp(echo.data, embedded, sizeof(embedded)) == 0);
    check(echo.data[echo.length] == '\0');
    check(echo.release != NULL);
    release_result(echo);

    VcExportStringResult empty = void300_export_empty();
    check(empty.data != NULL);
    check(empty.length == 0u);
    check(empty.data[0] == '\0');
    check(empty.release != NULL);
    release_result(empty);

    VcExportStringResult null_value = void300_export_null();
    check(null_value.data == NULL);
    check(null_value.length == 0u);
    check(null_value.release == NULL);
    return 0;
}
