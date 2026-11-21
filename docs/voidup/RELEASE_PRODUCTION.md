# Produce a VOIDUP release candidate

This is a release-engineering procedure, not a public download announcement.
The native manager and its authentication/transaction architecture already exist.
Do not replace them with a script installer. Current source still embeds **test
trust only**; the Linux producer is an engineering fixture producer. Public
publication, production trust deployment and native Windows/macOS installation
qualification have not been completed.

## Build the native manager

Reuse an admitted matching LLVM/Clang/LLD **23.1.4** build, HTTP providers, host
providers and their source/recipe receipts. The complete compiler must already
build on the chosen support-floor host. Reuse `tools/install.py` or the canonical
[contributor recipes](../install/README.md) to produce those internal inputs.
Python and CMake here are producer tools, never bootstrap runtime dependencies.

The existing native build target is sufficient:

```sh
cmake --build /absolute/matching/build --target voidc voidls voidup -j3
```

It invokes the matching native compiler on `apps/voidup/main.void` and emits
`BUILD/voidup/voidup` (`voidup.exe` on Windows). A direct equivalent is:

```sh
/absolute/matching/voidc build apps/voidup/main.void -o /absolute/fresh/output/voidup
```

Use the repository working directory and its matching standard sources. Do not
run the Windows or macOS result as a release claim without native qualification.
The source target itself needs no Python driver. To assemble the existing Linux
bootstrap prefix and its dependency notices, reuse:

```sh
python3 -B tools/build_voidup.py \
  --compiler /absolute/matching/build/voidc \
  --prefix /absolute/fresh/bootstrap-prefix \
  --licenses /absolute/matching/compiler-prefix/share/void/licenses
```

The helper refuses an existing output, compiles the VOID program, sets its
relative loader path, materializes shared-library aliases, and copies matching
notices. It now records compiler identity/hash, source and stdlib hashes, producer
host/libc, output file hashes and explicit nonproduction status in `build.json`.
Retain the complete prefix; shipping only the executable may lose shared libraries
and notices. This is a reproducible recipe with receipts, not proof of bit-for-bit
reproducibility or compatibility with an older OS. Build **inside** the selected
floor userspace; invoking a floor-built compiler on a newer host links newer CRT
objects. The current local PikaOS builds are not Ubuntu 24.04 release artifacts.

## Produce complete toolchains with the existing producer

`tools/package_voidup_fixtures.py` already composes the existing CMake `compiler`
and `tooling` components, materializes symlinks as regular files, includes the
Clang/LLD subset and Clang resources, rewrites native executable paths relative to
the version prefix, writes `toolchain.json`, and emits ZIPs plus exact inventories.
The native Runtime, host and HTTP providers, source receipts/notices, stdlib,
package definitions and VOID preparer accompany the compiler component.

Its current command is an **engineering-only** A/B qualification recipe:

```sh
python3 -B tools/package_voidup_fixtures.py \
  --build-a /absolute/floor/build-a --build-b /absolute/floor/build-b \
  --sdk /absolute/llvm-23.1.4 --output /absolute/private/engineering-fixtures
```

The output must contain the separately held existing engineering test keys at
`keys/offline-1.pem` and `offline-2.pem`; never print or archive them. The producer
checks the keys against the embedded public test roots, requires distinct actual
compiler build identities and produces a freshly signed catalog. Its report destination follows the engineering storage policy under
`~/Documents/Void.Language-archives/voidup-qualification/`; keep each new
qualification receipt separate from preserved runs.
Call its existing `produce(build, release_id, output, sdk)` function from a
task-owned harness and keep new receipts outside the checkout instead.

Before production, adapt this **same producer**, rather than adding another
release system, for reviewed release IDs, selected hosts, production signing key
identities and a nonhistorical receipt destination. The existing native inventory
schema and domain/signature format stay unchanged. Current source's fixed
`test-1`/`test-2`/`test-3` identities and private-trust diagnostics must become a
reviewed production trust build; changing only a download URL is insufficient.
This adaptation and the production key ceremony remain unperformed.

For each exact host payload, verify:

- `voidc --version` build identity agrees with signed catalog, inventory and
  `toolchain.json`; `voidls` comes from the same build.
- Native manifests and resource hashes resolve entirely within the retained
  version, except the declared OS C/C++ SDK/CRT/system libraries.
- All standard modules, compiler package inputs, native libraries, LLVM binaries
  and `sdk/lib/clang/23` resources and required notices/source are inventoried.
- Every file has the right executable classification. Private publication accepts
  neither symlinks nor hard links. Complete verification rejects extra files.
- The exact bootstrap and installed toolchain run on the stated ABI floor, with
  checkout/build/producer SDK unavailable. SDK packaging is not host portability.

Linux output remains the only implemented producer. Windows packaging must use
native x64 `/MT`, matching PE/runtime and SDK/resource identities; Apple packaging
must respect actual SDK/libc++ and ownership/platform restrictions. Existing
cross-compilation is not native acceptance. Existing redistribution obligations
in [licensing scope](../release/LICENSING_SCOPE.md) and
[the G29 payload audit](../release/G29_FIRST_RELEASE_READINESS.md) remain unresolved
where recorded; this candidate grants no release clearance.

## Production keys, catalogs and hosting

Use three independently controlled, locally managed Ed25519 keypairs and distinct
reviewed key IDs; retain the 2-of-3 threshold. Private keys stay offline in
owner-private storage, separate from checkout, CI, build contexts, bootstrap,
artifacts and evidence archives. Review/publicly authenticate the bootstrap's
public root keys through the release announcement and an independent trusted
channel before first use. TLS or a checksum downloaded beside an untrusted
bootstrap does not establish that initial trust. No paid certificate, developer
membership or commercial signing service is required by this scheme.

Sign **exact** catalog bytes after prefixing `VOIDUP CATALOG v1\n`. Use the
existing schema-1 envelope; no JSON reserialization after signing. Bind the exact
inventory byte count/hash, archive byte count/hash, release ID, host and immutable
compiler build identity. Keep a strictly increasing catalog serial, bounded
issued/expiry interval (at most 30 days), minimum-manager value and accumulated
revocations. Preserve prior signing/serial records offline. The native manager
retains accepted serial/digest/time/revocations before transfer; rollback must
never roll back that trust floor. Key rotation/root replacement still needs a
reviewed new bootstrap; there is no automatic root-update protocol.

The existing transport requires one explicitly chosen **HTTPS origin**, no query
or fragment, no redirects, and same-origin inventory/package references. The
signature envelope is fetched at `CATALOG_URL + ".sig"`. Publish the artifact and
inventory first, check their real responses/lengths/hashes, then atomically expose
the catalog and matching envelope using the existing origin's deployment process.
A transient catalog/envelope mismatch fails closed; retain the prior valid pair
for recovery. Never change bytes under an already published release ID.

Free existing infrastructure can host bytes, but **GitHub Releases browser asset
URLs redirect**, so they are not directly usable by the current no-redirect
transport. Workflow-artifact URLs also do not satisfy the catalog contract. No
VOID origin has been configured or verified to serve production bootstrap,
catalog, signature, inventories and large archives directly. Hosting that exact
contract is a remaining deployment dependency; do not silently loosen redirect,
origin, query, TLS or signature checks to make an arbitrary URL work.

## Qualify before publication

Use a fresh private root and 1 MiB hard/soft stacks. Exercise actual HTTPS,
threshold failures, inventory/artifact tampering, retained serials/revocations,
private modes, interrupted transfer and retry, default switching/rollback,
compiler versions, stdlib, project creation/JIT/native build and LSP handshake.
Verify `env` session/editor outputs and native package first-add with Python
absent from PATH. Check host development libraries separately from bundled SDK
contents. Run the exact native claims on each host; preserve all first failures.

The current assignment prepares a review candidate. Independent Golden Wrench
review precedes integration and publication. Boss retains Git, hosted CI, binary
installation/release and live Wiki publication authority. The Wiki export's
preimages and changed-file allowlist must be rechecked against the live remote
before the reviewed page update. No publication is performed by these commands.

Reference decisions:
[LLVM unique-directory permissions](https://llvm.org/doxygen/FileSystem_8h.html),
[GNU tar named members to stdout](https://www.gnu.org/software/tar/manual/html_node/Writing-to-Standard-Output.html),
[GitHub release asset response semantics](https://docs.github.com/en/rest/releases/assets#get-a-release-asset),
[Wiki Git publication](https://docs.github.com/en/communities/documenting-your-project-with-wikis/adding-or-editing-wiki-pages).
Upstream documentation informs implementation; the retained LLVM 23.1.4 source
headers and local native probes establish applicability to this candidate.
