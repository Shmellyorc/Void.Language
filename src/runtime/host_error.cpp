#include "host_internal.hpp"
#include <cerrno>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace vd::host {
Result error(std::int64_t kind,std::int64_t code) {Result r;r.kind=kind;r.code=code;return r;}
Result native_error(int code) {
#ifdef _WIN32
    std::int64_t kind=17;
    switch(code) {
    case ERROR_FILE_NOT_FOUND:case ERROR_PATH_NOT_FOUND:kind=1;break;
    case ERROR_ACCESS_DENIED:case ERROR_PRIVILEGE_NOT_HELD:kind=2;break;
    case ERROR_FILE_EXISTS:case ERROR_ALREADY_EXISTS:kind=3;break;
    case ERROR_INVALID_NAME:case ERROR_BAD_PATHNAME:case ERROR_INVALID_PARAMETER:case ERROR_FILENAME_EXCED_RANGE:kind=4;break;
    case ERROR_SHARING_VIOLATION:case ERROR_LOCK_VIOLATION:kind=8;break;
    case ERROR_NOT_SUPPORTED:case ERROR_INVALID_FUNCTION:case ERROR_CALL_NOT_IMPLEMENTED:kind=11;break;
    case ERROR_NOT_SAME_DEVICE:kind=13;break;
    case ERROR_DIRECTORY:kind=14;break;
    case ERROR_DIR_NOT_EMPTY:kind=16;break;
    }
#else
    auto kind=code==ENOENT?1:code==EACCES || code==EPERM?2:code==EEXIST?3:code==ELOOP || code==EINVAL?4:
        code==EBUSY || code==EWOULDBLOCK?8:code==ENOSYS || code==ENOTSUP?11:code==EXDEV?13:code==ENOTDIR?14:code==EISDIR?15:code==ENOTEMPTY?16:17;
#endif
    return error(kind,code);
}
}
