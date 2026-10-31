# VOID Language — Standard-Library Delivery Backlog

**Recorded:** 27 October 2026  
**Project:** `Shmellyorc/Void.Language`  
**Current checkpoint:** 30 October 2026, `15f7868b0af177f8f7d49c548392c79c7bd921d2`, LLVM/Clang/LLD 23.1.4. Original 27 October channel reference: `0d014e851851203e30d3b376ca99606a67d36a7a`.
**Governance:** 👑 Boss decides and alone handles Git/hosted CI; 🏮 Research audits; 🔧 Golden Wrench reviews; ⌨️ Keyboard implements authorized scope. This is a planning record, **not blanket authorization** for subsequent modules.

## Delivery sequence

| Priority | Module / area | State | Scope to research and deliver |
|---|---|---|---|
| Delivered | `os` | All 13 public functions implemented; registered native tests passed on Linux, Windows and macOS | [API and evidence](os/HANDOFF.md). No new independent source review or formal release lock claimed. |
| Delivered | `collections` | All 24 approved public helpers implemented; feature-complete for the accepted API scope; cross-platform CI passing | [API and evidence](collections/HANDOFF.md). No broader collection-engine claim. |
| Implemented; hold remains | `log` | Substantial approved logging API implemented; cross-platform tests passed | [Historical optimizer incident, performance OPEN and release HOLD](log/HANDOFF.md) remain unresolved. No causal repair is implied. |
| **NOW** | **`url` and `net/http`** | **Linux implementation handoff ready; independent acceptance pending** | [Current engineering](net/http/CURRENT_ENGINEERING.md), [guide/examples](net/http/README.md), [contract](net/http/DESIGN.md). Direct authoritative curl with four exact approved fixes; approved unmodified Linux TLS pin; existing Runtime worker pool and bounded async retirement. Actual source 2 GiB and native bulk 4 GiB+1 probes pass. Source bridge, bulk/multipart, protocols/security/policies, tooling and install closure are implemented. Native Windows/macOS recipes are unexecuted; provider worst-case allocation proof remains open. Full all-target local builds pass; [consolidated results and focused repairs](net/http/HANDOFF.md) are recorded. No manual patch/ZIP, Git action, hosted CI or root README change. |

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
- Boss records LLVM 23.1.4 hosted acceptance at `15f7868b`, CI #40982216303: seven successful jobs, 2,454 passing CTests, zero failures and five documented cross-volume skips. No independent source review or formal release lock is inferred.
- `os`, `collections` and `log` implementation/native status is synchronized in the delivery table above. Historical research, failed attempts and logging holds remain intact.
- Earlier historical compiler/LLVM incident families H1–H7 must not be called resolved without direct causal evidence.
- The current mission is Boss-authorized **`url`/`net/http` engineering**. Other backlog entries remain separately owned; listing them is not authorization.

**THREE CLANKERS. ONE VOID. STRONG BOLTS. NO GHOST BOLTS!**
