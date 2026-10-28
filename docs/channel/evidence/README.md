# Channel capability evidence

27 October 2026. Baseline **`72ee05db7e417645d434b3932bfc1e00ffd4ecb6`**.
[Archive](capability-probes.tar.gz), [archive hash/count receipt](RECEIPT.json),
[research](../RESEARCH.md), [proposed API](../DESIGN.md).

These are small scratch language-capability probes, not a channel implementation.
The unchanged existing Release compiler was used with an actual **1,048,576-byte
soft/hard child stack** and ASLR **2**. No ptrace, debugger, production edit,
Git mutation, hosted CI or system-policy change.

## Outcomes

**31 planned operations matched expected results:** seven accepted source checks,
six expected source rejections, fourteen successful JIT executions (seven each
with `--no-opt` and default O2), two native builds and two independently executed
native binaries. No signal, timeout or internal IR error occurred.

| Capability | Evidence |
|---|---|
| Generic event owner with `func(const t)` | Check; normal/O2 output7; linked native output7. |
| Identity-only managed dictionary key | Const-view contains/read/remove; normal/O2 output `true false`,7,`true false`. |
| Unique per-registration captured wrapper | Unequal wrappers for same callback; remove first leaves second; normal/O2 output `false true`,7. |
| Readonly payload/owner | Const managed message and const bucket emission pass; normal/O2 output7 twice. Mutable callback against const message signature rejects with VOID2001. |
| Type establishment | Empty generic constructor rejects with VOID2001; wrong payload string against i32 rejects with VOID2001. |
| File global | Generic event object initialized in an ordinary global; normal/O2 output7. |
| Nested snapshot mutation | Remove B, add C, nested emit: normal/O2 trace `AACB`. |
| State detach during active event | Null old nullable generic holder, replace/reenter, finish old snapshot: normal/O2 and linked native trace `ACB`. |
| Existing restrictions | Const subscription mutation rejects VOID2007; callable-containing async argument rejects VOID1103; result-returning handler rejects VOID2001. |

One **additional initial fixture-authoring error** was rejected by the compiler:
a local named `source` shadowed a type named `source`. This was ordinary correct
VOID2005/related diagnostics, not a compiler defect or abnormal exit. The original
source, driver, output and receipt are retained (`initial_shadowing.void`,
`additional.initial.py`, `initial-fixture-error.json`, `initial-shadowing-check.*`).
Corrected distinct-name fixtures are separate evidence. There were **32 total
compiler/build/native invocations** including that rejected authoring attempt.
No failure was erased or counted as a passing test.

## Provenance and reproduction

The archive has a relative `capability-probes/` prefix and includes all fixture
sources, original/corrected generators, commands, output/status/timing JSON,
raw stdout/stderr and `FINAL_PROVENANCE.json`. That manifest records 135 production
input hashes, exact source hashes, compiler hash, generated binary sizes/hashes,
stack/ASLR and scope. The archive excludes generated executable/cache contents.
Every archived member was reread and compared against its original content hash.

The initial and final compiler SHA-256 is:

```text
6ef159e55e285f89cd92403db724018f29f5d3cdc0c502da799d88df38f65eb1
```

Verify `RECEIPT.json`, then extract into a fresh scratch directory:

```text
tar -xzf <evidence>/capability-probes.tar.gz -C <fresh-scratch>
```

Drivers retain original absolute paths for provenance; change paths in scratch
copies only for a new checkout and use fresh output directories. Review commands
before running; reading this record is not a reason to repeat all probes.
`check` used existing source checking; runs used `voidc run <source> --no-opt`
and `voidc run <source>`; native probes used `voidc build <source> -o <scratch>`
and executed the result. Default automatic cache identities separate IR modes.
Existing `tests/text_encoding/crash.py` was armed to preserve exact `_crash.void`
inputs and provenance on an abnormal compiler/runner exit; none occurred.

No completed router, full test suite, Debug/strict sanitizer profile, real editor
UI, persistent-router fault injection/reload or Windows/macOS result is claimed.
Those engineering acceptance obligations are in [ROADMAP](../ROADMAP.md).
