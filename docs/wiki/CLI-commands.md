# Use the command line with confidence

The core development loop is short. From a
[manifest application](https://github.com/Shmellyorc/Void.Language/wiki/Projects-and-modules):

```sh
voidc check .
voidc run .
voidc build . --profile release
```

`check` analyzes source without startup effects. `run` executes through the JIT.
`build` creates a native executable. See [Native compilation](https://github.com/Shmellyorc/Void.Language/wiki/Native-compilation)
for output paths and linking resources.

## Ask the installed compiler

```sh
voidc --version
voidc --help
voidc check --help
voidc run --help
voidc build --help
voidc new --help
voidc add --help
```

Help describes the actual tool you selected. Keep `voidc`, `voidls` and their
standard sources from the same build, as described in [Installation](https://github.com/Shmellyorc/Void.Language/wiki/Installation).

## Choose a project and profile

| Task | Example |
| --- | --- |
| Create an application | `voidc new app --name MyGame` |
| Create a library | `voidc new classlib --name geometry` |
| Add a manifest to existing source | `voidc init .` |
| Check a selected project | `voidc check --project MyGame` |
| Run Debug source without disk caching | `voidc run --project MyGame --profile debug --no-cache` |
| Build a Release executable | `voidc build --project MyGame --profile release -o game` |
| Check one standalone file | `voidc check calculation.void` |
| Disable optimization | `voidc run . --no-opt --no-cache` |

Profiles are project-only. Bare explicit files use their ordinary options and
reject `--profile`. Manifest projects discover main; manifestless directories
select exact `main.void`. Explicit `--entry` overrides follow the
[canonical selector rules](https://github.com/Shmellyorc/Void.Language/blob/main/docs/CLI.md).

O2 is the ordinary default. `--no-opt` disables optimization. `--no-cache` changes
cache persistence, not optimization. `run --cache` selects compatible Debug code
and conflicts with `--no-cache`; do not combine it with an explicit incompatible
profile. Quote paths containing spaces. Use `--` before dash-leading source paths,
and before application arguments to `run`.

## Maintain dependencies

```sh
voidc package list
voidc add raylib
voidc tidy --check
voidc deps explain --project .
```

These are different operations: list known packages, add a project dependency,
check consistency and inspect a resolved graph. `tidy --check` does not silently
upgrade Git pins. For editable copies, removals and native approval, follow
[Packages and raylib](https://github.com/Shmellyorc/Void.Language/wiki/Packages-and-raylib) and the
[complete command reference](https://github.com/Shmellyorc/Void.Language/blob/main/docs/CLI.md).

## Interpret output and status

Zero means the compiler command succeeded; malformed usage usually returns 2,
and compilation/toolchain failures return 1. An application's own status is
propagated by `run` and can also be 1 or 2, so read the diagnostic rather than
inferring its source from the number alone. The legacy `entry(i32) -> i32` adapter
prints a numeric result and exits zero; it is different from application `main`.

The removed `--app` flag is rejected. Async entries require an
[explicit host](https://github.com/Shmellyorc/Void.Language/wiki/Async#check-and-execute-the-example). For a failure, start with
[Troubleshooting](https://github.com/Shmellyorc/Void.Language/wiki/Troubleshooting).

Next: [Editor setup](https://github.com/Shmellyorc/Void.Language/wiki/Editor-setup).
