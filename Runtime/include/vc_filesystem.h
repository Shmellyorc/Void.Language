#ifndef VC_FILESYSTEM_H
#define VC_FILESYSTEM_H
#include <stdbool.h>
#include <stdint.h>
/* VOID statuses: 0 success; 1 missing file; 2 missing path; 3 denied;
   4 exists; 5 invalid; 6 allocation failure; 7 unsupported; 8 IO;
   9 closed. Host error numbers never cross this boundary. */
bool vc_fs_windows(void);
int32_t vc_fs_open(const char *, int32_t, int32_t, void **);
int32_t vc_fs_close(void *);
void vc_fs_release(void *);
int32_t vc_fs_read(void *, uint8_t *, int32_t);
int32_t vc_fs_write(void *, const uint8_t *, int32_t);
int64_t vc_fs_seek(void *, int64_t, int32_t);
int64_t vc_fs_length(void *);
int32_t vc_fs_set_length(void *, int64_t);
int32_t vc_fs_flush(void *);
bool vc_fs_can_seek(void *);
int32_t vc_fs_resolve_absolute(const char *, uint8_t *, int32_t);
int32_t vc_fs_kind(const char *);
int32_t vc_fs_mkdir(const char *);
int32_t vc_fs_delete(const char *, bool);
#endif
