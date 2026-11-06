# Find a command

_Draft for Boss approval; repository documentation is canonical._

```sh
voidc --help
voidc --version
voidc check --help
voidc run --help
voidc build --help
voidc emit --help
```

| Goal | Command |
| --- | --- |
| Diagnose source without running startup | `voidc check main.void` |
| Run the current main application | `voidc run .` |
| Link a native executable | `voidc build . -o hello` |
| Emit an object for manual linking | `voidc emit main.void -o main.o` |
| Run a legacy numeric example | `voidc run calculation.void 10` |
| Run without O2 or disk caching | `voidc run . --no-opt --no-cache` |

`--cache` selects Debug code and reports cache status. It conflicts with
`--no-cache`. Unknown/duplicate options and `--app` reject with status 2.
Status 1 indicates a compiler/toolchain failure; applications can themselves
return nonzero statuses. Quote paths with spaces and use `--` before dash-leading
source paths. Read the [canonical command reference](https://github.com/Shmellyorc/Void.Language/blob/main/docs/CLI.md) for the complete contract.
