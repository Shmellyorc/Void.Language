# Private TOML parser for project manifests

Pinned toml++ 3.4.0, MIT licensed, from upstream commit
`30172438cee64926dc41fdd9c11fb3ba5b2ba9de`:
[upstream source](https://github.com/marzer/tomlplusplus/tree/30172438cee64926dc41fdd9c11fb3ba5b2ba9de).
`upstream.json` records exact upstream/header/license/patch SHA-256 values.

Only `src/frontend/project.cpp` includes this header. It uses memory parsing,
TOML 1.0, no parser exceptions, no formatters/unreleased extensions, eight nested
values and the local eight-segment key bound. No configure/build-time download
or runtime configuration hook exists. Public VOID headers expose no toml++ types.

`void-bounds-and-locations.patch` is the complete small diff from the pinned
upstream header. It bounds dotted/header keys before recursive table creation,
retains first-definition regions for duplicate diagnostics, anchors duplicate keys
at their second declaration, and rejects invalid key starters before an upstream
assertion/optimizer assumption. Qualification exposed that last issue with
`[=roject]`: Debug aborted and fatal UBSan rejected the assumption. The runtime
input guard fixes it; assertions and sanitizers stay enabled. This private adapter
is qualified with `TOML_EXCEPTIONS=0`; it is not a general replacement toml++ package.

`tests/project/provenance.py` verifies all hashes and reverses the patch in memory
to reconstruct the exact upstream bytes, without network access. This establishes
source consistency, not publisher authentication. Both installed compiler/tooling
components retain the MIT license, provenance and patch in
`share/void/licenses/tomlplusplus`, and the exact patched header and this record
in its `corresponding-source` directory. Run the same verifier with
`--installed <prefix>/share/void/licenses/tomlplusplus` to check an installed copy
and reconstruct the pinned upstream source from that copy without a checkout.
