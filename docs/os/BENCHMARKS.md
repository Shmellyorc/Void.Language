# OS query measurements

Linux x86_64, pinned Clang/LLVM 19.1.7, Release (`-O3 -DNDEBUG`) native Runtime.
Each repeated batch contains 2,000 queries. These are representative observations
on the shared development host, without CPU isolation or flushed kernel/service
caches, not latency guarantees. “First” means the first call to that operation
in a fresh measurement process; earlier operations may already have warmed a
shared native service. Filesystem location strings and environment contents are
never printed by the measurement.

Commands, from the repository root:

```sh
python3 tests/os/measure.py build/release/void_os_host 'build/release/os measurement'
env HOME= python3 tests/os/measure.py build/release/void_os_host 'build/release/os measurement passwd'
```

Raw results: [HOME present](evidence/performance-release-home.log),
[HOME empty, real passwd/NSS fallback](evidence/performance-release-passwd.log).
The measured host/compiler hashes are retained in
[evidence/acceptance-release-os-final-m4kaijj9/receipt.json](evidence/acceptance-release-os-final-m4kaijj9/receipt.json).
The final inventory also records subsequent test-only host strengthening.

| Layer and operation | First, µs | Repeated mean, µs |
|---|---:|---:|
| Native data-root selection, HOME present | 24.51 | 0.954 |
| Native home, HOME present | 0.330 | 0.227 |
| Native executable readlink | 33.85 | 3.164 |
| Native environment lookup/copy/validation | 6.130 | 0.339 |
| Native PID | 2.350 | 0.207 |
| Bridge + managed home publication | 6.440 | 0.711 |
| Bridge + managed executable publication | 6.830 | 3.966 |
| Bridge + managed environment publication | 3.630 | 1.381 |
| Bridge PID, no managed strings | 0.810 | 0.254 |
| Complete source data_dir, including ID/path composition | 433.827 | 80.123 |
| Native data-root selection, passwd fallback | 188.979 | 20.424 |
| Native home, passwd fallback | 22.400 | 23.347 |
| Bridge + managed home publication, passwd fallback | 72.339 | 26.025 |
| Complete source data_dir, passwd fallback | 876.545 | 117.468 |

The complete source getter includes ordinary facade checking, string/list/root
costs and existing `path.join`/filesystem reply construction. That is materially
more work than native root selection. These startup-scale results do not justify
replacing existing path semantics, introducing a hidden global cache or adding a
new path bridge. Applications can explicitly retain startup results in ordinary
globals, with the documented reload semantics.

Compilation is outside the execution timings. HOME-present source analysis took
87.164 ms and O2 compilation 734.776 ms; the fallback process took 90.143 ms and
787.099 ms respectively. The first/repeated source execution rows exclude both.
The benchmark Jit has no persistent cache policy, so cache lookup/population is
also outside these query costs. Separate actual miss/hit/bypass acceptance checks
verify code reuse and fresh process/Runtime observations; they make no cache
speed claim. Native resources/bundle private operations on Linux only classify a
plain host; their raw timings are not the public resource_dir cost.

Native calls use owned heap buffers with bounded retries. Runtime string
publication uses normal precise roots and allocation policy. The host test's
PID object-count assertion confirms zero managed string allocations. No global
memoization, watcher, background polling or new optimization was introduced.
