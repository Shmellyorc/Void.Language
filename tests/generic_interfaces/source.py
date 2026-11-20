"""Generic interfaces: exact nominal substitution, conformance and 1 MiB execution."""
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile

compiler, evidence = (Path(p).resolve() for p in sys.argv[1:])
root = Path(__file__).resolve().parents[2]
fixture = Path(__file__).with_name('main.void')
records = []
inputs = {str(p.relative_to(root)): p.read_text() for p in fixture.parent.glob('*.void')}

def small_stack():
    if os.name == 'posix':
        import resource
        resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))

def run(command, expected=0, output=None, error=None):
    result = subprocess.run(list(map(str, command)), capture_output=True, text=True,
                            timeout=90, preexec_fn=small_stack if os.name == 'posix' else None)
    row = dict(command=list(map(str, command)), expected=expected, actual=result.returncode,
               stdout=result.stdout, stderr=result.stderr, stack_bytes=1048576 if os.name == 'posix' else None)
    records.append(row)
    evidence.parent.mkdir(parents=True, exist_ok=True)
    evidence.write_text(json.dumps(dict(schema=1, inputs=inputs, records=records), indent=2) + '\n')
    assert result.returncode == expected, row
    if output is not None: assert result.stdout == output, row
    if error is not None: assert error.lower() in result.stderr.lower(), row

interface = 'interface reader<t>:\n    func read() -> t\n'
provider = 'struct cell:\n    n: i32\nfunc (c cell) read() -> i32:\n    return c.n\n'
main = 'func main():\n'
negatives = {
    'expanding_requirements': ('interface expanding<t>:\n    func next() -> expanding<[t]>?\n' + main + '    var view: expanding<i32>? = null\n', 'specialization dependency depth limit'),
    'expanding_header': ('interface expanding<t>:\n    func next() -> expanding<[t]>?\nfunc consume(const value: expanding<i32>):\n    return\n' + main + '    return\n', 'specialization dependency depth limit'),
    'expanding_constraints': ('interface expanding<t: expanding<[t]>>:\n    func read() -> t\n' + main + '    var view: expanding<i32>? = null\n', 'constraint'),
    'wrong_result': (interface + provider + main + '    var r: reader<string> = cell()\n', 'signature mismatch'),
    'distinct_specializations': (interface + provider + main + '    var r: reader<i32> = cell()\n    var s: reader<u32> = r\n', 'Type mismatch'),
    'missing_argument': (interface + main + '    var r: reader = null\n', 'generic'),
    'extra_argument': (interface + main + '    var r: reader<i32, string>? = null\n', 'number of generic type arguments'),
    'invented_argument': ('interface reader:\n    func read() -> i32\n' + main + '    var r: reader<i32>? = null\n', 'not a generic'),
    'unknown_argument': (interface + main + '    var r: reader<missing>? = null\n', 'Unsupported type'),
    'no_value_boxing': (interface + provider.replace('struct cell:', 'value struct cell:') + main + '    var r: reader<i32> = cell()\n', 'standalone value erasure'),
    'nullable_private_payload': ('private value struct hidden:\n    n: i32\n' + interface + main + '    var view: reader<hidden?>? = null\n', 'less-visible'),
    'less_visible_requirement': ('private struct hidden:\n    value: i32\ninterface reader<t>:\n    func read() -> hidden\n' + main + '    return\n', 'less-visible'),
    'receiver_on_interface': (interface + 'func (r reader<t>) extra() -> i32:\n    return 0\n' + main + '    return\n', 'Interfaces declare requirements'),
    'constraint_owner_mismatch': (interface + provider + 'struct holder<t>:\n    n: t\nfunc (h holder<t>) read<s: reader<t>>(value: s) -> t:\n    return value.read()\n' + main + '    print(holder(n = \"bad\").read(cell()))\n', 'constraint'),
    'duplicate_requirement': ('interface reader<t>:\n    func read() -> t\n    func read() -> t\n' + main + '    return\n', 'Duplicate'),
    'default_requirement': ('interface reader<t>:\n    func read(value: t = 0) -> t\n' + main + '    return\n', 'default'),
    'body_requirement': ('interface reader<t>:\n    func read() -> t:\n        return 0\n' + main + '    return\n', 'Expected'),
    'generic_requirement': ('interface reader<t>:\n    func read<u>() -> u\n' + main + '    return\n', 'Expected'),
    'async_requirement': ('interface reader<t>:\n    async func read() -> t\n' + main + '    return\n', 'Async interface'),
    'unknown_header_type': ('interface reader<t>:\n    func read() -> missing\n' + main + '    return\n', 'Unsupported type'),
    'noninterface_constraint': ('struct thing<t>:\n    n: t\nfunc read<t, s: thing<t>>(n: t, value: s):\n    return\n' + main + '    return\n', 'constraint'),
    'nullable_constraint': (interface + 'func read<s: reader<i32>?>(value: s):\n    return\n' + main + '    return\n', 'non-nullable'),
    'wrong_constraint': (interface + provider + 'func read<t, s: reader<t>>(seed: t, value: s) -> t:\n    return value.read()\n' + main + '    print(read("wrong", cell()))\n', 'constraint'),
    'wrong_parameter_const': ('interface sink<t>:\n    func put(const value: t)\nstruct cell:\n    n: i32\nfunc (c cell) put(value: i32):\n    c.n = value\n' + main + '    var view: sink<i32> = cell()\n', 'signature mismatch'),
    'ambiguous_promotion': (interface + provider + 'struct both:\n    using left: cell\n    using right: cell\n' + main + '    var view: reader<i32> = both()\n', 'Ambiguous'),
    'readonly': ('interface counter<t>:\n    func add(value: t)\nstruct cell:\n    n: i32\nfunc (c cell) add(value: i32):\n    c.n += value\n' + main + '    const c = cell()\n    var view: counter<i32> = c\n    view.add(1)\n', 'readonly'),
    'unwrapped_nullable': (interface + main + '    var r: reader<i32>? = null\n    print(r.read())\n', 'Unwrap'),
    'construct_interface': (interface + main + '    var r = reader<i32>()\n', 'cannot be constructed'),
}
with tempfile.TemporaryDirectory(prefix='void-generic-interfaces-') as directory:
    temporary = Path(directory)
    run([compiler, 'check', fixture])
    run([compiler, 'run', root / 'examples/interfaces/generic.void', '--no-cache'], output='42\nhello\n42\n42\n')
    for switches in [[], ['--no-opt']]:
        run([compiler, 'run', fixture, '--no-cache', *switches], output='generic interfaces preserved\n')
        target = temporary / ('main' + str(len(switches)))
        run([compiler, 'build', fixture, '-o', target, *switches])
        run([target], output='generic interfaces preserved\n')
    # File-private providers remain usable in their owner, but not at an import site.
    private_library = interface + provider.replace('func (c cell)', 'private func (c cell)')
    library = temporary / 'private_api.void'; library.write_text(private_library); inputs['private_api.void'] = private_library
    imported = 'use "./private_api.void" as api\n' + main + '    var view: api.reader<i32> = api.cell()\n'
    file = temporary / 'private_import.void'; file.write_text(imported); inputs[file.name] = imported
    run([compiler, 'check', file], 1, error='requirement')
    hidden_interface = private_library.replace('interface reader<t>', 'private interface reader<t>')
    hidden = temporary / 'private_interface_api.void'; hidden.write_text(hidden_interface); inputs[hidden.name] = hidden_interface
    hidden_main = imported.replace('private_api.void', 'private_interface_api.void')
    file = temporary / 'private_interface.void'; file.write_text(hidden_main); inputs[file.name] = hidden_main
    run([compiler, 'check', file], 1, error='not a generic type')
    private_owner = private_library + main + '    var view: reader<i32> = cell(n = 7)\n    print(view.read())\n'
    file = temporary / 'private_owner.void'; file.write_text(private_owner); inputs[file.name] = private_owner
    run([compiler, 'run', file, '--no-cache'], output='7\n')
    recursive = 'interface chain<t>:\n    func next() -> chain<t>?\n    func read() -> t\nstruct end:\n    n: i32\nfunc (e end) next() -> chain<i32>?:\n    return null\nfunc (e end) read() -> i32:\n    return e.n\n' + main + '    var value: chain<i32> = end(n = 9)\n    if value.next() != null:\n        print("wrong")\n    print(value.read())\n'
    file = temporary / 'recursive.void'; file.write_text(recursive); inputs[file.name] = recursive
    run([compiler, 'run', file, '--no-cache'], output='9\n')
    constrained_interface = interface + provider + 'interface factory<t: reader<i32>>:\n    func create() -> t\nstruct maker:\n\nfunc (m maker) create() -> cell:\n    return cell(n = 4)\n' + main + '    var f: factory<cell> = maker()\n    print(f.create().n)\n'
    file = temporary / 'constrained_interface.void'; file.write_text(constrained_interface); inputs[file.name] = constrained_interface
    run([compiler, 'run', file, '--no-cache'], output='4\n')
    bad = constrained_interface.replace('factory<cell> = maker()', 'factory<i32> = maker()')
    file = temporary / 'bad_interface_constraint.void'; file.write_text(bad); inputs[file.name] = bad
    run([compiler, 'check', file], 1, error='constraint')
    self_constraint = 'interface same<t: same<t>>:\n    func read() -> t\nstruct node:\n    n: i32\nfunc (n node) read() -> node:\n    return n\nfunc main():\n    var view: same<node> = node(n = 8)\n    print(view.read().n)\n'
    file = temporary / 'self_constraint.void'; file.write_text(self_constraint); inputs[file.name] = self_constraint
    for flags in ([], ['--no-opt']):
        run([compiler, 'run', file, '--no-cache', *flags], output='8\n')
    finite = ''.join(f'interface node{i}<t>:\n    func next() -> node{i+1}<t>?\n' for i in range(96)) + 'interface node96<t>:\n    func read() -> t\n' + main + '    var view: node0<i32>? = null\n    print(view == null)\n'
    file = temporary / 'finite_chain.void'; file.write_text(finite); inputs[file.name] = finite
    run([compiler, 'run', file, '--no-cache'], output='true\n')
    finite_cycle = 'interface flip<a, b>:\n    func swapped() -> flip<b, a>?\n' + main + '    var view: flip<i32, string>? = null\n    print(view == null)\n'
    file = temporary / 'finite_cycle.void'; file.write_text(finite_cycle); inputs[file.name] = finite_cycle
    run([compiler, 'run', file, '--no-cache'], output='true\n')
    # A has expression in a nested value default must see complete requirements.
    for name, source, output in [('has_default_missing', 'interface reader<t>:\n    func read() -> t\nvalue struct box<t>:\n    available: bool = t has reader<i32>\ninterface producer<t>:\n    func create() -> box<t>\nstruct missing:\n    n: i32\nstruct factory<t>:\n    n: i32\nfunc (f factory<t>) create() -> box<t>:\n    return box<t>()\nfunc main():\n    var view: producer<missing> = factory<missing>()\n    print(view.create().available)\n', 'false\n'), ('has_default_valid', 'interface reader<t>:\n    func read() -> t\nvalue struct box<t>:\n    available: bool = t has reader<i32>\ninterface producer<t>:\n    func create() -> box<t>\nstruct missing:\n    n: i32\nfunc (c missing) read() -> i32:\n    return c.n\nstruct factory<t>:\n    n: i32\nfunc (f factory<t>) create() -> box<t>:\n    return box<t>()\nfunc main():\n    var view: producer<missing> = factory<missing>()\n    print(view.create().available)\n', 'true\n')]:
        file = temporary / (name + ".void"); file.write_text(source); inputs[file.name] = source
        run([compiler, "run", file, "--no-cache"], output=output)
    for name, (source, error) in negatives.items():
        file = temporary / (name + '.void'); file.write_text(source); inputs[name] = source
        run([compiler, 'check', file], 1, error=error)
print(f'Generic interfaces: {len(records)} checks; JIT/AOT O0/O2 and source-located rejections; 1 MiB child stacks')
