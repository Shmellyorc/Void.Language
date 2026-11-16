# G46-P — Cross-host UTF-8 integration decision

**HOLD.** The optimized AOT integer regression remains established on the original host. There is insufficient evidence to decide whether it persists across physical x86-64 hosts. Keep the qualified G46-F implementation and its measured decode gains available for review. The accepted G44 baseline is `894e2e8d9930028f86737092d3ac74010676f389`.

## Host access and existing measurements

The available physical host matches G46-F's AMD Ryzen 7 1700X (Zen, family 23, model 1), x86-64, PikaOS 4, Linux `7.2.11-pikaos`. The only configured SSH target resolves to loopback. Both registered VirtualBox guests are hosted on this machine. No independent physical host was identified through the configured access. G46-P collected zero benchmark samples, performed no rebuild or relink, and has no second-host CPU, OS, placement, instruction, cycle or timing data.

The historical G46-F uninstrumented medians are 24.571 → 28.966 ms (+17.9%) for optimized AOT integer-1M and 97.742 → 115.340 ms (+18.0%) for integer-4M. Dependent-1M rose 6.0%. Direct native 1 KiB and 64 KiB decode times fell 48.6% and 56.6%, respectively. In the separately instrumented integer-4M region, both variants retired about 816 million instructions and cycles rose from 338.2 to 396.7 million. Those results and the placement intervention describe the original host. [G46-F report](G46F_UTF8_DECODE_REGRESSION_CORRECTION.md).

## Verified identities

The G44 worktree is clean at the accepted commit. All 3,319 archived qualification inputs, including compiler, Runtime and tests, match in both G46 and G46-F. The only non-documentation differences from G44 are the qualified `include/void/runtime.hpp`, `src/runtime/array.cpp`, `src/runtime/coordination.cpp` and `tests/stage8/sprint3.hpp`. Both ZIPs passed full CRC, membership and SHA-256 checks. The original reports, failed prototype and negative timing evidence remain in these unchanged archives.

| Evidence archive | Manifest entries verified | ZIP SHA-256 |
| --- | ---: | --- |
| `g46-text-unicode-json-performance-2026-11-16.zip` | 1,177 | `1a10d42d6156e2e9e57ce84fee63c8ddd0e18c6d807083f6af99420e614b90ac` |
| `g46f-utf8-decode-regression-2026-11-16.zip` | 709 | `c76129d1c0b1e95b446f3872a08ee0d67b44a197ec4e0183cbb4551bdf984742` |

The G46 archive retains the byte-identical emitted optimized suite objects with SHA-256 `f3d4d76e0e5322d3a7a507f97c2cd2d5bb0d7d596b4405f2b0c984b581a0a171`. The records retain the following linked image identities. Neither ZIP contains the runnable linked ELF files. G46-F previously recovered both byte for byte using the archived recipes and LLVM/Clang/LLD 23.1.4.

| Optimized AOT suite | Linked image SHA-256 | Integer loop / ABI poll / Runtime poll / worker checkpoint ELF addresses |
| --- | --- | --- |
| G44 | `117070c1ad8640a83c71c13cda63696514fb9d5e3b1dd1e68061cc3cfb4d2a1d` | `0x215220 / 0x2b0480 / 0x2ad900 / 0x2c1db0` |
| Qualified G46-F | `19a7546ee7889b7eea1678f24e8780e190b2f4f4b27bd5637859615be542f411` | `0x215300 / 0x2b07c0 / 0x2adc40 / 0x2c20f0` |

## Next qualification action

1. On an accessible, independent physical Linux x86-64 host, recover the two ordinary optimized images from both archives using G46-F's `REPLAY.md` and `relink.py matrix`. Verify both SHA-256 digests before executing. Record the executable hashes, ELF addresses, PIE/ASLR conditions and dynamic provider identities. If any linked digest differs, label those results as a host-specific rebuild or relink and keep them separate from an identical-image comparison.
2. Keep compilation outside sampling. Use the archived suite and independent `g44/prior-recipes/oracles.json` for integer-1M/4M, dependent-1M, text-256/1024 and JSON-32/128. Replay the focused native 1 KiB and 64 KiB decoder controls with the fixture and exact output checks from `replay-build.py` and `replay-measure.py`. Check every result, alternate G44/G46 order over balanced blocks on the same pinned core, retain raw pairs, and record CPU model, microarchitecture, microcode, OS, governor, boost, SMT and system load.
3. Collect retired instructions and cycles in separate matched counter blocks and record event availability, running fractions and IPC. Use the archived loop and Runtime symbol records to compare the same regions. Keep counter-observer effects out of the throughput decision and retain all adverse results.

The only G46-P changes are this report and the new ledger entry. No source, SDK, archive or Git history change was made.
