"""Run actual source memory streams against wire and cursor oracles in every mode."""
from pathlib import Path
import os
import random
import struct
import subprocess
import sys
import tempfile


compiler = Path(sys.argv[1]).resolve()
processes = 0
vectors = 0
model_steps = 64


def run(arguments, expected=None, success=True):
    global processes
    result = subprocess.run(list(map(str, arguments)), capture_output=True, timeout=240)
    assert (result.returncode == 0) == success, (arguments, result.returncode, result.stdout, result.stderr)
    assert b"warning" not in result.stderr.lower(), (arguments, result.stderr)
    if expected is not None:
        assert result.stdout == expected, (arguments, result.stdout, expected, result.stderr)
    processes += 1
    return result


def array(data):
    return "[" + ", ".join(map(str, data)) + "]"


def numeric_source():
    global vectors
    body = '''use io/memory
use encoding/binary

private func equal(const left: [u8], const right: [u8]) -> bool:
    if left.count != right.count:
        return false

    for value, index in left:
        if value != right[index]:
            return false

    return true

private func pattern<t>(const expected: [u8], order: binary.byte_order) -> bool:
    var reader = memory.stream(expected, order)
    var alias = reader

    if reader.read<t>() is Ok(var value):
        if alias.position != expected.count or not reader.eof:
            return false

        if not equal(binary.encode(value, order), expected):
            return false

        var writer = memory.stream([77, 88], order)

        if writer.seek(1) is Err(var reason):
            return false

        if writer.write(value) is Ok(var count):
            if count != expected.count or writer.position != count + 1:
                return false
        else:
            return false

        var actual = writer.to_bytes()

        if actual[0] != 77 or not equal(actual[1..<1 + expected.count], expected):
            return false

        if binary.decode<t>(actual[1..<1 + expected.count], order) is Ok(var decoded):
            if not equal(binary.encode(decoded, order), expected):
                return false
        else:
            return false

        var short = memory.stream(expected[0..<expected.count - 1], order)

        if short.read<t>() is Err(var reason):
            return (reason.kind is memory.error_kind.unexpected_eof) and short.position == 0

    return false

private func numeric<t>(value: t, const expected: [u8], order: binary.byte_order) -> bool:
    var writer = memory.stream(order = order)

    if writer.write(value) is Ok(var count):
        if count != expected.count or not equal(writer.to_bytes(), expected):
            return false
    else:
        return false

    var reader = memory.stream(expected, order)

    if reader.read<t>() is Ok(var decoded):
        return decoded == value and pattern<t>(expected, order)

    return false

'''
    rng = random.Random(2026102602)
    groups = []
    for width in (8, 16, 32, 64):
        for signed in (True, False):
            kind = ("i" if signed else "u") + str(width)
            low = -(1 << (width - 1)) if signed else 0
            high = (1 << (width - int(signed))) - 1
            values = {low, high, low + 1, high - 1, 0, 1}
            if signed:
                values.update((-1, -123, 123))
            values.update(rng.randint(low, high) for _ in range(3))
            cases = []
            for value in sorted(values):
                for order in ("little", "big"):
                    expected = value.to_bytes(width // 8, order, signed=signed)
                    cases.append(f"numeric<{kind}>({value}, {array(expected)}, binary.byte_order.{order})")
            groups.append((kind, cases))

    for kind, code, bits in (("f32", "f", 32), ("f64", "d", 64)):
        cases = []
        for value in (0.0, -0.0, 1.0, -1.0, 1.5, -123.25, 0.1):
            for order, prefix in (("little", "<"), ("big", ">")):
                expected = struct.pack(prefix + code, value)
                cases.append(f"numeric<{kind}>({repr(value)}{code}, {array(expected)}, binary.byte_order.{order})")
        fraction = 23 if bits == 32 else 52
        exponent = ((1 << (8 if bits == 32 else 11)) - 1) << fraction
        payloads = [0, 1, (1 << fraction) - 1, 1 << fraction, exponent - 1,
                    exponent, exponent + 1, exponent + (1 << (fraction - 1)), exponent + 12345]
        patterns = payloads + [(1 << (bits - 1)) | value for value in payloads]
        patterns += [rng.getrandbits(bits) for _ in range(4)]
        for value in patterns:
            for order in ("little", "big"):
                expected = value.to_bytes(bits // 8, order)
                cases.append(f"pattern<{kind}>({array(expected)}, binary.byte_order.{order})")
        groups.append((kind, cases))

    for kind, cases in groups:
        body += f"private func group_{kind}() -> i32:\n"
        for expression in cases:
            vectors += 1
            body += f"    if not {expression}:\n        return {vectors}\n\n"
        body += "    return 0\n\n"

    body += "func main() -> i32:\n"
    for kind, _ in groups:
        body += f"    var result_{kind} = group_{kind}()\n"
        body += f"    if result_{kind} != 0:\n        print(result_{kind})\n        return 1\n\n"
    body += f'    print("memory numeric: {vectors} vectors passed")\n    return 0\n'
    return body


def model_source():
    body = '''use io/memory

private func equal(const left: [u8], const right: [u8]) -> bool:
    if left.count != right.count:
        return false

    for value, index in left:
        if value != right[index]:
            return false

    return true

private func valid(const value: memory.stream, position: i64, const expected: [u8]) -> bool:
    if value.position != position or value.length != expected.count:
        return false

    if value.remaining != expected.count - position or value.eof != (position == expected.count):
        return false

    return equal(value.to_bytes(), expected)

private func count(outcome: result<i64, memory.error>, expected: i64) -> bool:
    if outcome is Ok(var value):
        return value == expected

    return false

private func bytes(outcome: result<[u8], memory.error>, const expected: [u8]) -> bool:
    if outcome is Ok(var value):
        return equal(value, expected)

    return false

private func failed<t>(outcome: result<t, memory.error>, eof: bool = false) -> bool:
    if outcome is Err(var reason):
        if eof:
            return reason.kind is memory.error_kind.unexpected_eof

        return reason.kind is memory.error_kind.invalid_range

    return false

func main() -> i32:
    var buffer = memory.stream([1, 2, 3])
    var alias = buffer
'''
    rng = random.Random(2026102603)
    data = bytearray((1, 2, 3))
    position = 0
    for step in range(model_steps):
        target = "buffer" if step % 2 else "alias"
        operation = rng.choice(("seek", "write", "up", "exact"))
        if operation == "seek":
            selected = rng.choice((-1, 0, len(data), len(data) + 1, rng.randint(0, len(data))))
            expression = f"count({target}.seek({selected}), {selected})" if 0 <= selected <= len(data) else f"failed({target}.seek({selected}))"
            if 0 <= selected <= len(data):
                position = selected
        elif operation == "write":
            payload = bytearray(rng.getrandbits(8) for _ in range(rng.randint(0, 7)))
            expression = f"count({target}.write_bytes({array(payload)}), {len(payload)})"
            data[position:position + len(payload)] = payload
            position += len(payload)
        else:
            amount = rng.randint(-2, 12)
            requested = amount
            name = "read_bytes" if operation == "up" else "read_exact"
            if amount < 0:
                expression = f"failed({target}.{name}({amount}))"
            elif operation == "exact" and amount > len(data) - position:
                expression = f"failed({target}.{name}({amount}), true)"
            else:
                amount = min(amount, len(data) - position)
                expected = data[position:position + amount]
                expression = f"bytes({target}.{name}({requested}), {array(expected)})"
                position += amount
        body += f"\n    if not {expression}:\n        return {step * 2 + 1}\n"
        body += f"\n    if not valid(buffer, {position}, {array(data)}):\n        return {step * 2 + 2}\n"
    return body + '\n    print("memory model: 64 transitions passed")\n    return 0\n'


with tempfile.TemporaryDirectory(prefix="VOID memory é😀 ") as directory:
    root = Path(directory)
    semantics = Path(__file__).with_name("semantics.void")
    numeric = root / "numeric.void"
    numeric.write_text(numeric_source(), encoding="utf-8")
    model = root / "model.void"
    model.write_text(model_source(), encoding="utf-8")
    programs = [(semantics, b"memory semantics passed\n"),
                (numeric, f"memory numeric: {vectors} vectors passed\n".encode()),
                (model, b"memory model: 64 transitions passed\n")]

    for path, expected in programs:
        run([compiler, "check", path])
        for flags in (["--no-opt"], []):
            run([compiler, "run", path, *flags], expected)
            executable = root / (path.stem + str(bool(flags)) + (".exe" if os.name == "nt" else ""))
            run([compiler, "build", path, "-o", executable, *flags])
            run([executable], expected)

    bad = root / "bad.void"
    invalid = [
        'const value = memory.stream()\n    value.seek(0)',
        'const value = memory.stream([1])\n    value.read_bytes(1)',
        'const value = memory.stream([1])\n    value.read<u8>()',
        'const value = memory.stream()\n    value.write<u8>(1)',
        'var value = memory.stream()\n    var hidden = value._state',
        'var value = memory._state(bytes = [], position = 0, order = binary.byte_order.little)',
        'var value = memory.stream()\n    value.position = 2',
        'var value = memory.stream()\n    value.order = binary.byte_order.big',
        'var value = memory.stream()\n    value.read<bool>()',
        'var value = memory.stream()\n    value.read<string>()',
        'var value = memory.stream()\n    value.write(true)',
        'var value = memory.stream()\n    value.write<i8>(128)',
        'var value = memory.stream()\n    value.seek(1.5)',
        'var value = memory.stream()\n    value.read_exact(1 as u64)',
    ]
    for body in invalid:
        bad.write_text('use io/memory\nuse encoding/binary\n\nfunc main():\n    ' + body + '\n', encoding="utf-8")
        run([compiler, "check", bad], success=False)

print(f"Memory execution: {vectors} numeric/IEEE vectors, {model_steps} model transitions, semantic fixture, 14 rejection cases; {processes} processes passed (check, JIT/O2, linked AOT/O2)")
