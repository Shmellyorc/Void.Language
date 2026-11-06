# Integrated performance: versioned evidence and local artifacts

The staged change contains the accepted implementation, five fixtures, final
reports, future-only SIMD roadmap, linked raw measurement JSON/CSV, JUnit and
qualification receipts, relevant patches and small qualification drivers.
No new optimization or correctness test was run during staging preparation.

Binaries, objects, libraries, source archives, editable candidate copies,
disposable run directories and unrelated investigations stay **local only**.
Nothing was deleted. Directory links expose a selected text subset after checkout,
not the complete original experiment tree. JSON command/artifact manifests are
historical receipts: recorded absolute paths and binary hashes do not promise
that every original local artifact is committed. Qualification scripts document
the completed local campaign and are not a portable replay bundle.

Original document bytes before publication-link updates are retained locally at
`experiments/staging-preparation/original-documents/`. The frozen manifests'
logical report aliases and helper link-count receipts describe those originals,
not the publication copies. Production/test hashes and all measurements remain
unchanged. [Publication mapping](publication-map.json) records both hashes.

Three directly linked historical CTest logs are published as `.txt` copies here;
the original disposable acceptance directories remain local. The 69,094-entry
starting inventory is represented by a compact source/protection receipt and its
full-original hash. Full unlinked diagnostic sources, binaries, native run
receipts and rejected-experiment trees are preserved locally.

## Original document preservation

| Document | Original SHA-256 |
| --- | --- |
| `docs/compiler-health/INTEGRATED_PERFORMANCE_QUALIFICATION.md` | `dc988f8228dd2ee775acf5bea9082f18e8057d2b141c8af5fdd1a6c4c52af21a` |
| `docs/compiler-health/INTERNAL_PERFORMANCE_REPAIR_RESULT.md` | `2fd91f85a4d30d55a5c1ec23dd2fab79c1244cd1c24b158868501ade9e614917` |
| `docs/log/HANDOFF.md` | `af09e112ccf5da42a4eddafbf3908e0c5b58cfb5e04d435a6dfe1591c3509bc3` |
| `docs/log/INTEGRATED_LOGGING_REFINEMENT_RESULT.md` | `3c1b65d2da6a0ee112392639b1b25b997880ce4592abd63578647f0b53465431` |
| `experiments/integration-qualification/README.md` | `3ee9215530823cb2cd4573ebe858f0ea2244fe8a4bd2c5dd9af55b24a97cf3e8` |
| `experiments/log-integrated/refinement/REPRODUCE.md` | `421a4d781812ba9d2445a5a682d588cd95986a34d8af08f66127f80332a5c183` |

## Referenced artifacts retained locally

## Local Artifact 1

`experiments/log-integrated/candidate/include/void/filesystem_abi.hpp` — Historical candidate source snapshot.

Bytes: 1460; SHA-256: `d1192c147aee8d650092a0c8d89b76e1fd8998d9775dec6c716a16af8baa77ab`.

## Local Artifact 2

`experiments/log-integrated/candidate/include/void/ir.hpp` — Historical candidate source snapshot.

Bytes: 15441; SHA-256: `7ca79371b2e9378836e23f0765db4003c0cf95c353893f1887150087de7a4725`.

## Local Artifact 3

`experiments/log-integrated/candidate/include/void/runtime.hpp` — Historical candidate source snapshot.

Bytes: 20243; SHA-256: `6fc4568cb22de1108a2e5540b108e288be16d327de64a55fb8a60abc1a57d1c3`.

## Local Artifact 4

`experiments/log-integrated/candidate/include/void/runtime_abi.hpp` — Historical candidate source snapshot.

Bytes: 9768; SHA-256: `9a31be262b39f36879ea06c1decb8bd628df44901a4c96e2aaffc0936a09cf98`.

## Local Artifact 5

`experiments/log-integrated/candidate/src/backend/llvm/backend.cpp` — Historical candidate source snapshot.

Bytes: 215470; SHA-256: `5cd38005b0fecb78fc01a1623c5d21acce6a058cb8b0b7fff07be4d3567cfbd7`.

## Local Artifact 6

`experiments/log-integrated/candidate/src/backend/llvm/filesystem.inc` — Historical candidate source snapshot.

Bytes: 6998; SHA-256: `81bdb2546d73eaff5ed5480d354bf8fd1114dc73568c37c87d0c85c23d1727bb`.

## Local Artifact 7

`experiments/log-integrated/candidate/src/frontend/capabilities.inc` — Historical candidate source snapshot.

Bytes: 52673; SHA-256: `c96983ec1704593bdbc7b97992094d5179f6e94f39b6121d5c59b62686112d0e`.

## Local Artifact 8

`experiments/log-integrated/candidate/src/frontend/compiler.cpp` — Historical candidate source snapshot.

Bytes: 545863; SHA-256: `d9c37ed1ec3214d4483b9dd68c3e5c6341251db9bc4f0dbbd7cedf33f06131ac`.

## Local Artifact 9

`experiments/log-integrated/candidate/src/frontend/filesystem.inc` — Historical candidate source snapshot.

Bytes: 2171; SHA-256: `a1c89150ab0a7999c765530f4285209feab5af1f979ab620583ff88756168ce5`.

## Local Artifact 10

`experiments/log-integrated/candidate/src/ir/filesystem.inc` — Historical candidate source snapshot.

Bytes: 6437; SHA-256: `3a2c76e2cfd76d6e9bc15ad9143ab0181e69be9062d36faa4931d12254a4e925`.

## Local Artifact 11

`experiments/log-integrated/candidate/src/ir/local_cleanup.inc` — Historical candidate source snapshot.

Bytes: 2513; SHA-256: `3d217d9e16d2c430fa8034f05934858c0c05d97f65bb4e2742016ff8f933506a`.

## Local Artifact 12

`experiments/log-integrated/candidate/src/ir/module.cpp` — Historical candidate source snapshot.

Bytes: 136103; SHA-256: `64ac2619ef446273e14cf7bead0e1d0c08a13e8f80218bdfccbeaf2205cfb58b`.

## Local Artifact 13

`experiments/log-integrated/candidate/src/ir/reference_capabilities.inc` — Historical candidate source snapshot.

Bytes: 25859; SHA-256: `86a8b9282c9d162223d8073b894e488826706701d9d1fd971d1c5f32a49e79b9`.

## Local Artifact 14

`experiments/log-integrated/candidate/src/runtime/abi.cpp` — Historical candidate source snapshot.

Bytes: 25585; SHA-256: `61af197d18c550cf3034c20aeb5ecd4317184c296973046e4ce504644e02d11f`.

## Local Artifact 15

`experiments/log-integrated/candidate/src/runtime/array.cpp` — Historical candidate source snapshot.

Bytes: 30679; SHA-256: `0bad6a2be76b4a62d5bf68fc5ed5371cc22dd7cfaa2b991ae1be8f0cad939231`.

## Local Artifact 16

`experiments/log-integrated/candidate/src/runtime/buffer.cpp` — Historical candidate source snapshot.

Bytes: 34858; SHA-256: `6c778751fb400a2fe025832681d2ec4d708a7a6e582cb02aa0a93a0b530a53f6`.

## Local Artifact 17

`experiments/log-integrated/candidate/src/runtime/filesystem_abi.cpp` — Historical candidate source snapshot.

Bytes: 22053; SHA-256: `e48d3c3efe1f158b1daf51918328823cce2ef2f90965cc00fd43defb83989323`.

## Local Artifact 18

`experiments/log-integrated/candidate/stdlib/io.void` — Historical candidate source snapshot.

Bytes: 71581; SHA-256: `3e22822f57761a486702919774c8d4e8bafe23d0ecef2c37c8e23fe7173e6ad4`.

## Local Artifact 19

`experiments/log-integrated/candidate/tests/advanced/tests.cpp` — Historical candidate source snapshot.

Bytes: 9934; SHA-256: `57b13f99b2e236542f6479f2f376f518f6a845c417a042a42185bae80dc1c2cc`.

## Local Artifact 20

`experiments/log-integrated/refinement/evidence/identity.json` — Full starting inventory (69,094 protected entries).

Bytes: 9903975; SHA-256: `a421d57f58213a70d73aa6f5b2955d7b33ad7583d43077107aaa0f60780d741d`.

## Local Artifact 21

`experiments/log-integrated/refinement/original-integrated-source.tar.gz` — Full local source archive.

Bytes: 72740523; SHA-256: `38ca2519da5fd44972d8936783e4d7a2c06eb56ed20525d5e470f1222c09526b`.
