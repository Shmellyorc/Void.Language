# Public surface and completeness audit

The compiler inventory contains **149 public declarations** across `io` and
`path`, including enum cases, metadata/error fields, properties and lifecycle
methods. [API.tsv](evidence/API.tsv) records each identity, actual source location,
written declaration and parameter/return documentation status.
[API-AUDIT.tsv](evidence/API-AUDIT.tsv) maps every row to registered tests and its
ownership contract. These inventories come from compiler-owned source snapshots;
they are not a second documentation implementation.

The [contract](CONTRACT.md) defines defaults, partial progress, cancellation,
platform behavior and resource lifetime. All public declarations carry `##`
documentation; the ownership test checks descriptions, every explicit parameter,
meaningful returns and the managed error base. The persistent LSP test separately
checks hover, definition, completion and parameter help against those declarations.

## Capability coverage

| Capability | Public surface | Principal tests |
| --- | --- | --- |
| Managed recoverable errors | `error`, `error_kind`, inherited `message/path/code`, native/cleanup codes and owned partial bytes | `io_ownership`, `io_errors_source`, `io_errors_async`, `io_native_close_failure`, `io_cross_volume`, `io_kernel` |
| Metadata and entry classification | `entry`, `entry_kind`, `metadata`, `exists` | `io_execution`, `io_async_execution`, `io_kernel` |
| Seven file modes | `open`, `open_async`, `open_mode` | `io_kernel`, `io_ownership`, `io_async_source`, `io_execution` |
| Byte reads and writes | file `read_bytes`, `read_exact`, `write_bytes`, paired `_async` methods | `io_execution`, `io_errors_source`, `io_errors_async`, `io_native_responsiveness` |
| Logical text lines | file `read_line`, `read_line_async`, `text_read` | `io_errors_source`, `io_errors_async`, `io_kernel` (CRLF across 64 KiB boundary) |
| Cursor, length and flush | file `seek`, `position`, `length`, `set_length`, `flush`, paired `_async` methods; `seek_origin` | `io_kernel`, `io_async_source`, `io_errors_source`, `io_errors_async` |
| Close and ownership | file `close`, `close_async`, `deinit`, private construction and shared aliases | `io_ownership`, `io_native_pending_close`, `io_native_cancelled`, `io_native_shutdown`, `io_native_close_failure` |
| Numeric wire representations | file `read<t>`, `write<t>`, `order`, paired `_async` methods | `io_execution`, `io_async_execution`, `io_wire_normal`, `io_wire_optimized`: 42 independent integer/IEEE vectors, all ten types and both byte orders |
| Strict raw and Pascal strings | file `read_string`, `write_string`, `read_pascal_string`, `write_pascal_string`, paired `_async` methods | wire tests, error fixtures, allocation sweeps; malformed UTF-8, NUL, Unicode, truncated frames and hostile prefixes |
| Whole-file operations | `read_bytes`, `read_text`, `write_bytes`, `write_text`, `append_bytes`, `append_text`, paired `_async` functions | source execution tests, `io_kernel` (real short-read FIFO, bounded reads, 3 MiB streaming) |
| File copy/move/delete | `copy_file`, `move_file`, `remove_file`, paired `_async` functions | source execution tests, `io_kernel` (exclusive collision, same inode/hard link and destination symlink), `io_cross_volume` |
| Directory creation/removal | `create_directory`, `create_directories`, `remove_directory`, `remove_tree`, paired `_async` functions | source execution tests, `io_kernel` (budget-before-mutation and symlink targets retained) |
| Directory lookup/enumeration | `entries`, `get_files`, `get_directories`, paired `_async` functions | source execution tests, `io_kernel`: filename glob, Unicode scalar `?`, recursion through unmatched directories, entry budgets, permission errors |
| Directory copy/move | `copy_directory`, `move_directory`, paired `_async` functions | source execution tests, `io_kernel`: descendant rejection, nonrecursive child rejection, merge and preserved symlinks; cross-device rename fixture |
| Symbolic links | `create_symlink`, `read_link`, paired `_async` functions | source execution tests and `io_kernel`; native Windows execution requires symlink privileges |
| Path composition/extraction | `join`, `normalize`, `filename`, `stem`, `extension`, `change_extension`, `parent`, `root`, `separator` | source execution tests and `io_kernel` |
| Path qualification | `is_absolute`, `is_relative`, `is_rooted`, `absolute`, `relative` | source execution tests and `io_kernel`; compiled Windows branches cover drives, UNC share roots and incompatible roots |

## Runtime and compiler boundaries

`io_native_jobs` covers bounded native admission, queued and pending cancellation,
owner publication failure, acquired-resource rollback and cleanup diagnostics.
`io_native_responsiveness` holds an actual native file read while another ready
source activation runs on the owner, using a single bounded worker. The linked
`io_probe_normal` and `io_probe_optimized` tests repeat that proof through the
explicit AOT adapter. `io_native_cancelled` cancels that actual entered read and
checks cleanup, roots and one native close after drain.

`io_native_opening_shutdown` and `io_native_whole_shutdown` hold actual native
opening while another host thread closes the Runtime. They require shutdown to
wait, then verify one close and a checked outcome after the gate is released.
These cover the admission interval before an acquired handle is published in the
native-resource ledger, including operations that never publish a file wrapper.

`io_ownership` forces collection and sweeps allocation failures until successful
recovery for synchronous and asynchronous opening, Pascal adaptation and cleanup,
in JIT and O2. `io_ir` independently rejects forged native operation authority,
arity, reply shapes and opcodes. Source negative cases retain readonly, numeric
generic and ordinary worker restrictions. Existing async/global/native-provider
regressions remain part of consolidated acceptance.

`io_examples_install` executes the practical example in JIT, O2 and linked AOT,
then stages and relocates the compiler component and executes against installed
`io`, `path`, memory and binary sources. `io_stack` runs the independent source
wire and filesystem program in actual compiler/JIT/AOT processes limited to a
1 MiB native stack. Strict sanitizer configurations retain their normal larger
stack; no sanitizer checks are disabled to manufacture small-stack evidence.

## Native platform registration

The tests are registered on Windows, macOS and Linux, with native branches for
host path and handle behavior. POSIX tests additionally use permissions, hard
links, invalid UTF-8 native filenames and a real FIFO. The cross-volume test
requires two distinct native filesystems; set `VOID_IO_SECOND_VOLUME` when the
default temporary directory shares the build volume. Its explicit skipped result
is **unverified**, not a successful cross-volume test. Native Windows symlink
tests require Developer Mode or suitable privileges.

Linux execution does not certify Windows Unicode, UNC, long-path, handle,
symlink or macOS rename behavior. Those remain native acceptance work for Boss's
later platform matrix. No hosted CI is run in this sprint.
