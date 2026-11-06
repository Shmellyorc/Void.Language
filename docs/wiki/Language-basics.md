# Write ordinary code

_Draft for Boss approval; repository documentation is canonical._

Indentation forms blocks. `var` introduces mutable storage, `const` an immutable
binding, and function signatures state parameter/result types. This example adds
the odd numbers up to a limit:

```void
func entry(limit: i32) -> i32:
    var total = 0
    for number in [1, 3, 5, 7, 9]:
        if number > limit:
            break
        total += number
    return total
```

Save as `basics.void`, then run:

```sh
voidc check basics.void
voidc run basics.void 5
# 9
```

The numeric `entry` adapter prints the result and exits zero.

With input 5, the loop adds 1 + 3 + 5. `break` stops before 7. Imports use
`use console` for standard sources, quoted importer-relative paths for project
modules, and `use lib/...` for local libraries. Keep imports and entry selection
explicit; directory discovery is not recursive package discovery.

Canonical details: [modules](https://github.com/Shmellyorc/Void.Language/blob/main/docs/MODULE_SYSTEM.md), [syntax conveniences](https://github.com/Shmellyorc/Void.Language/blob/main/docs/STAGE8_ERGONOMICS.md), [control flow and advanced features](https://github.com/Shmellyorc/Void.Language/blob/main/docs/ADVANCED_LANGUAGE.md).
