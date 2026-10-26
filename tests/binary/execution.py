"""Independent wire vectors exercise the installed-style source import in all modes."""
from pathlib import Path
import os
import random
import struct
import subprocess
import sys
import tempfile


COMPILER = Path(sys.argv[1]).resolve()
CHECKS = 0
VECTORS = 0


def run(arguments, expected=None, success=True, **kwargs):
    global CHECKS
    result = subprocess.run(list(map(str, arguments)), capture_output=True, timeout=180, **kwargs)
    assert (result.returncode == 0) == success, (arguments, result.returncode, result.stdout, result.stderr)
    if expected is not None:
        assert result.stdout == expected, (arguments, result.stdout, expected, result.stderr)
    assert b"warning" not in result.stderr.lower(), (arguments, result.stderr)
    CHECKS += 1
    return result


def array(data):
    return "[" + ", ".join(map(str, data)) + "]"


def source():
    global VECTORS
    helpers = '''use encoding/binary

func equal(const left: [u8], const right: [u8]) -> bool:
    if left.count != right.count:
        return false

    for item, index in left:
        if item != right[index]:
            return false

    return true

func numeric<t>(value: t, const expected: [u8], order: binary.byte_order) -> bool:
    var encoded = binary.encode(value, order)

    if not equal(encoded, expected):
        return false

    if binary.decode<t>(expected, order) is Ok(var decoded):
        return decoded == value and equal(binary.encode(decoded, order), expected)

    return false

func pattern<t>(const expected: [u8]) -> bool:
    if binary.decode<t>(expected) is Ok(var decoded):
        if not equal(binary.encode(decoded), expected):
            return false

        var reversed = expected[expected.count - 1..0]

        if not equal(binary.encode(decoded, binary.byte_order.big), reversed):
            return false

        if binary.decode<t>(reversed, binary.byte_order.big) is Ok(var big):
            return equal(binary.encode(big), expected)

    return false

func invalid<t>() -> bool:
    for count in 0..16:
        var bytes: [u8] = []

        for index in 0..<count:
            bytes.push(255)

        for selection in 0..1:
            var order = binary.byte_order.little

            if selection == 1:
                order = binary.byte_order.big

            var result = binary.decode<t>(bytes, order)
            var width = binary.encode<t>(0).count

            if (count as i64) == width:
                if result is Err(var reason):
                    return false
            else:
                if result is Ok(var value):
                    return false

                if result is Err(var reason):
                    if not (reason is binary.error.wrong_width):
                        return false

    return true

func propagated(const bytes: [u8]) -> result<i64, binary.error>:
    var value = binary.decode<i64>(bytes)?
    return Ok(value)

func ownership() -> bool:
    var first = binary.encode<i64>(-1)
    var second = binary.encode<i64>(-1)
    second[0] = 0

    if first[0] != 255:
        return false

    const input: [u8] = [1, 0, 0, 0]

    if binary.decode<i32>(input) is Ok(var value):
        if value != 1 or input[0] != 1:
            return false
    else:
        return false

    if propagated([]) is Err(var reason):
        return reason is binary.error.wrong_width

    return false

'''
    rng = random.Random(20261026)
    functions = []
    names = []

    def group(name, cases):
        global VECTORS
        names.append(name)
        lines = [f"func {name}() -> i32:"]
        for expression in cases:
            VECTORS += 1
            lines.extend([f"    if not {expression}:", f"        return {VECTORS}", ""])
        lines.append("    return 0\n\n")
        functions.append("\n".join(lines))

    for width in (8, 16, 32, 64):
        for signed in (True, False):
            kind = ("i" if signed else "u") + str(width)
            low = -(1 << (width - 1)) if signed else 0
            high = (1 << (width - int(signed))) - 1
            values = {low, high, 0, 1, high - 1, low + 1}
            if signed:
                values.update((-1, -2, -123, 123))
            else:
                values.update((128, 255))
            values.update(rng.randint(low, high) for _ in range(8))
            cases = []
            for value in sorted(values):
                for order in ("little", "big"):
                    expected = value.to_bytes(width // 8, order, signed=signed)
                    cases.append(f"numeric<{kind}>({value}, {array(expected)}, binary.byte_order.{order})")
            cases.append(f"invalid<{kind}>()")
            group("integer_" + kind, cases)

    for kind, code, bits in (("f32", "f", 32), ("f64", "d", 64)):
        cases = []
        for value in (0.0, -0.0, 1.0, -1.0, 1.5, -123.25, 0.1):
            for order, prefix in (("little", "<"), ("big", ">")):
                expected = struct.pack(prefix + code, value)
                cases.append(f"numeric<{kind}>({repr(value)}{code}, {array(expected)}, binary.byte_order.{order})")
        fraction = 23 if bits == 32 else 52
        exponent_mask = ((1 << (8 if bits == 32 else 11)) - 1) << fraction
        payloads = [0, 1, (1 << fraction) - 1, 1 << fraction,
                    exponent_mask - 1, exponent_mask, exponent_mask + 1,
                    exponent_mask + (1 << (fraction - 1)), exponent_mask + 12345]
        patterns = payloads + [(1 << (bits - 1)) | value for value in payloads]
        patterns += [rng.getrandbits(bits) for _ in range(16)]
        for value in patterns:
            cases.append(f"pattern<{kind}>({array(value.to_bytes(bits // 8, 'little'))})")
        cases.append(f"invalid<{kind}>()")
        group("float_" + kind, cases)

    calls = ["func main() -> i32:"]
    for name in names:
        calls.extend([f"    var failure_{name} = {name}()", f"    if failure_{name} != 0:",
                      f"        print(failure_{name})", "        return 1", ""])
    calls.extend(["    if not ownership():", "        return 2", "",
                  f'    print("binary: {VECTORS} vectors passed")', "    return 0", ""])
    return helpers + "".join(functions) + "\n".join(calls)


with tempfile.TemporaryDirectory(prefix="VOID binary é😀 ") as directory:
    root = Path(directory)
    path = root / "main.void"
    path.write_text(source(), encoding="utf-8")
    expected = f"binary: {VECTORS} vectors passed\n".encode()
    run([COMPILER, "check", path])

    for flags in (["--no-opt"], []):
        run([COMPILER, "run", path, *flags], expected)
        executable = root / ("binary" + str(bool(flags)) + (".exe" if os.name == "nt" else ""))
        run([COMPILER, "build", path, "-o", executable, *flags])
        run([executable], expected)

    invalid = [
        ('binary.encode(true)', 'Binary encoding requires'),
        ('binary.encode("text")', 'Binary encoding requires'),
        ('binary.encode([1, 2])', 'Binary encoding requires'),
        ('binary.decode<bool>([])', 'Binary encoding requires'),
        ('binary.decode<string>([])', 'Binary encoding requires'),
        ('binary.decode<binary.byte_order>([])', 'Binary encoding requires'),
        ('binary.decode([])', 'infer'),
        ('binary.decode<i32>([1, 2, 3, 4, 5], 0)', 'Type mismatch'),
        ('binary._binary_bits<i32>(1)', 'Unknown'),
        ('_binary_bits<i32>(1)', 'Unknown'),
        ('_binary_value<f32>(0)', 'Unknown'),
        ('_binary_width<u32>()', 'Unknown'),
    ]
    for index, (expression, message) in enumerate(invalid):
        path.write_text(f"use encoding/binary\n\nfunc main():\n    var value = {expression}\n", encoding="utf-8")
        result = run([COMPILER, "check", path], success=False)
        assert message.lower().encode() in result.stderr.lower(), (index, result.stderr)

print(f"Binary execution: {VECTORS} independent vectors; {CHECKS} processes passed (check, JIT/O2, linked AOT/O2, rejection checks)")
