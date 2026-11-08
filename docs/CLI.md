# voidc command reference

## Future project commands, 8 November 2026

The [project roadmap](PROJECT_ARCHITECTURE_ROADMAP.md) proposes `voidc init` and
`voidc new`, manifest-aware check/run/build, profile selection and eventual
resolve/package workflows. These are not current commands or accepted option
syntax. The approved entry direction requires no filename convention or mandatory
entry field: one valid main is selected by compiler analysis; ambiguity diagnoses.
The current command forms and `main.void` directory behavior below remain the
implemented baseline. Explicit source paths and standalone/top-level/legacy
execution must remain compatible. Init/new perform no implicit dependency install.

## Implemented command contract

This is the baseline public command contract. Its help/version output and
conventional `--` operand delimiter retain the existing source, entry, JIT/cache
and native-linking behavior. The project redesign above remains future work.

```text
voidc --help | -h
voidc --version
voidc help [check|run|build|emit]
voidc <command> --help | -h
voidc check <file-or-directory> [--no-opt]
voidc run [file-or-directory] [legacy-i32-input] [--no-opt] [--no-cache | --cache]
voidc build [file-or-directory] [-o executable] [--no-opt]
voidc emit <file-or-directory> -o <object> [--no-opt]
```

| Command | Result | Example |
| --- | --- | --- |
| `check` | Analyze source and imports without executing startup. An entry is not required. | `voidc check main.void` |
| `run` | JIT-execute the selected entry. | `voidc run .` |
| `build` | Compile/link a native application with matching Runtime/resources. | `voidc build . -o "output folder/hello"` |
| `emit` | Write a host-native object for manual linking. | `voidc emit main.void -o main.o` |

Run/build default to the current directory. Every directory argument selects its
exact `main.void`; explicit file paths may use other names. Options can occur
before or after operands. Quote paths with spaces. For a source filename starting
with a dash, use `voidc check -- -example.void` or `./-example.void`. The delimiter
ends option parsing; put `-o` and other options before it. Empty and malformed
invocations fail visibly. Duplicate/unknown options reject. Help must be its own
invocation (`voidc run --help`); it does not suppress invalid trailing arguments.
`--version` is global and reports the development project version, LLVM version,
SDK default target and whole-compiler build identity, not a release support promise.
Help starts with the same compiler version, followed by LLVM, usage, commands
and options. Version reporting returns before standard-source discovery and needs
no source project, installed stdlib or native resource manifest.

## Entries, input and exit status

Application entry selection is automatic. Supported `main` or executable top-level
code runs as an application; globals initialize before entry. Applications take
no numeric CLI input. `--app` is removed and always rejected, including alongside
help. Existing legacy `entry(i32) -> i32` selection/precedence remains supported:
`voidc run calculation.void 10` accepts one signed decimal i32, prints its returned
value and exits zero. Its default input is zero. For cwd input, spell the path:
`voidc run . 10`. Application process status and printed numeric result differ.

| Status | Meaning |
| --- | --- |
| 0 | Successful command/help/version; successful legacy numeric adapter. |
| 1 | Compilation, source/discovery, output or toolchain failure. |
| 2 | Invalid CLI invocation. |
| Application status | `run` propagates the application's return value; OS process-status limits apply. |

An application can itself return 1 or 2; those numbers alone do not identify a
compiler failure. Native applications retain their existing argument/status policy.

## Optimization and cache

O2 remains the default. `--no-opt` disables it. `run --cache` selects Debug code
and prints cache status; `--no-cache` disables disk caching without disabling O2.
The two cache options conflict and are available only for run. Eligible Debug JIT
runs use the existing automatic source-root cache, with its existing safe
unavailable fallback. `check --no-opt` remains a compatibility no-op. Other
non-run commands reject cache options. Cache policy and identity rules are unchanged. Rebuilding the changed driver
changes the whole-compiler fingerprint normally; old compiled cache entries are
not promised to survive that rebuild.

## Native output and installation

`build` defaults to `<entry-directory>/build/<entry-stem>` with `.exe` on Windows.
`-o` is caller-relative or absolute; absent parent directories are created.
Output/source collisions, `.void` outputs and non-file destinations reject; failed
builds preserve an existing good executable. Emit does not link; use `.o` on Unix
or `.obj` on Windows, and supply matching Runtime/dependencies when linking by hand.

Native builds use `void-native.txt` and `void-native-resources.txt` beside the actual
compiler, installed archives under `lib/void` and the original Clang/LLD **23.1.4**
SDK. They do not search the caller's cwd for replacement manifests. Resource hashes
and tool versions are checked. Missing resources include a reinstall hint; linker
failure reports identify the driver and point to host development prerequisites.
See [installation](install/README.md), [troubleshooting](TROUBLESHOOTING.md),
[legacy source contract](SOURCE_COMPILER.md) and [native workflow](STAGE9_APPLICATION_WORKFLOW.md).
