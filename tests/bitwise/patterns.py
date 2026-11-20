"""G81 P2: shift-aware pattern lookahead, with generic constructor boundaries."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile

compiler, report = (Path(p).resolve() for p in sys.argv[1:])
fixture = Path(__file__).with_suffix('.void')
records = []
inputs = {fixture.name: fixture.read_text()}

def run(command, expected=0, output=None, diagnostic=None):
    command = list(map(str, command))
    result = subprocess.run(command, capture_output=True, text=True, timeout=90)
    record = dict(command=command, expected=expected, actual=result.returncode,
                  stdout=result.stdout, stderr=result.stderr)
    records.append(record)
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(dict(schema=1, inputs=inputs, records=records), indent=2) + '\n')
    assert result.returncode == expected, record
    if output is not None: assert result.stdout == output, record
    if diagnostic is not None:
        messages = '\n'.join(line.split(': error: ', 1)[1] for line in result.stderr.splitlines() if ': error: ' in line)
        assert diagnostic in messages, record

with tempfile.TemporaryDirectory(prefix='VOID G81 patterns ') as directory:
    root = Path(directory)
    run([compiler, 'check', fixture])
    for options in (['--no-opt'], []):
        run([compiler, 'run', fixture, '--no-cache', *options], output='shift patterns and generic boundaries preserved\n')
        binary = root / ('patterns' + ('.O0' if options else '.O2'))
        run([compiler, 'build', fixture, '-o', binary, *options])
        run([binary], output='shift patterns and generic boundaries preserved\n')
    header = '''value struct factory<t>:
    item: t?
static func factory<t>.echo<u>(value: u) -> u:
    return value
enum packet<t>:
    empty
    value(data: t)
enum plain:
    one
func probe() -> i32:
    return 1
func identity<t>(value: t) -> t:
    return value
'''
    negatives = {
        'separated_left': ('if 2 is probe() < < 1:\n        pass', 'Expected an expression'),
        'separated_right': ('if 1 is probe() > > 0:\n        pass', 'Expected an expression'),
        'bool_call_shift': ('if 2 is identity<bool>(true) << 1:\n        pass', 'Shifts require integer'),
        'float_count': ('if 2 is probe() << 1.0:\n        pass', 'Shifts require integer'),
        'nongeneric_call': ('if 1 is probe<i32>() << 0:\n        pass', 'not generic'),
        'generic_arity': ('if 1 is identity<i32, u8>(1) >> 0:\n        pass', 'Incorrect number of generic type arguments'),
        'nongeneric_enum': ('var held = plain.one\n    if held is plain<i32>(_):\n        pass', 'not generic'),
        'nominal_enum': ('var held = packet<i32>.value(1)\n    if held is packet<string>.value(_):\n        pass', 'different nominal type'),
        'method_arity': ('if 1 is factory<i32>.echo<i32, u8>(1) >> 0:\n        pass', 'Incorrect number of generic type arguments'),
        'method_type': ('if 1 is factory<i32>.echo<string>(1) << 0:\n        pass', 'Type mismatch'),
        'generic_enum_arity': ('var held = packet<i32>.value(1)\n    if held is packet<i32, u8>.value(_):\n        pass', 'Incorrect number of generic type arguments'),
    }
    for name, (body, message) in negatives.items():
        file = root / (name + '.void')
        source = header + 'func main():\n    ' + body + '\n'
        file.write_text(source); inputs[file.name] = source
        run([compiler, 'check', file], 1, diagnostic=message)
print(f'G81 pattern correction: {len(records)} checks; is/match shifts, nested generics and expected rejections; JIT/AOT O0/O2')
