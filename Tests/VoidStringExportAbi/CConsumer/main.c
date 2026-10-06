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

extern int32_t void298_check_embedded(VcExportStringView text);
extern int32_t void298_check_null(VcExportStringView text);
extern int32_t void298_check_empty(VcExportStringView text);
extern int32_t void298_check_pair(VcExportStringView left, int32_t marker, VcExportStringView right);
extern VcExportStringResult void298_echo(VcExportStringView text);
extern VcExportStringResult void298_make_embedded(void);
extern VcExportStringResult void298_make_utf8(void);
extern VcExportStringResult void298_make_empty(void);
extern VcExportStringResult void298_make_null(void);

static void check(int condition)
{
    puts(condition ? "True" : "False");
}

static void release_result(VcExportStringResult *result)
{
    if (result->release != NULL)
        result->release(result->data);
    result->data = NULL;
    result->length = 0u;
    result->release = NULL;
}

int main(void)
{
    static const char embedded[] = { 'A', '\0', 'B' };
    static const char empty[] = "";
    static const char utf8[] = { (char)0xc3, (char)0xa9 };

    VcExportStringView embedded_view = { embedded, sizeof(embedded) };
    VcExportStringView null_view = { NULL, 0u };
    VcExportStringView empty_view = { empty, 0u };
    VcExportStringView utf8_view = { utf8, sizeof(utf8) };

    check(void298_check_embedded(embedded_view) == 1);
    check(void298_check_null(null_view) == 1);
    check(void298_check_empty(empty_view) == 1);
    static const char pair_left[] = { 'l', 'e', 'f', 't', '\0', 'v', 'a', 'l', 'u', 'e' };
    static const char pair_right[] = "right";
    VcExportStringView pair_left_view = { pair_left, sizeof(pair_left) };
    VcExportStringView pair_right_view = { pair_right, sizeof(pair_right) - 1u };
    check(void298_check_pair(pair_left_view, 298, pair_right_view) == 1);

    VcExportStringResult echo = void298_echo(embedded_view);
    check(echo.data != NULL);
    check(echo.length == 3u);
    check(memcmp(echo.data, embedded, 3u) == 0);
    check(echo.data[3] == '\0');
    check(echo.release != NULL);
    release_result(&echo);

    VcExportStringResult echoed_utf8 = void298_echo(utf8_view);
    check(echoed_utf8.data != NULL);
    check(echoed_utf8.length == 2u);
    check(memcmp(echoed_utf8.data, utf8, 2u) == 0);
    check(echoed_utf8.data[2] == '\0');
    check(echoed_utf8.release != NULL);
    release_result(&echoed_utf8);

    VcExportStringResult embedded_result = void298_make_embedded();
    static const char returned_embedded[] = { 'R', '\0', 'S' };
    check(embedded_result.data != NULL);
    check(embedded_result.length == 3u);
    check(memcmp(embedded_result.data, returned_embedded, 3u) == 0);
    check(embedded_result.release != NULL);
    release_result(&embedded_result);

    VcExportStringResult utf8_result = void298_make_utf8();
    check(utf8_result.length == 2u);
    check(memcmp(utf8_result.data, utf8, 2u) == 0);
    check(utf8_result.release != NULL);
    release_result(&utf8_result);

    VcExportStringResult empty_result = void298_make_empty();
    check(empty_result.data != NULL);
    check(empty_result.length == 0u);
    check(empty_result.data[0] == '\0');
    check(empty_result.release != NULL);
    release_result(&empty_result);

    VcExportStringResult null_result = void298_make_null();
    check(null_result.data == NULL);
    check(null_result.length == 0u);
    check(null_result.release == NULL);

    return 0;
}
