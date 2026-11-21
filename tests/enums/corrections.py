"""G78 review regressions: exact pattern identity and pure callable constants."""
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile

compiler, evidence = map(lambda value: Path(value).resolve(), sys.argv[1:])
records = []

def invoke(argv, expected=0, message=None):
    result = subprocess.run(list(map(str, argv)), capture_output=True, text=True, timeout=90)
    row = dict(argv=list(map(str, argv)), expected=expected, actual=result.returncode,
               stdout=result.stdout, stderr=result.stderr)
    records.append(row)
    evidence.parent.mkdir(parents=True, exist_ok=True)
    evidence.write_text(json.dumps(records, indent=2) + '\n')
    assert result.returncode == expected, row
    if message:
        diagnostic = '\n'.join(line.split(': error: ', 1)[1] for line in result.stderr.splitlines() if ': error: ' in line)
        assert message in diagnostic, row
    return result

TYPES = '''## Typed payloads for pattern identity tests.
public enum packet<t>:
    ## No payload.
    empty
    ## The supplied payload.
    ## @param data Stored value.
    value(data: t)
## A sparse signed generic enum.
public enum code<t>(i8):
    low = -2
    high = 7
## Generic flag bit combinations.
public flags enum mask<t>(u8):
    none = 0
    read
    write
'''
PATTERNS = '''use "./types.void" as api

enum tag:
    tag
    other

func main() -> i32:
    var held = api.packet<i32>.value(7)
    if not (held is api.packet<i32>(_)):
        return 1
    if held is api.packet<i32>(var whole):
        if whole != held:
            return 2
    else:
        return 3
    match held:
        api.packet<i32>.empty:
            return 4
        api.packet<i32>.value(var data):
            if data != 7:
                return 5
    var number = api.code<i32>.low
    if not (number is api.code<i32>.low) or number is api.code<i32>.high:
        return 6
    if number is api.code<i32>(var numeric):
        if numeric.value != -2:
            return 7
    else:
        return 8
    if not (number is api.code.low):
        return 12
    var bits = api.mask<i32>.read
    if not (bits is api.mask<i32>.read) or not (bits is api.mask<i32>(_)):
        return 9
    if not (bits is api.mask.read):
        return 13
    match bits:
        api.mask<i32>.read:
            pass
        _:
            return 10
    if tag.tag == tag.other or tag.other == tag.tag or tag.other.name != "other" or not tag.tag.is_defined():
        return 11
    return 0
'''
CALLABLES = '''func increase(input: i32) -> i32:
    return input + 1
func decrease(input: i32) -> i32:
    return input - 1
enum action<t>:
    empty
    callback(work: t)
const first = action<func(i32) -> i32>.callback(increase)
const second = action<func(i32) -> i32>.callback(decrease)
const same = first == first
const different = first != second
const null_one = action<(func(i32) -> i32)?>.callback(null)
const null_two = action<(func(i32) -> i32)?>.callback(null)
const present = action<(func(i32) -> i32)?>.callback(increase)
const null_equal = null_one == null_two
const null_different = null_one != present
func main() -> i32:
    if not same or not different or not null_equal or not null_different:
        return 1
    if same != (first == first) or different != (first != second):
        return 2
    if null_equal != (null_one == null_two) or null_different != (null_one != present):
        return 3
    return 0
'''
NEGATIVES = {
    'nongeneric_arguments': ('enum item:\n    value(data: i32)\nfunc main():\n    var held = item.value(7)\n    if held is item<string>.value(var data):\n        pass\n', 'not generic'),
    'nongeneric_whole_arguments': ('enum item:\n    value(data: i32)\nfunc main():\n    var held = item.value(7)\n    if held is item<string>(_):\n        pass\n', 'not generic'),
    'different_tagged_case': ('use "./types.void" as api\nfunc main():\n    var held = api.packet<i32>.value(7)\n    if held is api.packet<string>.value(_):\n        pass\n', 'different nominal type'),
    'different_whole_type': ('use "./types.void" as api\nfunc main():\n    var held = api.packet<i32>.empty\n    if held is api.packet<string>(_):\n        pass\n', 'different nominal type'),
    'different_numeric_case': ('use "./types.void" as api\nfunc main():\n    var held = api.code<i32>.low\n    if held is api.code<string>.low:\n        pass\n', 'different nominal type'),
    'different_flag_case': ('use "./types.void" as api\nfunc main():\n    var held = api.mask<i32>.read\n    if held is api.mask<string>.read:\n        pass\n', 'different nominal type'),
    'numeric_member_call': ('use "./types.void" as api\nfunc main():\n    var held = api.code<i32>.low\n    if held is api.code<i32>.low():\n        pass\n', 'omit parentheses'),
    'constant_user_call': ('func value() -> i32:\n    return 1\nenum item:\n    data(value: i32)\nconst bad = item.data(value()) == item.data(1)\n', 'explicitly marked compile func'),
    'constant_user_operator': ('value struct digit:\n    value: i32\n    operator ==(const left: digit, const right: digit) -> bool:\n        return true\nenum item:\n    data(value: digit)\nconst bad = item.data(digit(value = 1)) == item.data(digit(value = 2))\n', 'custom equality'),
}

with tempfile.TemporaryDirectory(prefix='VOID G78 review corrections ') as directory:
    root = Path(directory)
    (root / 'types.void').write_text(TYPES)
    for name, source in [('patterns', PATTERNS), ('callables', CALLABLES)]:
        file = root / (name + '.void')
        file.write_text(source)
        invoke([compiler, 'check', file])
        for options in (['--no-opt'], []):
            invoke([compiler, 'run', file, '--no-cache', *options])
            binary = root / (name + str(len(options)) + ('.exe' if os.name == 'nt' else ''))
            invoke([compiler, 'build', file, '-o', binary, *options])
            invoke([binary])
    for name, (source, message) in NEGATIVES.items():
        file = root / (name + '.void')
        file.write_text(source)
        invoke([compiler, 'check', file], 1, message)
print(f'G78 review corrections: {len(records)} compiler/application checks; JIT/AOT O0/O2')
