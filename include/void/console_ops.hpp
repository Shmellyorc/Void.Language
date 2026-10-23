#pragma once
#include "void/ir.hpp"
#include <array>
#include <string_view>
namespace vd::console {
inline constexpr std::string_view source_module="/@stdlib/console.void";
inline bool endpoint_type(ir::Type t){return t.definition && (ir::Type::key(t)=="/@stdlib/console.void::input" || ir::Type::key(t)=="/@stdlib/console.void::output");}
inline bool session_type(ir::Type t){return t.definition && ir::Type::key(t)=="/@stdlib/console.void::terminal_session";}
struct SourceOperation {std::string_view name,arguments;}; // e endpoint, t text, b bytes, s i64
inline constexpr std::array source_operations={
    SourceOperation{"_console_write","et"},SourceOperation{"_console_write_bytes","eb"},
    SourceOperation{"_console_read_char","es"},SourceOperation{"_console_read_text","ess"},
    SourceOperation{"_console_read_line","es"},SourceOperation{"_console_read_bytes","ess"},
    SourceOperation{"_console_redirected","e"},SourceOperation{"_console_flush","e"},
    SourceOperation{"_console_capabilities","e"},SourceOperation{"_console_size","e"},
    SourceOperation{"_console_position","e"},SourceOperation{"_console_cursor","ess"},
    SourceOperation{"_console_visible","es"},SourceOperation{"_console_color","ess"},
    SourceOperation{"_console_reset","e"},SourceOperation{"_console_clear","e"},
    SourceOperation{"_console_title","et"},SourceOperation{"_console_bell","e"},
    SourceOperation{"_console_key","ess"},SourceOperation{"_console_available","e"},
    SourceOperation{"_console_mode","es"},SourceOperation{"_console_end_mode","r"},SourceOperation{"_console_restore","r"}
};
inline bool output_operation(unsigned n){return n<=1 || n==7 || (n>=9 && n<=17);}
inline bool input_operation(unsigned n){return (n>=2 && n<=5) || (n>=18 && n<=20);}
}
