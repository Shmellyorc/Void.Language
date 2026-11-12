#pragma once
#include <array>
#include <string_view>
namespace vd::host {
// Private, statically admitted source operations; not source-visible FFI.
// t UTF-8 text, b bytes, s i64, a string list, z ZIP input list; uppercase owned resource identities.
struct Operation { std::string_view module, name, arguments; bool cleanup=false; };
inline constexpr std::string_view os="/@stdlib/os.void", crypto="/@stdlib/crypto.void",
    archive="/@stdlib/archive.void", io="/@stdlib/io.void", process="/@stdlib/process.void";
inline constexpr std::string_view address="/@stdlib/net/address.void", tcp="/@stdlib/net/tcp.void",
    udp="/@stdlib/net/udp.void";
#define VOID_NET_OPERATIONS(pair, sync, cleanup) \
    sync(net_parse, address, "t") \
    pair(net_resolve, address, "tsss") \
    pair(net_connect, tcp, "Stssssss") \
    pair(net_listen, tcp, "Ntsss") \
    pair(net_accept, tcp, "NSs") \
    pair(net_read, tcp, "Sss") \
    pair(net_write, tcp, "Sbs") \
    pair(net_shutdown, tcp, "S") \
    pair(net_close_stream, tcp, "S") \
    pair(net_close_listener, tcp, "N") \
    sync(net_stream_local, tcp, "S") \
    sync(net_stream_remote, tcp, "S") \
    sync(net_listener_local, tcp, "N") \
    pair(net_bind, udp, "Uts") \
    pair(net_send, udp, "Ubtss") \
    pair(net_receive, udp, "Uss") \
    pair(net_close_udp, udp, "U") \
    sync(net_udp_local, udp, "U") \
    cleanup(net_cleanup_stream, tcp, "S") \
    cleanup(net_cleanup_listener, tcp, "N") \
    cleanup(net_cleanup_udp, udp, "U")
inline constexpr std::array operations = {
    Operation{os,"_host_args_count",""}, Operation{os,"_host_arg","s"},
    Operation{crypto,"_host_hash_open","H"}, Operation{crypto,"_host_hash_update","Hb"},
    Operation{crypto,"_host_hash_final","H"}, Operation{crypto,"_host_hash_close","H"},
    Operation{crypto,"_host_hash_cleanup","H",true}, Operation{crypto,"_host_verify","bbb"},
    Operation{archive,"_host_zip_open","RFsss"},
    Operation{archive,"_host_zip_next","R"}, Operation{archive,"_host_zip_read","Rs"},
    Operation{archive,"_host_zip_close","R"}, Operation{archive,"_host_zip_cleanup","R",true},
    Operation{io,"_host_dir_create","Dt"}, Operation{io,"_host_dir_admit","Dt"},
    Operation{io,"_host_dir_child","DDt"}, Operation{io,"_host_dir_open","DDt"},
    Operation{io,"_host_dir_file","DFts"}, Operation{io,"_host_dir_lock","DLt"},
    Operation{io,"_host_dir_sync","D"}, Operation{io,"_host_dir_publish","DDt"},
    Operation{io,"_host_dir_replace","DFtDts"}, Operation{io,"_host_file_mode","Fs"},
    Operation{io,"_host_dir_close","D"}, Operation{io,"_host_lock_close","L"},
    Operation{io,"_host_dir_cleanup","D",true}, Operation{io,"_host_lock_cleanup","L",true},
    Operation{process,"_host_spawn","Ptaat"}, Operation{process,"_host_wait","P"},
    Operation{process,"_host_child_close","P"}, Operation{process,"_host_child_cleanup","P",true},
    Operation{io,"_host_file_mode_matches","Fs"},
    Operation{process,"_host_capture","taatssss"}, Operation{process,"_host_capture_async","taatssss"},
#define VOID_NET_SYNC(code, module, args) Operation{module,"_" #code,args},
#define VOID_NET_PAIR(code, module, args) VOID_NET_SYNC(code,module,args) Operation{module,"_" #code "_async",args},
#define VOID_NET_CLEANUP(code, module, args) Operation{module,"_" #code,args,true},
    VOID_NET_OPERATIONS(VOID_NET_PAIR, VOID_NET_SYNC, VOID_NET_CLEANUP)
#undef VOID_NET_CLEANUP
#undef VOID_NET_PAIR
#undef VOID_NET_SYNC
    Operation{io,"_host_dir_save","Dtbs"},
    Operation{archive,"_host_zip_encode","zssss"},
};
enum Code : unsigned {args_count,arg,hash_open,hash_update,hash_final,hash_close,hash_cleanup,verify,
 zip_open,zip_next,zip_read,zip_close,zip_cleanup,dir_create,dir_admit,dir_child,dir_open,dir_file,
 dir_lock,dir_sync,dir_publish,dir_replace,file_mode,dir_close,lock_close,dir_cleanup,lock_cleanup,
 spawn,wait,child_close,child_cleanup,file_mode_matches,capture,capture_async,
#define VOID_NET_SYNC(code, module, args) code,
#define VOID_NET_PAIR(code, module, args) code, code##_async,
 VOID_NET_OPERATIONS(VOID_NET_PAIR, VOID_NET_SYNC, VOID_NET_SYNC)
#undef VOID_NET_PAIR
#undef VOID_NET_SYNC
 dir_save,zip_encode,
};
constexpr bool capture_operation(unsigned op) {return op==capture || op==capture_async;}
constexpr bool network_operation(unsigned op) {return op>=net_parse && op<dir_save;}
constexpr unsigned network_sync(unsigned op) {
    switch(op) {
#define VOID_NET_SKIP(code, module, args)
#define VOID_NET_PAIR(code, module, args) case code##_async: return code;
    VOID_NET_OPERATIONS(VOID_NET_PAIR, VOID_NET_SKIP, VOID_NET_SKIP)
#undef VOID_NET_PAIR
#undef VOID_NET_SKIP
    default: return op;
    }
}
#undef VOID_NET_OPERATIONS
constexpr bool asynchronous(unsigned op) {return op==capture_async || network_sync(op)!=op;}
constexpr std::string_view resource_name(char c) {
    switch(c) {case 'H':return "sha256_state";case 'R':return "zip_reader";case 'D':return "private_directory";
        case 'L':return "directory_lock";case 'F':return "file";case 'P':return "child";
        case 'S':return "stream";case 'N':return "listener";case 'U':return "socket";default:return {};}
}
constexpr std::string_view resource_module(char c) {
    switch(c) {case 'H':return crypto;case 'R':return archive;case 'P':return process;
        case 'S':case 'N':return tcp;case 'U':return udp;default:return io;}
}
}
