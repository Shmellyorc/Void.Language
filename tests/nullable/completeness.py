"""G79: explicit nullable presence, ownership-facing contracts and checked errors."""
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile

compiler, evidence = map(lambda value: Path(value).resolve(), sys.argv[1:])
fixtures = Path(__file__).resolve().parent
records = []

def invoke(argv, expected=0, message=None):
    result = subprocess.run(list(map(str, argv)), capture_output=True, text=True, timeout=90)
    record = dict(argv=list(map(str, argv)), expected=expected, actual=result.returncode,
                  stdout=result.stdout, stderr=result.stderr)
    records.append(record)
    evidence.parent.mkdir(parents=True, exist_ok=True)
    evidence.write_text(json.dumps(records, indent=2) + '\n')
    assert result.returncode == expected, record
    if message:
        assert message in result.stderr, record
    return result

NEGATIVES = {
    'required_integer': ('var value: i32 = null', 'nullable'),
    'required_bool': ('var value: bool = null', 'nullable'),
    'required_string': ('var value: string = null', 'nullable'),
    'required_array': ('var value: [i32] = null', 'nullable'),
    'implicit_extraction': ('var value: i32? = 7\n    var required: i32 = value', 'Type mismatch'),
    'arithmetic': ('var value: i32? = 7\n    var invalid = value + 1', 'Unwrap nullable'),
    'condition': ('var value: bool? = true\n    if value:\n        return 1', 'Type mismatch'),
    'member': ('var value: string? = "ok"\n    var invalid = value.length', 'nullable'),
    'wrong_payload': ('var value: i32? = 1\n    var invalid = value as u32', 'Type mismatch'),
    'wrong_lift': ('var value: i32? = true', 'Type mismatch'),
    'option_distinct': ('var value: i32? = 7\n    var invalid: option<i32> = value', 'Type mismatch'),
    'result_distinct': ('var value: i32? = 7\n    var invalid: result<i32, string> = value', 'Type mismatch'),
    'readonly_lift': ('const held = node(value = 7)\n    var wrapped: carrier? = carrier(child = held)\n    var extracted = wrapped as carrier\n    extracted.child.value = 9', 'readonly'),
    'readonly_pattern': ('const held: carrier? = carrier(child = node(value = 7))\n    if held is Ok(var value):\n        value.child.value = 9', 'readonly'),
    'readonly_try': ('const held: carrier? = carrier(child = node(value = 7))\n    if (held as? carrier) is Some(var value):\n        value.child.value = 9', 'readonly'),
    'readonly_list': ('const held: [node]? = [node(value = 7)]\n    var values = held as [node]\n    values[0].value = 9', 'readonly'),
    'nominal': ('var value: one? = one.a\n    var invalid: two? = value', 'Type mismatch'),
    'double_suffix': ('var value: (i32?)? = null', 'second ?'),
    'constant_null_unwrap': ('var unused = 0', None),
}
PREFIX = '''struct node:
    value: i32
value struct carrier:
    child: node
enum one:
    a
enum two:
    a
'''

with tempfile.TemporaryDirectory(prefix='VOID G79 nullable ') as directory:
    root = Path(directory)
    for name in ('values', 'conversions', 'references', 'primitives'):
        source = fixtures / (name + '.void')
        invoke([compiler, 'check', source])
        for level in ('O0', 'O2'):
            options = ['--no-opt'] if level == 'O0' else []
            invoke([compiler, 'run', source, '--no-cache', *options])
            target = root / (name + level + ('.exe' if os.name == 'nt' else ''))
            invoke([compiler, 'build', source, '-o', target, *options])
            invoke([target])
    for name, (body, message) in NEGATIVES.items():
        source = root / (name + '.void')
        text = PREFIX + 'func main() -> i32:\n    ' + body + '\n    return 0\n'
        if name == 'constant_null_unwrap':
            text = 'const absent: i32? = null\nconst invalid = absent as i32\nfunc main() -> i32:\n    return 0\n'
            message = 'Null value in checked conversion'
        source.write_text(text)
        invoke([compiler, 'check', source], 1, message)
    for name, declarations in {
        'direct_inline': 'value struct recursive:\n    next: recursive?\n',
        'mutual_inline': 'value struct first:\n    next: second?\nvalue struct second:\n    next: first?\n',
        'option_inline': 'value struct recursive:\n    next: option<recursive>\n',
        'fixed_inline': 'value struct recursive:\n    next: [recursive?, 1]\n',
    }.items():
        source = root / (name + '.void')
        source.write_text(declarations + 'func main() -> i32:\n    return 0\n')
        invoke([compiler, 'check', source], 1, 'Recursive value layout')
    for type_name in ('i32', 'string', '[string]', 'carrier', 'one'):
        source = root / ('null_' + str(len(records)) + '.void')
        source.write_text(PREFIX + f'func main() -> i32:\n    var absent: {type_name}? = null\n    var invalid = absent as {type_name}\n    return 0\n')
        for level in ('O0', 'O2'):
            options = ['--no-opt'] if level == 'O0' else []
            invoke([compiler, 'run', source, '--no-cache', *options], 1, 'Null value in checked conversion')
            target = root / ('negative' + str(len(records)))
            invoke([compiler, 'build', source, '-o', target, *options])
            invoke([target], 1, 'Null value in checked conversion')
print(f'G79 nullable: {len(records)} checks, JIT/AOT O0/O2; expected failures retained')
