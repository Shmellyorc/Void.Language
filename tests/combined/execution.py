"""Real source parsing/error/console boundaries in JIT, O2 and linked native AOT."""
from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from test_platform import native_text

compiler = Path(sys.argv[1]).resolve()
fixtures = Path(__file__).parent
small = '--small-stack' in sys.argv
checks = 0


def stack():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1024 * 1024, 1024 * 1024))


def run(args, data=b'', status=0, expected=None):
    global checks
    result = subprocess.run(list(map(str, args)), input=data, capture_output=True,
                            preexec_fn=stack if small else None, timeout=180)
    assert result.returncode == status, (args, result.returncode, result.stdout, result.stderr)
    assert b'warning' not in result.stderr.lower(), result.stderr
    if expected is not None:
        assert result.stdout == expected, (args, result.stdout, expected)
    checks += 1
    return result


with tempfile.TemporaryDirectory(prefix='VOID combined ') as directory:
    root = Path(directory)
    checks_source = []
    number = 0

    def assertion(condition):
        global number
        number += 1
        checks_source.extend([f'    if not ({condition}):', f'        return {number}'])

    for signed in (True, False):
        for width in (8, 16, 32, 64):
            target = ('i' if signed else 'u') + str(width)
            low = -(1 << (width - 1)) if signed else 0
            high = (1 << (width - int(signed))) - 1
            for value in sorted({low, high, 0, 1, -1 if signed else 2}):
                assertion(f'good<{target}>("{value}", {value} as {target})')
            for text in (str(high + 1), str(low - 1), str(-high - 2) if signed else '-' + str(high + 1)):
                assertion(f'bad<{target}>("{text}", true)')
            assertion(f'good<{target}>("+1", 1 as {target})')
            assertion(f'good<{target}>("+0001", 1 as {target})')
            if not signed:
                assertion(f'bad<{target}>("-0", true)')
            for text in ('', ' 1', '1 ', '\\t1', '0x10', '1_0', '1.0', '+', '--1', '12x', 'é', '1\\0'):
                assertion(f'bad<{target}>("{text}", false)')
    for target, suffix, kind, bits in (('f32', 'f', 'f', 'I'), ('f64', 'd', 'd', 'Q')):
        values = ('0', '-0.0', '1.5', '-2.75', '.125', '5.', '1e+3',
                  '1.1754943508222875e-38', '1.401298464324817e-45', '3.4028234663852886e38') if target == 'f32' else (
                  '0', '-0.0', '1.5', '-2.75', '.125', '5.', '1e+3',
                  '2.2250738585072014e-308', '5e-324', '1.7976931348623157e308')
        for text in values:
            expected = struct.pack('<' + kind, float(text))
            byte_text = ', '.join(str(byte) for byte in expected)
            assertion(f'floating<{target}>("{text}", [{byte_text}])')
        for text in ('1e9999', '-1e9999', '1e-9999', '-1e-9999'):
            assertion(f'bad<{target}>("{text}", true)')
        for text in ('', ' 1', '1 ', 'nan', 'inf', 'Infinity', '1e', '.', '+.', '0x1p2', '1f', '1\\0'):
            assertion(f'bad<{target}>("{text}", false)')
    # The first case is just above an f32 halfway point: parsing via f64
    # would incorrectly double-round to 1.0. Test the actual target-width path.
    assertion('floating<f32>("1.0000000596046447753906250000000000000000000000001", [1, 0, 128, 63])')
    assertion('floating<f32>("1.000000059604644775390625", [0, 0, 128, 63])')
    assertion('floating<f32>("16777217", [0, 0, 128, 75])')
    assertion('floating<f64>("9007199254740993", [0, 0, 0, 0, 0, 0, 64, 67])')
    for text in ('True', 'FALSE', '1', '0', ' true', 'true ', 'true\\0', ''):
        assertion(f'bad<bool>("{text}", false)')
    assertion('good<bool>("true", true)')
    assertion('good<bool>("false", false)')
    assertion('bad<mode>("Left", false)')
    assertion('bad<mode>(" left", false)')
    assertion('bad<mode>("wrong.left", false)')
    assertion('bad<mode>("", false)')
    numeric = root / 'vectors.void'
    numeric.write_text('''use convert
use encoding/binary

enum mode:
    left
    right

private func good<t>(text: string, expected: t) -> bool:
    if convert.parse<t>(text) is Ok(var value):
        return value == expected
    return false

private func bad<t>(text: string, range: bool) -> bool:
    if convert.parse<t>(text) is Err(var reason):
        if reason.message == "" or reason.input != text:
            return false
        if range:
            return reason.kind is convert.error_kind.out_of_range
        return reason.kind is convert.error_kind.invalid_text
    return false

private func floating<t>(text: string, expected: [u8]) -> bool:
    if convert.parse<t>(text) is Ok(var value):
        var bytes = binary.encode(value)
        for index in 0..<bytes.count:
            if bytes[index] != expected[index]:
                return false
        return true
    return false

func main() -> i32:
''' + '\n'.join(checks_source) + '\n    return 0\n', encoding='utf-8')
    console = root / 'input.void'
    console.write_text('''use console

func main() -> i32:
    print(readline("Prompt: "))
    print(readline())
    print(readline())
    print(readline())
    print(read())
    print(read(2))
    print(read(-1))
    print(read(0))
    print(read())
    print(read())
    return 0
''', encoding='utf-8')
    console_input = 'Ada🎮\r\n\nB\rC\né🎮X'.encode() + b'\xff'
    console_output = 'Prompt: Ada🎮\n\nB\nC\né\n🎮X\n\n\n\n\n'.encode()
    run([compiler, 'check', fixtures / 'async.void'])
    scenarios = [(fixtures / 'error.void', b'', b''),
                 (fixtures / 'conversion.void', b'', b'conversion passed\n'),
                 (numeric, b'', b''), (console, console_input, console_output),
                 (fixtures / 'ownership.void', b'', native_text('1\n'))]
    for index, (source, data, expected) in enumerate(scenarios):
        run([compiler, 'check', source])
        for flags in (['--no-opt'], []):
            run([compiler, 'run', source, *flags], data, expected=expected)
            executable = root / f'executable-{index}-{bool(flags)}'
            run([compiler, 'build', source, '-o', executable, *flags])
            run([executable], data, expected=expected)
            if source.name == 'ownership.void':
                for scenario in range(1, 7):
                    run([compiler, 'run', source, scenario, *flags], expected=native_text('1\n'))
                    run([executable, scenario], expected=native_text('1\n'))
    failure = root / 'unwrap.void'
    failure.write_text('use convert\nfunc main() -> i32:\n    var failed: result<i32, string> = Err("reason")\n    return convert.unwrap(failed)\n', encoding='utf-8')
    for flags in (['--no-opt'], []):
        output = run([compiler, 'run', failure, *flags], status=1)
        assert b'convert.unwrap received Err' in output.stderr, output.stderr
        executable = root / f'unwrap-{bool(flags)}'
        run([compiler, 'build', failure, '-o', executable, *flags])
        output = run([executable], status=1)
        assert b'convert.unwrap received Err' in output.stderr, output.stderr
    negatives = [
        ('use convert\n', 'var invalid = convert.parse<string>("x")'),
        ('use convert\n', 'var invalid = convert.parse<[u8]>("x")'),
        ('use convert\nstruct object:\n    value: i32\n', 'var invalid = convert.parse<object>("x")'),
        ('use convert\nenum payload:\n    case(value: i32)\n', 'var invalid = convert.parse<payload>("case")'),
        ('use convert\n', 'var invalid = _convert_parse<i32>("1")'),
        ('use io/memory\n', 'var invalid = _memory_write([], [], 0)'),
    ]
    for prefix, body in negatives:
        failure.write_text(prefix + '\nfunc main():\n    ' + body + '\n', encoding='utf-8')
        run([compiler, 'check', failure], status=1)
    failure.write_text('''use error as core
struct child extends core.error:
    extra: i32
func inner() -> result<i32, child>:
    return Err(child(message = "x"))
func outer() -> result<i32, core.error>:
    return Ok(inner()?)
func main():
    var value = outer()
''', encoding='utf-8')
    rejected = run([compiler, 'check', failure], status=1)
    assert b'exactly matching error types' in rejected.stderr, rejected.stderr
    failure.write_text('''use convert
struct object:
    value: i32
func escape(const outcome: result<object, string>) -> object:
    return convert.unwrap(outcome)
func main():
    var value: result<object, string> = Ok(object())
    var escaped = escape(value)
    escaped.value = 3
''', encoding='utf-8')
    rejected = run([compiler, 'check', failure], status=1)
    assert b'readonly' in rejected.stderr.lower(), rejected.stderr

print(f'Combined execution: {number} parsing assertions; {checks} processes passed' +
      (' with actual 1 MiB stack' if small else ''))
