#include <stddef.h>
#include <stdint.h>

typedef struct VcExportStringView
{
    const char *data;
    size_t length;
} VcExportStringView;

extern int32_t void298_check_null(VcExportStringView text);

int main(void)
{
    VcExportStringView invalid = { NULL, 3u };
    (void)void298_check_null(invalid);
    return 0;
}
