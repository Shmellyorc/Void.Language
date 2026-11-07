"""Independent arbitrary-precision oracle, using shifts/bitwise operations, never VOID imports.

Blackman/Vigna xoshiro256** 1.0 and Vigna SplitMix64; public-domain algorithms:
https://prng.di.unimi.it/xoshiro256starstar.c
https://prng.di.unimi.it/splitmix64.c
"""
import struct
MASK = (1 << 64) - 1

def rounded(value, precision):
    return struct.unpack('<f', struct.pack('<f', value))[0] if precision == 32 else value

def bits(value, precision):
    return int.from_bytes(struct.pack('<f' if precision == 32 else '<d', value), 'little')

class Reference:
    def __init__(self, seed):
        self.state = []
        self.draws = 0
        for _ in range(4):
            seed = (seed + 0x9e3779b97f4a7c15) & MASK
            word = seed
            word = ((word ^ (word >> 30)) * 0xbf58476d1ce4e5b9) & MASK
            word = ((word ^ (word >> 27)) * 0x94d049bb133111eb) & MASK
            self.state.append(word ^ (word >> 31))

    def word(self):
        def rotate(x, n):
            return ((x << n) | (x >> (64 - n))) & MASK
        a, b, c, d = self.state
        result = (rotate((b * 5) & MASK, 7) * 9) & MASK
        shifted = (b << 17) & MASK
        c ^= a
        d ^= b
        b ^= c
        a ^= d
        c ^= shifted
        d = rotate(d, 45)
        self.state = [a, b, c, d]
        self.draws += 1
        return result

    def integer(self, width, signed=False, lower=None, upper=None, inclusive=False):
        offset = (1 << (width - 1)) if signed else 0
        if lower is None:
            return (self.word() >> (64 - width)) - offset
        size = upper - lower + inclusive
        if size == 1:
            return lower
        threshold = (1 << width) % size
        while True:
            candidate = self.word() >> (64 - width)
            if candidate >= threshold:
                return lower + candidate % size

    def unit(self, precision):
        n = 24 if precision == 32 else 53
        return (self.word() >> (64 - n)) / (1 << n)

    def floating(self, precision, lower, upper):
        r = lambda x: rounded(x, precision)
        while True:
            u = self.unit(precision)
            value = r(r(lower * r(1 - u)) + r(upper * u)) if lower < 0 < upper else r(lower + r(r(upper - lower) * u))
            if lower <= value < upper:
                return value

    def shuffle(self, values):
        values = values[:]
        for index in range(len(values) - 1, 0, -1):
            selected = self.integer(64, True, 0, index, True)
            values[index], values[selected] = values[selected], values[index]
        return values


def verify():
    # Pinned independent review vectors, not expectations derived from VOID.
    for seed, expected in [(0, '99ec5f36cb75f2b4 bf6e1f784956452a 1a5f849d4933e6e0 6aa594f1262d2d2c bba5ad4a1f842e59 ffef8375d9ebcaca 6c160deed2f54c98 8920ad648fc30a3f'), (42, '15780b2e0c2ec716 6104d9866d113a7e ae17533239e499a1 ecb8ad4703b360a1 fde6dc7fe2ec5e64 c50da53101795238 b82154855a65ddb2 d99a2743ebe60087'), (MASK, '8f5520d52a7ead08 c476a018caa1802d 81de31c0d260469e bf658d7e065f3c2f 913593fda1bca32a bb535e93941ba525 5ecda415c3c6dfde c487398fc9de9ae2')]:
        rng = Reference(seed)
        assert [rng.word() for _ in range(8)] == [int(x, 16) for x in expected.split()]
    for size in range(1, 257):
        counts = [0] * size
        for candidate in range(256 % size, 256):
            counts[candidate % size] += 1
        assert len(set(counts)) == 1
    rng = Reference(42)
    assert rng.integer(8, False, 0, 129) == 45 and rng.draws == 3
    rng = Reference(42)
    assert (rng.word() >= 1 << 63) is False
    assert rng.integer(32, True) == -519775866
    assert rng.integer(32, True, 0, 10) == 0
    assert rng.integer(32, True, -5, 10) == -1
    assert rng.integer(64, False, 0, MASK, True) == 0xfde6dc7fe2ec5e64
    assert bits(rng.unit(32), 32) == 0x3f450da5
    assert bits(rng.unit(64), 64) == 0x3fe7042a90ab4cbb
    assert [10, 20, 30][rng.integer(64, True, 0, 3)] == 10
    assert rng.shuffle([0, 1, 2, 3]) == [0, 1, 3, 2] and rng.draws == 11
    for precision in (32, 64):
        rng = Reference(0)
        upper = 1 + 2 ** (-23 if precision == 32 else -52)
        assert rng.floating(precision, 1, upper) == 1 and rng.draws == 3

if __name__ == '__main__':
    verify()
    print('Independent oracle: 24 published words, mixed 11-word sequence, 256 exhaustive residue domains and forced integer/float rejection passed')
