# Slice 4 immutable application metadata and OS locations

Baseline `2f667ea338127b8f91c3136804377ef3fa5218d6`. Golden Wrench accepted the [S3 closeout](SLICE3_FINAL_CLOSEOUT.md) for this local slice. Hosted CI remains on strict hold.

## Ownership and ABI

`ApplicationMetadata` owns a revision-1 record with six length-framed UTF-8 fields in project ID, name, SemVer, application ID, name and SemVer order. The revision and lengths are 32-bit little-endian values. The record is at most 2,072 bytes. The existing manifest identity, name and SemVer validators are shared with the compiler, independent IR verifier and Runtime decoder. Manifest application analysis captures the effective fields after defaults. A library retains package metadata and contributes no application record.

Each Runtime can explicitly admit a validated context or absence once. JIT preflights a supplied owner before admitting a global program or materializing its generation. Checked invocation freezes the context before calling generated entry code. A host-supplied context must match the compiled record when present, and an explicitly absent context conflicts with a compiled record. Matching invocation can repeat without a second admission. Separate manifest compilations and cache hits allocate distinct Runtimes when coordination is needed. Scalar code can retain the record without constructing a Runtime.

The AOT image carries the same immutable bytes. The additive private `vd_runtime_entry_context_v1` boundary takes a byte pointer and native extent, checks the record, and admits it before entering Managed code and the generated eager-global startup wrapper. Existing bootstrap symbols retain their argument layouts. Legacy numeric entry forwarding and application argument capture follow the existing adapters. Standalone coordinated startup seals absent context. C++ embedding structures `Compatibility`, `Runtime`, `ValidatedAnalysis`, and typed IR are same-build interfaces and consumers were rebuilt for their layout changes. The existing OS C bridge takes one additional private operation ordinal using its original reply layout.

The record contains only six identity fields. Selection revision and exact source snapshots remain separate. JIT keeps a build-recipe digest with the native code and its generation lease. AOT retains a separate image digest from the same versioned recipe, including target, effective profile, source inputs and verified resource digests for linked builds. No runtime reads of the manifest or checkout participate in admission. The AOT metadata and provenance constants also survive scalar code emission.

## Four direct-call adapters

After normal lookup the compiler checks the exact trusted `os` declaration and its finite private adapter mapping. The existing one-string facade remains the source navigation and function-value target. Typed IR independently checks the direct-call proof, admitted OS module scope, signatures and private owner. The private application-ID operation belongs to those adapters. It enters the existing rooted OS reply bridge and obtains the Runtime ID without querying platform roots. With an ID the private source adapter calls the existing explicit-ID facade. That facade retains its validator, OS placement and NativeSafe behavior.

Absent context returns `unavailable` (6), the exact operation name, empty path and native domain, native code zero and `Application identity is unavailable.`. Transitive worker analysis still rejects OS operations. Location queries create no directory and grant no authority.

## Acceptance coverage

| Case | Evidence |
|---|---|
| P29 | Changing display name and SemVer changes the compiled metadata and blocks replacement. The four location components retain the same app ID. |
| P30 | Debug and O2 native images run after relocation and checkout removal, with compiler stdlib lookup pointed at a missing path. The scalar native image retains the embedded record. |
| P31 | A shared imported facade uses two consuming Runtime IDs. Closing one Runtime leaves the other usable. A bare source can use an explicitly admitted host context. Checked library analysis exposes no application record. |
| P32 | Actual JIT and AOT calls verify all missing-context fields, including a poisoned Linux root oracle. Named and positional explicit calls retain placement and invalid ID code 2. |
| P33 | Invalid revision, length, field values, allocation failure, duplicate, conflict and late admission tests. Old and replacement JIT generations retain globals and distinct build provenance for compatible body edits. Metadata, entry, policy and global-contract changes reject replacement. Fresh Linux cache-hit owners. |
| P34 | Resolved alias, shadowed local module, explicit named ID, one-string callable and negative zero-argument function-value coercion. Independent typed IR rejects removed metadata and a forged private direct call. |
| P35 | All four zero-ID calls run in eager globals and main in normal/O2 JIT and AOT. Managed fault injection and collection verify checked failure and precise roots. Existing worker, low-stack, global and native-provider tests exercise the retained restrictions. |

## Corrective runs and qualification

The matched S3 compiler and stdlib rejected the new call at binding with VOID2004. Early S4 fixtures needed a direct OS include, enum pattern matching and an updated legacy oracle. The legacy fixture now executes the call beside an invalid manifest and checks unavailable. A library importing OS exposed a generic-navigation lookup through an empty entry identity. The fix uses the existing declarationless library scope. Fatal ASan found that the test allocator had left its `nothrow` allocation in a different allocation family. The paired test overload preserves fatal instrumentation and the allocation fault assertions. The LSP check confirmed that the source docs expose the omitted-ID form through signature help and that both call forms navigate to the same facade. [Original transcripts and reruns](evidence/slice4).

Native setup retained the access-denied copy failure and the rejected configurations for missing SDK and corresponding-source inputs. The host provider receipt binds the actual SDK path to its attested source root. The native builds used that root and the original pinned providers, with verified source backups, isolated build outputs and byte-checked restoration. The compiler needed the standard `NOMINMAX` and `WIN32_LEAN_AND_MEAN` guards around its Windows include. All three Linux preprocessed token streams were identical across the last Windows-only guard. Windows process reruns use the exact forward-slash paths returned by VOID against independently queried Known Folder roots. [Native setup and run receipts](evidence/slice4).

Linux Release, Debug and fatal ASan/UBSan each passed 36 focused CTests. Each profile exercised 63 Runtime and IR assertions and 61 compiled process assertions. Linux JIT showed cache miss/hit with different owners. A separate Linux probe changed only the revision in a test-owned linked image and observed rejection before its eager initializer printed. Another probe relocated manifest-selected legacy-entry binaries and verified the numeric argument after removing the checkout.

The Linux location oracles used four independent absolute XDG roots and repeated the calls with relative XDG values, observing `.local/share`, `.config`, `.cache` and `.local/state` under the controlled HOME. The Windows test queried Known Folder directly and observed `C:/Users/a/AppData/Local/org.test.context-game` for data, `C:/Users/a/AppData/Roaming/org.test.context-game` for config, and the `cache` and `state` children under the local app directory. These checks exercised both main-body and eager-global queries.

Windows 10 x64 build 19045 Release and Debug each qualified 22 focused suites, counting the corrected process rerun in place of the original path-oracle failure. Each final fixture passed 63 Runtime and IR assertions and 61 compiled process assertions. Windows Debug AOT first failed with disk-full error 112 during protected native staging. The retry completed after Boss freed space, with the same compiler and resource checks. The test-only relinks wrote their large artifacts to the private host share. Native persistent JIT caching, Windows sanitizer builds, Windows Server 2022, macOS and full clean build and install matrices remain **UNQUALIFIED**. [Consolidated qualification receipts](evidence/slice4-local.json).

## Source and Git inventory

The exact S4 patch is based on the accepted S3 working tree. It covers 35 source, test and documentation paths. Eleven starting paths were amended and 574 are byte-identical, including the earlier S1–S3 evidence. All 68,996 index entries match HEAD. The patch was applied to the starting bytes and checked in reverse against the final tree. [Source patch](evidence/slice4-source.patch) · [Layered inventory](evidence/slice4-inventory.txt).

## End-of-task temporary workspace cleanup

Three host probe trees and 13 Windows test paths were removed after qualification. The Windows cleanup reclaimed 2.33 GiB on C:, which ended with 8.63 GiB free. The receipts enumerate exact removed paths and retained native build-resource records. Cleanup touched only the listed S4 paths. Personal folders, previous slices, reusable build trees, caches, installed toolchains and the Git index are preserved. [Linux cleanup receipt](evidence/slice4/linux-cleanup-s4-results.json) · [Windows cleanup receipt](evidence/slice4/windows-cleanup-s4-results.json).

The verified Linux binaries remain in the existing Release, Debug and strict build trees. The earlier Windows cleanup included the Windows test binaries and build trees before the standing binary-retention directive arrived. The exact Windows binary hashes, test sources, provider and resource receipts, and rebuild commands remain available. The S3 checkpoint and private Windows source packet are retained for reconstruction. The earlier VM copy remains as requested.

For native reproduction, extract the private S4 source packet and overlay the recorded final updates, portable updates and lean-include update. Replace the two test fixtures with the final copies recorded in the corrective receipts. Verify the merged input hashes, then replay the CMake and test commands against the provider-attested SDK and source paths. [Native recipe and test receipts](evidence/slice4).

Stop for Golden Wrench's independent S4 review.
