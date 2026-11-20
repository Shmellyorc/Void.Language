"""G81 P1: static type selectors must not enter constructor value inference."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile

compiler, report = (Path(p).resolve() for p in sys.argv[1:])
fixture = Path(__file__).with_suffix('.void')
library = fixture.with_name('imported_owners_library.void')
inputs = {fixture.name: fixture.read_text(), library.name: library.read_text()}
records = []

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

with tempfile.TemporaryDirectory(prefix='VOID G81 imported owners ') as directory:
    root = Path(directory)
    run([compiler, 'check', fixture])
    for options in (['--no-opt'], []):
        run([compiler, 'run', fixture, '--no-cache', *options], output='imported generic owners preserved\n')
        binary = root / ('imported-owners' + ('.O0' if options else '.O2'))
        run([compiler, 'build', fixture, '-o', binary, *options])
        run([binary], output='imported generic owners preserved\n')
    (root / library.name).write_text(library.read_text())
    header = '''use "./imported_owners_library.void" as api
value struct local<t>:
    item: t
static func local<t>.echo<u>(value: u) -> u:
    return value
'''
    negatives = {
        'owner_arity': ('print(api.factory<i32, string>.one())', 'Incorrect number of generic type arguments'),
        'owner_missing': ('print(api.factory.one())', 'Cannot infer generic parameter'),
        'local_owner_missing': ('print(local.echo(1))', 'Cannot infer generic parameter'),
        'method_arity': ('print(api.factory<i32>.echo<i32, u8>(1))', 'Incorrect number of generic type arguments'),
        'nongeneric_method': ('print(api.factory<i32>.one<i32>())', 'not generic'),
        'nongeneric_owner': ('print(api.plain<i32>.one())', 'not generic'),
        'method_payload': ('print(api.factory<i32>.echo<i32>("wrong"))', 'Type mismatch'),
        'owner_payload': ('print(api.factory<i32>.same("wrong"))', 'Type mismatch'),
        'private_method': ('print(api.factory<i32>.secret())', 'not accessible'),
        'readonly_field': ('api.factory<i32>.limit = 8', 'static const'),
        'readonly_payload': ('const held = api.node(item = 7)\n    var projected = api.factory<i32>.read(held)\n    projected.item = 9', 'readonly'),
        'instance_member': ('print(api.factory<i32>.item)', 'Unknown static member'),
        'constructor_argument': ('var invalid = api.factory<i32>(missing = 3)', 'Unknown generic constructor argument'),
    }
    for name, (body, message) in negatives.items():
        file = root / (name + '.void')
        source = header + 'func main():\n    ' + body + '\n'
        file.write_text(source); inputs[file.name] = source
        run([compiler, 'check', file], 1, diagnostic=message)
print(f'G81 imported generic owners: {len(records)} checks; imported/local owners, constructors, static fields/methods; JIT/AOT O0/O2')
