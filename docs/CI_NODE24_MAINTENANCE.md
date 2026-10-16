# Official Actions Node 24 maintenance

16 October 2026. Separately authorized alongside Stage 2D. This change can be
reviewed/committed without the runtime implementation. Its code scope is exactly:

- `.github/workflows/ci.yml`
- `.github/workflows/phase23-stack-probe.yml`

Only existing official action references change: checkout v4 to v5, cache v4 to v5,
setup-python v5 to v6, upload-artifact v4 to v6, and setup-node v4 to v5 where present.
No action/step is added or removed. The third-party MSVC environment action stays
unchanged. This document is the independent maintenance evidence record.

Verified the upstream tagged action manifests: all five use `runs.using: node24`.
The [checkout v5 manifest](https://github.com/actions/checkout/blob/v5/action.yml),
[cache v5 manifest](https://github.com/actions/cache/blob/v5/action.yml),
[setup-python v6 manifest](https://github.com/actions/setup-python/blob/v6/action.yml),
[upload-artifact v6 manifest](https://github.com/actions/upload-artifact/blob/v6/action.yml)
and [setup-node v5 manifest](https://github.com/actions/setup-node/blob/v5/action.yml)
retain the inputs used here. The
[cache compatibility notes](https://github.com/actions/cache/blob/v5/README.md) and
[upload-artifact v6 notes](https://github.com/actions/upload-artifact/blob/v6/README.md)
require Actions Runner 2.327.1 or newer. These are hosted GitHub runners; their
actual runtime versions and action execution remain subject to owner-controlled
hosted verification, not a local runner-version claim.

The explicit `cache: npm` and extension lockfile path remain selected in setup-node;
its automatic package-manager detection introduces no additional cache selection
here. Artifact names/paths and v4-era archive/no-files/hidden-file defaults remain
selected as before. No environment flag suppresses a deprecation warning.

Local verification compared each workflow with locked cb601b4 after applying only
the five authorized version substitutions: byte-for-byte equality. The normal
matrix retains all seven entries, and the probe retains three native platforms.
Thus permissions, triggers, LLVM 19.1.7 SDK/cache keys, Python 3.12, Node 24.19.0,
commands, artifacts, sanitizer environment, conditions and failure policies have no
text changes. No action was dispatched and no remote repository was modified.
