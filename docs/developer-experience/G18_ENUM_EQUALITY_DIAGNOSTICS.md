# G18 · Context-aware enum comparison diagnostics

12 November 2026 · Baseline `35406a00aee892def5a0a7555c5beeae6fe284b3` · **IN PROGRESS pending independent Golden Wrench review.**

## Original failure

The supplied `VOID_Adventure_Instant_Keys_Complete_20261112.zip` passed its CRC
check and had SHA-256 `11bab7bfa21391ee79625757537b28611be5cdf2639ad3f1c9acfdfae872886a`.
All five VOID sources are byte-identical to the archive members and retain
their original imports. The original ZIP and reports stay with the G17
evidence. [Member hashes](G17_ASCII_ADVENTURE.md#original-member-sha-256-values) · [Maintained reproduction](../../tests/frontend/fixtures/adventure_instant_keys/)

Run `voidc check tests/frontend/fixtures/adventure_instant_keys/main.void`
with the retained baseline compiler or the candidate. The retained PikaOS
Release and Windows Debug compilers returned status 1 with empty stdout,
six `Value struct equality is not implemented.` errors, and no hints. Each was
`VOID2016` in module `/game.void` with a two-character operator span.
The candidate keeps status 1, error severity, code and locations, and names
the unsupported tagged enum operation.

| File | Line and column, 1-based | Right-hand case |
| --- | --- | --- |
| `game.void` | 27:28 | `console.key.up` |
| `game.void` | 29:33 | `console.key.down` |
| `game.void` | 31:33 | `console.key.left` |
| `game.void` | 33:33 | `console.key.right` |
| `game.void` | 35:33 | `console.key.escape` |
| `game.void` | 37:33 | `console.key.character` |

## Diagnostic contract

`Semantics::binary_expression` resolves operands and custom operators and
checks type compatibility before rejecting built-in equality on aggregates.
Ordinary tagged enums use the same aggregate kind as value structs. Their
nominal definition carries the case list. The diagnostic uses that list and
excludes the compiler's canonical `option` and `result` types. Ordinary value
structs, those canonical sums, and incompatible operands keep their prior
messages and codes. Numeric-backed enums have their own type kind.
[Compiler check](../../src/frontend/compiler.cpp) · [IR types](../../include/void/ir.hpp)

The error text is `Tagged enum values do not support built-in '==' or '!='.`
A resolved right-hand case construction without payload arguments gets the
specific case hint. That covers qualified names, import aliases and the
accepted empty-call spelling. A variable, payload construction or case
written only on the left receives guidance to handle cases and payloads
explicitly.

| Comparison | Hint |
| --- | --- |
| `==` with a resolved payload-free right-hand case | Use 'is' to test this enum case, or 'match' to handle multiple cases. |
| `!=` with a resolved payload-free right-hand case | Use a negated 'is' test to exclude this enum case, or 'match' to handle multiple cases. |
| Other rejected tagged enum equality | Use 'match' to inspect the cases and compare any payloads explicitly. |

The CLI renders the existing `Diagnostic.hint` and the language server
appends it to the same diagnostic message. The guarded code-action path
offers no enum rewrite. Equality resolution, generic specialization,
pattern checks, typed IR, LLVM lowering and Runtime behavior are unchanged.
[Usage and payload example](../ADVANCED_LANGUAGE.md#enum-case-tests-and-imports) · [Diagnostic reference](../DIAGNOSTICS.md#tagged-enum-equality)

## Focused checks

- `enum_equality_diagnostics` asserts the six original errors, exact operator
  positions and hints, aliases and qualified cases, both operators, arbitrary
  enum values, payload constructors and invalid operands. A generic enum
  specialization retains its request and declaration locations. Value
  structs, `option` and `result` keep their old diagnostics. The positive
  program checks and runs in O0 and O2 with numeric and string comparisons,
  managed reference identity, numeric-backed enums, declared enum equality,
  qualified and unqualified case patterns, and explicit payload comparison.
  A separate rejected collection-valued pattern demonstrates the boundary on
  arbitrary enum values. [Frontend regression](../../tests/frontend/enum_equality.py)
- `lsp_enum_equality` asserts one versioned error and its exact UTF-16 span
  after a BMP character and a surrogate pair on the same line. It covers an
  unsaved document, corrected `is` and `match` revisions, rapid and
  out-of-order edits, diagnostic removal on project reselection, and absence
  of a guarded quick fix. The disk source stays unchanged.
  [LSP regression](../../tests/lsp/enum_equality.py)
- The fifteen-test selection includes frontend, source patterns, numeric
  enum typed IR and process/LSP checks, operator declarations, enum and editor
  recovery, guarded fixes, project selection and protocol, the CLI contract,
  and Adventure. The two new CTests each have a 180-second limit. Existing
  tests and timeouts are unchanged.
- `ascii_adventure` runs the unchanged game in O0 and O2 JIT and independent
  Clang/LLD-linked AOT. It verifies walls, invalid input, locked exit, both
  treasures, twenty-move victory, EOF, usage and redirection. G17's physical
  keyboard and terminal restoration results remain in its accepted record.
  The candidate change affects diagnostic presentation only, so deterministic
  gameplay and focused frontend coverage were repeated.
  [Gameplay driver](../../tests/cli/ascii_adventure.py) · [G17 qualification](G17_ASCII_ADVENTURE.md)

### Native results

LLVM, Clang and LLD reported **23.1.4**. PikaOS 4 x86-64 ran kernel
`7.2.11-pikaos`. The strict build used RelWithDebInfo and
`VOID_SANITIZERS=ON`, with leak detection and fatal ASan/UBSan.
The guest identified itself as Windows 10 Home x64, build
`10.0.19045.6466`. Both Windows configurations used MSVC tools
`14.44.35207`, Windows SDK `10.0.19041.0`, verified provider prefixes,
and independently verified private temporary roots. The baseline came
from a Git archive. Each candidate source and test input was checked
against its checkout SHA-256 before rebuilding.

| Profile | Focused CTests | Wall time | Adventure |
| --- | --- | --- | --- |
| PikaOS Debug | 15/15 | 103.94 s | PASS, 97.06 s |
| PikaOS Release | 15/15 | 28.18 s | PASS, 25.48 s |
| PikaOS strict ASan/UBSan | 15/15 | 103.12 s | PASS, 92.62 s |
| Windows 10 Debug | 14/15 initial, 1/1 isolated replay | 184.27 s initial, 134.57 s replay | PASS on replay, 134.55 s |
| Windows 10 Release | 15/15 | 59.56 s | PASS, 36.84 s |

Windows Debug initially passed 14/15 in 184.27 s. Adventure passed every
gameplay assertion and printed success for all four lanes. The test failed
at 135.58 s during `TemporaryDirectory` cleanup because deleting
`O2/adventure.exe` returned `WinError 5`. A later probe found an ordinary
archive attribute and an inherited full-access ACL for owner rights. The
probe acquired and closed a share-mode-zero read handle, then deleted the
file on its first attempt. The source of the original refusal was
unconfirmed. The isolated CTest replay of `^ascii_adventure$` passed
with the same fixture, assertions and limits.

### Real editor

PikaOS VS Code 1.141.0, embedded Node 24.21.0, ran the existing extension
against the Release `voidls`. The initial disposable profile was untrusted.
The rerun used the repository host-test convention for an isolated trusted
profile. Its unsaved edit showed the error, `VOID2016` and the full hint in the
rendered Problems panel at line 4, column 35. Restoring `is` removed the
diagnostic and the Problems row. The test verified the unchanged disk source
and absence of a VOID enum quick fix. The editor and extension host exited
normally.

Windows 10 ran the native VS Code 1.141.0 archive, also with embedded Node
24.21.0, against the Debug `voidls`. It passed the same dirty-buffer,
rendered Problems, range, hint, action and correction checks. The observed
LSP range on both hosts was line 3, columns 34–36, in zero-based UTF-16.

### Replay

Use private, disposable temporary storage. Build with pinned LLVM/Clang/LLD
23.1.4 and the existing verified host and HTTP prefixes. On Windows run from
the Visual Studio 2022 x64 tools prompt with a verified private temporary
root. The same anchored CTest selection applies to both hosts.

```sh
selection='^(enum_equality_diagnostics|lsp_enum_equality|frontend|numeric_enum_(ir|process|lsp)|patterns_source|stage10_operator_declarations|lsp_enum_recovery|lsp_editor_recovery|lsp_quick_fixes|project_selection|project_protocol|ascii_adventure|cli_contract)$'
cmake --build "$build" --target voidc voidls void_tests void_project_selection void_numeric_enum_ir void_patterns_tests void_operator_declarations
ctest --test-dir "$build" --show-only=json-v1 -R "$selection"
ctest --test-dir "$build" --output-on-failure -j 2 -R "$selection"
```

For the strict run the exact settings were
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`. The new payload
example in Advanced Language passed `voidc check` and uncached `voidc run`.

## Retention and review state

Removed the task-owned Linux scratch tree, VM transfer tree and Windows
guest build/work tree after collecting the results. The original ZIP and
existing verified toolchains and PikaOS build profiles were retained.

| Filesystem | Free bytes before cleanup | Free bytes after cleanup |
| --- | ---: | ---: |
| PikaOS temporary filesystem | 16,338,497,536 | 16,581,836,800 |
| PikaOS VM transfer filesystem | 367,505,670,144 | 367,505,711,104 |
| Windows guest volume | 12,187,402,240 | 14,999,605,248 |

The worktree contains eight modified tracked paths and eight untracked
text files. The index is empty. `git diff --check`, whitespace checks on the
new files, Python syntax, added Markdown links and original fixture byte
comparisons passed.

## Disposition

G17 is integrated and independently source-reviewed at
`35406a00aee892def5a0a7555c5beeae6fe284b3`. G18 is ready for review of the
compiler diff and focused evidence. General tagged enum equality and any
future editor rewrite require their own semantic decision. The public
distribution and optimization HOLDs, Windows persistent cache HOLD,
Windows file-symlink privilege limitation, deferred deadline composition,
retained raylib callback HOLD, unsupported macOS managed-native and D5
scope, and deferred seven-job gate retain their recorded status.
[Milestone register](../roadmap/MILESTONE_REGISTER.md)
