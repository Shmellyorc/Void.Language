# G17 · ASCII Adventure and real application developer experience

12 November 2026 · Baseline `d453195bfcbd34db211f4dddd8fa0832acbf8733` · **IN PROGRESS pending independent Golden Wrench review.**

The maintained example is derived from Boss's supplied five-module Adventure.
It runs in an interactive terminal or through an explicit line-input path.
The reproduced source failures follow the current language rules. Compiler,
LSP, standard-library and Runtime production source are unchanged. G00-A
was independently reviewed at this baseline. Its public distribution HOLD,
G08 optimization and Windows cache HOLDs, and the deferred seven-job
project/raylib gate remain. [Example](../../examples/ascii_adventure/README.md) · [Milestone register](../roadmap/MILESTONE_REGISTER.md) · [G00-A audit](../release/G00_READINESS_AUDIT.md)

## Supplied evidence and chronology

All ZIPs passed CRC inspection. Each contains five VOID sources and an
original README and engineering report. The two Instant Keys copies were
byte-identical. Original files were read in disposable workspaces. The
appended reports retain the original wording, including their unverified claims.

| Archive | Bytes | SHA-256 | Original behavior or claim |
| --- | ---: | --- | --- |
| `VOID_Ascii_Adventure_Source_20261112.zip` | 5,579 | `dd4b69c766fa6d35c517462348f0267a6fd803bd419616a342d40140a9847901` | Structured `stdin.read_line()` with Enter. Static map verification only. |
| `VOID_Adventure_Enum_Match_Fix_20261112.zip` | 4,048 | `ccf77db92ba1822d7d7680e96fe98824f630053adf35fc0c3c95a265dd6623c6` | Pattern dispatch and a scoped noncanonical/no-echo session. Its report describes static verification. |
| `VOID_Adventure_Instant_Keys_Complete_20261112.zip` | 5,134 | `11bab7bfa21391ee79625757537b28611be5cdf2639ad3f1c9acfdfae872886a` | Keyboard reads with `==` on `console.key`, and no scoped admission. Its report describes source inspection only. |

The original source uses the checked endpoint `console.stdin.read_line()`.
The Instant Keys report calls the previous API `console.readline()`.
The Match Fix report says the renderer is unchanged. Archive member hashes
show that Match Fix also changes the renderer and rewrites the map's row
selection while retaining all 12 rows and their contents. `main.void` and
`player.void` are byte-identical across the originals.

## Unmodified execution

Baseline replay selected `main.void` explicitly from each extracted directory.
The local module imports resolved in all three revisions. The two rejected
graphs report errors in imported modules, with no entry or import-resolution
diagnostic. Entry execution is blocked by those errors. The accepted graph
reached the actual terminal-capability guard through both codegen lanes.

```sh
voidc check main.void
voidc run main.void --no-cache --no-opt
voidc emit main.void --no-opt -o baseline.o
```

Successful objects were linked by Clang/LLD 23.1.4 with the configured Runtime
and native-resource manifest. Windows used `.obj` and `.exe` and checked the
actual `lld-link.exe` selection in the driver plan. JIT and independent AOT
first received `q` through redirected stdin. The capability refusal is an
executed application exit with status 2.

| Exact revision | Check | Normal JIT | Emit and independent AOT |
| --- | --- | --- | --- |
| Original | Status 1. Five errors in `game.void`, one warning in `world.void`. | Status 1 with the same diagnostics. | Emit status 1, no object to link. |
| Enum Match Fix | Status 0. Entry and imported graph accepted. | Redirected stdin reached its capability refusal, status 2. A separate Release PTY control moved right, then quit, status 0. | Emit and link succeeded. Redirected execution reached the same capability refusal, status 2. |
| Instant Keys | Status 1. Six `VOID2016` errors at the enum comparisons. | Status 1 with the same diagnostics. | Emit status 1, no object to link. |

PikaOS Debug/Release/strict and Windows 10 Debug/Release reproduced these
check, JIT and object-admission outcomes. The complete source diagnostics
appear below. A disposable copy of Original with only the binding renamed
to `entered_line` checked and ran to `Goodbye!` with redirected `q`,
status 0. Its separate integer-match warning persisted.

## Language and diagnostic findings

`stdlib/console.void` declares `console.key` as an ordinary payload-free
enum. `console.key_event` is a value struct with a const `key: key` field.
`pressed.key == console.key.up` therefore compares enum values. Both case
values resolve. The frontend routes built-in aggregate equality through the
same `VOID2016` rejection as an ordinary value struct. The minimal compiler
diagnostic is at the `==` token, line 3, column 24.

```void
use console
func probe(pressed: console.key_event) -> bool:
    return pressed.key == console.key.up
```

`is` and `match` test a case tag and support payload patterns where present.
A plain string command continues to use ordinary string equality. No enum
equality behavior changed. [Language Bible](../LANGUAGE_BIBLE.md) · [Pattern contract](../ADVANCED_LANGUAGE.md#enum-case-tests-and-imports) · [Source enum](../../stdlib/console.void) · [Frontend equality](../../src/frontend/compiler.cpp)

| Isolated source probe | Observed result |
| --- | --- |
| Payload-free enum `==` and a separate `point` value struct `==` | Both reject with `VOID2016` at the operator. |
| `pressed.key is console.key.up` or `pressed.key is key.up` with `use console` | Both check successfully. |
| `match pressed.key` with `console.key.up:` or `key.up:`, then `_:` | Both check without diagnostics under `use console`. |
| `use console as term`, then `term.key.up:` | Checks without diagnostics. The same alias import with bare `key.up:` yields `VOID2002` at 4:9 for unknown `key`. |
| Small two-case payload-free enum with both cases listed | Exhaustive and warning-free. A payload case such as `notice.arrived(var place)` also checks in a complete enum match. |
| One known key arm with no wildcard, followed by a return | Status 0 with `VOID2021` at the match, line 3, column 5. A wildcard removes the warning. |
| Integer match with one literal arm and a return after the statement | Status 0 with `VOID2021` at 2:5. A wildcard makes the match itself exhaustive. Original's 12 literal rows leave all other i32 values unmatched. |
| `var input = "value"` after unqualified `use console` | `VOID2005` at 3:9 because the imported type name `input` is already visible. With an alias import, the same local declaration checks. |

A real `voidls` stdio probe opened the candidate's `game.void` in the
five-file graph. Debug and Release navigated from `player.start` to
`player.void`. A dirty overlay reintroducing the enum comparison produced
`VOID2016` over the two operator characters (UTF-16 zero-based line 60,
columns 27–29). Restoring the unsaved buffer cleared the diagnostic and
restored navigation in the same server. The maintained protocol, enum and
incomplete-editor-recovery CTests provide adjacent regression coverage.

| Developer observation | Classification and disposition |
| --- | --- |
| No shadowing | Intentional language rule. Original's `var input` is an application-source mistake. The remaining four errors disappear with one binding rename. |
| Enum equality assumption | Application-source mistake under existing semantics. The generic value-struct wording is a diagnostic clarity gap. |
| Qualified patterns | Documentation/example gap. Both visible type-name forms work with the unaliased import, and an alias qualifies the same enum identity. |
| Exhaustiveness | Intentional warning for missing coverage. The integer and enum negatives above produce the warning with the original contract. |
| Immediate keyboard input | Documentation/example gap. The blocking key call temporarily owns mode. The scoped session retains one mode across frames and checks restoration. |
| Console portability | Host/environment boundary. Redirection, terminfo, clear support and key direction availability depend on the actual endpoint. |
| Convenience `readline()` versus checked `read_line()` | Documented API distinction. The checked endpoint can distinguish empty input, EOF and I/O errors. |
| Standard-library discoverability | Documentation/example gap. The executable example and language/console notes give direct use and replay commands. |

No compiler, language-service or standard-library correctness defect was
established in these probes. A separate narrow diagnostic proposal could
keep `VOID2016` and the operator span, identify an already-resolved enum
type, and suggest `is` or `match`. Its tests would preserve rejection for
the actual enum comparison and compare the ordinary value-struct diagnostic
and valid qualified/unqualified pattern controls. This proposal requires
review and introduces no operator or parser change.

## Corrected application and gameplay

The example starts from Enum Match Fix. It retains the original 12×12 map,
start (1,1), treasures (3,5) and (6,9), exit (10,10), coordinates and turn rules.
`world.void` and `player.void` are byte-identical to the Match Fix archive.
`main.void` selects keyboard or explicit `--line` input. The shared
`advance` operation keeps movement and collision rules independent of the
terminal. `game.void` maps real key events through qualified patterns,
handles EOF/errors and checks scoped restoration. `renderer.void`
flushes the frame and displays controls appropriate to the chosen input path.
It clears supported output and separates redirected frames without controls.

The maintained gameplay test checks the full 12×12 initial frame, unchanged
turns for empty/invalid input and walls, movement to a locked exit, both
treasures, a revisit without recollection, and victory in 20 moves. It also
checks quit, empty input versus EOF, malformed UTF-8, redirected keyboard
refusal, invalid arguments and no terminal escape sequences in redirected
output. The same four lanes use uncached O0/O2 JIT and O0/O2
independently linked AOT. [Regression](../../tests/cli/ascii_adventure.py) · [Executable source](../../examples/ascii_adventure/main.void)

## Terminal behavior

| Control | Observed result |
| --- | --- |
| Actual PikaOS keyboard | Boss used the physical keyboard in xterm. PikaOS Debug normal JIT recorded Right to (2,1) and S to (2,2) without Enter, X with no turn change, and Esc with status 0. The display cleared and redrew. Full termios was restored exactly. Local flags were 35387 → 35377 → 35387 with ISIG preserved. |
| Actual Windows keyboard | Boss used the physical keyboard in the Windows 10 VM's native console. The Debug independently linked normal AOT executable recorded the same two moves, no turn change on X, Esc and status 0. The native screen buffer recorded the redraws. Console mode was 503 → 497 → 503. An earlier unknown input at the start advanced no turn. Its origin was not established. |
| Immediate read without a session | Separate controlled native tests started in line/echo mode and called blocking `read_key()` without `begin_mode`. On PikaOS Debug/Release/strict, a PTY-delivered Right sequence was recognized and the full mode restored. POSIX reported unknown physical key direction. On Windows Release, a controlled `ReadConsoleInputW` replay returned Right with known key-down and known key-up. Mode 503 → 497 → 503. |
| Checked keyboard error exit | Injected malformed UTF-8 into the candidate on a POSIX PTY and a malformed UTF-16 key record into a task-owned Windows console. The candidate returned 2 with `Console input failed: invalid_encoding`. Full prior mode restored on PikaOS Debug/Release/strict and Windows Debug/Release. These are fault controls. |
| Redirected input and EOF | Default keyboard mode refuses redirected input before drawing and directs the user to `--line`. Line mode reports EOF and exits 0. An empty line remains an invalid command. Keyboard EOF is a checked `closed` result; the game treats it as input completion. |
| Applicable cleanup | The app owns one `with` scope and checks `session.close()` on normal and typed-error returns. Existing native console tests cover session cleanup and restoration. The app preserves the host interrupt policy. Forced process termination and a lost terminal are outside the exact-restoration claim. |

The blocking `read_key()` call provides immediate input on a supported endpoint
even when the caller has not begun a session. The maintained game holds the
scoped mode to keep no-echo/noncanonical behavior across redraws. The check for
known key release avoids treating release as movement while accepting POSIX
logical key sequences. Physical keyboard tests above are distinct from PTY
bytes and injected Win32 records. [Console contract](../stage10/CONSOLE_IO.md#choosing-key-or-line-input) · [Native console tests](../../tests/runtime/console_keyboard.cpp)

## Native qualification and retained limits

All lanes used LLVM/Clang/LLD **23.1.4**. The local compiler source is identical
to the G12 integrated compiler source. The G00-A baseline changes only
documentation relative to G12. PikaOS 4 reported x86-64 kernel
`7.2.11-pikaos`. The guest OS reported Windows 10 Home x64
`10.0.19045.6466`, SDK 10.0.19041.0 and MSVC 14.44.35207. Both Windows
profiles rebuilt the exact Git source archive using attested provider
inputs, verified private NTFS roots, and SHA-256 checks for all 21 replay
inputs. Independent linking used the manifest resources and pinned driver.
Strict Linux used RelWithDebInfo, `VOID_SANITIZERS=ON`,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`.

| Compiler profile | Focused maintained tests | Game |
| --- | --- | --- |
| PikaOS Debug | 18/18 in 125.67 s | CTest passed in 82.21 s, all four lanes. |
| PikaOS Release | 18/18 in 36.48 s | CTest passed in 23.85 s, all four lanes. |
| PikaOS strict | 18/18 in 131.37 s, no sanitizer findings | CTest passed in 83.08 s, all four lanes. |
| Windows 10 Debug | 13/13 adjacent CTests in 89.27 s | Same maintained game command executed directly, all four lanes passed. |
| Windows 10 Release | 13/13 adjacent CTests in 33.82 s | Same maintained game command executed directly, all four lanes passed. |

The common adjacent selection was `runtime_console`, `runtime_console_input`,
`runtime_console_keyboard`, `runtime_console_terminal`, `numeric_enum_ir`,
`numeric_enum_process`, `numeric_enum_lsp`, `project_selection`,
`project_protocol`, `lsp_enum_recovery`, `lsp_editor_recovery`, `frontend`
and `cli_contract`. Linux also ran `runtime_console_native_input`,
`runtime_console_native_keyboard`, `runtime_console_native_terminal`,
`runtime_console_endpoints` and `ascii_adventure`. Active registrations were
checked before execution. Existing timeouts and assertions were preserved.

```sh
ctest --test-dir "$build" --output-on-failure --no-tests=error -R '^ascii_adventure$'
python3 -B tests/cli/ascii_adventure.py "$build/voidc" "$build/void-native.txt"
```

The Windows direct replay used the same Python fixture and `voidc.exe` in
the x64 Visual Studio developer environment. The new maintained test has a
1200 s CTest timeout and 180 s per child. The qualified keyboard hosts are
xterm on PikaOS and the console observed on Windows 10. Other emulators
and macOS have no G17 keyboard acceptance.

The G12 Windows file-symlink incident remains separate. `io_execution`
exited 91 at check 91, printed `check 91 failed` and had empty stderr.
The separate `io_atomic_save_native` control directly observed error
1314 (`ERROR_PRIVILEGE_NOT_HELD`), refused a junction and passed. Both
G12 archive tests passed in Windows Debug and Release. The unchanged
privileged replay remains a future qualification. [Exact prior incident](../release/G00_READINESS_AUDIT.md#windows-file-symlink-incident)

## Changed-file inventory and disposition

| Area | Exact paths |
| --- | --- |
| Executable example | `examples/ascii_adventure/main.void`, `examples/ascii_adventure/game.void`, `examples/ascii_adventure/world.void`, `examples/ascii_adventure/player.void`, `examples/ascii_adventure/renderer.void`, `examples/ascii_adventure/README.md` |
| Maintained regression | `CMakeLists.txt`, `tests/cli/ascii_adventure.py` |
| Documentation | `docs/ADVANCED_LANGUAGE.md`, `docs/stage10/CONSOLE_IO.md`, `docs/roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md`, `docs/roadmap/MILESTONE_REGISTER.md`, `docs/developer-experience/G17_ASCII_ADVENTURE.md` |

Golden Wrench should review the executable example and evidence before Boss
considers integration. Enum equality and the optional diagnostic improvement
remain separate decisions. No hosted CI, staging, commit, push or publication.


Documentation validation checked 21 new local links and anchors and compiled
the new pattern example. The 13 intended paths are text/source files.
`git diff --check` passed. HEAD remains the baseline, with five modified
tracked files, eight untracked files and an empty staged diff.

Task-owned Linux and Windows workspaces and the VM transfer share were
removed. Free bytes rose from 16,342,118,400 to 16,585,158,656 on Linux
tmpfs and from 12,499,316,736 to 14,999,343,104 on Windows. The reused
Linux build configurations and verified toolchains/provider inputs were
retained. The supplied archives were left at their original locations.
[Retention policy](../projects/VOID_ARCHIVE_AND_RETENTION_POLICY.md)

## Original member SHA-256 values

| Member | Original | Enum Match Fix | Instant Keys |
| --- | --- | --- | --- |
| `main.void` | `a376dbc24e8b6d36dc365a853f4d867300e7a48aff81187cfe938cca224aa465` | `a376dbc24e8b6d36dc365a853f4d867300e7a48aff81187cfe938cca224aa465` | `a376dbc24e8b6d36dc365a853f4d867300e7a48aff81187cfe938cca224aa465` |
| `game.void` | `5b0b054b0c5db32ba567a4d56fdd6f3ed974b7ed57cdd78b23b6c1b21b17edce` | `f07864478fa67db2f4733d22071a006536eaceb56a31985194408d9dce97e86e` | `bab4f96a06bb8bee1aa5cdd89686cdddaa2ba1efa673a9553275e75117f841d3` |
| `world.void` | `0411c4539d35d1e53f5fb6b1b9dec349bb1f31f007873f4b7335761c1a883871` | `a005abae26ca6ece01bce9e229f2bb355ea9f52307c122c5242b4f5c13201583` | `a005abae26ca6ece01bce9e229f2bb355ea9f52307c122c5242b4f5c13201583` |
| `player.void` | `63086ba86221b1e9ea191e6f6c514f2ecf421688026a8640f209c961e0901f2d` | `63086ba86221b1e9ea191e6f6c514f2ecf421688026a8640f209c961e0901f2d` | `63086ba86221b1e9ea191e6f6c514f2ecf421688026a8640f209c961e0901f2d` |
| `renderer.void` | `b0f4653489083552451d3a3b65cd4ec2507c7e6b4b999888f0afc2b1273e1ce1` | `2b49be465295f8d4d504f16d79f9048ba5de9c6f6f3c68ececb2a62bbf69ca14` | `2b49be465295f8d4d504f16d79f9048ba5de9c6f6f3c68ececb2a62bbf69ca14` |
| `README.md` | `eae704f7404806c21ef01e86f3ac3e143c2de1efda9a100018bb0c9288d421d8` | `2198c60facc6c000b233ef25e90657de80407c6acd345b3ea5411e5aed059771` | `b6c47036825e0234d186e054b81110953dfd1efd71c567aa16c4e8c123c117a7` |
| `REPORT.md` | `efbfbdf8a5e9b1ab457a341b6ae7bb9bcb0d495dbf146be0574ddb6d8f6c1ebf` | `2d0e01a2be76e3e7844e76eaee3aca03a094e3ff13af1d4c1ef5d15ebedce2ea` | `c4f89db3019c553453b3c625a8cdb581446bc759ac9010e8b0dd442e6cd9df99` |

## Complete baseline diagnostics

PikaOS Release source diagnostics are below. The paths use the archive
variant instead of its disposable absolute directory.

### Original

```text
original/game.void:22:48: error: Duplicate declaration or shadowing of 'input'.
    code: VOID2005

 22 |         if console.stdin.read_line() is Ok(var input):
    |                                                ^^^^^
    Module: /game.void
    Hint: Assign to the existing variable, or choose a distinct name.

original/game.void:23:16: error: Unknown identifier 'input'.
    code: VOID2002

 23 |             if input is console.text_read.eof:
    |                ^^^^^
    Module: /game.void
    Hint: Declare it with var before use.

original/game.void:26:16: error: Unknown identifier 'input'.
    code: VOID2002

 26 |             if input is console.text_read.text(var line):
    |                ^^^^^
    Module: /game.void
    Hint: Declare it with var before use.

original/game.void:26:25: error: Unknown struct type in pattern.
    code: VOID1103

 26 |             if input is console.text_read.text(var line):
    |                         ^^^^^^^
    Module: /game.void

original/game.void:27:27: error: Unknown identifier 'line'.
    code: VOID2002

 27 |                 command = line
    |                           ^^^^
    Module: /game.void
    Hint: Declare it with var before use.

original/world.void:6:5: warning: Match may leave values unmatched.
    code: VOID2021

 6 |     match y:
   |     ^^^^^
    Module: /world.void
```

### Instant Keys

```text
instant/game.void:27:28: error: Value struct equality is not implemented.
    code: VOID2016

 27 |             if pressed.key == console.key.up:
    |                            ^^
    Module: /game.void

instant/game.void:29:33: error: Value struct equality is not implemented.
    code: VOID2016

 29 |             else if pressed.key == console.key.down:
    |                                 ^^
    Module: /game.void

instant/game.void:31:33: error: Value struct equality is not implemented.
    code: VOID2016

 31 |             else if pressed.key == console.key.left:
    |                                 ^^
    Module: /game.void

instant/game.void:33:33: error: Value struct equality is not implemented.
    code: VOID2016

 33 |             else if pressed.key == console.key.right:
    |                                 ^^
    Module: /game.void

instant/game.void:35:33: error: Value struct equality is not implemented.
    code: VOID2016

 35 |             else if pressed.key == console.key.escape:
    |                                 ^^
    Module: /game.void

instant/game.void:37:33: error: Value struct equality is not implemented.
    code: VOID2016

 37 |             else if pressed.key == console.key.character:
    |                                 ^^
    Module: /game.void
```

## Supplied engineering reports

Verbatim `REPORT.md` members.

<details>
<summary>Original</summary>

````markdown
# VOID ASCII Adventure — Delivery report

Date: 2026-11-12

## Delivered

Five `.void` source files: `main.void`, `game.void`, `world.void`, `player.void`, and `renderer.void`. The application has an actual loop, two treasures, locked exit until both are collected, walls, movement, ASCII rendering, a turn counter, invalid-input handling, explicit EOF handling, quit and win conditions. The code depends only on VOID source modules and the standard `console` module.

## Source provenance

The source was written using the latest repository Language Bible (`docs/LANGUAGE_BIBLE.md`), working import examples in `website/public/docs/snippets/modules/`, the `examples/stage10_console_time/main.void` application entry and printing, and `stdlib/console.void` line-input and output API. VOID's `for:` unbounded loop is used instead of unsupported `while`.

## Verification

PASS: independent static checks confirm every map row is exactly 12 columns, borders are walls, there are exactly two treasure coordinates (3,5) and (6,9), a single exit (10,10), and a traversable four-direction path from start (1,1) to both treasures and the exit. All five source files are present.

NOT RUN: actual `voidc check`, JIT `voidc run`, native `voidc build`, and platform execution. No `voidc` executable is installed in the task environment. Accordingly, compile acceptance and runtime gameplay remain unverified; any compiler diagnostics should be handled against the specific installed VOID revision rather than declared passing.

## Suggested compiler acceptance

Run in this directory:

```sh
voidc check main.void
voidc run main.void
voidc build main.void -o void-adventure
```

Then feed `q` to confirm clean exit, try a move into a wall, acquire both treasures, and enter `X` to test victory. CLI is based on the repository Stage 10 documented workflow.

## Scope

No source was committed or pushed to the user's GitHub repository. No non-VOID gameplay code is required. Static verification used the host tooling only and is not shipped as part of the game.
````

</details>

<details>
<summary>Enum Match Fix</summary>

````markdown
# Engineering repair report

**Cause:** The previous `game.void` compared `pressed.key` against `console.key` alternatives using `==`. The compiler does not support ordinary equality for this enum representation (value-struct equality). The repository's `docs/collections/FAILURES.md` and `docs/os/IMPLEMENTATION.md` explicitly document using enum `is` or `match` instead.

**Repair:** `game.void` now dispatches on `pressed.key` using exhaustive-pattern-safe `match ... _:`; string equality on commands remains valid. Input uses a scoped terminal mode with `echo = false` and `line_input = false` as demonstrated in the repository's `tests/stage10/console_keys.void`, so keys can be read without Enter. Existing map, player, renderer and entry point are unchanged.

**Verification:** Confirmed 12 rows × 12 cells, unchanged module imports, and no remaining `pressed.key ==` comparisons via static checks. No `voidc` binary is present in this environment, so successful compile, native terminal handling, and gameplay remain unverified.
````

</details>

<details>
<summary>Instant Keys</summary>

````markdown
# Engineering handoff — instant-keyboard update

## Changes

- Replaced `console.readline()` with `console.stdin.read_key()` and direction mapping, including arrow keys.
- Checks terminal key-input capability before entering the game loop.
- Ignores reported key-release events.
- Uses `console.stdout.clear()` when the terminal advertises clear support; otherwise separates frames.
- Carries a status line that tells the player what occurred on the previous move.
- Retains the 12×12 map, both treasures, locked exit, collision checks, and turn counter.
- Does not request terminal mode changes or hide the cursor, avoiding restoration-state obligations.

## Verification

Source packaged and inspected. No VOID compiler executable is available here, so this update is **not yet compile- or runtime-verified**. Run `voidc check .` and `voidc run .` inside a supported interactive terminal.
````

</details>

## G18 follow-up

G17 was integrated and independently reviewed at
`35406a00aee892def5a0a7555c5beeae6fe284b3`. G18 preserves the original five
Instant Keys source files in the maintained diagnostic fixture and checks all
six original comparison locations. The compiler now identifies the tagged enum
comparison and provides pattern guidance. The accepted game continues to use
`match` and its existing terminal and gameplay code.
[G18 implementation and qualification](G18_ENUM_EQUALITY_DIAGNOSTICS.md)
