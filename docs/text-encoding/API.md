# Compiler-owned public API inventory

Generated from validated compiler declaration metadata, with zero public documentation
warnings. Every explicit parameter and function return is documented. Non-function
fields/types/cases have meaningful descriptions; their return column is n/a.

Full machine-readable source/signature/test mapping: [API.tsv](evidence/API.tsv).
Tests refer to module-level suites under `tests/text_encoding/`; types, fields and
error cases are exercised through their public operations. The focused ASCII suite
exhaustively checks byte classifiers; ownership concentrates on allocating operations.
Source paths are repository-relative below. The inventory contains 73 declarations.

| Declaration | Actual source | Signature | Parameters / return | Verification |
| --- | --- | --- | --- | --- |
| `encoding/ascii.void::error_kind::variant::non_ascii` | `stdlib/encoding/ascii.void:8` | `non_ascii` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/ascii.void::error_kind::variant::limit_exceeded` | `stdlib/encoding/ascii.void:11` | `limit_exceeded` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/ascii.void::error_kind` | `stdlib/encoding/ascii.void:6` | `enum error_kind:` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/ascii.void::error::field::kind` | `stdlib/encoding/ascii.void:17` | `kind: error_kind` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/ascii.void::error::field::offset` | `stdlib/encoding/ascii.void:20` | `offset: i64` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/ascii.void::error` | `stdlib/encoding/ascii.void:15` | `struct error extends core.error:` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/ascii.void::encode` | `stdlib/encoding/ascii.void:40` | `func encode(value: string) -> result<[u8], error>:` | yes / yes | oracles, focused, ownership, lsp |
| `encoding/ascii.void::decode` | `stdlib/encoding/ascii.void:56` | `func decode(const bytes: [u8]) -> result<string, error>:` | yes / yes | oracles, focused, ownership, lsp |
| `encoding/ascii.void::is_alpha` | `stdlib/encoding/ascii.void:70` | `func is_alpha(byte: u8) -> bool:` | yes / yes | oracles, focused, ownership, lsp |
| `encoding/ascii.void::is_digit` | `stdlib/encoding/ascii.void:77` | `func is_digit(byte: u8) -> bool:` | yes / yes | oracles, focused, ownership, lsp |
| `encoding/ascii.void::is_alphanumeric` | `stdlib/encoding/ascii.void:84` | `func is_alphanumeric(byte: u8) -> bool:` | yes / yes | oracles, focused, ownership, lsp |
| `encoding/ascii.void::is_upper` | `stdlib/encoding/ascii.void:91` | `func is_upper(byte: u8) -> bool:` | yes / yes | oracles, focused, ownership, lsp |
| `encoding/ascii.void::is_lower` | `stdlib/encoding/ascii.void:98` | `func is_lower(byte: u8) -> bool:` | yes / yes | oracles, focused, ownership, lsp |
| `encoding/ascii.void::is_whitespace` | `stdlib/encoding/ascii.void:105` | `func is_whitespace(byte: u8) -> bool:` | yes / yes | oracles, focused, ownership, lsp |
| `encoding/ascii.void::is_printable` | `stdlib/encoding/ascii.void:112` | `func is_printable(byte: u8) -> bool:` | yes / yes | oracles, focused, ownership, lsp |
| `encoding/ascii.void::is_control` | `stdlib/encoding/ascii.void:119` | `func is_control(byte: u8) -> bool:` | yes / yes | oracles, focused, ownership, lsp |
| `encoding/ascii.void::to_lower` | `stdlib/encoding/ascii.void:126` | `func to_lower(byte: u8) -> u8:` | yes / yes | oracles, focused, ownership, lsp |
| `encoding/ascii.void::to_upper` | `stdlib/encoding/ascii.void:136` | `func to_upper(byte: u8) -> u8:` | yes / yes | oracles, focused, ownership, lsp |
| `encoding/base64.void::alphabet::variant::standard` | `stdlib/encoding/base64.void:8` | `standard` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/base64.void::alphabet::variant::url_safe` | `stdlib/encoding/base64.void:11` | `url_safe` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/base64.void::alphabet` | `stdlib/encoding/base64.void:6` | `enum alphabet:` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/base64.void::padding::variant::required` | `stdlib/encoding/base64.void:17` | `required` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/base64.void::padding::variant::omitted` | `stdlib/encoding/base64.void:20` | `omitted` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/base64.void::padding` | `stdlib/encoding/base64.void:15` | `enum padding:` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/base64.void::error_kind::variant::invalid_length` | `stdlib/encoding/base64.void:26` | `invalid_length` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/base64.void::error_kind::variant::invalid_character` | `stdlib/encoding/base64.void:29` | `invalid_character` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/base64.void::error_kind::variant::invalid_padding` | `stdlib/encoding/base64.void:32` | `invalid_padding` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/base64.void::error_kind::variant::noncanonical_bits` | `stdlib/encoding/base64.void:35` | `noncanonical_bits` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/base64.void::error_kind::variant::limit_exceeded` | `stdlib/encoding/base64.void:38` | `limit_exceeded` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/base64.void::error_kind` | `stdlib/encoding/base64.void:24` | `enum error_kind:` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/base64.void::error::field::kind` | `stdlib/encoding/base64.void:44` | `kind: error_kind` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/base64.void::error::field::offset` | `stdlib/encoding/base64.void:47` | `offset: i64` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/base64.void::error` | `stdlib/encoding/base64.void:42` | `struct error extends core.error:` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/base64.void::encode` | `stdlib/encoding/base64.void:114` | `func encode(const bytes: [u8], selected: alphabet = alphabet.standard, pad: padding = padding.required) -> string:` | yes / yes | oracles, focused, ownership, lsp |
| `encoding/base64.void::try_encode` | `stdlib/encoding/base64.void:123` | `func try_encode(const bytes: [u8], selected: alphabet = alphabet.standard, pad: padding = padding.required) -> result<string, error>:` | yes / yes | oracles, focused, ownership, lsp |
| `encoding/base64.void::decode` | `stdlib/encoding/base64.void:176` | `func decode(value: string, selected: alphabet = alphabet.standard, pad: padding = padding.required) -> result<[u8], error>:` | yes / yes | oracles, focused, ownership, lsp |
| `encoding/hex.void::error_kind::variant::invalid_length` | `stdlib/encoding/hex.void:8` | `invalid_length` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/hex.void::error_kind::variant::invalid_digit` | `stdlib/encoding/hex.void:11` | `invalid_digit` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/hex.void::error_kind::variant::limit_exceeded` | `stdlib/encoding/hex.void:14` | `limit_exceeded` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/hex.void::error_kind` | `stdlib/encoding/hex.void:6` | `enum error_kind:` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/hex.void::error::field::kind` | `stdlib/encoding/hex.void:20` | `kind: error_kind` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/hex.void::error::field::offset` | `stdlib/encoding/hex.void:23` | `offset: i64` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/hex.void::error` | `stdlib/encoding/hex.void:18` | `struct error extends core.error:` | yes / n/a | oracles, focused, ownership, lsp |
| `encoding/hex.void::encode` | `stdlib/encoding/hex.void:60` | `func encode(const bytes: [u8], uppercase: bool = false) -> string:` | yes / yes | oracles, focused, ownership, lsp |
| `encoding/hex.void::try_encode` | `stdlib/encoding/hex.void:68` | `func try_encode(const bytes: [u8], uppercase: bool = false) -> result<string, error>:` | yes / yes | oracles, focused, ownership, lsp |
| `encoding/hex.void::decode` | `stdlib/encoding/hex.void:91` | `func decode(value: string) -> result<[u8], error>:` | yes / yes | oracles, focused, ownership, lsp |
| `text.void::error_kind::variant::invalid_range` | `stdlib/text.void:8` | `invalid_range` | yes / n/a | oracles, focused, ownership, lsp |
| `text.void::error_kind::variant::invalid_argument` | `stdlib/text.void:11` | `invalid_argument` | yes / n/a | oracles, focused, ownership, lsp |
| `text.void::error_kind::variant::limit_exceeded` | `stdlib/text.void:14` | `limit_exceeded` | yes / n/a | oracles, focused, ownership, lsp |
| `text.void::error_kind` | `stdlib/text.void:6` | `enum error_kind:` | yes / n/a | oracles, focused, ownership, lsp |
| `text.void::error::field::kind` | `stdlib/text.void:20` | `kind: error_kind` | yes / n/a | oracles, focused, ownership, lsp |
| `text.void::error` | `stdlib/text.void:18` | `struct error extends core.error:` | yes / n/a | oracles, focused, ownership, lsp |
| `text.void::trim` | `stdlib/text.void:110` | `func trim(value: string) -> string:` | yes / yes | oracles, focused, ownership, lsp |
| `text.void::trim_start` | `stdlib/text.void:117` | `func trim_start(value: string) -> string:` | yes / yes | oracles, focused, ownership, lsp |
| `text.void::trim_end` | `stdlib/text.void:124` | `func trim_end(value: string) -> string:` | yes / yes | oracles, focused, ownership, lsp |
| `text.void::is_empty` | `stdlib/text.void:131` | `func is_empty(value: string) -> bool:` | yes / yes | oracles, focused, ownership, lsp |
| `text.void::is_blank` | `stdlib/text.void:138` | `func is_blank(value: string) -> bool:` | yes / yes | oracles, focused, ownership, lsp |
| `text.void::index_of` | `stdlib/text.void:217` | `func index_of(value: string, search: string) -> i64:` | yes / yes | oracles, focused, ownership, lsp |
| `text.void::last_index_of` | `stdlib/text.void:225` | `func last_index_of(value: string, search: string) -> i64:` | yes / yes | oracles, focused, ownership, lsp |
| `text.void::contains` | `stdlib/text.void:233` | `func contains(value: string, search: string) -> bool:` | yes / yes | oracles, focused, ownership, lsp |
| `text.void::starts_with` | `stdlib/text.void:260` | `func starts_with(value: string, search: string) -> bool:` | yes / yes | oracles, focused, ownership, lsp |
| `text.void::ends_with` | `stdlib/text.void:268` | `func ends_with(value: string, search: string) -> bool:` | yes / yes | oracles, focused, ownership, lsp |
| `text.void::substring` | `stdlib/text.void:277` | `func substring(value: string, start: i64, count: i64 = -1) -> result<string, error>:` | yes / yes | oracles, focused, ownership, lsp |
| `text.void::split` | `stdlib/text.void:328` | `func split(value: string, delimiter: string, max_parts: i64 = 1048576) -> result<[string], error>:` | yes / yes | oracles, focused, ownership, lsp |
| `text.void::split_lines` | `stdlib/text.void:358` | `func split_lines(value: string, max_parts: i64 = 1048576) -> result<[string], error>:` | yes / yes | oracles, focused, ownership, lsp |
| `text.void::join` | `stdlib/text.void:406` | `func join(const values: [string], separator: string = "") -> result<string, error>:` | yes / yes | oracles, focused, ownership, lsp |
| `text.void::replace` | `stdlib/text.void:468` | `func replace(value: string, search: string, replacement: string) -> result<string, error>:` | yes / yes | oracles, focused, ownership, lsp |
| `text.void::replace_first` | `stdlib/text.void:477` | `func replace_first(value: string, search: string, replacement: string) -> result<string, error>:` | yes / yes | oracles, focused, ownership, lsp |
| `text.void::repeat` | `stdlib/text.void:485` | `func repeat(value: string, count: i64) -> result<string, error>:` | yes / yes | oracles, focused, ownership, lsp |
| `text.void::pad_left` | `stdlib/text.void:535` | `func pad_left(value: string, width: i64, padding: string = " ") -> result<string, error>:` | yes / yes | oracles, focused, ownership, lsp |
| `text.void::pad_right` | `stdlib/text.void:544` | `func pad_right(value: string, width: i64, padding: string = " ") -> result<string, error>:` | yes / yes | oracles, focused, ownership, lsp |
| `text.void::to_lower` | `stdlib/text.void:551` | `func to_lower(value: string) -> string:` | yes / yes | oracles, focused, ownership, lsp |
| `text.void::to_upper` | `stdlib/text.void:558` | `func to_upper(value: string) -> string:` | yes / yes | oracles, focused, ownership, lsp |
