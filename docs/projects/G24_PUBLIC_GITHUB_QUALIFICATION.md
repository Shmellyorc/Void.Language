# G24 public GitHub qualification — 2026-11-13

**Result: FAILED at public source acquisition. Public-origin G24 acceptance remains
blocked.** The exact public pin was attempted on PikaOS Debug/Release/strict and
native Windows 10 Debug/Release. Every `voidc deps fetch` exited 1 with `VOID5006`:

```text
HTTPS Git transport exceeded its private storage limit.
```

No verified public snapshot, source/native lock, executable or D5 package was
created. Successful pre-push local HTTPS qualification is preserved separately;
it does not establish successful consumption of this public repository.

## Exact origin, pin and fresh consumers

- Origin: `https://github.com/Shmellyorc/Void.Language.git`
- Integrated commit: `6a7cb50b2d35023324053694275596c9a4a7b328`
- Package subdirectory: `verified-packages/raylib`
- Source package: `org.voidlang.raylib`, `0.1.0`
- Native requirement: `org.voidlang.raylib`, `6.0.0`, ABI `3`

Real `git ls-remote`, with TLS verification, advertised that exact SHA for both
HEAD and `refs/heads/main`. This confirms the public ref advertisement; it is not
a completed VOID object/snapshot verification.

Each fresh external consumer copied the existing `examples/raylib-consumer`
template and ran its unchanged configure script:

```sh
python3 configure.py --rev 6a7cb50b2d35023324053694275596c9a4a7b328 \
  --origin https://github.com/Shmellyorc/Void.Language.git
voidc deps fetch --project /absolute/fresh/consumer
```

There was no loopback origin, local Git substitution, copied dependency snapshot,
manual lock, pre-seeded package cache or fallback download. D3 performed the actual
public smart HTTPS fetch using its existing isolated transport.

Linux consumers are under
`/home/shmellyorc/VM/g24-public-github-20261113/linux-{debug,release,strict}/consumer`.
Windows consumers are under `C:/Users/a/VoidG24PublicGithub/{Debug,Release}/consumer`.
They are outside the compiler source checkout. Their manifests and failure logs
are retained in the evidence directory below.

## Native environments and results

| Compiler build | Public fetch | Resolve / check / normal and O2 JIT / AOT build / D5 creation | Native VS Code 1.141.0 |
| --- | --- | --- | --- |
| PikaOS 4 Debug | FAIL, exit 1, private storage limit | Blocked by absent snapshot/lock | Missing-lock diagnostic; Check and Package Project exit 1 |
| PikaOS 4 Release | Same failure | Same blocked stages | Same observed behavior |
| PikaOS 4 strict ASan/UBSan | Same failure, without a sanitizer report | Same blocked stages | No additional strict editor run; native Debug/Release editor runs above |
| Windows 10 Debug | Same failure | Same blocked stages | Missing-lock diagnostic; Check and Package Project exit 1 |
| Windows 10 Release | Same failure | Same blocked stages | Same observed behavior |

All compiler version receipts report LLVM `23.1.4`. Clang and LLD are pinned to
`23.1.4`. The Windows guest reports `Windows-10-10.0.19045-SP0` (its VM display name
is `Windows 11`). Windows commands ran through VS 2022 `VsDevCmd.bat -arch=x64
-host_arch=x64 -winsdk=10.0.19041.0`: VSCMD `17.14.42`, VCTools `14.44.35207`, with
both host and target architecture recorded as x64. Existing verified native
catalogs were supplied explicitly; no native preparation ran during Git fetch.

The strict attempt retained:

```text
ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
```

This task did not reach execution of a raylib consumer on any host. Compiler/editor
process execution must not be reported as successful raylib JIT/AOT execution.

## Reproduced cause and existing limits

The existing D3 implementation fetches and verifies the **whole pinned repository**.
`subdir` selects a package after acquisition; it does not restrict the fetched
objects or exported repository inventory. The transport is already depth one.

| Existing bound | Current contract | Integrated repository |
| --- | --- | --- |
| Private HTTPS transport storage | 96 MiB; 1,024 transport entries; 256 KiB diagnostics | All five fetches terminate in private-storage accounting |
| Exported snapshot inventory | 8,192 entries, including directories | 56,059 files before directories |
| Exported snapshot source | 64 MiB | 737,068,740 blob bytes, about 702.92 MiB |
| Selected raylib package alone | Subject to whole-repository acquisition above | 16 files, 1,648,932 bytes |

Repository/package sizes were calculated from the local integrated Git objects
with the same commit advertised by GitHub. They are expected commit identities,
not objects admitted by a successful public VOID fetch.

A repeated Linux Release fetch was monitored in a dedicated private temp root.
The sampler observed 100,597,870 stored bytes in five files, including one
100,597,759-byte file, immediately before the guard terminated acquisition. The
10 ms sampler does not capture every final write. The temp root was empty after
failure. Windows also left no `void-git-*` directories in its configured private
temp roots. No failed project published `.void/git-deps` or `void.lock`.

The limits are documented in the existing
[D3 HTTPS closeout](DEPENDENCY_D3_HTTPS_CLOSEOUT.md#transport-and-verification) and
implemented in [transport accounting](../../src/frontend/project_git_process.cpp)
and [snapshot acquisition](../../src/frontend/project_git.cpp). This is a
reproducible incompatibility between the published repository's size and the
existing whole-repository acquisition contract. Increasing only the transport
constant would still leave the inventory and source-byte bounds exceeded.

No production correction was made: completing acquisition here requires changing
the bounded acquisition/snapshot scope or changing what is published at the
source origin. Either exceeds this verification-only, no-architecture-change
mission. No bounds, TLS checks or verification rules were weakened, and no
alternate package infrastructure was introduced. Public acceptance requires that
acquisition incompatibility to be resolved and this exact workflow rerun.

## Source and lock identities

| Identity | Result |
| --- | --- |
| Public advertised revision | `6a7cb50b2d35023324053694275596c9a4a7b328` |
| Expected commit root tree, from local integrated objects | `3f946126d4b871642d703162442d05f8e8c355ad` |
| Expected package Git tree, from local integrated objects | `7f2f486a45a53c168468fbbcb4f1916bdcb61f15` |
| Expected package manifest SHA-256, from local integrated objects | `64cf6f24fcb31fa48f3b113da07ac6bd786307a1af68aed260219db2b4fad4da` |
| Expected generated API SHA-256, from local integrated objects | `44c1914da60921b330c192949a221b3ad6e0504cde4a0a595dd22e8968b7cbb9` |
| Verified public D3 snapshot digest | Unavailable: acquisition failed |
| Verified public package/source lock identity | Unavailable: no lock published |
| Consumer native lock identity | Unavailable: source resolution prevented lock creation |

For reproducibility, the supplied **local approved catalog** TOML SHA-256 values
are recorded below. These are catalog inputs, not successful public-consumer locks.
All catalogs identify native version `6.0.0`, ABI `3`, revision `raylib-6.0-api-v3`.
The machine receipt also retains descriptor and every runtime/link/notice identity.

| Catalog | TOML SHA-256 |
| --- | --- |
| Linux Debug/Release | `bc984b0435499059d44d68456778af25fb9a3c60ac966f4bb0ed1908df2a2c7a` |
| Linux strict | `48712df5f791e78f6b88ef10af9a71f01c2c5d795a53d39161f204dbd2ef33f0` |
| Windows Debug | `815b167040d4f3426fa0282f9a70e243cc21dc33ec5b44df76262f6e7407d74f` |
| Windows Release | `156a376c8a634e7bb1923d33ec7b132da841441838a92fd745750333f302549b` |

## Downstream commands and editor behavior

Every host additionally attempted the following on its failed fresh consumer:

| Requested stage | Observed result |
| --- | --- |
| `resolve` | Exit 1, `VOID5006`: missing Git dependency snapshot |
| `resolve --locked`, `deps explain --json` | Exit 1: missing `void.lock` |
| `check` / imports | Exit 1, `VOID5011`: missing `void.lock` |
| Normal and O2 `run --allow-native` | Exit 1: missing lock; application did not execute |
| Native `build --allow-native` | Exit 1: missing lock; no executable |
| `package --allow-native` | Exit 1: missing lock; no D5 output |
| Execute AOT; verify/relocate/execute D5 | Not run: no artifacts exist |

Native VS Code 1.141.0 was exercised on Linux Debug/Release and Windows
Debug/Release using the existing raylib editor fixture. The fixture's small
optional missing-snapshot branch records the actual blocked state:

- Missing-lock diagnostics remain visible in the manifest.
- Check and Package Project both start compiler tasks and exit 1.
- A `get_ray_collision_box` Text-kind completion candidate appears without a
  signature/detail. This is insufficient to establish a resolved package import;
  the receipt explicitly records `packageCompletionQualified: false`.
- No editor package is produced. Package Project's existing explicit native
  authorization behavior is unchanged.

Earlier UI harness attempts expected the later missing-snapshot diagnostic rather
than the earlier missing-lock diagnostic, and assumed no text completion candidate.
Those assertions were corrected to record the observed failure state. The initial
Windows evidence path also hit VS Code's UNC write restriction; final receipts
were written locally and copied back. Earlier logs remain preserved. These harness
corrections do not change the public-fetch result or qualify blocked stages.

## Retained native leak limitations

Existing independent external-leak findings remain explicit and unsuppressed:
256 bytes in Gallium after window shutdown; 408 bytes in raylib's WAV music context
and 64 bytes in its sound-alias converter residual. Earlier strict headless and
bounded callback ownership results remain separate from those failing full-device
paths. No new public-consumer graphics/audio check reached execution here, and no
device path is described as clean. See the
[implementation/qualification record](G23_G24_IMPLEMENTATION.md#strict-leak-classification).

## Evidence and worktree

The consolidated machine receipt, exact command arguments/exit codes, five fresh
consumers, native SDK/shell/catalog identities, editor JSON, cleanup measurement
and original logs are retained at:

```text
/home/shmellyorc/VM/g24-public-github-20261113/public-origin-receipt.json
/home/shmellyorc/VM/g24-public-github-20261113/
```

Production code is unchanged. This task changes only this report, the milestone
status and the optional failure-observation branch in
`editors/vscode/src/test/raylibHost.ts`. `git diff --check` passes; the index remains
empty and HEAD stays at the integrated SHA. No staging, commits, pushes or hosted
CI were performed.
