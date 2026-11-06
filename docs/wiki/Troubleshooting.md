# Fix the development loop

_Draft for Boss approval; repository documentation is canonical._

1. Record `voidc --version`, OS/architecture, source revision and the exact command.
2. Confirm PATH resolves the installed compiler/server you intended.
3. Read the first source/toolchain diagnostic; an application status is not printed output.
4. Inspect `source-install.json` and adjacent logs for a failed source-install step.
5. Follow the [canonical troubleshooting table](https://github.com/Shmellyorc/Void.Language/blob/main/docs/TROUBLESHOOTING.md)
   for SDK hashes, HTTP prefixes, installed stdlib, native resources and editor startup.

Common fixes: remove `--app`, pass the correct source path, remove stale stdlib
overrides, keep native manifests/archives together, and retain the original SDK
for native builds. Do not replace missing verified libraries with ambient ones.
A clean-machine or Windows/macOS claim requires execution on that native platform.
