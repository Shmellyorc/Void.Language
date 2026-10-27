"""Pure bit/classification and ordered-operation oracle; no host transcendental math."""
import math
import struct


def bits(value, precision):
    return int.from_bytes(struct.pack('<f' if precision == 32 else '<d', value), 'little')


def value(raw, precision):
    return struct.unpack('<f' if precision == 32 else '<d', raw.to_bytes(precision // 8, 'little'))[0]


def rounded(number, precision):
    try:
        return value(bits(number, precision), precision)
    except OverflowError:
        return math.copysign(math.inf, number)


def ordered(raw, precision):
    sign = 1 << (precision - 1)
    return (~raw & ((1 << precision) - 1)) if raw & sign else raw | sign


def ulps(actual, expected, precision):
    return abs(ordered(actual, precision) - ordered(expected, precision))


def classify(raw, precision):
    exponent = (raw >> (23 if precision == 32 else 52)) & (255 if precision == 32 else 2047)
    fraction = raw & ((1 << (23 if precision == 32 else 52)) - 1)
    if exponent == (255 if precision == 32 else 2047):
        return 'nan' if fraction else '-inf' if raw >> (precision - 1) else '+inf'
    if raw & ((1 << (precision - 1)) - 1) == 0:
        return '-zero' if raw >> (precision - 1) else '+zero'
    return 'finite'


def lerp(a, b, w, precision):
    r = lambda x: rounded(x, precision)
    if not all(math.isfinite(x) for x in (a, b, w)):
        return math.nan
    if w == 0:
        return a
    if w == 1:
        return b
    if a == b:
        return a
    result = r(r(a * r(1 - w)) + r(b * w)) if a < 0 < b or b < 0 < a else r(a + r(r(b - a) * w))
    if 0 < w < 1:
        result = min(max(a, b), max(min(a, b), result))
    return result


def inverse_lerp(a, b, x, precision):
    r = lambda y: rounded(y, precision)
    if not all(math.isfinite(y) for y in (a, b, x)) or a == b:
        return math.nan
    if x == a:
        return 0.0
    if x == b:
        return 1.0
    numerator, denominator = r(x - a), r(b - a)
    if not math.isfinite(numerator) or not math.isfinite(denominator):
        numerator, denominator = r(r(x / 2) - r(a / 2)), r(r(b / 2) - r(a / 2))
    return r(numerator / denominator)
