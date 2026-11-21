# Automatic native-object cache policy

27 October 2026 engineering policy, updated by the 21 November 2026 G95
candidate. G95 awaits independent review.

`voidc run main.void` keeps optimized JIT as its default. After full source
analysis, a validated native-object hit skips typed lowering, LLVM lowering,
IR O2 and machine generation. A miss compiles with IR O2 and LLVM Default native
code generation, then uses the existing private `.void/cache/v1/` publication.

`--no-opt` continues to disable the IR O2 pipeline while retaining LLVM Default
native code generation. Its objects have a separate identity. `--no-cache`
bypasses persistent lookup and publication. Ordinary automatic cache use is
silent, preserving application output and existing warning diagnostics.

Compatibility disposition: **`--cache` is retained as the legacy no-IR-O2
cached-run alias**, including `Debug cache: <status>` on stderr. It still
selects no-IR-O2 regardless of option order, and still applies only to `run`.
It conflicts explicitly with `--no-cache`. Existing usages in cache process,
async, globals, native and JSON host tests were examined; the O2-bypass assertion
in `tests/cache/tests.cpp` is replaced by O2 separation/fresh-JIT hit coverage.
The `DebugCache` C++ name remains source-compatible; the policy now supports
both IR optimization modes. An ordinary `Jit` without a cache policy remains
uncached for embedding clients.

Identity includes the compiler/link-input build digest, Runtime ABI versions,
host triple/CPU/features, actual LLVM Default codegen/relocation/code model and
data layout, IR O2 policy, source emission policy, entry/root policy, complete
validated graph/source/import bytes and contracts, and explicit Runtime policy.
The new native-object identity domain cannot consume old Debug-only entries.
Private directory/file checks, no-follow opens, digest admission, isolated ORC
generations and atomic no-replace publication are unchanged. Custom host native
bindings disable persistence. Windows uses the existing safe unavailable path.

The cache stores executable objects, never application globals or heap state.
Each ordinary CLI process initializes fresh application state. Loaded metadata,
constructors, global initialization and Runtime admission follow the existing
cache loader. Corrupt or unsafe candidates are misses; publication does not
replace an existing winner. Removing such a candidate permits publication on a
later miss. Interrupted temporary files are ignored. Project directories own
independent caches.

G95 permits verified dynamic native plans, including managed raylib, while
retaining fresh native approval, provider verification and Runtime admission on
every run. Exact descriptor/module/target and closure/payload identities join
the existing object recipe. Temporary `.void/run` staging roots and live provider
addresses are never persistent authority. Eligibility requires complete image
identities or a managed payload containing both provider and adapter identities;
the existing loader verifies those contents on every admission. Unmanaged
conventional plans that only name files remain uncached, including after a
provider/adapter rebuild. Arbitrary host-linked C thunks also remain unavailable.
The first creation of the reserved root `.void` name no longer
invalidates an otherwise unchanged project selection; existing selections with
that name retain their recipe and real directory freshness checks remain exact.

The existing store retains ten recent entries with validated envelopes, using
directory locks, immutable publication and inode-checked eviction. Successful
materialization updates recency without changing object bytes or mtime. Open
readers and live generations remain usable after disk eviction. Corrupt/unsafe
objects, interrupted temporary files and unknown contents are preserved outside
the entry count. Interrupted or failed maintenance can defer pruning until a
later successful operation; no total-byte quota or recursive cleanup is added.

Compatible explicit native reloads and ordinary runs share these validated
dynamic objects with their own Runtime admission. `voidc run` has no automatic
source watcher or automatic hot-reload loop. See the
[G95 report](../../projects/G95_VOIDC_PERSISTENT_CACHE.md) for measurements,
qualification, storage rules and remaining limitations.
