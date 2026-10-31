#pragma once
#include "void/ir.hpp"
#include <array>
#include <string_view>
namespace vd::http {
inline constexpr std::string_view source_module = "/@stdlib/net/http.void";
enum class SourceOperation : unsigned {
    open, start, send, response, read, write, finish_upload, finish,
    close_transfer, close_client, clear_cookies, download, upload, capabilities, form,
    cleanup_client, cleanup_transfer
};
struct SourceContract { std::string_view name, arguments; bool asynchronous; };
// C/T mutable client/transfer, g/r/o private flat config/request/options,
// F mutable IO file, b readonly bytes, q readonly form items, s i64.
// No user-selected FFI surface.
inline constexpr std::array source_operations = {
    SourceContract{"_http_open", "Cg", false},
    SourceContract{"_http_start", "CTro", false},
    SourceContract{"_http_send", "Cro", true},
    SourceContract{"_http_response", "T", true},
    SourceContract{"_http_read", "Ts", true},
    SourceContract{"_http_write", "Tb", true},
    SourceContract{"_http_finish_upload", "T", false},
    SourceContract{"_http_finish", "T", true},
    SourceContract{"_http_close_transfer", "T", true},
    SourceContract{"_http_close_client", "C", true},
    SourceContract{"_http_clear_cookies", "C", false},
    SourceContract{"_http_download", "CroF", true},
    SourceContract{"_http_upload", "CroFs", true},
    SourceContract{"_http_capabilities", "", false},
    SourceContract{"_http_form", "q", false},
    SourceContract{"_http_cleanup_client", "C", false},
    SourceContract{"_http_cleanup_transfer", "T", false}
};
enum class SourceRecord : unsigned { reply, config, request, options, part, header, history, form_item, count };
struct RecordContract { std::string_view name, fields; };
// n i64, t string, b bool, B bytes, H headers, D history, P parts,
// S strings, f nullable mutable io.file. These schemas validate both IR and ABI.
inline constexpr std::array source_records = {
    RecordContract{"_reply", "nnnnbnntHDnbHBnnnnnbnnnnnBttttnnn"},
    RecordContract{"_config", "tntttntnttSbnnnnnnnnt"},
    RecordContract{"_request", "ttHnBPnntt"},
    RecordContract{"_options", "nnnnnnnnnnnnn"},
    RecordContract{"_part", "tttHBfn"},
    RecordContract{"header", "tBb"},
    RecordContract{"_history", "nttb"},
    RecordContract{"_form_item", "ttb"}
};
static_assert(source_records.size() == static_cast<unsigned>(SourceRecord::count));
inline bool resource_type(ir::Type type, std::string_view name) {
    return type.kind == ir::Type::managed_ref && !type.nullable && type.definition &&
        type.definition->module_identity == source_module &&
        type.definition->identity == std::string(source_module) + "::" + std::string(name);
}
}
