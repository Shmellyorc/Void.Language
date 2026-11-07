#pragma once
#include <array>
#include <string_view>
namespace vd::host {
// Private, statically admitted source operations; not source-visible FFI.
// t UTF-8 text, b bytes, s i64, a string list; uppercase owned resource identities.
struct Operation { std::string_view module, name, arguments; bool cleanup=false; };
inline constexpr std::string_view os="/@stdlib/os.void", crypto="/@stdlib/crypto.void",
    archive="/@stdlib/archive.void", io="/@stdlib/io.void", process="/@stdlib/process.void";
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
    Operation{io,"_host_file_mode_matches","Fs"}
};
enum Code : unsigned {args_count,arg,hash_open,hash_update,hash_final,hash_close,hash_cleanup,verify,
 zip_open,zip_next,zip_read,zip_close,zip_cleanup,dir_create,dir_admit,dir_child,dir_open,dir_file,
 dir_lock,dir_sync,dir_publish,dir_replace,file_mode,dir_close,lock_close,dir_cleanup,lock_cleanup,
 spawn,wait,child_close,child_cleanup,file_mode_matches};
constexpr std::string_view resource_name(char c) {
    switch(c) {case 'H':return "sha256_state";case 'R':return "zip_reader";case 'D':return "private_directory";
        case 'L':return "directory_lock";case 'F':return "file";case 'P':return "child";default:return {};}
}
constexpr std::string_view resource_module(char c) {
    switch(c) {case 'H':return crypto;case 'R':return archive;case 'P':return process;default:return io;}
}
}
