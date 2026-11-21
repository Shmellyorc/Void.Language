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

The separate compiler resource `verified-packages/raylib-prepare.void`
assembles the consumer catalog through native VOID tooling. `voidc add raylib`
stages and authenticates it with the bundled source snapshot before hosting it.
It uses native HTTP, SHA-256, ZIP and process APIs; Linux tar.gz decoding uses
`/usr/bin/tar` with bounded stdout after full asset authentication.

The retained internal maintenance helper `verified-packages/raylib-prepare.py`
can prepare custom qualification inputs. For a local review build, invoke it
with `--voidc`, `--clang`, `--header`, `--library`, `--license` and a fresh
`--managed-catalog` directory. Windows also needs `--import-library`.
Use `--sanitize` with the instrumented Linux Runtime. The supplied
header must match the pinned bytes from the library's `upstream/`
directory. Re-run the package and native verification tests when
changing preparation or the binding. Keep official release hashes
and their licenses with the verified inputs.


G77 source-only `try_` companions remain in `src/raylib.void`; native generation
never overwrites that public module. `public_docs.verify_fallible_api` requires
all 34 companions, their existing ordered parameters, real `result<t, load_error>`
returns, ownership/result docs and documented error declarations. It preserves
the 757-function original coverage. `tests/project/raylib_documentation.py
--docs-only` also injects four broken-companion documentation cases alongside
the twenty historical meaning errors. Native acceptance remains the 600 pinned
C APIs; source companions are not counted as new native ABI declarations.

## Canonical numeric units and value defaults

Public snake_case keeps `2D` and `3D` together: `Camera3D` becomes `camera_3d`,
`BeginMode2D` becomes `begin_mode_2d`, and `DrawTriangleStrip3D` becomes
`draw_triangle_strip_3d`. Trailing numbers alone stay attached (`vector3`,
`compute_sha256`, `load_utf8`). `names.py` separates public spelling from the
original `abi_snake` mapping. Native record/binding identifiers and C symbols
retain their published bytes.

Only the 14 canonical public spellings exist. `NUMERIC_PUBLIC_NAMES` records
retired names solely so verification rejects their reintroduction. The physical
public module contains one implementation per operation, calling the unchanged
native binding directly. There are no compatibility wrappers or duplicate
constructors. The generated inventory keeps its native `void_name` identity and
adds `public_name` where needed; it no longer emits `compatibility_aliases`.
Regeneration verifies canonical public declarations and rejects retired names.

Source helpers provide pure value defaults for vectors, identity matrices,
opaque colors, empty rectangles, cameras, identity transforms, rays, no-hit
collisions and zero-sized boxes. Patch source rectangles and bone names remain
required, while borders/layout and root-parent values default. Hardware VR
calibration and event-specific automation inputs remain required. The package
README and physical `##` comments specify the actual values. No native owner or
compiler construction semantics change.

Maintained documentation, native naming and IntelliSense checks target canonical
names only and check retired-name absence. The final cleanup does **not** rerun
these checks, compiler suites, editor tests or benchmarks, at Boss direction;
previous results remain historical evidence rather than final-cleanup acceptance.
