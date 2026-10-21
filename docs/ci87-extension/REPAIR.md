# CI #87 Linux Debug extension path repair

21 October 2026. Baseline `10f77fac23273c5af3fa3be6683c72c8ac059584`.
**Locally verified; Wrench review and hosted verification pending.**

## Cause and correction

Boss's verified [CI #87 result](https://github.com/Shmellyorc/Void.Language/actions/runs/39606175355)
is six successful jobs and Ubuntu Debug failing extension `npm test` after native
build/tests. The workflow configures `build/${{ matrix.configuration }}` with
single-configuration Ninja, yielding `build/Debug/voidls`. The unit test's optional
fallback is `build/debug/voidls`. Case-sensitive Linux therefore fails copying the
nonexistent fallback path with ENOENT.

Only the existing extension unit-test step gains its supported override:

```yaml
env:
  VOIDLS_TEST_SERVER: ${{ github.workspace }}/build/Debug/voidls${{ runner.os == 'Windows' && '.exe' || '' }}
```

This absolute path is independent of the step's `editors/vscode` working directory.
It selects `build/Debug/voidls` on Linux/macOS and `build/Debug/voidls.exe` on
Windows, matching the existing real-editor path expression and CMake output.
No production discovery or unit-test behavior changes.

## Focused verification

- `VOIDLS_TEST_SERVER=/home/shmellyorc/Documents/Void.Language/build/debug/voidls npm test`
  with Node 24.19.0: TypeScript check, compilation and bundle passed; **4/4 existing
  unit tests passed, zero failed, zero skipped**. This is the actual local Debug
  binary; the local build directory is lowercase and the explicit override is used.
  [Exact test output](unit-tests.txt).
- Confirmed local `CMAKE_BUILD_TYPE=Debug`, generated native link output `-o voidls`,
  no runtime output-directory override, and the workflow's `-G Ninja` / uppercase
  `build/Debug` configuration. The `.exe` suffix is conditional on Windows only.
- Compared workflow against baseline after removing exactly the new environment
  block: byte-identical. All seven matrix entries, strict ASan/UBSan settings,
  actual editor tests and all other commands remain unchanged.
- `git diff --check` passed. Root README, source/tests and earlier CI #87/Stage 8
  evidence are unchanged. No full matrix, native Windows/macOS run or hosted
  workflow execution was performed for this repair.

This new record supplements historical evidence; it does not rewrite CI #87's
outcome or establish a hosted lock. Boss retains commits, pushes, branches and
hosted CI controls. Stop here for Wrench review.
