# VOID Language — Standard-Library Delivery Backlog

**Recorded:** 27 October 2026  
**Project:** `Shmellyorc/Void.Language`  
**Reference HEAD:** `0d014e851851203e30d3b376ca99606a67d36a7a` (channel implementation)  
**Governance:** 👑 Boss decides and alone handles Git/hosted CI; 🏮 Research audits; 🔧 Golden Wrench reviews; ⌨️ Keyboard implements authorized scope. This is a planning record, **not blanket authorization** for subsequent modules.

## Delivery sequence

| Priority | Module / area | State | Scope to research and deliver |
|---|---|---|---|
| **NOW** | **`os`** | **Final architecture GO; Boss authorizes engineering through the separate Keyboard mission** | **All 13 approved public functions**, exact Linux/Windows/macOS policies, checked private native bridge, installation/tooling, native JIT/O2/AOT, GC/errors, cache/reload, strict test evidence and documented native-platform limits. Local working-tree `docs/os/{RESEARCH,DESIGN,ROADMAP}.md` contains authoritative design; these may be uncommitted. |
| **NEXT 1** | **`collections`** | Backlog; **research not yet authorized by this record** | Useful operations missing from built-in fixed/growable arrays and dictionaries: stable sorting, comparators, ordered searches, find/find-index, predicates, filtering/mapping, reverse/copy, fold/reduce and aggregations as justified. Audit existing builtin operations first; preserve readonly, value/reference semantics, callback effects, allocation limits, iteration invalidation and JIT/AOT. Avoid LINQ clones, mandatory containers or another collection engine. |
| **NEXT 2** | **`log`** | Backlog; research/implementation separately approved | Practical configurable logging: severity filtering; clear messages/formatting, timestamps, console and file sinks, structured context if supported naturally, explicit flush/close, rotation/retention where appropriate, failure handling and bounded memory. Research captured-callback cost, eager interpolation, async/concurrent emission policy, file lifecycle and sharing using existing IO/time/OS state directories. Avoid a second formatter, log compiler or unbounded queue. |
| **NEXT 3** | **`net/http` foundation** | Backlog; research/implementation separately approved | A complete **secure HTTP-client foundation**, not a pretend `get()` wrapper: URL validation, requests/methods, headers, status, redirects, body streaming and size limits, TLS verification, DNS, connect/read/overall timeouts, error model, real async cancellation/resource cleanup, HTTP protocol/version boundaries, proxies/auth/cookies where justified. Clarify dependency on `net/tcp`, TLS provider and separate `url`; preserve distinct protocol APIs and platform-native security. Start with a coherent supported client scope; do not claim the whole networking ecosystem is finished. |

## `os` architecture now settled

The approved **13 public functions** in `docs/os/DESIGN.md` are:

`data_dir(app_id)`, `config_dir(app_id)`, `cache_dir(app_id)`, `state_dir(app_id)`, `home_dir()`, `temp_dir()`, `executable_path()`, `resource_dir()`, `env(name)`, `platform()`, `architecture()`, `process_id()`, and `is_bundle()`.

- **Linux:** XDG_DATA_HOME, XDG_CONFIG_HOME, XDG_CACHE_HOME, XDG_STATE_HOME; defaults are HOME-relative under `.local/share`, `.config`, `.cache`, `.local/state`; honor launcher/container overrides. Portable `runtime_dir` is deliberately excluded.
- **Windows:** LocalAppData/id for data; RoamingAppData/id for configuration; LocalAppData/id/cache and LocalAppData/id/state; native Known Folders and UTF-16; desktop Win32 scope. The placement is Boss-approved.
- **macOS:** Foundation user-domain Application Support, Caches, container-aware home/temp; macOS `.app` uses actual `mainBundle.resourceURL`, other processes use executable parent for resources. This policy is Boss-approved.
- **Working directory:** Already supported by `path.absolute("", "")`; no duplicate `os.cwd()` bridge.
- **All locations:** getters return paths without creating, validating writability, or claiming containment/security; existing `io` performs mutation. JIT returns **host** executable/resource/PID identity, linked AOT returns the application process identity.
- **OS errors:** keep `os.error` distinct from `io.error`; native domain/code retained; invalid UTF-8 and absent vs empty environment distinguished; do not leak environment secrets.
- **Native-platform evidence:** Linux-local success must not be called Windows/macOS native acceptance. No Git mutations or hosted CI by assistants.

## Adjacent capabilities — ownership, not silent omissions

| Capability | Intended owner / policy |
|---|---|
| Host path composition, normalization and cwd snapshot | **Existing `path`** |
| File/directory operations, metadata, async file jobs and exclusive file creation | **Existing `io`** |
| Secure private temporary file/directory acquisition, cleanup and race policy | **Future `io` enhancement**, separately scoped |
| Process launch/wait/signals/termination, process-wide cwd or environment mutation, raw arguments | **Future `process`**, separately scoped |
| Command-line argument parsing | **Future `args`**, separately scoped |
| Detailed OS versions, physical CPU/memory, emulation, account/security/packaging identity | **Future `system`**, separately scoped |
| Linux XDG_RUNTIME_DIR or equivalent IPC/session-specific security contracts | **Platform-specific future design**; do not synthesize from temp/cache/state |
| `net/tcp`, `net/udp`, TLS, URL codec, proxies | **Networking dependencies**; separate modules/contracts where justified |
| `compression`/archives, `crypto`/`checksum`, `regex`, `sync`, `testing`/`bench`, database | **Longer-term backlog**; no authorization and no implied surface yet |

## Definition of "feature-complete" for each approved module

1. **Complete a bounded, useful capability**, not a tiny demonstration; prepare an explicit API inventory and adjacent-responsibility matrix before implementation.
2. **Reuse current language/Runtime/compiler services** rather than duplicating `event`, formatting, file IO, collections or async schedulers.
3. **Define the real ownership/error/readonly/async semantics** and reject unsupported behavior explicitly; no guessed native paths or unsafe fallback.
4. **Test good and bad cases**, allocation failure, managed rooting and lifetime, cache and reload, normal/O2 JIT, independently executed linked AOT, real 1 MiB stacks, relevant strict sanitizers, and compiler-owned tooling.
5. **Cross-platform claims require actual native results**; mark missing Windows/macOS/packaged/sandbox evidence as pending, not passed. Preserve first failures and reproduction provenance.
6. **Keep one module per explicitly authorized engineering sprint** after Research → Wrench → Boss approval. No Git staging, commits, pushes, branches, hosted CI or protected root README edits by Clankers.
7. **Deliver source, docs, examples, API matrix, benchmark evidence, changed-file inventory, hashes and a Wrench handoff.** Wrench independently reviews before Boss performs Git and hosted CI.

## Current checkpoint

- `channel` was independently reviewed and pushed at `0d014e85` with `[skip ci]`. Hosted cross-platform acceptance remains a separate Boss-controlled gate.
- `os` research's revised 13-function contract and 25-capability matrix received **Golden Wrench architecture GO**; Boss approves beginning the complete engineering mission. No `os` implementation/verification is yet evidenced by this backlog.
- Earlier historical compiler/LLVM incident families H1–H7 must not be called resolved without direct causal evidence.
- Prioritize **`collections` → `log` → `net/http` foundation** as Research missions after `os`, subject to Boss decisions. **Do not implement future backlog merely because it appears here.**

**THREE CLANKERS. ONE VOID. STRONG BOLTS. NO GHOST BOLTS!**
