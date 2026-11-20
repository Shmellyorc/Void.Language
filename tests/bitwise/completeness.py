"""G81: exact-width integer operators, masked counts, authority and generic syntax."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile

compiler, evidence = (Path(p).resolve() for p in sys.argv[1:])
records, inputs = [], {}

def run(command, expected=0, output=None, error=None):
    result = subprocess.run(list(map(str, command)), capture_output=True, text=True, timeout=90)
    row = dict(command=list(map(str, command)), expected=expected, actual=result.returncode,
               stdout=result.stdout, stderr=result.stderr)
    records.append(row)
    evidence.parent.mkdir(parents=True, exist_ok=True)
    evidence.write_text(json.dumps(dict(schema=1, inputs=inputs, records=records), indent=2) + '\n')
    assert result.returncode == expected, row
    if output is not None: assert result.stdout == output, row
    if error is not None: assert error in result.stderr, row


def signed(value, width, sign):
    value &= (1 << width) - 1
    return value - (1 << width) if sign == 'i' and value & (1 << (width - 1)) else value

with tempfile.TemporaryDirectory(prefix='VOID G81 ') as directory:
    root = Path(directory)
    declarations, checks = [], []
    types = [f'{sign}{width}' for sign in ('i', 'u') for width in (8, 16, 32, 64)]
    for kind in types:
        sign, width = kind[0], int(kind[1:])
        mask = (1 << width) - 1
        value = -(1 << (width - 1)) + 5 if sign == 'i' else mask - 4
        declarations.append(f'const folded_{kind}: {kind} = ({value} as {kind}) << -1\nconst huge_{kind}: {kind} = ({value} as {kind}) << (18446744073709551615 as u64)\n')
        for count_type in types:
            declarations.append(f'''func shift_{kind}_{count_type}(value: {kind}, count: {count_type}) -> {kind}:
    value <<= count
    value >>= count
    return value
''')
            count = -1 if count_type[0] == 'i' else (1 << int(count_type[1:])) - 1
            n = count & (width - 1)
            left = signed(value << n, width, sign)
            answer = left >> n
            checks.append(f'    if shift_{kind}_{count_type}({value}, {count}) != {answer}:\n        return 10\n')
        # Explicitly wider counts and negative counts must never truncate into
        # the left value type before the language-defined masking operation.
        for count in (0, 1, width - 1, width, width + 1, -1, -width, -width - 1, 9223372036854775807, -9223372036854775808):
            n = count & (width - 1)
            left, right = signed(value << n, width, sign), value >> n
            checks.append(f'''    if (({value} as {kind}) << ({count} as i64)) != {left}:
        return 11
    if (({value} as {kind}) >> ({count} as i64)) != {right}:
        return 12
''')
        checks.append(f'''    if folded_{kind} != {signed(value << (width-1), width, sign)} or huge_{kind} != folded_{kind}:
        return 13
    var bits_{kind}: {kind} = 6
    bits_{kind} |= 9
    bits_{kind} &= 14
    bits_{kind} ^= 3
    if bits_{kind} != 13 or ~bits_{kind} != {signed(~13,width,sign)}:
        return 14
''')
        declarations.append(f'''const right_{kind}: {kind} = ({value} as {kind}) >> ({width} as u64)
const complement_{kind}: {kind} = ~({value} as {kind})
const logical_{kind}: {kind} = (({value} as {kind}) & 7) | (2 ^ 1)
''')
        checks.append(f'''    if right_{kind} != {value} or complement_{kind} != {signed(~value,width,sign)} or logical_{kind} != {(value & 7) | 3}:
        return 15
''')
    source = root / 'widths.void'
    source.write_text('\n'.join(declarations) + '\nfunc main() -> i32:\n' + ''.join(checks) + '    print("integer widths preserved")\n    return 0\n')
    fixture = Path(__file__).with_suffix('.void')
    for source, output in [(source, 'integer widths preserved\n'), (fixture, 'integer masks and packing preserved\n'), (Path(__file__).resolve().parents[2] / 'examples/learn/integer_bitwise.void', 'packed: 1193046\nred: 18\ngreen: 52\nblue: 86\n')]:
        inputs[source.name] = source.read_text()
        run([compiler, 'check', source])
        for flags in (['--no-opt'], []):
            run([compiler, 'run', source, '--no-cache', *flags], output=output)
            target = root / (source.stem + str(len(flags)))
            run([compiler, 'build', source, '-o', target, *flags])
            run([target], output=output)
    negatives = {
        'bool_left': ('var x = true << 1', 'Shifts require integer'),
        'bool_count': ('var x = 1 >> true', 'Shifts require integer'),
        'float_left': ('var x = 1.0 << 1', 'Shifts require integer'),
        'float_count': ('var x = 1 >> 1.0', 'Shifts require integer'),
        'string': ('var x = "one" << 1', 'Shifts require integer'),
        'nullable': ('var x: i32? = 1\n    var y = x >> 1', 'Shifts require integer'),
        'compound_float': ('var x = 1.0\n    x <<= 1', 'Shifts require integer'),
        'compound_count': ('var x = 1\n    x >>= true', 'Shifts require integer'),
        'readonly_local': ('const x = 1\n    x <<= 1', 'readonly'),
        'readonly_field': ('const x = cell(value = 1)\n    x.value >>= 1', 'readonly'),
        'readonly_array': ('const x = [1]\n    x[0] <<= 1', 'readonly'),
        'mixed_width': ('var x: u8 = 1\n    var y: i32 = 1\n    var z = x | y', 'Type mismatch'),
        'bool_bits': ('var x = true & false', 'Bitwise'),
        'float_complement': ('var x = ~1.0', 'Bitwise'),
        'enum_shift': ('var x = mode.one << 1', 'Shifts require integer'),
        'flag_shift': ('var x = mask.one >> 1', 'Shifts require integer'),
        'flag_compound': ('var x = mask.one\n    x <<= 1', 'Shifts require integer'),
        'enum_count': ('var x = 1 << mode.one', 'Shifts require integer'),
        'flag_count': ('var x = 1 >> mask.one', 'Shifts require integer'),
        'ordinary_bits': ('var x = mode.one | mode.one', 'Bitwise'),
        'generic_float': ('var x = shifter<f32>(1.0)', 'Shifts require integer'),
        'wrong_result': ('var x: u64 = 1\n    var y: u8 = x << 1', 'Type mismatch'),
        'separate_angles': ('var x = 1 < < 2', 'Expected'),
    }
    header = 'func shifter<t>(value: t) -> t:\n    return value << 1\nstruct cell:\n    value: i32\nenum mode(u8):\n    one = 1\nflags enum mask(u8):\n    one\n'
    for name, (body, error) in negatives.items():
        source = root / (name + '.void')
        source.write_text(header + '\nfunc main():\n    ' + body + '\n')
        inputs[source.name] = source.read_text()
        run([compiler, 'check', source], 1, error=error)
print(f'G81: {len(records)} compiler/application checks; all integer/count widths; JIT/AOT O0/O2')
