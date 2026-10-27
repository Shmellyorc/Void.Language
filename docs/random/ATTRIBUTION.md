# Version-1 algorithm attribution

The deterministic transition implements **xoshiro256\*\* 1.0**, by David Blackman
and Sebastiano Vigna (2018), and the seed expander implements **SplitMix64**, by
Sebastiano Vigna (2015). The authors dedicate these implementations to the public
domain to the extent legally possible and grant permission to use, copy, modify
and distribute for any purpose, without warranty.

Primary reference sources inspected on 26 October 2026:

- [xoshiro256** transition](https://prng.di.unimi.it/xoshiro256starstar.c),
  specifically `next` and its rotate helper. Jump/long-jump code is not adopted.
- [SplitMix64 expansion](https://prng.di.unimi.it/splitmix64.c), currently
  redirected upstream to `splitmix.c`, specifically `next`.

VOID's source uses unsigned wrapping multiplication/division for fixed shifts
and rotations plus a private XOR operation. The independent retained Python model
uses arbitrary-precision arithmetic, masking and bitwise shifts instead. The
published vectors in RESEARCH.md predate this implementation. Algorithm version
1 additionally pins high-bit extraction, signed ordinals, rejection thresholds,
float operation precision and order, Fisher–Yates traversal and word consumption.
No future upstream update changes this contract automatically.

The OS bridge follows [Linux getrandom](https://man7.org/linux/man-pages/man2/getrandom.2.html),
[Microsoft BCryptGenRandom](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptgenrandom)
and [Apple getentropy](https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/man/man2/getentropy.2)
contracts. Source branches and link declarations do not constitute native
Windows/macOS execution evidence.
