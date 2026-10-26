#pragma once

#include "void/filesystem.hpp"
#include "void/ir.hpp"

#include <array>
#include <string_view>

namespace vd::filesystem {
inline constexpr std::string_view source_module = "/@stdlib/io.void";
inline constexpr unsigned claim_operation = static_cast<unsigned>(Operation::create_symlink) + 1;
inline constexpr unsigned release_operation = claim_operation + 1;
inline constexpr unsigned cleanup_operation = release_operation + 1;

struct SourceOperation {
    std::string_view name;
    std::string_view arguments; // t text, b bytes, s i64, f readonly file, F mutable file
};

inline constexpr std::array source_operations = {
    SourceOperation{"_io_path_join", "tt"},
    SourceOperation{"_io_path_normalize", "t"},
    SourceOperation{"_io_path_filename", "t"},
    SourceOperation{"_io_path_stem", "t"},
    SourceOperation{"_io_path_extension", "t"},
    SourceOperation{"_io_path_change_extension", "tt"},
    SourceOperation{"_io_path_parent", "t"},
    SourceOperation{"_io_path_root", "t"},
    SourceOperation{"_io_path_is_absolute", "t"},
    SourceOperation{"_io_path_is_relative", "t"},
    SourceOperation{"_io_path_is_rooted", "t"},
    SourceOperation{"_io_path_absolute", "tt"},
    SourceOperation{"_io_path_relative", "tt"},
    SourceOperation{"_io_path_separator", ""},
    SourceOperation{"_io_open", "Fts"},
    SourceOperation{"_io_read_bytes", "Fs"},
    SourceOperation{"_io_read_exact", "Fs"},
    SourceOperation{"_io_read_line", "Fs"},
    SourceOperation{"_io_write_bytes", "Fb"},
    SourceOperation{"_io_seek", "Fss"},
    SourceOperation{"_io_position", "f"},
    SourceOperation{"_io_length", "f"},
    SourceOperation{"_io_set_length", "Fs"},
    SourceOperation{"_io_flush", "F"},
    SourceOperation{"_io_close", "F"},
    SourceOperation{"_io_read_all", "ts"},
    SourceOperation{"_io_write_all", "tbs"},
    SourceOperation{"_io_exists", "t"},
    SourceOperation{"_io_metadata", "ts"},
    SourceOperation{"_io_copy_file", "tts"},
    SourceOperation{"_io_move_file", "tts"},
    SourceOperation{"_io_remove_file", "ts"},
    SourceOperation{"_io_create_directory", "t"},
    SourceOperation{"_io_create_directories", "t"},
    SourceOperation{"_io_remove_directory", "ts"},
    SourceOperation{"_io_remove_tree", "tss"},
    SourceOperation{"_io_entries", "ttss"},
    SourceOperation{"_io_get_files", "ttss"},
    SourceOperation{"_io_get_directories", "ttss"},
    SourceOperation{"_io_copy_directory", "ttsss"},
    SourceOperation{"_io_move_directory", "tts"},
    SourceOperation{"_io_read_link", "t"},
    SourceOperation{"_io_create_symlink", "tts"},
    SourceOperation{"_io_claim", "f"},
    SourceOperation{"_io_release", "f"},
    SourceOperation{"_io_cleanup", "F"}
};
static_assert(source_operations.size() == cleanup_operation + 1);

inline bool file_type(ir::Type type) {
    return type.kind == ir::Type::managed_ref && !type.nullable &&
           ir::Type::key(type) == "/@stdlib/io.void::file";
}

inline bool mutable_file(unsigned operation) {
    return source_operations.at(operation).arguments.find('F') != std::string_view::npos;
}
}
