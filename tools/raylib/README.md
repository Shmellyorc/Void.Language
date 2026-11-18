# raylib binding maintenance

This directory contains the Clang-based generator, adapter definitions and
API inventory used to verify the bundled raylib library. Run the following
from a VOID checkout with the pinned Clang 23.1.4 toolchain on PATH.

```sh
python3 tools/raylib/generate.py --check --require-complete --clang clang
```

Omit `--check` to regenerate the C wrapper, descriptor include, parity
header and `inventory.json`. Review both the generated changes and the
physical VOID declarations under `verified-packages/raylib/src/`. The
generator checks the public VOID API names and leaves the VOID files for
manual review. `inventory.json` associates each upstream declaration with
its public VOID name, exposure category and ownership contract.

`public_docs.py` also checks every public function's description, ordered
`@param` tags and applicable `@return` tag. Raylib parameter labels follow
the pinned Clang declarations in snake_case. A name colliding with a public
declaration receives `_value`, and the reserved word `event` becomes
`event_value`. Explicit mappings cover the owned window/device terminals,
font-atlas output removal, previous-codepoint cursor and retained callbacks.
The pinned header's actual point labels `p1` through `p4` are retained.

The generator preserves hand-maintained VOID implementations and documentation.
Both regeneration and `--check` reject missing documentation or stale public
parameter names. Review the wrapper body and its native adapter when changing
an adapted signature, especially output values, retained views and transfers.
The checks also reject known documentation mistakes involving boolean queries,
copied static results, texture filters, font color keys, grid spacing, vertex
colors and numeric counts. These targeted guards do not replace a review
against the pinned header and the VOID ownership adapters. Parameter names
alone cannot determine units, return meanings or resource consumption.

Run the documentation-only regression without building the compiler:

```sh
python3 tests/project/raylib_documentation.py --docs-only
```

The separate compiler resource `verified-packages/raylib-prepare.py`
assembles a checked native catalog. For a local review build, invoke it
with `--voidc`, `--clang`, `--header`, `--library`, `--license` and a fresh
`--managed-catalog` directory. Windows also needs `--import-library`.
Use `--sanitize` with the instrumented Linux Runtime. The supplied
header must match the pinned bytes from the library's `upstream/`
directory. Re-run the package and native verification tests when
changing preparation or the binding. Keep official release hashes
and their licenses with the verified inputs.
