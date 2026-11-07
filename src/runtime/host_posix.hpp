#pragma once
#include <cerrno>
#ifdef __APPLE__
#include <sys/acl.h>
#endif
namespace vd::host {
inline bool safe_acl(int fd) {
#ifdef __APPLE__
    // Darwin extended allow ACEs need not be reflected in st_mode. Reject them
    // conservatively, including inherited grants; never repair an admitted ACL.
    acl_t acl=::acl_get_fd_np(fd,ACL_TYPE_EXTENDED);
    if(!acl)return errno==ENOENT;
    acl_entry_t entry{};bool safe=true;
    for(int selected=ACL_FIRST_ENTRY;;selected=ACL_NEXT_ENTRY) {
        // Darwin returns zero for an entry and EINVAL at the end, unlike Linux.
        if(::acl_get_entry(acl,selected,&entry)) {safe=errno==EINVAL;break;}
        acl_tag_t tag{};
        if(::acl_get_tag_type(entry,&tag) || tag!=ACL_EXTENDED_DENY){safe=false;break;}
    }
    ::acl_free(acl);return safe;
#else
    (void)fd;return true;
#endif
}
}
