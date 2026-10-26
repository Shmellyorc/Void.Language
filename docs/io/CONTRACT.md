# File I/O and host paths — intended public contract

26 October 2026. Boss authorizes implementation from working-tree baseline
`084fe1ebf26e3f6376ed3e569e1b6c1a74499b17`. This document records the complete
intended contract before implementation. The completed [API audit](API.md),
[handoff](HANDOFF.md) and [local receipt](evidence/RECEIPT.md) now map these
capabilities to actual source, documentation and executed tests. This is not a
final acceptance lock.

No os module, application-data discovery, hosted CI, Git mutations, root README
changes, general FFI, reflection, replacement scheduler or codec is authorized.
Runtime paths are host filesystem paths, independent of source VFS/import rules.

## Paths

`use path` provides the following functions. Fallible path functions return
`result<T, io.error>`; path imports the common I/O error rather than inventing a
second filesystem error taxonomy.

| Function | Result payload | Meaning |
| --- | --- | --- |
| join(parts:[string]) | string | Join all components using host root/drive rules; rooted components replace an earlier prefix as on the host |
| normalize(value:string) | string | Lexically remove redundant separators/dots and cancellable parent components |
| filename(value:string) | string | Last component, empty after a trailing separator |
| stem(value:string) | string | Filename without its final extension; a leading-dot filename alone has no extension |
| extension(value:string) | string | Final extension including its dot, empty when absent |
| change_extension(value:string, new_extension:string) | string | Replace/remove the final extension, adding a dot when necessary |
| parent(value:string) | string | Lexical parent, with roots retained |
| root(value:string) | string | Host root name and root directory, if present |
| is_absolute(value:string) | bool | Fully qualified host path |
| is_relative(value:string) | bool | Complement of is_absolute |
| is_rooted(value:string) | bool | Has a root name or root directory; Windows drive-relative paths can be rooted but not absolute |
| absolute(value:string, base:string="") | string | Normalize against an absolute explicit base, or one current-working-directory snapshot |
| relative(value:string, base:string="") | string | Lexical relative path between absolute paths resolved against the same working-directory snapshot; incompatible roots fail |
| separator() | string | Preferred native separator |

Lexical operations neither check existence nor resolve symbolic links. Empty
components, dotfiles, repeated/trailing separators, roots, Windows drive-relative
and UNC roots have explicit tests. UTF-8 is checked using existing facilities;
embedded NUL is a recoverable invalid-path error. Ordinary io paths cannot be
empty. Path absolute's empty value denotes the chosen base. Relative targets for relative(value,base) resolve against the chosen base. No function changes
process cwd. No os/app-data API is introduced.

## Errors, metadata and policies

`io.error` is managed and extends error.error without duplicating message/path/code.
Additional fields: operation:string, kind:error_kind, os_code:i64,
bytes_transferred:i64, partial:[u8], cleanup_code:i64, target:string,
entries_processed:i64. Partial bytes are owned. Code is the portable category;
os_code retains the native number. Error kinds cover not_found, permission_denied,
already_exists, invalid_path, invalid_mode, invalid_range, closed, busy,
unexpected_eof, invalid_utf8, unsupported, limit_exceeded, cross_device,
not_directory, is_directory, not_empty, and io_failure.

Metadata records include path, name, kind (file/directory/symlink/other), size and
Unix modification seconds/nanoseconds. Metadata defaults to inspecting the link
itself; explicit follow_links inspects its target. Existence is false only for
missing paths; permission and genuine native failures remain Err.

Native failures, ordinary Err and structured task cancellation remain distinct.
Writes/copies/recursive mutations are not transactions: failures preserve actual
byte/entry progress and cleanup errors, and can leave partial destination changes.
Allocation failures remain checked Runtime failures; they do not manufacture Err
or imply rollback of filesystem effects.

## File modes and ownership

`open(value, mode=open_mode.read, order=binary.byte_order.little)` and open_async
return a managed file. Modes are read, write (create/truncate), append
(create/no truncate), read_write (existing), create (read/write create/truncate),
open_or_create (read/write, no truncate), and create_new (exclusive read/write).
Missing parents are never created. Append writes always select EOF even after a
seek; reads and the shared logical cursor otherwise follow the selected mode.

File construction is private; use open/open_async to establish acquisition cleanup.
One resource and cursor are shared by aliases. Native handles are noninheritable
or close-on-exec and close exactly once. Successful close invalidates all aliases.
Double close is an idempotent successful cleanup. Explicit with/deinit and Runtime
shutdown use the existing native resource ledger. GC does not run source destructors;
forgotten open files remain Runtime-owned until close/shutdown, as existing native
resources do. Acquisition/publication failures must not strand an opened handle.

Every file operation has a paired _async method when it can block. Public file
surface: read_bytes(max_count), read_exact(count), read_line(max_bytes=67108864),
write_bytes(bytes), write_string(value), seek(offset,origin=start), position(),
length(), set_length(length), flush(), close(), read<t>(), write<t>(value),
read_string(count), read_pascal_string(max_bytes=67108864),
write_pascal_string(value). order is an infallible readonly property. read_line
returns text_read.text(value) or text_read.eof, preserving an empty entered line.

Reads up to count can be short; exact reads loop and report unexpected EOF with
all consumed bytes and actual progress. Errors can advance file position, unlike
memory's rollback contract. Lines remove LF/CRLF/CR, including final unterminated
lines; raw and line text use the existing strict UTF-8 decoder. Raw writes add no
prefix/newline/terminator. Pascal framing is exactly binary u32 UTF-8 byte count
plus payload in selected endian order. All ten numeric primitives reuse binary.

Seek origins start/current/end are checked against signed representability;
files may seek beyond EOF, and later writes can create native sparse gaps. Truncate
can extend with zero bytes and does not implicitly reposition the cursor. Logical
position accounts for read-ahead. Flush asks the host to flush regular-file data;
it is not an unconditional power-loss or hardware durability promise.

Public operations reserve one resource operation scope; overlapping unrelated
operations return recoverable busy instead of blocking the owner on a file lock.
Canonical nested wire helpers retain that reservation across awaits, so a Pascal
frame cannot be interleaved by another alias. Await an operation before closing;
a racing close returns busy without freeing a handle under native work. Structured
cancellation drains an entered syscall before releasing its reservation and running
resource cleanup. Read/write methods require mutable cursor authority; metadata
queries retain ordinary readonly authority.

## Whole-file and filesystem functions

All blocking functions below have paired _async functions with identical payloads
and error/progress policies. Byte/text whole-file reads default to a 64 MiB budget;
callers can select another nonnegative max_bytes. Reads continue to actual EOF,
not the first short read. Returned arrays/text are owned.

* read_bytes(value,max_bytes), read_text(value,max_bytes).
* write_bytes(value,bytes,mode=write), write_text(value,text,mode=write).
* append_bytes(value,bytes), append_text(value,text).
* exists(value), metadata(value,follow_links=false).
* copy_file(source,target,replace=false), move_file(source,target,replace=false),
  remove_file(value,missing_ok=false).
* create_directory(value), create_directories(value),
  remove_directory(value,missing_ok=false), remove_tree(value,missing_ok=false,max_entries=100000).
* entries(value,pattern="*",recursive=false,max_entries=100000),
  get_files(value,pattern="*",recursive=false,max_entries=100000),
  get_directories(value,pattern="*",recursive=false,max_entries=100000).
* copy_directory(source,target,recursive=true,replace=false,max_entries=100000),
  move_directory(source,target,replace=false).
* read_link(value), create_symlink(target,value,directory=false).

Enumeration returns bounded ordinary arrays, not a new collection/iterator system.
Entries contain metadata; get_files/get_directories contain matching path strings.
Patterns match the filename only: * matches any sequence, ? one Unicode scalar;
other characters are literal and separators are invalid. Matching is host-native
case sensitivity (Windows ordinal-insensitive, POSIX sensitive). Recursive searches
visit unmatched real directories too. Entries observed as symlinks/reparse
points are not traversed; visited directory identities prevent traversal cycles. Directory symlinks
appear only in entries. No permission failure is silently skipped.

Copy-directory is iterative, preserves symlinks without following their targets,
and rejects copying into itself/its descendants. Without replace the destination
must be new; replace merges into a real directory and replaces ordinary files,
without deleting unrelated entries or following colliding destination symlinks.
Copying a source link uses exclusive link creation; any existing destination at
that link's name fails, including when replace is enabled for ordinary files.
Nonrecursive copy copies direct files/links and rejects child directories rather
than silently omitting them. Size/entry bounds apply before further work.

Moves are native renames of the entry/tree, never an implicit copy/delete across
filesystems. Cross-device moves return cross_device. Nonreplacement must use a
native exclusive rename, not a racy exists-then-overwrite check. Replacement follows
native file/empty-directory rules; nonempty directories fail. File removal unlinks
a link itself and rejects directories; remove_directory removes only an empty real
directory. Only explicit remove_tree recursively deletes, without following links.

Directory snapshots and recursive mutations are ordinary host namespace operations,
not an atomic namespace snapshot or a filesystem security boundary. Callers needing
isolation must synchronize concurrent namespace replacement. In particular, no
claim is made that a path-based recursive operation contains malicious concurrent
ancestor replacement. Exclusive creation/rename uses native no-replace operations;
copy checks the actual opened file identities before truncating a destination.
Removal uses native file-versus-directory operations at mutation time rather than
treating a prior metadata observation as authority to remove a replacement type.

## Real async implementation obligations

Extend the existing bounded executor with native jobs, not source-worker exemptions.
Owned native paths/buffers are prepared on the owner before admission. Workers run
only the native operation, in NativeSafe state, with no borrowed managed payload or
generated source callback. Owner completion builds rooted managed outputs and applies
task status. Existing groups/cancellation/drains/code leases remain authoritative.
Queued cancellation can avoid the syscall; an entered noninterruptible syscall must
drain safely. Pending resource leases prevent close/free under OS access. Existing
source-worker/global/FFI restrictions remain unchanged.

A deterministic blocked native job plus another ready task must prove owner
responsiveness, including JIT/O2 and native adapters. No async CLI main is added.

## Completion gate

Final inventory must include actual signatures, source locations, documentation and
per-capability tests. Debug/Release/strict sanitizer JIT/O2/linked AOT, allocations,
GC, aliases, cleanup, native races, 1 MiB stacks, installed sources and LSP must pass.
Windows/macOS tests are registered but remain unverified until run natively. Existing
binary/memory/console/convert/time/globals/VOIDLS regressions remain required.
