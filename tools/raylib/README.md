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


G77 source-only `try_` companions remain in `src/raylib.void`; native generation
never overwrites that public module. `public_docs.verify_fallible_api` requires
all 34 companions, their existing ordered parameters, real `result<t, load_error>`
returns, ownership/result docs and documented error declarations. It preserves
the 757-function original coverage. `tests/project/raylib_documentation.py
--docs-only` also injects four broken-companion documentation cases alongside
the twenty historical meaning errors. Native acceptance remains the 600 pinned
C APIs; source companions are not counted as new native ABI declarations.

## Numeric units and compatibility

Public snake_case keeps units such as `2D` and `3D` together: `Camera3D`
becomes `camera_3d`, `BeginMode2D` becomes `begin_mode_2d`, and
`DrawTriangleStrip3D` becomes `draw_triangle_strip_3d`. A trailing number
without a letter stays attached (`vector3`, `compute_sha256`, `load_utf8`).
`names.py` separates public spelling from the original `abi_snake` mapping.
Native record/binding identifiers and C symbols retain their published bytes.

The 14 preferred functions forward to the existing public implementations.
Original spellings remain documented compatibility functions. The generated
inventory retains its existing `void_name` native identity and adds `public_name`
and `compatibility_aliases` for affected entries. Regeneration checks both public
forms and their signatures; it does not regenerate the physical VOID source.
Camera helpers in both spellings accept named/default arguments. Their defaults
are pure native value construction, with no compiler or native ownership changes.

The documentation-only test includes naming and compatibility mutations. For
native construction/ABI checks, supply an already-prepared, explicit catalog:

```sh
python3 tests/project/raylib_naming.py /path/to/voidc /path/to/catalog /fresh/evidence
```

`--graphics` additionally exercises all preferred/old drawing spellings with a
real hidden window; it requires a native display. Keep fatal sanitizer policies
when running the headless cases. Known upstream device leak limits still apply.
