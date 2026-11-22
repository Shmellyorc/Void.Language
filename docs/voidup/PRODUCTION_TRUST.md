# VOIDUP production key enrollment and offline signing

G96 preparation for Boss review. **No production keys or key IDs are approved or
provisioned by this candidate.** Engineering test roots must never enter a public
installer. No private key or signing secret belongs in Git, CI, a toolchain,
bootstrap, VM transfer, engineering evidence or an evidence archive.

The existing signature domain is unchanged: exact catalog bytes prefixed with
`VOIDUP CATALOG v1\n`, Ed25519, two distinct valid endorsements from three enrolled
public roots. TLS authenticates transport; it does not replace this threshold.

## Free, locally managed ceremony

Choose three independent custodians/storage locations. Each custodian uses their
own offline POSIX machine or private offline media and runs the existing Python
producer environment with `cryptography` available. Python is an offline producer
dependency, never a consumer installer dependency. No paid signing service,
certificate subscription or commercial key manager is required.

Boss first chooses the three key IDs. The variables below are operator-supplied
values, not approved IDs or paths in this repository. Each custodian runs once:

```sh
python3 -B tools/voidup_release.py keygen \
  --key-id "$KEY_ID" --directory "$OFFLINE_KEY_DIRECTORY"
```

The command requires a new directory outside checkout/evidence, rejects linked or
shared-writable ancestors, creates it as 0700, and exclusively creates a 0600
`private.pem` and public-only `public.json`. Keep private media offline with an
independent protected backup. The PEM is not passphrase-encrypted by this command;
protect the offline medium accordingly. Native Windows private-key storage is
not claimed: its DACL ceremony requires separate qualification.

Transfer **only** each custodian's `public.json`. Compare IDs and full public-key
fingerprints over an independent trusted channel. Do not centralize the three
private keys to simplify release signing.

Prepare a public-only enrollment candidate:

```sh
python3 -B tools/voidup_release.py enroll \
  --public custodian-one.public.json --public custodian-two.public.json \
  --public custodian-three.public.json --approval "$BOSS_REVIEW_REFERENCE" \
  --output production-public-keys.json
```

Boss reviews and separately approves the exact IDs and public bytes. Only then
add the approved public configuration at
`release/voidup/production-public-keys.json` through authorized integration.
The file does not exist in G96. Its absence intentionally blocks production
preparation/publication. A review-reference string is an audit pointer, not an
automatic authorization grant.

The producer requires three distinct IDs/keys and refuses the embedded engineering
roots or `test-*` IDs as production trust. The production builder generates an
isolated source overlay with those public roots, the official catalog and a
production label. It does not edit the engineering source or expose runtime key
overrides. Root rotation remains a separately reviewed bootstrap operation.

## Sign without bringing private keys together

Prepare an exact catalog with increasing serial, bounded expiry, numeric stable
versions, native host labels and complete signed artifact/inventory identities.
The `catalog` command verifies a supplied previous catalog and envelope and carries
its revocations forward. Keep the last published serial and exact bytes in the
custodians' offline release ledger.

```sh
python3 -B tools/voidup_release.py catalog \
  --public-config production-public-keys.json --serial "$NEXT_SERIAL" \
  --release linux.release.json --release windows.release.json \
  --previous previous-catalog.json --output catalog.json
```

For the first catalog only, omit `--previous`. Include only actually qualified
hosts; never manufacture a Windows/macOS descriptor. Each custodian reviews the
same catalog digest, version, inventories, bootstrap hashes, source revision,
native qualification and revocations, then signs on their offline machine:

```sh
python3 -B tools/voidup_release.py sign \
  --public-config production-public-keys.json --catalog catalog.json \
  --key "$KEY_ID=$OFFLINE_PRIVATE_PEM" --output endorsement.json
```

Return the public endorsement only. A single endorsement is deliberately not
installable. Combine two independently produced endorsements:

```sh
python3 -B tools/voidup_release.py combine \
  --public-config production-public-keys.json --catalog catalog.json \
  --endorsement endorsement-one.json --endorsement endorsement-two.json \
  --output catalog.json.sig
```

Combination verifies both signatures over the exact domain-prefixed bytes and
rejects duplicate/unknown signers. Never reserialize or edit the catalog after
signing. A third valid endorsement is optional. Private keys are never CI inputs.

## First public bootstrap trust

Before public download, Boss must authenticate the bootstrap's public roots and
exact artifact hashes in the release announcement and an independent trusted
channel. A checksum or public-key file fetched beside an untrusted executable is
not an independent initial trust anchor. OS code signing/SmartScreen/Gatekeeper
behavior needs native qualification; this scheme does not invent an OS-issued
certificate or claim those prompts have disappeared.

[Release production](RELEASE_PRODUCTION.md) describes the prepared GitHub workflows,
immutable assets, stable catalog publication and the remaining native gates.
