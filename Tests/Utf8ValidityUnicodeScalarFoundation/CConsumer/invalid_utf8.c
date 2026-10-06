#include <stddef.h>
#include <stdint.h>

typedef struct VcExportStringView
{
    const char *data;
    size_t length;
} VcExportStringView;

extern int32_t void301_accept_export(VcExportStringView text);

int main(void)
{
    static const unsigned char invalid[] = {0xf4u, 0x90u, 0x80u, 0x80u};
    VcExportStringView view = { (const char *)invalid, sizeof(invalid) };
    (void)void301_accept_export(view);
    return 0;
}
