"""G80 correction: concrete generic static results must feed outer inference."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile

compiler, evidence = (Path(p).resolve() for p in sys.argv[1:])
fixture = Path(__file__).with_suffix('.void').resolve()
records = []
inputs = {fixture.name: fixture.read_text()}
def run(command, expected=0, output=None, error=None):
    command = list(map(str, command))
    result = subprocess.run(command, capture_output=True, text=True, timeout=90)
    row = dict(command=command, expected=expected, actual=result.returncode,
               stdout=result.stdout, stderr=result.stderr)
    records.append(row); evidence.parent.mkdir(parents=True, exist_ok=True)
    evidence.write_text(json.dumps(dict(schema=1, inputs=inputs, records=records), indent=2) + '\n')
    assert result.returncode == expected, row
    if output is not None: assert result.stdout == output, row
    if error is not None: assert error in result.stderr, row

with tempfile.TemporaryDirectory(prefix='VOID G80 nested ') as directory:
    root = Path(directory)
    run([compiler, 'check', fixture])
    for switches in [[], ['--no-opt']]:
        run([compiler, 'run', fixture, '--no-cache', *switches], output='nested static results preserved\n')
        target = root / ('program' + str(len(switches)))
        run([compiler, 'build', fixture, '-o', target, *switches])
        run([target], output='nested static results preserved\n')
    keep = 'func keep<u>(value: u) -> u:\n    return value\n'
    ordinary = 'value struct utility:\n    static count = 0\nstatic func utility.echo<t>(value: t) -> t:\n    return value\nstatic func utility.zero() -> i32:\n    return 0\n'
    generic = 'value struct storage<owner>:\n    value: owner?\nstatic func storage<owner>.echo<t>(value: t) -> t:\n    return value\nstatic func storage<owner>.zero() -> i32:\n    return 0\n'
    negatives = {
        'extra_method_types': (ordinary + keep + 'func main():\n    print(keep(utility.echo<i32, string>(7)))\n', 'Incorrect number of generic type arguments'),
        'generic_owner_extra_types': (generic + keep + 'func main():\n    print(keep(storage<string>.echo<i32, string>(7)))\n', 'Incorrect number of generic type arguments'),
        'nongeneric_method': (ordinary + keep + 'func main():\n    print(keep(utility.zero<string>()))\n', 'not generic'),
        'nongeneric_method_generic_owner': (generic + keep + 'func main():\n    print(keep(storage<string>.zero<i32>()))\n', 'not generic'),
        'wrong_payload': (ordinary + keep + 'func main():\n    print(keep(utility.echo<i32>("wrong")))\n', 'Type mismatch'),
        'nominal_payload': ('struct left:\n    value: i32\nstruct right:\n    value: i32\n' + ordinary + keep + 'func main():\n    print(keep(utility.echo<left>(right(value = 1))))\n', 'Type mismatch'),
        'private_owner': ('value struct utility:\n    static count = 0\nprivate static func utility.echo<t>(value: t) -> t:\n    return value\nfunc main():\n    print(utility.echo(7))\n', 'not accessible'),
        'readonly_payload': ('struct node:\n    value: i32\nvalue struct utility:\n    static count = 0\nstatic func utility.read<t>(const value: t) -> t:\n    return value\n' + keep + 'func main():\n    const held = node(value = 7)\n    var projected = keep(utility.read(held))\n    projected.value = 9\n', 'readonly'),
    }
    for name, (source, error) in negatives.items():
        file = root / (name + '.void'); file.write_text(source); inputs[file.name] = source
        run([compiler, 'check', file], 1, error=error)
print(f'G80 nested static results: {len(records)} checks; ordinary/generic managed/value owners; JIT/AOT O0/O2; expected rejections retained')
