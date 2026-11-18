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

The separate compiler resource `verified-packages/raylib-prepare.py`
assembles a checked native catalog. For a local review build, invoke it
with `--voidc`, `--clang`, `--header`, `--library`, `--license` and a fresh
`--managed-catalog` directory. Windows also needs `--import-library`.
Use `--sanitize` with the instrumented Linux Runtime. The supplied
header must match the pinned bytes from the library's `upstream/`
directory. Re-run the package and native verification tests when
changing preparation or the binding. Keep official release hashes
and their licenses with the verified inputs.
