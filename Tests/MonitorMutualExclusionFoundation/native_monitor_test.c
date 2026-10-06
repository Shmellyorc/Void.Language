#include "vc_thread.h"

#include <stdio.h>

int main(void)
{
    int owner_a = 1;
    int owner_b = 2;
    VcNativeMonitor *monitor = vc_native_monitor_create();
    printf("%s\n", monitor != NULL ? "True" : "False");
    printf("%s\n", vc_native_monitor_enter(monitor, &owner_a) ? "True" : "False");
    printf("%s\n", vc_native_monitor_enter(monitor, &owner_a) ? "True" : "False");
    printf("%s\n", !vc_native_monitor_exit(monitor, &owner_b) ? "True" : "False");
    printf("%s\n", vc_native_monitor_exit(monitor, &owner_a) ? "True" : "False");
    printf("%s\n", vc_native_monitor_exit(monitor, &owner_a) ? "True" : "False");
    printf("%s\n", !vc_native_monitor_exit(monitor, &owner_a) ? "True" : "False");
    printf("%s\n", vc_native_monitor_enter(monitor, &owner_b) ? "True" : "False");
    printf("%s\n", vc_native_monitor_exit(monitor, &owner_b) ? "True" : "False");
    printf("%s\n", vc_native_monitor_destroy(monitor) ? "True" : "False");
    return 0;
}
