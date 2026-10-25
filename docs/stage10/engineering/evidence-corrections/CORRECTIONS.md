# Stage 10 final evidence corrections — 24 October 2026

Both findings in [Golden Wrench's independent review](../../../Stage10_Wrench_Review.md)
are corrected. **Documentation/evidence only. Stage 10 remains INCOMPLETE;
native gates and Boss release decisions are unchanged.**

## Exact corrections

1. `step42-results.json`: restored every field of `historical_pre_ci4_checkpoint`
   from the authentic pre-CI #4 record, Git blob
   `52cebf0d00ae6f57bb08f1fcd32a25038ed5a339`, available at local HEAD
   `92136acaf114a492452a7d84b31f174f5b305def`. Its Windows/macOS platform entries
   again describe CI #98 / experiment #3 and the then-unverified repair. The
   accidental later job lists, passing counts and receipts are removed from that
   historical snapshot. All Step 42 values outside that snapshot are unchanged,
   including current CI #4 results and all earlier investigation objects.
2. `EVIDENCE.md`: restored the original **21 October 2026** research date,
   inspected commit `e25816bdb80e3f32613edf77c9cf3258232af966`, and research-only
   qualification verbatim under **Historical research baseline**, below the
   current acceptance summary. Original research findings retain their scope.

Updated the two affected live closeout payload hashes. The native-final handoff
now distinguishes its original reviewed snapshot from this corrected package;
its live inventory carries the updated handoff hash. These are the only five
existing files changed by this correction, listed with before/after hashes in
[verification](verification.json) and the [exact inventory](changed-files.json).

## Review artifacts and identity

| Artifact | Role |
|---|---|
| [corrections.patch](corrections.patch) | Exact incremental diff for the five existing documentation/receipt files, relative to their pre-correction local bytes |
| [local-closeout-corrected.patch](local-closeout-corrected.patch) | Updated complete 39-path closeout diff against `92136acaf114a492452a7d84b31f174f5b305def` |
| [local-closeout-corrected.tar.gz](local-closeout-corrected.tar.gz) | Corrected 39-file review payload; exactly three members differ from the original: the two corrected documents and the closeout hash inventory |
| [payload.sha256](payload.sha256) | Exact corrected archive-member hashes, including the inventory and prior validation receipt |
| [verification.json](verification.json) | Authentic-source equality, preservation, hash, JSON, link and read-only patch checks |

The original reviewed archive remains byte-identical at SHA-256
`50c492f20740dd802f85d7e5a9a466f6c892f9fac91f7f121fd6d77ba41ae39f`;
the original patch remains
`61607afa1d74cb7f1d10ab0cf59119399103938764fc3997f2ec4e844e8da921`.
They are historical review artifacts. Their embedded manifests and dated
validation describe those original payloads, not the corrected working documents.

The **corrected** archive SHA-256 is
`3b1e4f26872733c8b8f5970574b765188f44c19f7be6a0227da5030789f88fdf`;
the corrected full patch is
`9df93e6b98b25f78e2313e2145417db88e0bf5bf3d550baf9d5cf0bf64a98480`.
Unpack that archive into a separate inspection directory, then check its files
using `sha256sum -c /absolute/path/to/evidence-corrections/payload.sha256`.
The original and corrected archives are deliberately different versions.

CI #4's seven logs/results, CI #98 / experiment #3 failures, original Step 41
evidence and local 28-command records are preserved. No compiler, Runtime, test,
example or workflow changed. Only focused documentation/package validation ran;
no Git mutation, hosted CI, native-host/VirtualBox investigation or next stage.
Return to Golden Wrench/Boss for review; no new independent approval is claimed.
