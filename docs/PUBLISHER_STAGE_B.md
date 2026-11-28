# Publisher Stage B private diagnostics

Stage B introduced the publication object emitter, private diagnostic map and
final-image metadata auditor. Milestone 3 uses these components in the
native-host publishing command and stores private artifacts separately from
the public distribution. See [production publishing](CLI.md#production-publishing)
for the current CLI and storage contract.

The record below describes the Stage B foundation and its original qualification
boundary.

## Diagnostic contract

`emit_publication_object` uses the ordinary LLVM lowerer, optimizer, roots,
cleanup and async adapters. It substitutes diagnostic constants at checked
failure sites, source call frames and global-cell descriptions. It retains full
DWARF in the intermediate object for private split-symbol extraction. That
object and the unstripped linked image contain private data and are not shipping
artifacts. No production artifact transaction or publication capability is added.

`RuntimeFailure` is unchanged: a borrowed string pointer, a count and 32 borrowed
frame pointers. The count still becomes 33 to mark truncation. Every replacement
is a real NUL-terminated string, with no pointer tagging or Runtime ABI change:

```text
VOID-D1 <64 lowercase hex build ID> <decimal site ID> <decimal category>\n
```

The build ID is SHA-256 of two length-prefixed fields: the domain
`void.private-diagnostics.v1` and a lowercase hex SHA-256 semantic-input digest.
Field lengths are unsigned 64-bit big-endian, as in `project::Digest::field`.
The future admitted publisher must supply the exact normalized input digest,
including source content, target, toolchain, optimization, policy and dependency
identities; physical producer paths must not enter those public inputs. This
API does not admit a project or construct that input snapshot itself. Changing
those inputs requires a new digest. Site IDs are one-based emission ordinals,
stable only within that emission and build. They are not persistent source IDs.

Categories are 1 Runtime, 2 arithmetic, 3 bounds, 4 null value, 5 source call and
6 global access. Classification uses fixed compiler descriptions, with unknown
errors falling back to Runtime. Detailed descriptions remain in the map.
Existing Runtime/native helper messages retain their existing ABI and text;
mapped source-call frames identify their language boundary. This is not a new
Runtime-wide failure engine.

## Private map version 1

All integers below are unsigned 32-bit little-endian. A string is its byte
length followed by exactly those bytes, without a terminator. Strings are
opaque byte sequences; the frontend's path encoding is preserved.

1. Magic bytes `VOID-PRIVATE-DIAGNOSTICS`, NUL, byte 1.
2. Build ID string, final-image SHA-256 string, site count.
3. For each implicit one-based site ID: category, line, column, function string,
   physical source path string, source excerpt string and private description
   string. Function strings include the original module identity.

A draft cannot serialize until bound once to nonempty final-image bytes. Binding
freezes site additions. Readers validate the version, lowercase digest shape,
category, field/count/total bounds and exact end of input. Limits are 16 MiB per
map, 65,536 sites and 1 MiB per field, with a conservative header reserve.
Coordinates are original frontend coordinates, including zero for synthetic
locations. Outlined code may produce multiple site IDs for one source location.

Resolution takes the map bytes, actual final-image bytes and one exact public
token including its newline. It checks SHA-256 of the image, build ID, site ID
and category; missing maps, mismatched images/builds and malformed/unknown sites
throw explicit errors. There is no nearest-build fallback. A byte change after
binding, including later signing, requires a newly associated map. The map is
plaintext and unauthenticated: SHA-256 association is not issuer authentication
or tamper-proof provenance. Key management/encryption/signing remain later work.

Only identity, site ordinal and category are public diagnostic fields. Maps,
split symbols, source excerpts and unstripped intermediates belong in a
separate owner-only archive, never in a distributable directory. Persistence
and permissions remain the future publisher's transactional responsibility.
The library itself performs no filesystem writes or uploads.

## Linux final-image qualification

The Linux controlled harness links using the existing same-build native input
list and LLD, extracts `llvm-objcopy --only-keep-debug`, then writes a separate
image with `--strip-all --remove-section=.comment`. These flags apply to the
controlled ELF fixture only. Its dynamic symbols and unwind records are checked
with `llvm-readobj`; an actual linked function address is resolved with
`llvm-symbolizer` against the split file. No source-bearing debuglink is added.
The private map binds to the stripped bytes, after reduction.

Auditor policy revision 1 retains Stage A behavior. Revision 2 permits dynamic
symbols, rejects static symbol metadata under `reject_symbols`, detects ELF
compiler comments and private-map signatures, reports dynamic/static symbols
and unwind-section presence, and classifies Mach-O external definitions/imports in validated LC_DYSYMTAB
ranges separately from local, private-external and debug nlists. PE imports and
exports are observed independently of their section names. Revision 1 still
rejects any symbol-table entries. Revision 2 rejects compressed ELF sections rather than
claiming their contents were scanned. Section presence does not prove unwind
correctness; the independent oracle supplies fixture evidence. The report
schema remains additive version 1. General closure/export qualification remains
outside this revision.

`publish_private_binary` tests O0/O2 sync and async failures, observable defer
cleanup, the 32-frame limit, private map resolution, wrong/missing maps, stripped
development leakage controls, split-symbol address resolution, compressed
section rejection and accidental map inclusion. A separate retained-identity
fixture deliberately produces a function-name finding: a defer capture's Runtime
schema key needs that identity. It must not be renamed without ABI review.
Development diagnostics and DWARF are independently tested. Whole-process error
timings are informational and include startup, scheduling and I/O; they do not
measure isolated error-path overhead.

## Limits and native qualification requirements

A clean canary scan is not proof of secrecy. It covers selected bytes and ASCII
UTF-16LE forms, not arbitrary encodings or compressed/encrypted payloads.
Compressed debug content is rejected, not decoded. Ordinary packed strings and
native libraries need separate policies and inspection. Runtime type/catalog,
capture, reflection, FFI/export identities, user literals and required legal
notices remain public where required. Some contain source-level names. No
closed-world identity rewriting, obfuscation, LTO, PGO or SLP change is introduced.
Existing producer RUNPATHs and host ABI floors are reported, not fixed here.

Windows x64 requires native controlled source/F5/failure/async/cleanup tests,
actual PE import/export and unwind equivalence, and an established private
archive route for VOID's current embedded DWARF. Do not assume a PDB exists.
If PDB support is later introduced, match its GUID and age exactly. Qualify
CodeView path removal and final bytes using pinned tools before enabling it.

macOS ARM64 requires native tests, dsymutil before stripping, exact Mach-O/dSYM
UUID matches, ASLR-relative address symbolication, retained exports/unwind/load
commands and a final audit after any separately authorized signing. Neither
foreign-format parsing nor this Linux harness qualifies a native publisher.

Native crash collection is not added. Manual split-symbol address resolution is
qualified on Linux; crash capture, relative-PC/load-image recording, archive
retention transactions and automated offline crash reports remain explicit
integration requirements. No hosted compiler matrix is dispatched by Stage B.

## Upstream references

The installed tools identify as LLVM 23.1.4. The patch-specific documentation URL
was unavailable during this implementation; the official 23.1 release guide and
installed tool behavior were checked instead. No claim is made that a rolling
web page is the exact installed patch revision.

- [LLVM 23.1 llvm-objcopy](https://releases.llvm.org/23.1.0/docs/CommandGuide/llvm-objcopy.html)
- [LLVM symbolizer](https://llvm.org/docs/CommandGuide/llvm-symbolizer.html)
- [Upstream split-debug reconstruction discussion 59370](https://github.com/llvm/llvm-project/issues/59370)
- [Upstream split-debug implementation discussion D67090](https://reviews.llvm.org/D67090)
- [Apple UUID and dSYM association](https://developer.apple.com/documentation/xcode/building-your-app-to-include-debugging-information)
- [Microsoft exact symbol matching](https://learn.microsoft.com/en-us/windows/desktop/dxtecharts/debugging-with-symbols)
