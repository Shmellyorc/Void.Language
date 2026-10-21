# Stage 8 Sprint 4 — application entry and trusted static providers

> Superseded CLI/authorization status, 21 October 2026: Boss authorized all three
> Stage 9 milestones and removed `--app` entirely, including compatibility use.
> [Current workflow](STAGE9_APPLICATION_WORKFLOW.md) and
> [acceptance](stage9/ACCEPTANCE.md) supersede conflicting proposals/status below.
> Historical decisions, experiments and verification records remain intact.


Status: implemented and locally verified; Wrench review pending; not hosted locked.
Boss approved all three design checkpoints on 20 October 2026; see
[DECISIONS.md](DECISIONS.md). Sprints 5–6 remain future work.

## Entry and hosting contract

Application invocation is explicit: `voidc run <file-or-directory> --app` or
`voidc emit <file-or-directory> -o <object> --app`. Both support `--no-opt`;
Debug JIT retains `--cache`. Applications accept no numeric operand and do not
print their returned status. Compile/runtime failure exits 1; invalid invocation
exits 2. Explicit i32 statuses follow platform process-exit rules. `voidc` reports caught compile/run failures
on stderr; linked AOT retains the existing adapter's checked-failure diagnostics
on stdout. Neither prints a successful application status.

Executable top-level statements in the selected source run in source order in a
compiler-owned body. Top-level variables are locals, unavailable to ordinary
functions. Ordinary module constants keep their existing semantics. Fallthrough
or bare return produces 0; an i32 return supplies status. Other return types
reject. Imported executable top-level statements reject without running them.

Alternatively, a zero-argument synchronous `main()` returns no result (status 0)
or i32. Explicit `main() -> i32` must return on every reachable path; its ordinary
return checker supplies no implicit zero. Eligible main and executable top-level
statements compete and reject. No arbitrary function scan occurs.

A declaration-only program with `entry(i32)` retains legacy precedence, including
an ordinary main helper. Legacy run/emit still accept their numeric input, print
the result and exit 0 on success. Applications require explicit `--app`; existing
legacy commands never silently select an application. Exact-case project
`main.void` discovery and explicit alternate files retain Sprint 1's path rules.

The compiler lowers application entries to an internal i32(i32) adapter, reusing
normal source calls, typed IR validation, LLVM JIT/AOT, precise roots, checked
failures and Runtime cleanup. Entry selection participates in analysis, cache
and reload identity. Editor analysis supplies no execution authority. Compiler-owned body and adapter
names do not appear as written declarations or editor rename targets; ordinary
top-level local bindings retain compiler-validated navigation and rename.

## Bounded static-provider contract

`native_fixture_project()` retains the original fixture. `native_sink_project()`
explicitly admits `void.sink.v1` at `/native_sink.void` by default, using its own
compiled catalog. Descriptors require exact names, symbols, parameter/result
shapes and ownership/callback flags. A fixture symbol cannot be admitted by a
sink descriptor. CLI and LSP imports alone cannot grant either provider trust.

The disposable sink supports i32 `echo`, owned UTF-8 `copy`, `acquire(bool)`
returning nullable opaque `resource`, `retain`, byte readback, `release_text`,
`close`, idempotent cleanup `destroy`, and one-shot `register`/`invoke`/`unregister`.
Embedded NUL bytes survive length-aware transfer. No foreign-thread callback API
is added to the sink catalog. Resources enforce acquisition-thread authority;
provider identities cannot cross the shared Runtime registry.

Both compiled providers use the same private transport, resource registry,
callback admission/drain, roots and generation leases. Each C implementation owns
independent opaque state. Resources cannot be constructed, extended, inspected or
transferred through async boundaries by source code. Callback registration keeps
existing direct-function/literal-closure provenance rules and worker restrictions.
Explicit close and Runtime shutdown quiesce callbacks before releasing provider
state, roots or code leases. A closed resource is destroyed once.

This remains one explicitly admitted descriptor per source graph, with two
available static catalogs. It is not multi-provider package discovery. Dynamic
libraries, raw pointers, a public FFI DSL, new worker privileges, general argv,
async main and top-level await remain outside Sprint 4.

## Verification

[Sprint 4 acceptance](stage8/SPRINT4_ACCEPTANCE.md) records 148/148 each in
Debug, Release and strict sanitizers (444/444 final unique outcomes), both 1 MiB
frontend gates, 163 process checks per configuration and retained preliminary
failures/corrections. No rerun inflates the unique suite count.
No Windows/macOS execution, actual editor client verification or hosted lock can
be inferred from Linux local tests. The seven-job hosted gate remains deferred
until the end of Stage 8 unless Boss explicitly authorizes an earlier run.
