"""G78 semantic contracts across JIT/AOT O0/O2; record genuine failures."""
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile

compiler, evidence = map(lambda p: Path(p).resolve(), sys.argv[1:])
records = []
fixtures = Path(__file__).resolve().parent

def invoke(argv, expected=0, message=None):
    result = subprocess.run(list(map(str, argv)), capture_output=True, text=True, timeout=90)
    record = dict(argv=list(map(str, argv)), status=result.returncode, stdout=result.stdout, stderr=result.stderr)
    records.append(record)
    evidence.parent.mkdir(parents=True, exist_ok=True)
    evidence.write_text(json.dumps(records, indent=2) + '\n')
    assert result.returncode == expected, record
    if message is not None:
        assert message in result.stderr, record
    return result

with tempfile.TemporaryDirectory(prefix='VOID G78 enums ') as directory:
    root = Path(directory)
    for name in ('payloads', 'generic', 'metadata', 'contracts'):
        source = fixtures / (name + '.void')
        invoke([compiler, 'check', source])
        for level in ('O0', 'O2'):
            opt = ['--no-opt'] if level == 'O0' else []
            invoke([compiler, 'run', source, '--no-cache', *opt])
            target = root / (name + level)
            invoke([compiler, 'build', source, '-o', target, *opt])
            invoke([target])
    widths = []
    checks = []
    for sign in ('u', 'i'):
        for width in (8, 16, 32, 64):
            backing = f'{sign}{width}'
            minimum = -(1 << (width - 1)) if sign == 'i' else 0
            maximum = (1 << (width - (sign == 'i'))) - 1
            widths.append(f'flags enum flag_{backing}({backing}):\n    none = 0\n    one\n    two\n')
            widths.append(f'const all_{backing} = ~flag_{backing}.none\n')
            checks.append(f'''    var bits_{backing} = ~flag_{backing}.none
    if bits_{backing} != all_{backing} or bits_{backing}.value != {(-1 if sign == 'i' else maximum)}:
        return 21
    if ({minimum} as? flag_{backing}) is Some(var min_{backing}):
        if min_{backing}.value != {minimum}:
            return 22
    else:
        return 23
    if ({maximum} as? flag_{backing}) is Some(var max_{backing}):
        if max_{backing}.value != {maximum}:
            return 24
    else:
        return 25
''')
    source = root / 'widths.void'
    source.write_text('\n'.join(widths) + '\nfunc main() -> i32:\n' + ''.join(checks) + '    return 0\n')
    invoke([compiler, 'check', source])
    for opt in (['--no-opt'], []):
        invoke([compiler, 'run', source, '--no-cache', *opt])
        binary = root / ('widths' + str(len(opt)))
        invoke([compiler, 'build', source, '-o', binary, *opt])
        invoke([binary])
    negatives = {
        'nominal': ('enum a:\n    one\nenum b:\n    one\nfunc main():\n    var wrong = a.one == b.one\n', 'Type mismatch'),
        'list_equality': ('value struct item:\n    number: i32\nenum bag:\n    list(items: [item])\nfunc main():\n    var wrong = bag.list([item()]) == bag.list([item()])\n', 'equality'),
        'float_key': ('enum bag:\n    number(x: f32)\nfunc main():\n    var wrong: {bag: i32} = {}\n', 'dictionary'),
        'list_key': ('enum bag:\n    list(items: [i32])\nfunc main():\n    var wrong: {bag: i32} = {}\n', 'dictionary'),
        'custom_key': ('value struct digit:\n    value: i32\n    operator ==(const left: digit, const right: digit) -> bool:\n        return left.value == right.value\nenum key:\n    item(data: digit)\nfunc main():\n    var wrong: {key: i32} = {}\n', 'Dictionary key contains a type defining custom operator =='),
        'ordinary_bits': ('enum mode(u8):\n    one = 1\nfunc main():\n    var wrong = mode.one | mode.one\n', 'Bitwise'),
        'float_bits': ('func main():\n    var wrong: f32 = 1\n    wrong |= 2\n', 'Bitwise'),
        'cross_flags': ('flags enum a:\n    one\nflags enum b:\n    one\nfunc main():\n    var wrong = a.one | b.one\n', 'Type mismatch'),
        'enum_cast': ('enum a(u8):\n    one = 1\nenum b(u8):\n    one = 1\nfunc main():\n    var wrong = a.one as b\n', 'distinct enums do not convert implicitly'),
        'payload_values': ('enum bag:\n    number(x: i32)\nfunc main():\n    var wrong = bag.values()\n', 'cannot invent payloads'),
        'exhausted_flags': ('flags enum mask(u8):\n    top = 128\n    beyond\n', 'Automatic flag value exceeds the backing width'),
    }
    for name, (text, message) in negatives.items():
        source = root / (name + '.void')
        source.write_text(text)
        invoke([compiler, 'check', source], 1, message)
    for name, text in {
        'missing_member': 'enum mode(u8):\n    one = 1\nfunc main():\n    var raw: u8 = 2\n    var wrong = raw as mode\n',
        'out_of_range': 'flags enum mask(u8):\n    one\nfunc main():\n    var raw: i32 = 256\n    var wrong = raw as mask\n',
    }.items():
        source = root / (name + '.void')
        source.write_text(text)
        invoke([compiler, 'check', source])
        for opt in (['--no-opt'], []):
            result = invoke([compiler, 'run', source, '--no-cache', *opt], 1, 'conversion')
            binary = root / (name + str(len(opt)))
            invoke([compiler, 'build', source, '-o', binary, *opt])
            invoke([binary], 1, 'conversion')
print(f'G78: {len(records)} checked compiler/application invocations; JIT/AOT O0/O2')
