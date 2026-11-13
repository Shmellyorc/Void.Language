# VOID component and Runtime redistribution notices

VOID-original code is MIT-licensed. Read `VOID-MIT-LICENSE.txt` and
`LICENSING_SCOPE.md` in the installed `share/void/licenses` directory. Other code
and data retain their own terms. `NOTICE_INVENTORY.json` identifies exact sources,
extractions, hashes and installation destinations. The paths below are relative
to that license directory. This index covers existing compiler/tooling installs;
it is not public-release clearance.

| Installed component/material | License texts and attribution |
| --- | --- |
| Compiler `voidc`, Runtime/native archives, installed stdlib sources, server `voidls`, original Vim/Neovim integration | `VOID-MIT-LICENSE.txt`, `LICENSING_SCOPE.md` for original portions only |
| LLVM code linked into either tool | `llvm-23.1.4/LICENSE.TXT` (Apache with LLVM exceptions and legacy text); `ConvertUTF-NOTICES.txt` (older Unicode terms); `xxhash-NOTICES.txt` (BSD-2); `BLAKE3-LICENSE.txt` (CC0/Apache alternatives); `Support-NOTICES.txt` (additional upstream Support attributions including BSD regex and public-domain origins). The common set is included conservatively with both tools; this is not a claim that every listed subcomponent occurs in each binary. |
| LLVM transitive zstd 1.5.7 | `zstd-1.5.7/LICENSE` (selected BSD route) and `EMBEDDED-NOTICES.txt`, including separate divsufsort attribution retained conservatively |
| LLVM zlib use | `zlib-1.3.2/LICENSE`; tooling has this independent copy even without compiler HTTP notices. Host library versions may differ; exact build receipts still govern actual inclusion. |
| Windows LLVM libxml2 2.15.4 | `libxml2-2.15.4/Copyright` and `FILE-NOTICES.txt` on Windows. Separate per-file authors/terms are retained. This conservative notice set is installed for both tools without claiming that both retain all XML code. |
| Ryu numeric formatting | `LICENSE-Boost` (Boost route of the upstream Boost/Apache alternative) |
| date code, including local modifications | `LICENSE.txt` and `NOTICE` retain the upstream MIT texts, detailed authors and modification information |
| IANA tzdata 2026e | `LICENSE` preserves its public-domain declaration and listed code-file exceptions; `stage10-versions.json` pins the common sources |
| Unicode 16.0 runtime data | `UNICODE-LICENSE.txt`; Unicode data copyright © 1991–2024 Unicode, Inc. The installed license text carries its upstream 1991–2026 notice. Full mappings derive from Python Unicode tables and properties from Perl UCD inputs. Python is distributed under the PSF license: https://docs.python.org/3/license.html . Input/output hashes remain in repository `docs/text-encoding/evidence/unicode.json`; additional PSF/Perl-derived material obligations remain under provenance review. |
| Compiler curl 8.23.0 | `http/COPYING`, `http/NOTICE`, `http/VOID-PROVENANCE.json`, `http/http-versions.json`; preserve corrections and original curl terms |
| Compiler/tooling project parser toml++ 3.4.0 | `tomlplusplus/LICENSE` (MIT), `upstream.json` and the complete private `void-bounds-and-locations.patch`; `corresponding-source/toml.hpp` and `README.md` preserve the exact patched source and reconstruction instructions. This does not add a parser or native dependency to generated applications. |
| Compiler nghttp2 1.70.0, c-ares 1.34.9, zlib 1.3.2 | Existing license files under their respective `http/` versioned subdirectories (MIT, MIT, Zlib) |
| Compiler libpsl 0.21.5 | `http/libpsl-0.21.5/` main MIT texts **and** `LICENSE.chromium` for BSD-licensed Chromium lookup code |
| Compiler PSL data | `http/publicsuffix/LICENSE` (MPL-2.0), `public_suffix_list.dat` (covered source) and `UPSTREAM.json`, pinned revision `8437e5262f13305f94087abe699b4fe2be1d1de8`. Recipients can find the covered source in this directory; preserve it and these notices when distributing the covered data/code. |
| Linux/macOS compiler AWS-LC 5.12.0 | `http/aws-lc-5.12.0/LICENSE`, `NOTICE`, `UPSTREAM.json`. Preserve the complete mixed permissive grant/attribution, not an Apache-only replacement. |
| Linux compiler libidn2 2.3.8 and libunistring 1.5 | `http/libidn2-2.3.8/` and `http/libunistring-1.5/`: GPL/LGPL texts, upstream READMEs and libidn2 Unicode notice. Source archives are under `http/corresponding-source/`. |

Some existing HTTP notices are copied on all platforms although the corresponding
libraries are not used there. `http/openssl-3.5.9/` is retained historical notice
material; it does not identify an active OpenSSL 3.5.9 provider. Do not delete
upstream/historical notices to make an inventory look smaller.

## GNU libraries: prominent use notice and unresolved obligations

The Linux compiler uses and installs **GNU libidn2 2.3.8 and GNU libunistring 1.5**.
Their libraries offer LGPL-3.0-or-later OR GPL-2.0-or-later. The planned shared
library route uses LGPLv3+; the GPL and LGPL license texts and copyright notices
are available in the directories above. This MIT license for VOID imposes no
restriction on modifying those libraries or reverse engineering for debugging
changes to them. Any later application/distribution terms must preserve the
applicable rights.

The pinned source archive hashes match the retained audit. Provisioning scripts
and a dependency build receipt are installed alongside them, but the scripts
expect a repository layout/toolchain. Complete usable corresponding-source/build
instructions and the LGPL replacement mechanism remain **UNRESOLVED**. Native
resource checks reject changed library hashes; the manifest is editable. A
reviewed ABI-compatible replacement plus manifest-update procedure for the
compiler and generated applications has not been established here. Do not weaken
integrity checks or assume that shared linking alone closes these duties.
Conditional installation-information duties and distribution terms also need
review for the chosen delivery model.

## Generated native programs and external prerequisites

The native builder emits a program object and passes Runtime/fixture archives,
then HTTP archive/shared flags to external Clang 23.1.4. Linker member selection
determines the retained code. Programs containing covered original Runtime/stdlib
code need the VOID MIT notice; third-party portions may need their own notices
and source/replacement information. Carry the relevant materials with each
distributed application and copied library. Object-only output requires a later
link; it is not a standalone cleared application. LLVM's compiler-output exception
does not waive VOID Runtime or HTTP dependency terms.

LLVM/Clang/LLD 23.1.4 and complete SDKs remain external. Host glibc/loader,
libstdc++/libgcc, system zlib, Microsoft OS libraries/SDK, Schannel, Apple
SDK/frameworks/libc++/libSystem and optional system terminfo/ncurses are not copied
by this notice change. Embedded runtime/header code still needs its applicable
terms reviewed. Windows `/MT` and DIA/SDK conditions, Apple SDK-derived material,
exact final platform closures, and crypto/jurisdiction questions remain open.
VS Code/npm/VSIX and experimental installers are outside this distribution scope.

Public release remains **HOLD** pending owner-controlled qualification and the
unresolved compliance questions. The historical file200 measurement of 20.091 ms
against the <10 ms target is retained; Boss's later directive makes that target
advisory, not a release blocker. See [current preview policy](PREVIEW_BLOCKERS.md).
Notice installation and MIT selection do not clear the remaining release holds.


## Runtime host primitive dependencies

Supported-host builds additionally link the pinned libsodium 1.0.22 and libarchive 3.8.11
static providers recorded in `third_party/host-versions.json`. The libsodium source
also includes the approved three-file ASan/alignment backport, separately identified
in `host/patches/libsodium-1.0.22/PROVENANCE.json` and its exact patch. Original
archive hashes remain unchanged. Libsodium carries
ISC, BSD terms for SHA-256 and controlling component notices; libarchive carries BSD and the component
terms in its COPYING/source files. The compiler component installs these notices,
per-source notice inventories, exact source archives, dependency receipts and
build/verification scripts under `share/void/licenses/host`. Libarchive reuses the
existing pinned zlib; it enables no additional external codec or crypto provider.
The public APIs expose only SHA-256, detached Ed25519 verification and the bounded
stored/deflate ZIP profile. These additions do not establish public distribution
clearance for the complete compiler/SDK closure or native Windows/macOS acceptance.

The host corresponding-source directory contains the original archives and a
`repro/` layout with the exact patch/policy, SDK selector, builder and verifier
inputs. To reconstruct the reviewed sodium source without an SDK or network:

```sh
python3 <host>/corresponding-source/repro/tools/verify_host_deps.py \
  --archives <host>/corresponding-source --reconstruct-source <fresh-directory>
```

This verifies the base hash, applies the approved patch with no Git index/history
changes, and checks all reviewed source postimages. It does not claim a standalone
compiler rebuild: native SDKs and the verified HTTP/zlib closure remain external
build prerequisites. The receipt binds the applied source and exact recipe inputs;
original per-file notice snapshots describe upstream inputs and are retained.
