# Organize an application and its libraries

A manifest project gives the compiler an explicit source inventory, application
identity and build settings. Create one as shown in [First program](https://github.com/Shmellyorc/Void.Language/wiki/First-program).

```sh
voidc new app --name Orbit
voidc check --project Orbit
voidc run --project Orbit
voidc build --project Orbit --profile release
```

To adopt existing sources, run `voidc init` in their directory. Review the generated
`void.project.toml`; it does not invent a library entry for an arbitrary source tree.
The [CLI guide](https://github.com/Shmellyorc/Void.Language/wiki/CLI-commands) explains selectors, profiles and entry overrides.

## Import a project module

For this directory layout:

```text
src/
  app.void
  greeting.void
```

Put the following complete module in `src/greeting.void`:

```void
func message() -> string:
    return "Hello from another module"
```

Use it from `src/app.void`:

```void
use "greeting.void" as greeting

func main():
    print(greeting.message())
```

Running the project prints `Hello from another module`. The quoted path is
relative to the importing source. A module file can expose functions without
providing a second application main. Keep each application with one admitted main;
manifest projects do not require its filename to be `main.void`.

## Know which source you are importing

| Import | Source |
| --- | --- |
| `use io as io` | Standard module shipped with the matching tools |
| `use "greeting.void" as greeting` | Importer-relative project source |
| `use lib/geometry` | Project-owned local library source |
| `use pkg/raylib as rl` | Declared and locked package source |

Public is the default for ordinary declarations. `private` is file-private for
ordinary module declarations; `internal` follows the owning library domain.
Type members have their own access rules. See [Functions](https://github.com/Shmellyorc/Void.Language/wiki/Functions) for calls
and [Generics and interfaces](https://github.com/Shmellyorc/Void.Language/wiki/Generics-and-interfaces) for reusable contracts.

Use `voidc new classlib --name geometry` for a separate source library. Generated
library manifests record their known `package.entry`. Add it to a consumer with
`voidc add geometry --dir ../geometry`, then import `pkg/geometry`.
[Packages and raylib](https://github.com/Shmellyorc/Void.Language/wiki/Packages-and-raylib) explains copies, locks and native consent.

Next: [CLI commands](https://github.com/Shmellyorc/Void.Language/wiki/CLI-commands).
Precise contracts: [Modules](https://github.com/Shmellyorc/Void.Language/blob/main/docs/MODULE_SYSTEM.md), [Local libraries](https://github.com/Shmellyorc/Void.Language/blob/main/docs/LOCAL_LIBRARIES.md).
