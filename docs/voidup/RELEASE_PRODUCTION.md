# Produce and publish VOIDUP

G96 prepares the existing native manager for a public first-install route. It
reuses the existing compiler/tooling install components, package producer,
threshold format, inventory checks and dedicated transactional store. It adds
platform bootstrap glue, not another toolchain installer or authentication system.

**Production is not published.** Boss has not yet provisioned/approved the three
production public keys. No engineering key may be shipped as production trust.
See [production trust](PRODUCTION_TRUST.md) for the free offline ceremony and
[the production-readiness report](../projects/VOIDUP_PRODUCTION_READINESS.md) for
current native results, sizes and exact review evidence. The G96 report retains
its historical accepted implementation evidence.

## Native builds and trust profiles

Reuse an admitted matching LLVM/Clang/LLD **23.1.4** compiler, Runtime, HTTP and
host providers. Producer Python/CMake are build tools, never consumer dependencies.
Build on the actual support-floor OS; a floor-built compiler invoked on a newer
host can still link newer system startup/runtime objects.

The existing `voidup` CMake target builds the native manager. The consumer bundle
uses the same manager with a small C++ platform launcher:

```sh
python3 -B tools/build_voidup.py --compiler /matching/build/voidc \
  --licenses /matching/compiler-prefix/share/void/licenses \
  --trust-config release/voidup/production-public-keys.json \
  --prefix /fresh/bootstrap-prefix
```

The configuration contains public roots only. Production configuration requires
the official catalog and refuses the known engineering roots and `test-*` IDs.
The builder creates an isolated source overlay with enrolled IDs/public bytes;
it does not change checked-in trust, the signature domain or the 2-of-3 threshold.
Without the configuration it remains a private engineering build and its built-in
production source fails closed. Test profiles may embed an isolated HTTPS origin.

The platform launcher delegates setup and all proxy execution to native VOIDUP.
After a verified installation it retains the bootstrap and exposes stable tools;
Unix integration uses held directory descriptors and appends quoted profile
entries, while the Windows branch prepares user-registry PATH integration and
notification. Toolchain admission, immutable publication, trust-floor retention,
locking, verification and rollback remain in `apps/voidup/*.void`. Ordinary
directory/profile access follows the OS permission policy, including shared
locations. Profile replacement preserves owner/group identities, mode bits and access ACLs without adding
inherited grants that were absent from the original. Native path/type/identity
checks remain; signing-key storage keeps its explicit confidentiality contract.

## Downloadable bootstrap packages

```sh
python3 -B tools/voidup_release.py bootstrap --production \
  --prefix /fresh/bootstrap-prefix --host linux-x86_64 \
  --output /fresh/assets/voidup-linux-x86_64.run
```

Native Windows uses `--host windows-x86_64` and a `.zip` output. Its ZIP contains
`Install VOID.cmd` and the native binaries. Linux's `.run` is a small extraction
wrapper invoking the native launcher; it pins standard extraction utilities and
cleans its exclusively created temporary directory. It does not verify or install
a toolchain itself. A JSON receipt records exact compressed/expanded bytes and
payload hashes. Engineering bundles cannot pass `--production`, and a producer
cannot relabel a Linux build as Windows/macOS.

The executable is not assumed standalone. Keep its required shared-library aliases
as ordinary files and preserve matching dependency notices/source material. Record
ELF/PE dependencies on the declared native host. macOS engineering recipes relocate the admitted dependency closure and resolve
the consumer's selected Apple SDK through `xcrun`. They remain natively
**UNQUALIFIED** and excluded from public production preparation/assembly. A ZIP
recipe or ad-hoc signature is not Gatekeeper/notarization acceptance.

Archive metadata uses `SOURCE_DATE_EPOCH`, defaulting to 1980-01-01 UTC, with
normalized ownership/modes and sorted members. Identical admitted build inputs
produce identical archives. This does not claim bit-identical independent
compiler builds. Bootstrap packaging checks the complete runnable file set against
its build receipt. Package receipts record installed bytes and native dependencies.

## Complete toolchains and signed stable metadata

`tools/package_voidup_fixtures.py:produce` remains the single package producer.
G96 adds native Windows executable classification/SDK names while retaining the
Linux recipe, install components, materialized aliases, SDK resources, immutable
build identity, exact inventories and ZIP publication. The release entry point is:

```sh
python3 -B tools/voidup_release.py package --build /matching/Release/build \
  --sdk /matching/llvm-23.1.4 --version 0.1.0 \
  --release-id void-0.1.0-linux-x86_64 --output /fresh/release
```

Windows uses a native Windows build and an appropriate ID. No cross-produced file
is native qualification. macOS ARM64 engineering toolchains use `ld64.lld`, retain platform identity
`macos-aarch64-14.0`, relocate non-system Mach-O dependencies and remove producer
SDK/header paths from the native manifest. Actual native macOS 14.0 execution
and distribution acceptance are still required before claiming that support floor.

Each new release descriptor adds an authenticated optional `selection` object:

```json
{"selection":{"version":"0.1.0","channel":"stable"}}
```

Its two fields are required when the object is present. Old schema-1 catalogs and
retained installation records lack the object: they remain verifiable/selectable
by explicit ID and can be rolled back, but are never assigned an automatic version
by guessing from their ID. Published IDs/content/classification stay immutable.
Automatic selection uses numeric three-part version order, exact host, stable
channel and retained revocations. New catalogs require manager version 2.

The offline [trust procedure](PRODUCTION_TRUST.md) prepares an increasing-serial
catalog, carries authenticated previous revocations, collects independent
endorsements and combines the threshold envelope over the exact bytes. Do not
put private keys or signing secrets into CI. Keep expiry at most 30 days and
refresh the channel before expiry. Manager downgrade/key rotation is not an
automatic rollback feature; toolchain rollback stays within the current manager.

`--valid-days` measures the interval from the catalog's `issued` timestamp,
including its 60-second clock allowance. The 30-day boundary therefore fits the
same maximum interval enforced by the native manager and publication verifier.

## GitHub hosting without weakening authentication

The built-in official source is the repository's `voidup-stable` release asset
`catalog.json`, with the signature envelope at `catalog.json.sig`. That endpoint
is configured in source; it is **not a published-download claim**.

Payloads and inventories use immutable versioned GitHub Release assets. Ordinary
private origins retain same-origin HTTPS and no redirects. For the exact official
repository release-asset path, the native transport inspects a HEAD response and
permits only a single HTTPS handoff to
`release-assets.githubusercontent.com/github-production-release-asset/…` on port
443. Short-lived CDN queries are permitted only on that validated handoff; userinfo,
fragments, downgrade, arbitrary hosts/ports and second redirects remain rejected.

The final GET/download still disables automatic redirects. Existing bounded
get/download code checks status, raw representation and exact counts; catalog
signatures and all signed hashes/inventories remain authoritative after transport.
Normal metadata/connect/header/read budgets remain 30 seconds. A normal package
transfer has a bounded one-hour total budget so a large SDK does not require an
expert timeout flag. An explicit `--timeout-ms` overrides both budgets for tests.

[GitHub documents](https://docs.github.com/en/rest/releases/assets#get-a-release-asset)
streaming or redirecting release assets. A read-only HEAD against the existing
public pinned LLVM asset independently confirmed the specific CDN host/path shape;
it was not a VOID production-channel demonstration. Windows user-environment
notification follows [Microsoft's documented WM_SETTINGCHANGE route](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-settingchange).

## Prepared manual workflows

1. `voidup-release.yml` reuses `tools/ci/install_smoke.py` to build matching native
   inputs, verify the installed compiler/Runtime/stdlib/LSP, and run focused VOIDUP
   first-install/policy checks with ephemeral in-memory engineering keys. Linux and
   Windows prepare complete payloads only after their native checks. The Mac job
   attempts the prepared engineering first-install recipe; it produces no public
   payload. A workflow file is not evidence of native execution.
2. Production preparation is opt-in and requires the approved public configuration.
   It emits unsigned version payloads, inventories, bootstrap packages, hashes and
   native qualification receipts. It neither signs with private keys nor publishes.
3. Custodians review those exact bytes and source revision offline and return the
   signed catalog/envelope. Integrating public enrollment/metadata requires Boss's
   separate Git authorization.
4. `voidup-publish.yml` is a separate manually authorized operation, disabled unless
   its explicit `publish` input is true. It admits a successful preparation run,
   assembles qualified assets, checks the production public-root fingerprint,
   threshold/freshness and exact payload hashes, refuses an existing immutable
   version tag, and publishes version assets at the prepared source revision.
   It uploads the stable catalog/envelope **last**. A transient pair mismatch fails
   closed; it is never a reason to bypass signatures.

No workflow was dispatched for G96 local qualification. Native CI execution,
approved public roots, exact support-floor payload validation, distribution
notices/source review, initial bootstrap trust announcement and Boss publication
approval remain real pre-publication work. See the report for each platform's
actual status. The workflow is not evidence that those jobs have run.

## Publication inputs and immutable hosting

Preparation receipts bind the exact assets to the source SHA and public-root
fingerprint. Assembly requires `--source-sha` for the admitted preparation run;
missing, changed or extra manifest entries are refused before output creation.
Mac production assembly remains disabled pending native distribution acceptance.

Under separately authorized hosting setup, create the mutable `voidup-stable`
release **before** enabling GitHub immutable releases. Immutable version releases
are composed as drafts with all assets attached, then published. The publisher
requires an existing mutable stable channel and serializes channel updates. It
authenticates the previous catalog pair, rejects serial/revocation regression and
release-identity rewrites, then uploads the new matching pair last. An incomplete
existing pair requires operator recovery rather than treating it as a first release.

See [GitHub's immutable release guidance](https://docs.github.com/en/code-security/concepts/supply-chain-security/immutable-releases).
No hosting setting or channel was changed during engineering. Website links stay
unavailable until the exact native artifacts, public roots and publication are
approved. Use versioned asset links for the downloadable bootstrap; its embedded
authenticated stable catalog selects the toolchain independently.
